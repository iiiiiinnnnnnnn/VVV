-- Aracore：待機・徘徊 → 発見時の威嚇 → 追跡 → 突進 → 休憩 → 次の攻撃。
-- ジャンプを選んだ場合は、着地後に残り回数を確認する。HP半分で形態変化。
-- C++：索敵、視認判定、移動、アニメーション、ジャンプの実処理。
-- Lua：いつ何をするか、待ち時間、行動の切り替えを決める。
--
-- 調整するときは「1. 調整値」と「4. 各状態での判断」を中心に読む。
-- 複数の条件が同時に成立した場合は、上に書かれた条件を優先する。

-- ============================================================================
-- 1. 調整値
-- ============================================================================
local parameters = {
    -- 索敵・視界
    SearchRange              = 35.0,   -- プレイヤーを探す距離
    LostRange                = 50.0,   -- 追跡をやめる距離
    SightRayHeight           = 2.0,    -- 視線を飛ばす位置の高さ
    SightRayLength           = 35.0,   -- 視線が届く長さ
    SightTargetHeight        = 1.0,    -- 相手のどの高さを見るか
    SightHorizontalFov       = 120.0,  -- 左右の視野角（度）
    SightVerticalFov         = 75.0,   -- 上下の視野角（度）
    TurnThreshold            = 10.0,    -- この角度よりずれていたら旋回する（度）

    -- 徘徊
    FreedomMinDistance       = 5.0,    -- 徘徊先を選ぶ最小距離
    FreedomMaxDistance       = 18.0,   -- 徘徊先を選ぶ最大距離
    FreedomMoveSpeed         = 1.4,    -- 徘徊の移動速度

    -- 待ち時間
    FreedomWaitDuration      = 1.5,    -- 徘徊を始めるまでの待機時間（秒）

    -- 戦闘行動
    ThreatDuration           = 3.0,    -- 発見時に威嚇する時間（秒）
    ChaseMoveSpeed           = 3.2,    -- 追跡の移動速度
    ChaseDuration            = 6.0,    -- 追跡してから突進準備に移るまでの時間（秒）
    TurningMoveSpeed         = 1.4,    -- 突進前に近づくときの移動速度
    ChargePrepareDuration    = 1.0,    -- 突進準備の最短時間（秒）
    ChargeMoveSpeed          = 10.5,    -- 突進の移動速度
    ChargeOverDistance       = 10.0,   -- 相手の位置を越えて走る距離
    ChargeMaxDuration        = 2.0,    -- 突進を続ける最長時間（秒）
    RecoveryDuration         = 0.8,    -- 攻撃後に休む時間（秒）

    -- ジャンプ
    JumpChance               = 0.25,   -- 休憩後にジャンプを選ぶ確率（0～1）
    JumpTakeoffDelay         = 0.65,   -- ジャンプ開始から離陸までの時間（秒）
    JumpDuration             = 2.4,    -- 空中にいる時間（秒）
    JumpHeight               = 15.0,   -- ジャンプの高さ
    JumpLandingRecovery      = 0.9,    -- 着地後に動き出すまでの時間（秒）
    JumpTerrainDeformPower   = 1.25,   -- 着地時の地形変形の強さ
    JumpTerrainDeformRadius  = 8.0,    -- 着地時の地形変形の半径

    -- 怒り状態・形態変化
    AngryIdleScale           = 0.4,    -- 怒り状態で待ち時間に掛ける倍率
    AngryJumpChance          = 0.5,    -- 怒り状態でジャンプを選ぶ確率（0～1）
    AngryJumpCount           = 2.0,    -- 怒り状態の連続ジャンプ回数
    AngryLowHpJumpCount      = 3.0,    -- HPが25％以下の連続ジャンプ回数
    PhaseThreatDuration      = 3.2,    -- 形態変化の威嚇が始まってから待つ時間（秒）
    PhaseColorChangeDuration = 5.0,    -- 形態変化で体色が変わる時間（秒）
    DeathDissolveAmount      = 0.5,   -- HP減少に応じたディゾルブの最大量
}

-- 同名のC++パラメーターへ渡す。以降はGetFloatで最新値を読む。
for name, value in pairs(parameters) do
    SetFloat(name, value)
end

-- ============================================================================
-- 2. 状態と、C++側で実行する動き
-- ============================================================================
-- Luaの状態名 → C++で登録している処理名。
-- この表は、各状態で実行する動きの対応表。判断条件は「4」に書く。
local stateActions = {
    Wait = "FreedomWait",  -- 待機
    Wander = "FreedomWander",  -- 徘徊
    Wander_Turn = "WanderTurn",  -- 徘徊中の旋回
    Threat = "Threat",  -- 発見時の威嚇
    Chase = "Chase",  -- プレイヤーを追跡
    Charge_Turn = "ChargeTurn",  -- 接近・突進方向の調整
    Charge = "Charge",  -- 突進
    Recovery = "Recovery",  -- 攻撃後の休憩
    Jump_To_Center = "JumpCenter",  -- ジャンプ
    Phase_Threat = "PhaseThreat",  -- 形態変化の威嚇
    Repeat_Jump = "JumpCenter",  -- 連続ジャンプの次の1回
}

local currentState = nil
local stateElapsed = 0
local remainingJumps = 0
local angryParametersApplied = false

-- ============================================================================
-- 3. 状態の開始・終了を扱う共通処理（通常は変更不要）
-- ============================================================================
-- Actionの第2引数は、C++のどの処理を呼ぶかを表す。
local ACTION_ENTER = 1   -- 開始時：移動先の設定や演出の開始
local ACTION_UPDATE = 2  -- 毎フレーム：移動・ジャンプなどを更新
local ACTION_LATE = 3    -- フレーム後半の更新
local ACTION_EXIT = 4    -- 終了時：移動停止や重力の復帰など

-- 指定した状態の、開始・更新・終了処理をC++に依頼する。
local function RunStateAction(stateName, actionPhase)
    local actionName = stateActions[stateName]

    if actionName then
        Action(actionName, actionPhase)
    end
end

local function ChangeState(nextState)
    if nextState == currentState then
        return
    end

    -- 今の行動を終了してから、次の行動を開始する。
    -- 連続ジャンプも、1回ごとに終了・開始を呼んで跳び直す。
    RunStateAction(currentState, ACTION_EXIT)

    currentState = nextState
    stateElapsed = 0
    SetFloat("StateTime", 0)
    SetBool("ActionFinished", false)
    SetState(currentState)
    RunStateAction(nextState, ACTION_ENTER)

    -- 休憩に入った時点で、次にジャンプするか抽選する。
    if currentState == "Recovery" then
        remainingJumps = 0
        local jumpChance = math.max(0, math.min(1, GetFloat("JumpChance")))
        local shouldJump = math.random() < jumpChance
        SetBool("ShouldJump", shouldJump)

    -- ジャンプ開始時に連続回数を決め、今回の1回分を消費する。
    elseif currentState == "Jump_To_Center" or currentState == "Repeat_Jump" then
        if remainingJumps <= 0 then
            remainingJumps = 1

            if GetBool("Angry") then
                local countParameter = "AngryJumpCount"

                if GetBool("LowHealth") then
                    countParameter = "AngryLowHpJumpCount"
                end

                local jumpCount = math.floor(GetFloat(countParameter))
                remainingJumps = math.max(1, math.min(5, jumpCount))
            end
        end

        remainingJumps = remainingJumps - 1
    end
end

-- ============================================================================
-- 4. 各状態での判断（条件は上から順に評価）
-- ============================================================================
-- 待機
local function UpdateWaiting()
    -- 今の行動が完了したら、徘徊へ進む。
    if GetBool("ActionFinished") then
        ChangeState("Wander")
        return
    end

    -- HP減少による形態変化が可能なら、威嚇演出へ進む。
    if GetBool("PhaseChangeRequested") then
        ChangeState("Phase_Threat")
        return
    end

    -- 索敵範囲内のプレイヤーが見えたら、発見時の威嚇を始める。
    if GetBool("IsTargetInSearchRange") and GetBool("IsTargetVisible") then
        ChangeState("Threat")
        return
    end
end

-- 徘徊
local function UpdateWandering()
    -- 進行方向への角度が大きい間は、旋回へ切り替える。
    if GetBool("IsTurning") then
        ChangeState("Wander_Turn")
        return
    end

    -- 目的地への移動が終わったら、次の行動へ進む。
    if not GetBool("HasDestination") then
        ChangeState("Wait")
        return
    end

    -- HP減少による形態変化が可能なら、威嚇演出へ進む。
    if GetBool("PhaseChangeRequested") then
        ChangeState("Phase_Threat")
        return
    end

    -- 索敵範囲内のプレイヤーが見えたら、発見時の威嚇を始める。
    if GetBool("IsTargetInSearchRange") and GetBool("IsTargetVisible") then
        ChangeState("Threat")
        return
    end
end

-- 徘徊中の旋回
local function UpdateWanderTurning()
    -- 向きが合い、目的地も残っていたら徘徊に戻る。
    if not GetBool("IsTurning") and GetBool("HasDestination") then
        ChangeState("Wander")
        return
    end

    -- 目的地への移動が終わったら、次の行動へ進む。
    if not GetBool("HasDestination") then
        ChangeState("Wait")
        return
    end

    -- HP減少による形態変化が可能なら、威嚇演出へ進む。
    if GetBool("PhaseChangeRequested") then
        ChangeState("Phase_Threat")
        return
    end

    -- 索敵範囲内のプレイヤーが見えたら、発見時の威嚇を始める。
    if GetBool("IsTargetInSearchRange") and GetBool("IsTargetVisible") then
        ChangeState("Threat")
        return
    end
end

-- 発見時の威嚇
local function UpdateThreat()
    -- HP減少による形態変化が可能なら、威嚇演出へ進む。
    if GetBool("PhaseChangeRequested") then
        ChangeState("Phase_Threat")
        return
    end

    -- 追跡範囲の外に出たら、待機に戻る。
    if not GetBool("IsTargetInLostRange") then
        ChangeState("Wait")
        return
    end

    -- 今の行動が完了したら、プレイヤーを追跡へ進む。
    if GetBool("ActionFinished") then
        ChangeState("Chase")
        return
    end
end

-- プレイヤーを追跡
local function UpdateChasing()
    -- 今の行動が完了したら、接近・突進方向の調整へ進む。
    if GetBool("ActionFinished") then
        ChangeState("Charge_Turn")
        return
    end

    -- HP減少による形態変化が可能なら、威嚇演出へ進む。
    if GetBool("PhaseChangeRequested") then
        ChangeState("Phase_Threat")
        return
    end

    -- 追跡範囲の外に出たら、待機に戻る。
    if not GetBool("IsTargetInLostRange") then
        ChangeState("Wait")
        return
    end
end

-- 接近・突進方向の調整
local function UpdateChargePreparation()
    -- 準備時間が過ぎ、相手の方向も向けたら突進する。
    if GetBool("ActionFinished") and GetBool("IsFacingTarget") then
        ChangeState("Charge")
        return
    end

    -- HP減少による形態変化が可能なら、威嚇演出へ進む。
    if GetBool("PhaseChangeRequested") then
        ChangeState("Phase_Threat")
        return
    end

    -- 追跡範囲の外に出たら、待機に戻る。
    if not GetBool("IsTargetInLostRange") then
        ChangeState("Wait")
        return
    end
end

-- 突進
local function UpdateCharging()
    -- 目的地への移動が終わったら、次の行動へ進む。
    if not GetBool("HasDestination") then
        ChangeState("Recovery")
        return
    end

    -- 今の行動が完了したら、攻撃後の休憩へ進む。
    if GetBool("ActionFinished") then
        ChangeState("Recovery")
        return
    end

    -- HP減少による形態変化が可能なら、威嚇演出へ進む。
    if GetBool("PhaseChangeRequested") then
        ChangeState("Phase_Threat")
        return
    end

    -- 追跡範囲の外に出たら、待機に戻る。
    if not GetBool("IsTargetInLostRange") then
        ChangeState("Wait")
        return
    end
end

-- 攻撃後の休憩
local function UpdateRecovery()
    -- 休憩が終わり、ジャンプを選んでいたら跳ぶ。
    if GetBool("ActionFinished") and GetBool("ShouldJump") then
        ChangeState("Jump_To_Center")
        return
    end

    -- ジャンプしない場合は、次の突進を準備する。
    if GetBool("ActionFinished") and not GetBool("ShouldJump") then
        ChangeState("Charge_Turn")
        return
    end

    -- HP減少による形態変化が可能なら、威嚇演出へ進む。
    if GetBool("PhaseChangeRequested") then
        ChangeState("Phase_Threat")
        return
    end

    -- 追跡範囲の外に出たら、待機に戻る。
    if not GetBool("IsTargetInLostRange") then
        ChangeState("Wait")
        return
    end
end

-- ジャンプ
local function UpdateJumping()
    -- 着地後の待ち時間が終わり、残り回数があればもう一度跳ぶ。
    if GetBool("ActionFinished") and GetBool("ContinueJump") then
        ChangeState("Repeat_Jump")
        return
    end

    -- 連続ジャンプが終わったら、次の突進を準備する。
    if GetBool("ActionFinished") and not GetBool("ContinueJump") then
        ChangeState("Charge_Turn")
        return
    end

    -- HP減少による形態変化が可能なら、威嚇演出へ進む。
    if GetBool("PhaseChangeRequested") then
        ChangeState("Phase_Threat")
        return
    end

    -- 追跡範囲の外に出たら、待機に戻る。
    if not GetBool("IsTargetInLostRange") then
        ChangeState("Wait")
        return
    end
end

-- 形態変化の威嚇
local function UpdatePhaseThreat()
    -- 今の行動が完了したら、プレイヤーを追跡へ進む。
    if GetBool("ActionFinished") then
        ChangeState("Chase")
        return
    end
end

-- 連続ジャンプの次の1回
local function UpdateRepeatedJump()
    -- 着地後の待ち時間が終わり、残り回数があればもう一度跳ぶ。
    if GetBool("ActionFinished") and GetBool("ContinueJump") then
        ChangeState("Jump_To_Center")
        return
    end

    -- 連続ジャンプが終わったら、次の突進を準備する。
    if GetBool("ActionFinished") and not GetBool("ContinueJump") then
        ChangeState("Charge_Turn")
        return
    end

    -- HP減少による形態変化が可能なら、威嚇演出へ進む。
    if GetBool("PhaseChangeRequested") then
        ChangeState("Phase_Threat")
        return
    end

    -- 追跡範囲の外に出たら、待機に戻る。
    if not GetBool("IsTargetInLostRange") then
        ChangeState("Wait")
        return
    end
end

-- 現在の状態に対応する判断処理を選ぶ。
local stateUpdates = {
    Wait = UpdateWaiting,
    Wander = UpdateWandering,
    Wander_Turn = UpdateWanderTurning,
    Threat = UpdateThreat,
    Chase = UpdateChasing,
    Charge_Turn = UpdateChargePreparation,
    Charge = UpdateCharging,
    Recovery = UpdateRecovery,
    Jump_To_Center = UpdateJumping,
    Phase_Threat = UpdatePhaseThreat,
    Repeat_Jump = UpdateRepeatedJump,
}

-- ============================================================================
-- 5. HP・形態変化・行動完了の判定
-- ============================================================================
local function UpdateHealthAndPhaseChange()
    local healthRatio = GetFloat("Life") / math.max(1, GetFloat("MaxLife"))
    local phaseChangeQueued = not GetBool("Angry") and healthRatio <= 0.5

    SetBool("LowHealth", healthRatio <= 0.25)
    SetBool("PhaseChangeQueued", phaseChangeQueued)

    -- 空中など、C++側が切り替え不可としている間は形態変化を待つ。
    SetBool("PhaseChangeRequested", phaseChangeQueued and GetBool("CanChangePhase"))
end

local function ApplyAngryParametersOnce()
    if not GetBool("Angry") or angryParametersApplied then
        return
    end

    -- 倍率を毎フレーム掛け続けないよう、怒り状態に入った後に一度だけ適用する。
    angryParametersApplied = true
    SetTrackingTurnSpeed(4)

    local durationScale = math.max(0.1, math.min(1, GetFloat("AngryIdleScale")))
    local shortenedDurations = {
        "FreedomWaitDuration",
        "ChaseDuration",
        "ChargePrepareDuration",
        "RecoveryDuration",
        "JumpLandingRecovery",
    }

    for _, parameterName in ipairs(shortenedDurations) do
        SetFloat(parameterName, GetFloat(parameterName) * durationScale)
    end

    SetFloat("JumpChance", GetFloat("AngryJumpChance"))
end

-- 時間だけで完了できる行動と、その時間を指定するパラメーター。
local actionDurations = {
    Wait = "FreedomWaitDuration",
    Threat = "ThreatDuration",
    Chase = "ChaseDuration",
    Charge_Turn = "ChargePrepareDuration",
    Charge = "ChargeMaxDuration",
    Recovery = "RecoveryDuration",
}

local function UpdateActionCompletion()
    local durationParameter = actionDurations[currentState]

    if durationParameter then
        local duration = GetFloat(durationParameter)
        SetBool("ActionFinished", stateElapsed >= duration)
    end

    -- 追跡は、時間切れ前でも距離5未満まで近づいたら完了する。
    if currentState == "Chase" and GetBool("CloseToTarget") then
        SetBool("ActionFinished", true)
    end

    if currentState == "Jump_To_Center" or currentState == "Repeat_Jump" then
        local takeoffDelay = math.max(0, GetFloat("JumpTakeoffDelay"))
        local flightDuration = math.max(0.01, GetFloat("JumpDuration"))
        local landingRecovery = math.max(0, GetFloat("JumpLandingRecovery"))
        local finishedTime = takeoffDelay + flightDuration + landingRecovery

        -- 時間だけで終えず、C++から着地済みの通知が来るまで待つ。
        local jumpFinished = GetBool("JumpLanded") and stateElapsed >= finishedTime
        local continueJump = remainingJumps > 0 and not GetBool("PhaseChangeQueued")

        SetBool("ActionFinished", jumpFinished)
        SetBool("ContinueJump", continueJump)
    elseif currentState == "Phase_Threat" then
        -- 威嚇アニメーションが実際に始まってからの時間で判定する。
        local presentationStarted = GetBool("PhasePresentationStarted")
        local elapsedEnough = GetFloat("PhaseThreatElapsed") >= GetFloat("PhaseThreatDuration")

        SetBool("ActionFinished", presentationStarted and elapsedEnough)
    end
end

-- ============================================================================
-- 6. C++から呼ばれる入口
-- ============================================================================
-- 停止・無効化・再読み込み時。現在の行動を終了し、動作を後始末する。
function Stop()
    RunStateAction(currentState, ACTION_EXIT)
    StopMovement()

    currentState = nil
    stateElapsed = 0
    SetState("Stopped")
end

-- ゲーム開始・再読み込み時は、待機状態から始める。
function Start()
    Stop()
    ChangeState("Wait")
end

-- dtは前フレームから経過した秒数。
function Update(dt)
    if not currentState then
        Start()
    end

    stateElapsed = stateElapsed + dt
    SetFloat("StateTime", stateElapsed)

    -- 1. HPと形態変化の条件を更新する。
    UpdateHealthAndPhaseChange()
    ApplyAngryParametersOnce()

    -- 2. 近距離かどうかを、C++側の移動処理にも伝える。
    local closeToTarget = GetBool("HasTarget") and GetFloat("TargetDistance") < 5
    SetBool("CloseToTarget", closeToTarget)

    -- 現在の行動の実処理をC++で進め、その結果を見て次の状態を判断する。
    RunStateAction(currentState, ACTION_UPDATE)

    UpdateActionCompletion()

    local updateState = stateUpdates[currentState]
    updateState()
end

-- フレーム後半に必要なC++側の処理を呼ぶ。
function LateUpdate()
    RunStateAction(currentState, ACTION_LATE)
end
