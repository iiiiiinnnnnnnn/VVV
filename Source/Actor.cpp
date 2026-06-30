// Actor.cpp

#include "Actor.h"
#include "imgui.h"
#include "Components.h"
#include "ActorManager.h"

void Actor::Update()
{
    transform.Update();

    Object::Update();
}

void Actor::DrawGUI()
{
    ImGui::PushID(this);
    debugGUIOpen =
        ImGui::CollapsingHeader(name.empty() ? "Unnamed Object" : name.c_str());

    if (debugGUIOpen)
    {
        ImGui::Checkbox("Hide Debugs", &hideDebugs);

        Object::DrawGUI();

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

        componentList.DrawGUI();

        if (ImGui::TreeNode("User param"))
        {
            OnDrawGUI();
            ImGui::TreePop();
        }
    }
    ImGui::PopID();

    ImGui::Separator();
}

Actor* Actor::FindActorByTag(const std::string& searchTag) const
{
    if (!actorManager) return nullptr;
    for (const std::shared_ptr<Actor>& actor : actorManager->GetActors())
    {
        if (!actor || actor->IsPendingDestroy()) continue;
        if (actor->CompareTag(searchTag))
            return actor.get();
    }
	return nullptr;
}
