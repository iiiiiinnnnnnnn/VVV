// Rigidbody.cpp

#include "Rigidbody.h"
#include "Actor.h"
#include "IconsFontAwesome5.h"

Rigidbody::Rigidbody(Object* owner, PxRigidActor* actor) : Component(owner), rigidActor(actor)
{
    Actor* ownerActor = Component::GetOwnerAsActor();

    // ユーザーデータにActorのポインタをセット
    rigidActor->userData = ownerActor;

    // シーンに登録
    PhysicsManager::Instance().GetSceneContext().GetScene()->addActor(*rigidActor);
}

Rigidbody::~Rigidbody()
{
    if (rigidActor)
    {
        // 自分が所属しているPhysXシーンから除去してから解放する。
        // 非同期ロード用の別シーンで生成された場合でも正しい側から外せる。
        if (PxScene* scene = rigidActor->getScene())
        {
            scene->removeActor(*rigidActor);
        }
        rigidActor->release();
        rigidActor = nullptr;
    }
}

void Rigidbody::Update()
{
    Actor* ownerActor = Component::GetOwnerAsActor();
    if (!ownerActor || !rigidActor) return;

    if (rigidActor->is<PxRigidStatic>())
    {
        return;
    }

    if (PxRigidDynamic* dynamic = rigidActor->is<PxRigidDynamic>())
    {
        if (dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC)
        {
            return;
        }
    }

    PxTransform pose = rigidActor->getGlobalPose();

    ownerActor->transform.position = Vector3(
        pose.p.x,
        pose.p.y,
        pose.p.z);

    ownerActor->transform.rotation = Quaternion(
        pose.q.x,
        pose.q.y,
        pose.q.z,
        pose.q.w);

    ownerActor->transform.Update();
}

void Rigidbody::DrawGUI()
{
    if (ImGui::TreeNode(ICON_FA_WEIGHT_HANGING " Rigidbody"))
    {
        ImGui::Text("Type: %s", rigidActor->getConcreteTypeName());

        if (rigidActor) {
            Vector3 pos = Conv::ToVector3(rigidActor->getGlobalPose().p);
            ImGui::Text("Rigidbody Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
        }
        ImGui::TreePop();
    }
}

void Rigidbody::SetPosition(const Vector3& pos)
{
    PxTransform t = rigidActor->getGlobalPose();
    t.p = { pos.x, pos.y, pos.z };
    rigidActor->setGlobalPose(t);
}

void Rigidbody::SetRotation(const Quaternion& rot)
{
    PxTransform t = rigidActor->getGlobalPose();
    t.q = { rot.x, rot.y, rot.z, rot.w };
	rigidActor->setGlobalPose(t);
}

RigidbodyStatic::RigidbodyStatic(Object* owner)
    : Rigidbody(owner, PhysicsManager::Instance().CreateStatic(Component::GetOwnerAsActor(owner)->transform.matrix))
{

}

RigidbodyStatic::RigidbodyStatic(Object* owner, Matrix matrix)
    : Rigidbody(owner, PhysicsManager::Instance().CreateStatic(matrix))
{
	// エラー用
    Component::GetOwnerAsActor();
}

void RigidbodyStatic::DrawGUI()
{
    if (ImGui::TreeNode(ICON_FA_WEIGHT_HANGING " RigidbodyStatic"))
    {
        if (rigidActor)
        {
            PxRigidStatic* static_ = rigidActor->is<PxRigidStatic>();

            ImGui::Text("Type: %s", rigidActor->getConcreteTypeName());

            // 位置表示
            Vector3 pos = Conv::ToVector3(static_->getGlobalPose().p);
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
        }

        ImGui::TreePop();
    }
}

RigidbodyDynamic::RigidbodyDynamic(Object* owner)
    : Rigidbody(owner, PhysicsManager::Instance().CreateDynamic(Component::GetOwnerAsActor(owner)->transform.matrix))
{

}

void RigidbodyDynamic::DrawGUI()
{
    if (ImGui::TreeNode(ICON_FA_WEIGHT_HANGING " RigidbodyDynamic"))
    {
        if (rigidActor)
        {
            PxRigidDynamic* dynamic = rigidActor->is<PxRigidDynamic>();

            ImGui::Text("Type: %s", rigidActor->getConcreteTypeName());

            // 位置表示
            Vector3 pos = Conv::ToVector3(dynamic->getGlobalPose().p);
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);

            // velocity 表示＆編集
            PxVec3 pxVel = dynamic->getLinearVelocity();
            float vel[3] = { pxVel.x, pxVel.y, pxVel.z };
            if (ImGui::DragFloat3("Velocity", vel, 0.1f))
            {
                dynamic->setLinearVelocity({ vel[0], vel[1], vel[2] });
            }

            // 速度の大きさ表示
            ImGui::Text("Speed: %.2f", sqrtf(pxVel.x * pxVel.x + pxVel.y * pxVel.y + pxVel.z * pxVel.z));

            // リセット
            if (ImGui::Button("Reset Velocity"))
            {
                dynamic->setLinearVelocity({ 0, 0, 0 });
                dynamic->setAngularVelocity({ 0, 0, 0 });
            }
        }
        ImGui::TreePop();
    }
}

void RigidbodyDynamic::LateUpdate()
{
    Actor* ownerActor = Component::GetOwnerAsActor();
    PxRigidDynamic* dynamic = rigidActor->is<PxRigidDynamic>();
    if (!ownerActor || !dynamic) return;
    if (!(dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC)) return;

    SetPosition(ownerActor->transform.position);
    SetRotation(ownerActor->transform.rotation);
}

void RigidbodyDynamic::SetKinematic(bool isKinematic)
{
    PxRigidDynamic* dynamic = rigidActor->is<PxRigidDynamic>();
    if (dynamic)
    {
        if (isKinematic)
        {
            dynamic->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
            dynamic->setRigidBodyFlag(PxRigidBodyFlag::eUSE_KINEMATIC_TARGET_FOR_SCENE_QUERIES, true);
        }
        else
        {
            dynamic->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
            dynamic->setRigidBodyFlag(PxRigidBodyFlag::eUSE_KINEMATIC_TARGET_FOR_SCENE_QUERIES, false);
        }
	}
}

void RigidbodyDynamic::AddForce(const Vector3& force)
{
    static_cast<PxRigidDynamic*>(rigidActor)->addForce({ force.x, force.y, force.z });
}

void RigidbodyDynamic::SetVelocity(const Vector3& v)
{
    static_cast<PxRigidDynamic*>(rigidActor)->setLinearVelocity({ v.x, v.y, v.z });
}
