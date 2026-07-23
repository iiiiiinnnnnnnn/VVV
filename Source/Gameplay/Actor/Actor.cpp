// Actor.cpp

#include "Gameplay/Actor/Actor.h"

#include "Physics/RigidBody/Rigidbody.h"
#include "Physics/Collider/CharacterController.h"
#include "Physics/Core/PhysicsComponent.h"
#include <imgui.h>

void Actor::Destroy(float delay)
{
    Object::Destroy(delay);
	CollisionEventCallback& eventCallback =
		PhysicsManager::Instance().GetSceneContext().GetEventCallback();

	for (const auto& component : components)
	{
		if (PhysicsComponent* collider = dynamic_cast<PhysicsComponent*>(component.get()))
		{
			eventCallback.RemoveCollider(collider);
			CCHitReport::RemoveColliderFromAll(collider);
			collider->SetActive(false);
		}
		if (CharacterController* controller = dynamic_cast<CharacterController*>(component.get()))
			controller->ReleaseController();
		if (auto* rigidbody = dynamic_cast<Rigidbody*>(component.get()))
        {
            rigidbody->SetSceneEnabled(false);
        }
    }
}
void Actor::Update()
{
    transform.Update();

    Object::Update();
}

void Actor::DrawGUI()
{
    ImGui::PushID(this);
    const bool inspectorOpen = ImGui::CollapsingHeader(name.empty() ? "Unnamed Object" : name.c_str());
    if (inspectorOpen)
    {
        Transform::TransformChangedResult res = transform.DrawGUI();
        if (res.positionChanged)
        {
            auto rb = GetComponent<Rigidbody>();
            if (rb)
            {
                RigidbodyDynamic* rbd = dynamic_cast<RigidbodyDynamic*>(rb);
                if (rbd)
                {
                    rbd->SetVelocity(Vector3::Zero);
                }
                rb->SetPosition(transform.position);
            }

            auto cc = GetComponent<CharacterController>();
            if (cc)
                cc->SetPosition(transform.position);
        }
        if (res.rotationChanged)
        {
            auto rb = GetComponent<Rigidbody>();
            if (rb)
                rb->SetRotation(transform.rotation);
        }
        if (res.scaleChanged)
        {
            // you have no idea what's come
        }

        Object::DrawGUI();

        if (ImGui::TreeNode("User param"))
        {
            OnDrawGUI();
            ImGui::TreePop();
        }
    }
    ImGui::PopID();
}
