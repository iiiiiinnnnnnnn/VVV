// BoxCollider.cpp

#include "BoxCollider.h"
#include "RenderContext.h"
#include "Rigidbody.h"
#include "Graphics.h"
#include "IconsFontAwesome5.h"

BoxCollider::BoxCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, const Vector3& size, PxMaterial* material)
    : BoxCollider(owner, layerId, rigidbody, size, Vector3::Zero, material)
{
}

BoxCollider::BoxCollider(
    Object* owner,
    LayerId layerId,
    Rigidbody* rigidbody,
    const Vector3& size,
    const Vector3& localPosition,
    PxMaterial* material)
    : PhysicsComponent(owner, layerId)
    , rigidbody(rigidbody)
    , material(material)
    , size(size)
    , localPosition(localPosition)
{
    // エラー用

    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
}

void BoxCollider::Render(const RenderContext& rc)
{
	if (!showDebug) return;

    PxTransform pose =
        rigidbody->GetRigidActor()->getGlobalPose() *
        MakeLocalPose();

    Quaternion rotation(pose.q.x, pose.q.y, pose.q.z, pose.q.w);

    Game::Graphics::Instance().GetShapeRenderer()->DrawBox(
        Conv::ToVector3(pose.p), rotation.ToEuler(), size, {0.0f, 1.0f, 0.0f, 1.0f});
}

void BoxCollider::OnAwake()
{
    UpdateShape();
}

PxTransform BoxCollider::MakeLocalPose() const
{
    return PxTransform(
        PxVec3(localPosition.x, localPosition.y, localPosition.z));
}

void BoxCollider::UpdateShape()
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
        PxBoxGeometry(size.x, size.y, size.z), *material);
    shape->userData = this;
    shape->setLocalPose(MakeLocalPose());

    // ownerのlayerをシェイプに反映
    PhysicsManager::SetLayerToShape(shape, layerId);
    rigidActor->attachShape(*shape);
}

void BoxCollider::DrawGUI()
{
    bool changed = false;
    changed |= ImGui::DragFloat3("Size", &size.x, 0.01f, 0.01f, 100.0f);
    changed |= ImGui::DragFloat3("Local Position", &localPosition.x, 0.01f);

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
