-- Luaは行動の判断と遷移を担当し、移動・旋回・アニメーションはC++のActionで更新する。
local parameters = {
    FreedomWaitDuration = 2.0,
    AttackPrepareDuration = 1.3,
    RecoveryDuration = 0.8,

    SearchRange = 20.0,
    LostRange = 30.0,
    SightRayHeight = 1.0,
    SightRayLength = 30.0,
    SightTargetHeight = 1.0,
    SightHorizontalFov = 90.0,
    SightVerticalFov = 60.0,
    TurnThreshold = 20.0,
    TurnAngularSpeed = 180.0,

    FreedomMinDistance = 3.0,
    FreedomMaxDistance = 10.0,
    FreedomMoveSpeed = 1.0,
    ApproachMoveSpeed = 2.0,

    AttackMoveSpeed = 8.0,
    AttackOverDistance = 20.0,
}

for name, value in pairs(parameters) do
    SetFloat(name, value)
end

local ACTION_ENTER = 1
local ACTION_UPDATE = 2
local ACTION_LATE = 3
local ACTION_EXIT = 4
local currentState = nil
local stateElapsed = 0

local function ChangeState(nextState)
    if nextState == currentState then
        return
    end

    if currentState then
        Action(currentState, ACTION_EXIT)
    end

    currentState = nextState
    stateElapsed = 0
    SetFloat("StateTime", 0)
    SetState(currentState)
    Action(currentState, ACTION_ENTER)
end

local function UpdateWaiting()
    if GetBool("IsTargetVisible") then
        ChangeState("Approach")
        return
    end

    if stateElapsed >= GetFloat("FreedomWaitDuration") then
        ChangeState("Wander")
    end
end

local function UpdateWandering()
    if GetBool("IsTargetVisible") then
        ChangeState("Approach")
        return
    end

    if not GetBool("HasDestination") then
        ChangeState("Wait")
    end
end

local function UpdateApproach()
    if not GetBool("IsTargetInLostRange") then
        ChangeState("Wait")
        return
    end

    -- 物理的に直進でき、今フレームの旋回で許容角度に入った場合だけ構える。
    if GetBool("CanCharge") and GetBool("IsFacingTarget") then
        ChangeState("Attack_Prepare")
    end
end

local function UpdateAttackPreparation()
    if not GetBool("IsTargetInLostRange") then
        ChangeState("Wait")
        return
    end

    if not GetBool("CanCharge") or not GetBool("IsFacingTarget") then
        ChangeState("Approach")
        return
    end

    if stateElapsed >= GetFloat("AttackPrepareDuration") then
        ChangeState("Chase")
    end
end

local function UpdateCharging()
    if not GetBool("HasDestination") then
        ChangeState("Recovery")
        return
    end

    if not GetBool("IsTargetInLostRange") then
        ChangeState("Wait")
    end
end

local function UpdateRecovery()
    if not GetBool("IsTargetInLostRange") then
        ChangeState("Wait")
        return
    end

    if stateElapsed >= GetFloat("RecoveryDuration") then
        ChangeState("Approach")
    end
end

local stateUpdates = {
    Wait = UpdateWaiting,
    Wander = UpdateWandering,
    Approach = UpdateApproach,
    Attack_Prepare = UpdateAttackPreparation,
    Chase = UpdateCharging,
    Recovery = UpdateRecovery,
}

function Stop()
    if currentState then
        Action(currentState, ACTION_EXIT)
    end
    StopMovement()
    currentState = nil
    stateElapsed = 0
    SetState("Stopped")
end

function Start()
    Stop()
    ChangeState("Wait")
end

function Update(dt)
    if not currentState then
        Start()
    end

    stateElapsed = stateElapsed + dt
    SetFloat("StateTime", stateElapsed)

    -- C++の移動結果を受け取ってから遷移を判断し、前フレームの角度を使わない。
    Action(currentState, ACTION_UPDATE)
    stateUpdates[currentState]()
end

function LateUpdate()
    if currentState then
        Action(currentState, ACTION_LATE)
    end
end
