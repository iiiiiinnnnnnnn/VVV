-- Native code supplies perception and physical actions; Lua owns decisions and timing.
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
    TurnThreshold = 15.0,
    FreedomMinDistance = 3.0,
    FreedomMaxDistance = 10.0,
    FreedomMoveSpeed = 1.0,
    AttackMoveSpeed = 6.0,
    AttackOverDistance = 16.0,
}
for name, value in pairs(parameters) do
    if type(value) == "boolean" then SetBool(name, value) else SetFloat(name, value) end
end

local paths = {
    Wait = { "Freedom", "FreedomWait" },
    Wander = { "Freedom", "FreedomWander" },
    Turn = { "Freedom", "Turn" },
    Face_Target = { "Attacking", "Turn" },
    Attack_Prepare = { "Attacking", "FaceTarget" },
    Chase = { "Attacking", "AttackChase" },
    Recovery = { "Attacking", "Stop" },
}
local state, elapsed = nil, 0

local function invoke(path, phase, first, last, step)
    for i = first, last, step do
        if path[i] ~= "" then Action(path[i], phase) end
    end
end

local function change(nextState)
    if nextState == state then return end
    local old, new = paths[state] or {}, paths[nextState]
    local common = 0
    while common < #old and common < #new and old[common + 1] == new[common + 1] do
        common = common + 1
    end
    -- Distinct states must exit and enter even when they share the same native action.
    common = math.min(common, math.max(0, #old - 1), #new - 1)
    invoke(old, 4, #old, common + 1, -1)
    state, elapsed = nextState, 0
    SetFloat("StateTime", 0)
    SetBool("ActionFinished", false)
    SetState(state)
    invoke(new, 1, common + 1, #new, 1)
end

function Stop()
    local active = paths[state] or {}
    invoke(active, 4, #active, 1, -1)
    StopMovement()
    state, elapsed = nil, 0
    SetState("Stopped")
end

function Start()
    Stop()
    change("Wait")
end

function Update(dt)
    if not state then Start() end
    elapsed = elapsed + dt
    SetFloat("StateTime", elapsed)
    local active = paths[state]
    invoke(active, 2, 1, #active, 1)
    if state == "Wait" then
        if GetFloat("StateTime") >= GetFloat("FreedomWaitDuration") then change("Wander"); return end
        if GetBool("IsTargetVisible") then change("Face_Target"); return end
    elseif state == "Wander" then
        if GetBool("IsTurning") then change("Turn"); return end
        if not GetBool("HasDestination") then change("Wait"); return end
        if GetBool("IsTargetVisible") then change("Face_Target"); return end
    elseif state == "Turn" then
        if not GetBool("IsTurning") and GetBool("HasDestination") then change("Wander"); return end
        if not GetBool("HasDestination") then change("Wait"); return end
        if GetBool("IsTargetVisible") then change("Face_Target"); return end
    elseif state == "Face_Target" then
        if GetBool("IsFacingTarget") then change("Attack_Prepare"); return end
        if not GetBool("IsTargetInLostRange") then change("Wait"); return end
    elseif state == "Attack_Prepare" then
        if GetFloat("StateTime") >= GetFloat("AttackPrepareDuration") then change("Chase"); return end
        if not GetBool("IsTargetInLostRange") then change("Wait"); return end
    elseif state == "Chase" then
        if not GetBool("HasDestination") then change("Recovery"); return end
        if not GetBool("IsTargetInLostRange") then change("Wait"); return end
    elseif state == "Recovery" then
        if GetFloat("StateTime") >= GetFloat("RecoveryDuration") then change("Face_Target"); return end
        if not GetBool("IsTargetInLostRange") then change("Wait"); return end
    end
end

function LateUpdate()
    local active = paths[state] or {}
    invoke(active, 3, 1, #active, 1)
end
