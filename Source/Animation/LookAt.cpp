#include "Animation/LookAt.h"
#include <ImGuiToolKit/Drag.h>
#include <algorithm>
#include <cmath>
#include "Gameplay/Component/ActorHitstopComponent.h"
#include "Gameplay/Actor/Entity.h"
#include "Physics/Collider/VMDLColliderComponent.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Resource/VMDLModel.h"
#include "Gameplay/Actor/Actor.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Core/RenderContext.h"
#include "Application/Time/GameTime.h"
#include "Gameplay/Actor/ActorManager.h"

LookAt::LookAt(Object* owner, VMDLModel* model,
    const std::string& nodeName1,
    const std::string& nodeName2,
    const std::string& nodeName3)
    : Component(owner), model(model)
{
    const auto* renderer = owner->GetComponent<VMDLModelComponent>();

    float modelYaw = 0.0f;
    if (renderer && renderer->GetModel() == model)
    {
        modelYaw = renderer->GetModelYawOffset();
    }
    const Matrix modelRotation = Matrix::CreateRotationY(modelYaw);

    InitializeNode(lookNodes[0], nodeName1, modelRotation);
    if (!nodeName2.empty())
    {
        InitializeNode(lookNodes[1], nodeName2, modelRotation);
        if (!nodeName3.empty())
        {
            InitializeNode(lookNodes[2], nodeName3, modelRotation);
        }
    }
}

void LookAt::InitializeNode(LookNode& lookNode, const std::string& nodeName, const Matrix& modelRotation)
{
    lookNode.nodeIndex = model->GetNodeIndex(nodeName.c_str());
    if (lookNode.nodeIndex < 0) return;

    const auto& node = model->GetNodes().at(lookNode.nodeIndex);
    const Matrix reference = node.globalTransform * modelRotation;
    const Matrix inverseReference = reference.Invert();

    lookNode.forward = Vector3::TransformNormal(Vector3::UnitZ, inverseReference);
    lookNode.forward.Normalize();

    lookNode.up = Vector3::TransformNormal(Vector3::Up, inverseReference);
    lookNode.up.Normalize();

    lookNode.right = lookNode.up.Cross(lookNode.forward);
    lookNode.right.Normalize();

    lookNode.up = lookNode.forward.Cross(lookNode.right);
    lookNode.up.Normalize();
}

void LookAt::Update()
{
    ActorManager* actorManager = ActorManager::GetActive();
    found = false;

    if (!actorManager) return;
    if (lookDistance <= 0.0f) return;

    float nearestDistanceSq = lookDistance * lookDistance;

    for (Actor* actor : actorManager->GetActors())
    {
        if (!actor || actor == owner || !actor->IsActive() || actor->IsPendingDestroy()) continue;

        const auto* entity = dynamic_cast<const Entity*>(actor);
        if (entity && entity->IsDead()) continue;

        bool matchesFilter = false;
        for (const auto& filterTag : filterTags)
        {
            if (actor->CompareTag(filterTag))
            {
                matchesFilter = true;
                break;
            }
        }
        if (!matchesFilter) continue;

        const auto* renderer = actor->GetComponent<VMDLModelComponent>();
        if (!renderer) continue;

        const auto* collider = renderer->GetAttachmentCollider("LOOKHEAD");
        const auto* lookHead = dynamic_cast<const VMDLColliderComponent*>(collider);
        if (!lookHead) continue;

        const float distanceSq = Vector3::DistanceSquared(
            actor->transform.position, owner->GetTransform()->position);
        if (distanceSq >= nearestDistanceSq) continue;

        nearestDistanceSq = distanceSq;
        found = true;
        target = lookHead->GetWorldPosition();
    }
}

void LookAt::LateUpdate()
{
    if (!model || lookNodes[0].nodeIndex < 0) return;
    if (ActorHitstopComponent::IsModelStopping(owner, model)) return;

    int nodeCount = 0;
    for (const LookNode& lookNode : lookNodes)
    {
        if (lookNode.nodeIndex >= 0) ++nodeCount;
    }

    // 補正量を均等に分けるため、全ノードの計算には補正前のワールド行列を使用する。
    const float weight = 1.0f / static_cast<float>(nodeCount);
    const float followTime = smoothSpeed * Game::Time::deltaTime;
    const float rate = std::clamp(1.0f - expf(-followTime), 0.0f, 1.0f);

    for (LookNode& lookNode : lookNodes)
    {
        if (lookNode.nodeIndex < 0) continue;
        UpdateNode(lookNode, weight, rate);
    }

    model->UpdateTransform(model->GetWorldTransform());
}

void LookAt::UpdateNode(LookNode& lookNode, float weight, float rate)
{
    auto& node = model->GetNodes().at(lookNode.nodeIndex);
    const Matrix nodeWorld = model->GetScaledAttachmentTransform(node.worldTransform);
    const Matrix inverseWorld = nodeWorld.Invert();

    // ワールドの上方向を共通の基準とし、角度と回転軸をワールド空間で求める。
    Vector3 worldUp = Vector3::Up;
    Vector3 worldForward = Vector3::TransformNormal(lookNode.forward, nodeWorld);
    worldForward.Normalize();

    const float forwardHeight = worldForward.Dot(worldUp);
    Vector3 flatForward = worldForward - worldUp * forwardHeight;
    if (flatForward.LengthSquared() < 0.000001f) return;
    const float forwardHorizontal = flatForward.Length();
    flatForward.Normalize();

    Vector3 worldRight = worldUp.Cross(flatForward);
    worldRight.Normalize();

    float yaw = 0.0f;
    float pitch = 0.0f;

    if (found)
    {
        Vector3 direction = target - nodeWorld.Translation();
        if (direction.LengthSquared() >= 0.000001f)
        {
            direction.Normalize();
            const float front = direction.Dot(flatForward);
            const float side = direction.Dot(worldRight);
            const float height = direction.Dot(worldUp);

            if (front > 0.001f)
            {
                const float horizontal = sqrtf(front * front + side * side);
                const float currentElevation = atan2f(forwardHeight, forwardHorizontal);
                const float targetElevation = atan2f(height, horizontal);

                yaw = atan2f(side, front);
                pitch = targetElevation - currentElevation;
                yaw = std::clamp(yaw, -maxAngleYaw, maxAngleYaw) * weight;
                pitch = std::clamp(pitch, -maxAnglePitch, maxAnglePitch) * weight;
            }
        }
    }

    lookNode.currentYaw += (yaw - lookNode.currentYaw) * rate;
    lookNode.currentPitch += (pitch - lookNode.currentPitch) * rate;

    // Yaw後の水平な右軸でPitchを適用する。首の傾いたローカルUp軸は使わない。
    const Matrix worldYaw = Matrix::CreateFromAxisAngle(worldUp, lookNode.currentYaw);
    Vector3 worldPitchAxis = Vector3::TransformNormal(worldRight, worldYaw);

    Vector3 localYawAxis = Vector3::TransformNormal(worldUp, inverseWorld);
    Vector3 localPitchAxis = Vector3::TransformNormal(worldPitchAxis, inverseWorld);
    localYawAxis.Normalize();
    localPitchAxis.Normalize();

    const Matrix yawMatrix = Matrix::CreateFromAxisAngle(localYawAxis, lookNode.currentYaw);
    const Matrix pitchMatrix = Matrix::CreateFromAxisAngle(localPitchAxis, -lookNode.currentPitch);

    const Matrix localCorrection = yawMatrix * pitchMatrix;
    const Matrix animatedRotation = Matrix::CreateFromQuaternion(node.rotation);
    node.rotation = Quaternion::CreateFromRotationMatrix(localCorrection * animatedRotation);
    node.rotation.Normalize();
}

void LookAt::Render(const RenderContext& rc)
{
    if (!showDebug) return;
    if (!model) return;
    if (lookNodes[0].nodeIndex < 0) return;

    const Vector3 nodePosition = model->GetScaledAttachmentTransform(
        model->GetNodes()[lookNodes[0].nodeIndex].worldTransform).Translation();

    Game::Graphics::Instance().GetPrimitiveRenderer()->DrawLine(
        nodePosition,
        target,
        Color(1.0f, 1.0f, 0.0f, 1.0f),
        Color(1.0f, 1.0f, 0.0f, 1.0f));
}

void LookAt::DrawGUI()
{
    ImGui::Text("found : %s", found ? "true" : "false");
    ImGuiToolKit::DragPosFloat3("Target", &target.x, 0.01f);

	ImGuiToolKit::DragRangeFloat("範囲", &lookDistance, 0.1f, 0.0f, 1000.0f);

    float maxAngleYawDegrees = DEG(maxAngleYaw);
    if (ImGuiToolKit::DragRotFloat("最大 Yaw", &maxAngleYawDegrees, 0.1f, 0.0f, 180.0f))
    {
        maxAngleYaw = RAD(maxAngleYawDegrees);
    }

    float maxAnglePitchDegrees = DEG(maxAnglePitch);
    if (ImGuiToolKit::DragRotFloat("最大 Pitch", &maxAnglePitchDegrees, 0.1f, 0.0f, 180.0f))
    {
        maxAnglePitch = RAD(maxAnglePitchDegrees);
    }

    ImGuiToolKit::DragSpeedFloat("Smooth Speed", &smoothSpeed, 0.1f, 0.0f, 100.0f);
}
