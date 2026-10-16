-- Deer：待機 → 徘徊 → 発見 → 向き直り → 構え → 突進 → 休憩。
-- C++：索敵、視認判定、移動、アニメーション、ジャンプの実処理。
-- Lua：いつ何をするか、待ち時間、行動の切り替えを決める。
--
-- 調整するときは「1. 調整値」と「4. 各状態での判断」を中心に読む。
-- 複数の条件が同時に成立した場合は、上に書かれた条件を優先する。

-- ============================================================================
-- 1. 調整値
-- ============================================================================
local parameters = {
    -- 待ち時間
    FreedomWaitDuration   = 2.0,    -- 徘徊を始めるまでの待機時間（秒）
    AttackPrepareDuration = 1.3,    -- 突進前に構える時間（秒）
    RecoveryDuration      = 0.8,    -- 攻撃後に休む時間（秒）

    -- 索敵・視界
    SearchRange           = 20.0,   -- プレイヤーを探す距離
    LostRange             = 30.0,   -- 追跡をやめる距離
    SightRayHeight        = 1.0,    -- 視線を飛ばす位置の高さ
    SightRayLength        = 30.0,   -- 視線が届く長さ
    SightTargetHeight     = 1.0,    -- 相手のどの高さを見るか
    SightHorizontalFov    = 90.0,   -- 左右の視野角（度）
    SightVerticalFov      = 60.0,   -- 上下の視野角（度）
    TurnThreshold         = 15.0,   -- この角度よりずれていたら旋回する（度）

    -- 徘徊
    FreedomMinDistance    = 3.0,    -- 徘徊先を選ぶ最小距離
    FreedomMaxDistance    = 10.0,   -- 徘徊先を選ぶ最大距離
    FreedomMoveSpeed      = 1.0,    -- 徘徊の移動速度

    -- 突進
    AttackMoveSpeed       = 7.0,    -- 突進の移動速度
    AttackOverDistance    = 30.0,   -- 相手の位置を越えて走る距離
}

-- 同名のC++パラメーターへ渡す。以降はGetFloatで最新値を読む。
for name, value in pairs(parameters) do
    SetFloat(name, value)
end

-- ============================================================================
-- 2. 状態と、C++側で実行する動き
-- ============================================================================
-- 配列の先頭は共通処理、末尾はその状態固有の処理。
-- 空文字は共通処理なし。ここで使う名前はC++側の登録名と一致させる。
local stateActions = {
    Wait = { "Freedom", "FreedomWait" },  -- 待機
    Wander = { "Freedom", "FreedomWander" },  -- 徘徊
    Turn = { "Freedom", "Turn" },  -- 徘徊中の旋回
    Face_Target = { "Attacking", "Turn" },  -- 相手へ向き直る
    Attack_Prepare = { "Attacking", "FaceTarget" },  -- 突進の構え
    Chase = { "Attacking", "AttackChase" },  -- 突進
    Recovery = { "Attacking", "Stop" },  -- 攻撃後の休憩
}

local currentState = nil
local stateElapsed = 0

-- ============================================================================
-- 3. 状態の開始・終了を扱う共通処理（通常は変更不要）
-- ============================================================================
-- Actionの第2引数は、C++のどの処理を呼ぶかを表す。
local ACTION_ENTER = 1   -- 開始時：移動先の設定や演出の開始
local ACTION_UPDATE = 2  -- 毎フレーム：移動・ジャンプなどを更新
local ACTION_LATE = 3    -- フレーム後半の更新
local ACTION_EXIT = 4    -- 終了時：移動停止や重力の復帰など

local function RunActions(actionList, actionPhase, firstIndex, lastIndex, step)
    for index = firstIndex, lastIndex, step do
        local actionName = actionList[index]

        if actionName ~= "" then
            Action(actionName, actionPhase)
        end
    end
end

local function ChangeState(nextState)
    if nextState == currentState then
        return
    end

    local previousActions = stateActions[currentState] or {}
    local nextActions = stateActions[nextState]
    local sharedActionCount = 0

    while sharedActionCount < #previousActions and
        sharedActionCount < #nextActions and
        previousActions[sharedActionCount + 1] == nextActions[sharedActionCount + 1] do
        sharedActionCount = sharedActionCount + 1
    end
    -- 共通処理は引き継ぐ。ただし末尾の状態固有処理は必ず終了・再開する。
    -- 特に連続ジャンプは、同じJumpCenterでも1回ずつ開始し直す必要がある。
    sharedActionCount = math.min(sharedActionCount, math.max(0, #previousActions - 1), #nextActions - 1)
    RunActions(previousActions, ACTION_EXIT, #previousActions, sharedActionCount + 1, -1)
    currentState = nextState
    stateElapsed = 0
    SetFloat("StateTime", 0)
    SetBool("ActionFinished", false)
    SetState(currentState)
    RunActions(nextActions, ACTION_ENTER, sharedActionCount + 1, #nextActions, 1)
end

-- ============================================================================
-- 4. 各状態での判断（条件は上から順に評価）
-- ============================================================================
-- 待機
local function UpdateWaiting()
    -- 設定した待ち時間が過ぎたら、徘徊へ進む。
    if GetFloat("StateTime") >= GetFloat("FreedomWaitDuration") then
        ChangeState("Wander")
        return
    end

    -- プレイヤーが見えたら、攻撃に備えて向き直る。
    if GetBool("IsTargetVisible") then
        ChangeState("Face_Target")
        return
    end
end

-- 徘徊
local function UpdateWandering()
    -- 進行方向への角度が大きい間は、旋回へ切り替える。
    if GetBool("IsTurning") then
        ChangeState("Turn")
        return
    end

    -- 目的地への移動が終わったら、次の行動へ進む。
    if not GetBool("HasDestination") then
        ChangeState("Wait")
        return
    end

    -- プレイヤーが見えたら、攻撃に備えて向き直る。
    if GetBool("IsTargetVisible") then
        ChangeState("Face_Target")
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

    -- プレイヤーが見えたら、攻撃に備えて向き直る。
    if GetBool("IsTargetVisible") then
        ChangeState("Face_Target")
        return
    end
end

-- 相手へ向き直る
local function UpdateFacingTarget()
    -- 相手の方向を向けたら、突進前の構えへ進む。
    if GetBool("IsFacingTarget") then
        ChangeState("Attack_Prepare")
        return
    end

    -- 追跡範囲の外に出たら、待機に戻る。
    if not GetBool("IsTargetInLostRange") then
        ChangeState("Wait")
        return
    end
end

-- 突進の構え
local function UpdateAttackPreparation()
    -- 設定した待ち時間が過ぎたら、突進へ進む。
    if GetFloat("StateTime") >= GetFloat("AttackPrepareDuration") then
        ChangeState("Chase")
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

    -- 追跡範囲の外に出たら、待機に戻る。
    if not GetBool("IsTargetInLostRange") then
        ChangeState("Wait")
        return
    end
end

-- 攻撃後の休憩
local function UpdateRecovery()
    -- 設定した待ち時間が過ぎたら、相手へ向き直るへ進む。
    if GetFloat("StateTime") >= GetFloat("RecoveryDuration") then
        ChangeState("Face_Target")
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
    Turn = UpdateWanderTurning,
    Face_Target = UpdateFacingTarget,
    Attack_Prepare = UpdateAttackPreparation,
    Chase = UpdateCharging,
    Recovery = UpdateRecovery,
}

-- ============================================================================
-- 5. C++から呼ばれる入口
-- ============================================================================
-- 停止・無効化・再読み込み時。終了処理を逆順に呼び、動作を後始末する。
function Stop()
    local activeActions = stateActions[currentState] or {}
    RunActions(activeActions, ACTION_EXIT, #activeActions, 1, -1)
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

    -- 現在の行動の実処理をC++で進め、その結果を見て次の状態を判断する。
    local activeActions = stateActions[currentState]
    RunActions(activeActions, ACTION_UPDATE, 1, #activeActions, 1)

    local updateState = stateUpdates[currentState]
    updateState()
end

-- フレーム後半に必要なC++側の処理を呼ぶ。
function LateUpdate()
    local activeActions = stateActions[currentState] or {}
    RunActions(activeActions, ACTION_LATE, 1, #activeActions, 1)
end
