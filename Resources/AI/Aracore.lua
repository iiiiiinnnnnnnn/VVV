-- Native code supplies perception and physical actions; Lua owns decisions and timing.
local parameters = {
    SearchRange = 35.0,
    LostRange = 50.0,
    SightRayHeight = 2.0,
    SightRayLength = 35.0,
    SightTargetHeight = 1.0,
    SightHorizontalFov = 120.0,
    SightVerticalFov = 75.0,
    TurnThreshold = 8.0,
    FreedomMinDistance = 5.0,
    FreedomMaxDistance = 18.0,
    FreedomMoveSpeed = 1.4,
    FreedomWaitDuration = 1.5,
    ThreatDuration = 3.0,
    ChaseMoveSpeed = 3.2,
    ChaseDuration = 6.0,
    TurningMoveSpeed = 1.4,
    ChargePrepareDuration = 1.0,
    ChargeMoveSpeed = 6.5,
    ChargeOverDistance = 10.0,
    ChargeMaxDuration = 2.5,
    RecoveryDuration = 1.5,
    JumpChance = 0.25,
    JumpTakeoffDelay = 0.65,
    JumpDuration = 2.4,
    JumpHeight = 10.0,
    JumpLandingRecovery = 0.9,
    JumpTerrainDeformPower = 1.25,
    JumpTerrainDeformRadius = 8.0,
    AngryIdleScale = 0.4,
    AngryJumpChance = 0.7,
    AngryJumpCount = 2.0,
    AngryLowHpJumpCount = 3.0,
    PhaseThreatDuration = 3.2,
    PhaseColorChangeDuration = 3.0,
    DeathDissolveAmount = 0.35,
}
for name, value in pairs(parameters) do
    if type(value) == "boolean" then SetBool(name, value) else SetFloat(name, value) end
end

local paths = {
    Wait = { "", "FreedomWait" },
    Wander = { "", "FreedomWander" },
    Wander_Turn = { "", "WanderTurn" },
    Threat = { "Threat" },
    Chase = { "", "Chase" },
    Charge_Turn = { "", "ChargeTurn" },
    Charge = { "", "Charge" },
    Recovery = { "", "Recovery" },
    Jump_To_Center = { "", "JumpCenter" },
    Phase_Threat = { "PhaseThreat" },
    Repeat_Jump = { "", "JumpCenter" },
}
local state, elapsed = nil, 0
local remainingJumps, angryApplied = 0, false

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
    if state == "Recovery" then
        remainingJumps = 0
        SetBool("ShouldJump", math.random() < math.max(0, math.min(1, GetFloat("JumpChance"))))
    elseif state == "Jump_To_Center" or state == "Repeat_Jump" then
        if remainingJumps <= 0 then
            remainingJumps = 1
            if GetBool("Angry") then
                local count = GetBool("LowHealth") and "AngryLowHpJumpCount" or "AngryJumpCount"
                remainingJumps = math.max(1, math.min(5, math.floor(GetFloat(count))))
            end
        end
        remainingJumps = remainingJumps - 1
    end
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
    local ratio = GetFloat("Life") / math.max(1, GetFloat("MaxLife"))
    SetBool("LowHealth", ratio <= 0.25)
    SetBool("PhaseChangeQueued", not GetBool("Angry") and ratio <= 0.5)
    SetBool("PhaseChangeRequested", GetBool("PhaseChangeQueued") and GetBool("CanChangePhase"))
    if GetBool("Angry") and not angryApplied then
        angryApplied = true
        SetTrackingTurnSpeed(4)
        local scale = math.max(0.1, math.min(1, GetFloat("AngryIdleScale")))
        for _, name in ipairs({"FreedomWaitDuration", "ChaseDuration", "ChargePrepareDuration", "RecoveryDuration", "JumpLandingRecovery"}) do
            SetFloat(name, GetFloat(name) * scale)
        end
        SetFloat("JumpChance", GetFloat("AngryJumpChance"))
    end
    SetBool("CloseToTarget", GetBool("HasTarget") and GetFloat("TargetDistance") < 5)
    local active = paths[state]
    invoke(active, 2, 1, #active, 1)
    local duration = ({Wait="FreedomWaitDuration", Threat="ThreatDuration", Chase="ChaseDuration", Charge_Turn="ChargePrepareDuration", Charge="ChargeMaxDuration", Recovery="RecoveryDuration"})[state]
    if duration then SetBool("ActionFinished", elapsed >= GetFloat(duration)) end
    if state == "Chase" and GetBool("CloseToTarget") then SetBool("ActionFinished", true) end
    if state == "Jump_To_Center" or state == "Repeat_Jump" then
        local finished = math.max(0, GetFloat("JumpTakeoffDelay")) + math.max(0.01, GetFloat("JumpDuration")) + math.max(0, GetFloat("JumpLandingRecovery"))
        SetBool("ActionFinished", GetBool("JumpLanded") and elapsed >= finished)
        SetBool("ContinueJump", remainingJumps > 0 and not GetBool("PhaseChangeQueued"))
    elseif state == "Phase_Threat" then
        SetBool("ActionFinished", GetBool("PhasePresentationStarted") and GetFloat("PhaseThreatElapsed") >= GetFloat("PhaseThreatDuration"))
    end
    if state == "Wait" then
        if GetBool("ActionFinished") then change("Wander"); return end
        if GetBool("PhaseChangeRequested") then change("Phase_Threat"); return end
        if GetBool("IsTargetInSearchRange") and GetBool("IsTargetVisible") then change("Threat"); return end
    elseif state == "Wander" then
        if GetBool("IsTurning") then change("Wander_Turn"); return end
        if not GetBool("HasDestination") then change("Wait"); return end
        if GetBool("PhaseChangeRequested") then change("Phase_Threat"); return end
        if GetBool("IsTargetInSearchRange") and GetBool("IsTargetVisible") then change("Threat"); return end
    elseif state == "Wander_Turn" then
        if not GetBool("IsTurning") and GetBool("HasDestination") then change("Wander"); return end
        if not GetBool("HasDestination") then change("Wait"); return end
        if GetBool("PhaseChangeRequested") then change("Phase_Threat"); return end
        if GetBool("IsTargetInSearchRange") and GetBool("IsTargetVisible") then change("Threat"); return end
    elseif state == "Threat" then
        if GetBool("PhaseChangeRequested") then change("Phase_Threat"); return end
        if not GetBool("IsTargetInLostRange") then change("Wait"); return end
        if GetBool("ActionFinished") then change("Chase"); return end
    elseif state == "Chase" then
        if GetBool("ActionFinished") then change("Charge_Turn"); return end
        if GetBool("PhaseChangeRequested") then change("Phase_Threat"); return end
        if not GetBool("IsTargetInLostRange") then change("Wait"); return end
    elseif state == "Charge_Turn" then
        if GetBool("ActionFinished") and GetBool("IsFacingTarget") then change("Charge"); return end
        if GetBool("PhaseChangeRequested") then change("Phase_Threat"); return end
        if not GetBool("IsTargetInLostRange") then change("Wait"); return end
    elseif state == "Charge" then
        if not GetBool("HasDestination") then change("Recovery"); return end
        if GetBool("ActionFinished") then change("Recovery"); return end
        if GetBool("PhaseChangeRequested") then change("Phase_Threat"); return end
        if not GetBool("IsTargetInLostRange") then change("Wait"); return end
    elseif state == "Recovery" then
        if GetBool("ActionFinished") and GetBool("ShouldJump") then change("Jump_To_Center"); return end
        if GetBool("ActionFinished") and not GetBool("ShouldJump") then change("Charge_Turn"); return end
        if GetBool("PhaseChangeRequested") then change("Phase_Threat"); return end
        if not GetBool("IsTargetInLostRange") then change("Wait"); return end
    elseif state == "Jump_To_Center" then
        if GetBool("ActionFinished") and GetBool("ContinueJump") then change("Repeat_Jump"); return end
        if GetBool("ActionFinished") and not GetBool("ContinueJump") then change("Charge_Turn"); return end
        if GetBool("PhaseChangeRequested") then change("Phase_Threat"); return end
        if not GetBool("IsTargetInLostRange") then change("Wait"); return end
    elseif state == "Phase_Threat" then
        if GetBool("ActionFinished") then change("Chase"); return end
    elseif state == "Repeat_Jump" then
        if GetBool("ActionFinished") and GetBool("ContinueJump") then change("Jump_To_Center"); return end
        if GetBool("ActionFinished") and not GetBool("ContinueJump") then change("Charge_Turn"); return end
        if GetBool("PhaseChangeRequested") then change("Phase_Threat"); return end
        if not GetBool("IsTargetInLostRange") then change("Wait"); return end
    end
end

function LateUpdate()
    local active = paths[state] or {}
    invoke(active, 3, 1, #active, 1)
end
