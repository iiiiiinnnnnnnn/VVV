// Actor.cpp

#include "Actor.h"

#include "ActorManager.h"
#include "Rigidbody.h"
#include "CharacterController.h"
#include <imgui.h>

void Actor::Update()
{
    transform.Update();

    Object::Update();
}

void Actor::DrawGUI()
{
    ImGui::PushID(this);
    if (ImGui::CollapsingHeader(name.empty() ? "Unnamed Object" : name.c_str()))
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

Actor* Actor::FindActorByTag(const std::string& searchTag) const
{
    if (!actorManager) return nullptr;
    for (Actor* actor : actorManager->GetActors())
    {
        if (!actor || actor->IsPendingDestroy()) continue;
        if (actor->CompareTag(searchTag))
            return actor;
    }
	return nullptr;
}
