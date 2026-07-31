// EnemyAIFlow.cpp

#include "Gameplay/AI/EnemyAIFlow.h"

#include <algorithm>
#include <fstream>
#include <limits>

#include "Application/Time/GameTime.h"
#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Physics/Core/PhysicsManager.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Physics/Navigation/NavMeshAgent.h"
#include "Rendering/Core/Graphics.h"

#include "imgui.h"
#include "Core/Foundation/Json.h"

EnemyAIFlow::EnemyAIFlow(Object* owner)
    : AIFlow(owner)
{
    SetGraphPath("Data/AI/EnemyAI.json");
    SetFloat("SearchRange", 15.0f);
    SetFloat("LostRange", 22.5f);
    SetFloat("SightRayLength", 16.5f);
    SetFloat("SightRayHeight", 0.05f);
    SetFloat("SightTargetHeight", 1.0f);
    SetFloat("SightHorizontalFov", 90.0f);
    SetFloat("SightVerticalFov", 60.0f);
    SetBool("HasTarget", false, true);
    SetBool("IsTargetInSearchRange", false, true);
    SetBool("IsTargetInLostRange", false, true);
    SetBool("IsTargetInSightAngle", false, true);
    SetBool("IsTargetVisible", false, true);
    SetFloat("TargetDistance", std::numeric_limits<float>::max(), true);
    SetVector3("TargetPosition", Vector3::Zero, true);
}

void EnemyAIFlow::OnDisabled()
{
    AIFlow::OnDisabled();
    StopMovement();
}

void EnemyAIFlow::OnRender(const RenderContext&)
{
    if (!showDebug || !showSightDebug) return;

    const Actor* ownerActor = dynamic_cast<const Actor*>(owner);
    if (!ownerActor) return;

    const float searchRange = GetFloat("SearchRange", 20.0f);
    const float lostRange = std::max(
        GetFloat("LostRange", searchRange * 1.5f),
        searchRange);
    const Color white(1.0f, 1.0f, 1.0f, 1.0f);
    const Color lostRangeColor(1.0f, 0.65f, 0.15f, 1.0f);
    if (searchRange > 0.0f)
    {
        Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(
            ownerActor->transform.position,
            searchRange,
            white);
    }
    if (lostRange > searchRange)
    {
        Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(
            ownerActor->transform.position,
            lostRange,
            lostRangeColor);
    }

    PrimitiveRenderer* primitiveRenderer =
        Game::Graphics::Instance().GetPrimitiveRenderer();
    const float sightLength = GetFloat("SightRayLength", 30.0f);
    const float horizontalFov = std::clamp(
        GetFloat("SightHorizontalFov", 90.0f), 1.0f, 179.0f);
    const float verticalFov = std::clamp(
        GetFloat("SightVerticalFov", 60.0f), 1.0f, 179.0f);
    const float halfWidth = tanf(RAD(horizontalFov * 0.5f)) * sightLength;
    const float halfHeight = tanf(RAD(verticalFov * 0.5f)) * sightLength;
    const Matrix sightRotation = Matrix::CreateFromQuaternion(ownerActor->transform.rotation);

    Vector3 corners[4] = {
        Vector3(-halfWidth, -halfHeight, sightLength),
        Vector3(+halfWidth, -halfHeight, sightLength),
        Vector3(+halfWidth, +halfHeight, sightLength),
        Vector3(-halfWidth, +halfHeight, sightLength)
    };
    for (Vector3& corner : corners)
        corner = sightRayStart + Vector3::TransformNormal(corner, sightRotation);

    for (const Vector3& corner : corners)
        primitiveRenderer->DrawLine(sightRayStart, corner, white, white);
    for (int index = 0; index < 4; ++index)
        primitiveRenderer->DrawLine(corners[index], corners[(index + 1) % 4], white, white);

    primitiveRenderer->DrawLine(sightRayStart, sightRayEnd, white, white);
    if (sightRayHit)
        Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(sightRayEnd, 0.1f, white);
}

void EnemyAIFlow::CreateDefaultChaseGraph()
{
    ClearGraph();
    GetParameters().clear();
    SetFloat("SearchRange", 20.0f);
    SetFloat("LostRange", 30.0f);
    SetFloat("SightRayLength", 30.0f);
    SetFloat("SightRayHeight", 1.0f);
    SetFloat("SightTargetHeight", 1.0f);
    SetFloat("SightHorizontalFov", 90.0f);
    SetFloat("SightVerticalFov", 60.0f);
    SetBool("HasTarget", false, true);
    SetBool("IsTargetInSearchRange", false, true);
    SetBool("IsTargetInLostRange", false, true);
    SetBool("IsTargetInSightAngle", false, true);
    SetBool("IsTargetVisible", false, true);
    SetFloat("TargetDistance", std::numeric_limits<float>::max(), true);
    SetVector3("TargetPosition", Vector3::Zero, true);

    State& idle = AddState("Idle", "Idle");
    const int idleId = idle.id;
    State& walk = AddState("Walk", "Walk");
    const int walkId = walk.id;
    State& run = AddState("Run", "Run");
    const int runId = run.id;
    SetEntryState(idleId);

    Transition& idleToWalk = AddTransition(idleId, walkId);
    idleToWalk.conditions.push_back({"IsTargetVisible", CompareOp::IsTrue});
    idleToWalk.conditions.push_back({"IsTargetInSearchRange", CompareOp::IsTrue});
    Transition& walkToIdle = AddTransition(walkId, idleId);
    walkToIdle.conditions.push_back({"IsTargetInLostRange", CompareOp::IsFalse});
    Transition& walkToRun = AddTransition(walkId, runId);
    walkToRun.conditions.push_back({"TargetDistance", CompareOp::Greater, 10.0f});
    Transition& runToIdle = AddTransition(runId, idleId);
    runToIdle.conditions.push_back({"IsTargetInLostRange", CompareOp::IsFalse});
    Transition& runToWalk = AddTransition(runId, walkId);
    runToWalk.conditions.push_back({"TargetDistance", CompareOp::LessEqual, 10.0f});
    BindCallbacks();
}

void EnemyAIFlow::SetSearchRange(float value)
{
    const float searchRange =
        std::max(value, 0.0f);
    SetFloat("SearchRange", searchRange);
    if (GetFloat("LostRange", searchRange) < searchRange)
        SetFloat("LostRange", searchRange);
}

void EnemyAIFlow::SetLostRange(float value)
{
    SetFloat(
        "LostRange",
        std::max(
            value,
            GetFloat("SearchRange", 20.0f)));
}

void EnemyAIFlow::LockOn(Actor* actor)
{
    if (!actor || actor == owner) return;
    lockedTarget = actor;
    target = actor;
}

void EnemyAIFlow::SetMovementLocked(bool value)
{
    if (value && !movementLocked) FaceTarget();
    movementLocked = value;
    if (movementLocked) StopMovement();
}

void EnemyAIFlow::MoveToTarget(float speed)
{
    if (movementLocked)
    {
        StopMovement();
        return;
    }

    if (!target || !GetBool("IsTargetInLostRange"))
    {
        StopMovement();
        return;
    }

    if (!navMeshAgent) navMeshAgent = owner->GetComponent<NavMeshAgent>();
    if (!navMeshAgent)
    {
        FaceTarget();
        return;
    }

    if (NavMeshActor* navMeshActor = NavMeshActor::GetActive())
        navMeshActor->SetAgentRadius(agentRadius);
    navMeshAgent->SetSpeed(speed);
    navMeshAgent->MoveToTarget(target);
    FaceTarget();
}

void EnemyAIFlow::StopMovement()
{
    if (!navMeshAgent) navMeshAgent = owner->GetComponent<NavMeshAgent>();
    if (navMeshAgent) navMeshAgent->Stop();
}

void EnemyAIFlow::FaceTarget()
{
    if (!target) return;

    Vector3 direction = target->transform.position - owner->GetTransform()->position;
    direction.y = 0.0f;
    if (direction.LengthSquared() <= eps) return;

    const float targetYaw = atan2f(direction.x, direction.z);
    const Quaternion targetRotation = Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f);
    const float rate = 1.0f - expf(-trackingTurnSpeed * Game::Time::deltaTime);
    owner->GetTransform()->SetRotation(
        Quaternion::Slerp(owner->GetTransform()->rotation, targetRotation, rate));
}

void EnemyAIFlow::UpdateBlackboard()
{
    Actor* ownerActor = dynamic_cast<Actor*>(owner);
    ActorManager* actorManager = ActorManager::GetActive();

    const float searchRange =
        GetFloat("SearchRange", 20.0f);
    const float lostRange =
        std::max(
            GetFloat(
                "LostRange",
                searchRange * 1.5f),
            searchRange);
    float closestDistance = std::numeric_limits<float>::max();
    if (ownerActor && actorManager)
    {
        bool targetIsValid = false;
        for (Actor* actor : actorManager->GetActors())
        {
            if (!actor || actor == ownerActor || actor->IsPendingDestroy()) continue;
            if (actor != target || !actor->IsActive()) continue;

            closestDistance = Vector3::Distance(
                actor->transform.position,
                ownerActor->transform.position);
            targetIsValid =
                closestDistance <= lostRange;
            break;
        }

        if (!targetIsValid)
        {
            if (lockedTarget == target)
                lockedTarget = nullptr;
            target = nullptr;
            closestDistance =
                std::numeric_limits<float>::max();
        }

        if (!target)
        {
            for (Actor* actor : actorManager->GetActors())
            {
                if (!actor ||
                    actor == ownerActor ||
                    actor->IsPendingDestroy() ||
                    !actor->IsActive())
                {
                    continue;
                }
                if (!actor->CompareTag(targetTag)) continue;

                const float distance =
                    Vector3::Distance(
                        actor->transform.position,
                        ownerActor->transform.position);
                if (distance > searchRange ||
                    distance >= closestDistance)
                {
                    continue;
                }

                closestDistance = distance;
                target = actor;
            }
        }
    }
    else
    {
        target = nullptr;
        lockedTarget = nullptr;
    }

    const bool hasTarget = target != nullptr;
    SetBool("HasTarget", hasTarget, true);
    SetBool(
        "IsTargetInSearchRange",
        hasTarget &&
        closestDistance <= searchRange,
        true);
    SetBool(
        "IsTargetInLostRange",
        hasTarget &&
        closestDistance <= lostRange,
        true);
    SetFloat("TargetDistance", closestDistance, true);
    SetVector3("TargetPosition", hasTarget ? target->transform.position : Vector3::Zero, true);
    UpdateSightRay();
    if (target && target == lockedTarget)
    {
        SetBool("IsTargetInSightAngle", true, true);
        SetBool("IsTargetVisible", true, true);
    }
}

void EnemyAIFlow::UpdateSightRay()
{
    const Actor* ownerActor = dynamic_cast<const Actor*>(owner);
    if (!ownerActor)
    {
        SetBool("IsTargetInSightAngle", false, true);
        SetBool("IsTargetVisible", false, true);
        return;
    }

    const float rayLength = GetFloat("SightRayLength", 30.0f);
    const float rayHeight = GetFloat("SightRayHeight", 1.0f);
    const float targetHeight = GetFloat("SightTargetHeight", 1.0f);
    const float horizontalFov = std::clamp(
        GetFloat("SightHorizontalFov", 90.0f), 1.0f, 179.0f);
    const float verticalFov = std::clamp(
        GetFloat("SightVerticalFov", 60.0f), 1.0f, 179.0f);

    Vector3 forward = ownerActor->transform.forward;
    Vector3 right = ownerActor->transform.right;
    Vector3 up = Vector3::TransformNormal(
        Vector3::Up, Matrix::CreateFromQuaternion(ownerActor->transform.rotation));
    if (forward.LengthSquared() <= eps) forward = Vector3::UnitZ;
    if (right.LengthSquared() <= eps) right = Vector3::UnitX;
    if (up.LengthSquared() <= eps) up = Vector3::UnitY;
    forward.Normalize();
    right.Normalize();
    up.Normalize();

    sightRayStart = ownerActor->transform.position + up * rayHeight;
    sightRayEnd = sightRayStart + forward * rayLength;
    sightRayHit = false;
    if (!target)
    {
        SetBool("IsTargetInSightAngle", false, true);
        SetBool("IsTargetVisible", false, true);
        return;
    }

    const Vector3 targetPoint = target->transform.position + Vector3::Up * targetHeight;
    Vector3 toTarget = targetPoint - sightRayStart;
    const float targetDistance = toTarget.Length();
    if (targetDistance <= eps)
    {
        SetBool("IsTargetInSightAngle", true, true);
        SetBool("IsTargetVisible", true, true);
        sightRayEnd = targetPoint;
        return;
    }

    const float forwardDistance = toTarget.Dot(forward);
    const float rightDistance = toTarget.Dot(right);
    const float upDistance = toTarget.Dot(up);
    const float horizontalAngle = DEG(atan2f(rightDistance, forwardDistance));
    const float verticalAngle = DEG(atan2f(
        upDistance, sqrtf(forwardDistance * forwardDistance + rightDistance * rightDistance)));
    const bool inSightAngle =
        forwardDistance > 0.0f &&
        targetDistance <= rayLength &&
        fabsf(horizontalAngle) <= horizontalFov * 0.5f &&
        fabsf(verticalAngle) <= verticalFov * 0.5f;
    SetBool("IsTargetInSightAngle", inSightAngle, true);
    if (!inSightAngle)
    {
        SetBool("IsTargetVisible", false, true);
        return;
    }

    toTarget /= targetDistance;
    sightRayEnd = targetPoint;
    PhysicsManager::PhysicsRaycastHit hit;
    if (PhysicsManager::Instance().Raycast(
        sightRayStart,
        toTarget,
        targetDistance + 0.25f,
        hit,
        Layers::Get("Enemy"),
        ownerActor))
    {
        sightRayHit = true;
        sightRayEnd = hit.position;
        SetBool("IsTargetVisible", hit.layerId == Layers::Get("Player"), true);
        return;
    }
    SetBool("IsTargetVisible", false, true);
}

void EnemyAIFlow::DrawFlowInspector()
{
    ImGui::Text("Target: %s", target ? target->GetName().c_str() : "None");
    ImGui::Text("Locked Target: %s", lockedTarget ? lockedTarget->GetName().c_str() : "None");
    ImGui::Text("Target Distance: %.2f", GetFloat("TargetDistance"));
    ImGui::Text(
        "In Search Range: %s",
        GetBool("IsTargetInSearchRange") ? "true" : "false");
    ImGui::Text(
        "In Lost Range: %s",
        GetBool("IsTargetInLostRange") ? "true" : "false");
    ImGui::Text("In Sight Angle: %s", GetBool("IsTargetInSightAngle") ? "true" : "false");
    ImGui::Text("Target Visible: %s", GetBool("IsTargetVisible") ? "true" : "false");
    ImGui::Text(
        "NavMesh: %s",
        navMeshAgent ? navMeshAgent->GetStatusMessage().c_str() : "Not cached yet");
    ImGui::Text("Move Speed: %.2f", navMeshAgent ? navMeshAgent->GetMoveAmount() : 0.0f);
    ImGui::Checkbox("Show Sight Debug", &showSightDebug);

    float searchRange = GetFloat("SearchRange", 20.0f);
    if (ImGui::DragFloat("Search Range", &searchRange, 0.25f, 0.0f, 1000.0f))
        SetSearchRange(searchRange);
    float lostRange = GetFloat(
        "LostRange",
        searchRange * 1.5f);
    if (ImGui::DragFloat(
        "Lost Range",
        &lostRange,
        0.25f,
        searchRange,
        1000.0f))
    {
        SetLostRange(lostRange);
    }
    float sightRayLength = GetFloat("SightRayLength", 30.0f);
    if (ImGui::DragFloat("Sight Ray Length", &sightRayLength, 0.25f, 0.0f, 1000.0f))
        SetFloat("SightRayLength", std::max(sightRayLength, 0.0f));
    float sightRayHeight = GetFloat("SightRayHeight", 1.0f);
    if (ImGui::DragFloat("Sight Ray Height", &sightRayHeight, 0.05f, 0.0f, 100.0f))
        SetFloat("SightRayHeight", std::max(sightRayHeight, 0.0f));
    float sightTargetHeight = GetFloat("SightTargetHeight", 1.0f);
    if (ImGui::DragFloat("Sight Target Height", &sightTargetHeight, 0.05f, 0.0f, 100.0f))
        SetFloat("SightTargetHeight", std::max(sightTargetHeight, 0.0f));
    float horizontalFov = GetFloat("SightHorizontalFov", 90.0f);
    if (ImGui::DragFloat("Sight Horizontal FOV", &horizontalFov, 0.5f, 1.0f, 179.0f))
        SetFloat("SightHorizontalFov", std::clamp(horizontalFov, 1.0f, 179.0f));
    float verticalFov = GetFloat("SightVerticalFov", 60.0f);
    if (ImGui::DragFloat("Sight Vertical FOV", &verticalFov, 0.5f, 1.0f, 179.0f))
        SetFloat("SightVerticalFov", std::clamp(verticalFov, 1.0f, 179.0f));
    ImGui::DragFloat("Tracking Turn Speed", &trackingTurnSpeed, 0.1f, 0.0f, 30.0f);
}

bool EnemyAIFlow::LoadFlowExtension(const std::string& path)
{
    std::ifstream stream(path);
    if (!stream) return false;

    try
    {
        json root;
        stream >> root;
        targetTag = root.value("targetTag", targetTag);
        agentRadius = root.value("agentRadius", agentRadius);
        trackingTurnSpeed = root.value("trackingTurnSpeed", trackingTurnSpeed);
        const float searchRange =
            GetFloat("SearchRange", 20.0f);
        SetLostRange(
            GetFloat(
                "LostRange",
                searchRange * 1.5f));
        SetBool("IsTargetInLostRange", false, true);

        // Migrate old loss conditions while keeping acquisition conditions unchanged.
        for (AIFlow::State& state : GetStates())
        {
            for (AIFlow::Transition& transition :
                state.transitions)
            {
                for (AIFlow::Condition& condition :
                    transition.conditions)
                {
                    if (condition.parameterName !=
                        "IsTargetInSearchRange")
                    {
                        continue;
                    }
                    if (condition.compare !=
                        CompareOp::IsFalse)
                    {
                        continue;
                    }

                    condition.parameterName =
                        "IsTargetInLostRange";
                }
            }
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool EnemyAIFlow::SaveFlowExtension(const std::string& path) const
{
    json root;
    {
        std::ifstream stream(path);
        if (!stream) return false;
        try
        {
            stream >> root;
        }
        catch (...)
        {
            return false;
        }
    }

    root["targetTag"] = targetTag;
    root["agentRadius"] = agentRadius;
    root["trackingTurnSpeed"] = trackingTurnSpeed;
    std::ofstream stream(path);
    if (!stream) return false;
    stream << root.dump(4);
    return stream.good();
}
