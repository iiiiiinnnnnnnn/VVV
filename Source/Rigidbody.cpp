// Rigidbody.cpp

#include "Rigidbody.h"

Rigidbody::Rigidbody(Actor* owner, PxRigidActor* actor) : Component(owner), rigidActor(actor) {
}

void Rigidbody::Update(float elapsedTime)
{
    if (rigidActor) {
        Vector3 pos = VEC(rigidActor->getGlobalPose().p);
        owner->transform.position = pos;
    }
}

void Rigidbody::DrawGUI(float elapsedTime)
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

StaticRigidbody::StaticRigidbody(Actor* owner)
    : Rigidbody(owner, PhysicsManager::Instance().CreateStatic(owner->transform.matrix)) {
}

void StaticRigidbody::DrawGUI(float elapsedTime)
{
    if (ImGui::CollapsingHeader("StaticRigidbody")) {
        ImGui::Text("Type: %s", rigidActor->getConcreteTypeName());
        if (rigidActor) {
            Vector3 pos = VEC(rigidActor->getGlobalPose().p);
            ImGui::Text("StaticRigidbody Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
        }
    }
}

DynamicRigidbody::DynamicRigidbody(Actor* owner)
    : Rigidbody(owner, PhysicsManager::Instance().CreateDynamic(owner->transform.matrix)) {
}

void DynamicRigidbody::DrawGUI(float elapsedTime)
{
    if (ImGui::CollapsingHeader("DynamicRigidbody")) {
        ImGui::Text("Type: %s", rigidActor->getConcreteTypeName());
        if (rigidActor) {
            Vector3 pos = VEC(rigidActor->getGlobalPose().p);
            ImGui::Text("DynamicRigidbody Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
        }
    }
}

void DynamicRigidbody::AddForce(const Vector3& force)
{
    static_cast<PxRigidDynamic*>(rigidActor)->addForce({ force.x, force.y, force.z });
}

void DynamicRigidbody::SetVelocity(const Vector3& v)
{
    static_cast<PxRigidDynamic*>(rigidActor)->setLinearVelocity({ v.x, v.y, v.z });
}
