// BoneCapsuleCollider.cpp

#include "Physics/Collider/BoneCapsuleCollider.h"
#include "Rendering/Core/RenderContext.h"
#include "Physics/RigidBody/Rigidbody.h"
#include "Rendering/Core/Graphics.h"

BoneCapsuleCollider::BoneCapsuleCollider(Object* owner, LayerId layerId, Model* model, int nodeIndex, float radius, float height, Matrix offset, PxMaterial* material, bool isTrigger, bool freezePositions, bool freezeRotations)
    : BoneCollider(owner, layerId, model, nodeIndex, offset, material, isTrigger, freezePositions, freezeRotations)
    , radius(radius), height(height)
{
    InitializeShape();
}

PxShape* BoneCapsuleCollider::CreateShape(PxPhysics* physics, PxMaterial* material) const
{
    return physics->createShape(
        PxCapsuleGeometry(radius, height * 0.5f),
        *material,
        true);
}

PxTransform BoneCapsuleCollider::GetLocalPose() const
{
    return PxTransform(
        PxVec3(0, 0, 0),
        PxQuat(DirectX::XM_PIDIV2, PxVec3(0, 0, 1)));
}

void BoneCapsuleCollider::Render(const RenderContext& rc)
{
	if (!showDebug) return;

    PxTransform pose = ghostActor->getGlobalPose() *
        GetLocalPose() *
        PxTransform(PxVec3(0, 0, 0), PxQuat(-DirectX::XM_PIDIV2, PxVec3(0, 0, 1)));

    Game::Graphics::Instance().GetShapeRenderer()->DrawCapsule(
        Conv::ToMatrix(pose),
        radius,
        height,
        Color(0.8f, 0.0f, 1.0f, 1.0f));
}

void BoneCapsuleCollider::DrawGUI()
{
    bool changed = false;
    changed |= ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 10.0f);
    changed |= ImGui::DragFloat("Height", &height, 0.01f, 0.01f, 10.0f);
    radius = std::max(radius, 0.01f);
    height = std::max(height, 0.01f);
    if (changed)
    {
        InitializeShape();
    }
    DrawBoneSettingsGUI();
}