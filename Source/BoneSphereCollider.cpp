// BoneSphereCollider.cpp

#include "BoneSphereCollider.h"
#include "RenderContext.h"
#include "Rigidbody.h"
#include "Graphics.h"
#include "IconsFontAwesome5.h"

BoneSphereCollider::BoneSphereCollider(Object* owner, LayerId layerId, Model* model,
    int nodeIndex, float radius, Matrix offset, PxMaterial* material,
    bool isTrigger, bool freezePositions, bool freezeRotations)
    : BoneCollider(owner, layerId, model, nodeIndex, offset, material, isTrigger, freezePositions, freezeRotations)
    , radius(radius)
{
    InitializeShape();
}

PxShape* BoneSphereCollider::CreateShape(PxPhysics* physics, PxMaterial* material) const
{
    return physics->createShape(PxSphereGeometry(radius), *material, true);
}

void BoneSphereCollider::Render(const RenderContext& rc)
{
    if (!showDebug) return;

    PxTransform t = ghostActor->getGlobalPose();
    Matrix m = Conv::ToMatrix(t);
    Vector3 pos(m._41, m._42, m._43);

    Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(
        pos, radius, Color(1.0f, 0.0f, 1.0f, 1.0f));
}

void BoneSphereCollider::DrawGUI()
{
    bool changed = false;
    changed |= ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 10.0f);
    if (radius < 0.01f) radius = 0.01f;
    if (changed)
    {
        InitializeShape();
    }
    DrawBoneSettingsGUI();
}