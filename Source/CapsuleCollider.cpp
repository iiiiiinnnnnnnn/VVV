// CapsuleCollider.cpp

#include "CapsuleCollider.h"
#include "RenderContext.h"
#include "Rigidbody.h"
#include "Graphics.h"
#include "IconsFontAwesome5.h"

CapsuleCollider::CapsuleCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, float radius, float height, PxMaterial* material)
    : CapsuleCollider(
    owner,
    layerId,
    rigidbody,
    radius,
    height,
    Vector3::Zero,
    material)
{
}

CapsuleCollider::CapsuleCollider(
    Object* owner,
    LayerId layerId,
    Rigidbody* rigidbody,
    float radius,
    float height,
    const Vector3& localPosition,
    PxMaterial* material)
    : PhysicsComponent(owner, layerId)
    , rigidbody(rigidbody)
    , material(material)
    , radius(radius)
    , height(height)
    , localPosition(localPosition)
{
    // エラー用

    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
}

void CapsuleCollider::OnAwake()
{
    UpdateShape();
}

PxTransform CapsuleCollider::MakeLocalPose() const
{
    return PxTransform(
        PxVec3(localPosition.x, localPosition.y, localPosition.z),
        PxQuat(DirectX::XM_PIDIV2, PxVec3(0, 0, 1))
    );
}

void CapsuleCollider::Render(const RenderContext& rc)
{
	if (!showDebug) return;

    PxTransform pose =
        rigidbody->GetRigidActor()->getGlobalPose() *
        MakeLocalPose();

    Game::Graphics::Instance().GetShapeRenderer()->DrawCapsule(
        Conv::ToMatrix(pose),
        radius,
        height,
        {0.0f, 1.0f, 0.0f, 1.0f});
}

void CapsuleCollider::UpdateShape()
{
    if (!rigidbody) return;

    PxRigidActor* rigidActor = rigidbody->GetRigidActor();
    if (!rigidActor) return;

    PxPhysics* physics = PhysicsManager::Instance().GetPhysics();

    // 古いシェイプを削除
    if (shape)
    {
        rigidActor->detachShape(*shape);
        shape->release();
        shape = nullptr;
    }

    // 新しいシェイプを生成
    shape = physics->createShape(
        PxCapsuleGeometry(radius, height * 0.5f), *material);
    shape->userData = this;
    shape->setLocalPose(MakeLocalPose());

    // ownerのlayerをシェイプに反映
    PhysicsManager::SetLayerToShape(shape, layerId);
    rigidActor->attachShape(*shape);
}

void CapsuleCollider::DrawGUI()
{
    bool changed = false;
    changed |= ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 100.0f);
    changed |= ImGui::DragFloat("Height", &height, 0.01f, 0.01f, 100.0f);
    changed |= ImGui::DragFloat3("Local Position", &localPosition.x, 0.01f);
    if (radius < 0.01f) radius = 0.01f;

    if (changed) UpdateShape();

    if (ImGui::TreeNode(ICON_FA_GRIP_LINES " Material"))
    {
        float sfriction = material->getStaticFriction();
        float dfriction = material->getDynamicFriction();
        float restitution = material->getRestitution();
        if (ImGui::DragFloat("Static Friction", &sfriction, 0.01f, 0.0f, 1.0f))
            material->setStaticFriction(sfriction);
        if (ImGui::DragFloat("Dynamic Friction", &dfriction, 0.01f, 0.0f, 1.0f))
            material->setDynamicFriction(dfriction);
        if (ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 1.0f))
            material->setRestitution(restitution);
        ImGui::TreePop();
    }
}