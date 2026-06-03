// Rigidbody.cpp

#include "Rigidbody.h"
#include "Actor.h"

Rigidbody::Rigidbody(Object* owner, PxRigidActor* actor) : Component(owner), rigidActor(actor)
{
    Actor* ownerActor = dynamic_cast<Actor*>(owner);
    _ASSERT_EXPR(ownerActor != nullptr, L"Object is not Actor");

    // シーンに登録
    PhysicsManager::Instance().GetSceneContext().GetScene()->addActor(*rigidActor);
}

void Rigidbody::Update()
{
    Actor* ownerActor = dynamic_cast<Actor*>(owner);
    _ASSERT_EXPR(ownerActor != nullptr, L"Object is not Actor");

    if (rigidActor) {
        Vector3 pos = VEC(rigidActor->getGlobalPose().p);
        ownerActor->transform.position = pos;
    }
}

void Rigidbody::DrawGUI()
{
    if (ImGui::TreeNode("Rigidbody"))
    {
        ImGui::Text("Type: %s", rigidActor->getConcreteTypeName());

        if (rigidActor) {
            Vector3 pos = VEC(rigidActor->getGlobalPose().p);
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

RigidbodyStatic::RigidbodyStatic(Object* owner)
    : Rigidbody(owner, PhysicsManager::Instance().CreateStatic(dynamic_cast<Actor*>(owner)->transform.matrix))
{
    Actor* ownerActor = dynamic_cast<Actor*>(owner);
    _ASSERT_EXPR(ownerActor != nullptr, L"Object is not Actor");
}

RigidbodyStatic::RigidbodyStatic(Object* owner, Matrix matrix)
    : Rigidbody(owner, PhysicsManager::Instance().CreateStatic(matrix))
{
    Actor* ownerActor = dynamic_cast<Actor*>(owner);
    _ASSERT_EXPR(ownerActor != nullptr, L"Object is not Actor");
}

void RigidbodyStatic::DrawGUI()
{
    if (ImGui::TreeNode("RigidbodyStatic"))
    {
        if (rigidActor)
        {
            PxRigidStatic* static_ = rigidActor->is<PxRigidStatic>();

            ImGui::Text("Type: %s", rigidActor->getConcreteTypeName());

            // 位置表示
            Vector3 pos = VEC(static_->getGlobalPose().p);
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
        }

        ImGui::TreePop();
    }
}

RigidbodyDynamic::RigidbodyDynamic(Object* owner)
    : Rigidbody(owner, PhysicsManager::Instance().CreateDynamic(dynamic_cast<Actor*>(owner)->transform.matrix))
{
    Actor* ownerActor = dynamic_cast<Actor*>(owner);
    _ASSERT_EXPR(ownerActor != nullptr, L"Object is not Actor");
}

void RigidbodyDynamic::DrawGUI()
{
    if (ImGui::TreeNode("RigidbodyDynamic"))
    {
        if (rigidActor)
        {
            PxRigidDynamic* dynamic = rigidActor->is<PxRigidDynamic>();

            ImGui::Text("Type: %s", rigidActor->getConcreteTypeName());

            // 位置表示
            Vector3 pos = VEC(dynamic->getGlobalPose().p);
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

void RigidbodyDynamic::AddForce(const Vector3& force)
{
    static_cast<PxRigidDynamic*>(rigidActor)->addForce({ force.x, force.y, force.z });
}

void RigidbodyDynamic::SetVelocity(const Vector3& v)
{
    static_cast<PxRigidDynamic*>(rigidActor)->setLinearVelocity({ v.x, v.y, v.z });
}
