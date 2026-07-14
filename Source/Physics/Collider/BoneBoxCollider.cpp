// BoneBoxCollider.cpp

#include "Physics/Collider/BoneBoxCollider.h"
#include "Rendering/Core/RenderContext.h"
#include "Physics/RigidBody/Rigidbody.h"
#include "Rendering/Core/Graphics.h"

BoneBoxCollider::BoneBoxCollider(Object* owner, LayerId layerId, Model* model, int nodeIndex,
    const Vector3& size, Matrix offset, PxMaterial* material,
    bool isTrigger, bool freezePositions, bool freezeRotations)
    : BoneCollider(owner, layerId, model, nodeIndex, offset, material, isTrigger, freezePositions, freezeRotations)
    , size(size)
{
    InitializeShape();
}

PxShape* BoneBoxCollider::CreateShape(PxPhysics* physics, PxMaterial* material) const
{
    return physics->createShape(
        PxBoxGeometry(size.x, size.y, size.z),
        *material,
        true);
}

void BoneBoxCollider::Render(const RenderContext& rc)
{
	if (!showDebug)  return;

    PxTransform t = ghostActor->getGlobalPose();
    Quaternion rotation(t.q.x, t.q.y, t.q.z, t.q.w);
    Game::Graphics::Instance().GetShapeRenderer()->DrawBox(
        Conv::ToVector3(t.p),
        rotation.ToEuler(),
        size,
        Color(1.0f, 0.2f, 0.0f, 1.0f));
}

void BoneBoxCollider::DrawGUI()
{
    bool changed = false;
    changed |= ImGui::DragFloat3("Size", &size.x, 0.01f, 0.01f, 10.0f);
    size.x = std::max(size.x, 0.01f);
    size.y = std::max(size.y, 0.01f);
    size.z = std::max(size.z, 0.01f);
    if (changed)
    {
        InitializeShape();
    }
    DrawBoneSettingsGUI();
}