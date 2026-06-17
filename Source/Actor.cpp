// Actor.cpp

#include "Actor.h"
#include "imgui.h"
#include "Components.h"

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
        Object::DrawGUI();

        if (ImGui::TreeNode("Actor Info"))
        {
            ImGui::Text("Tag: %s", tag.c_str());
			ImGui::Text("Layer: %d", layer);
			ImGui::TreePop();
        }

        Transform::TransformChangedResult res = transform.DrawGUI();
        if (res.positionChanged)
        {
            auto rb = GetComponent<Rigidbody>();
            if (rb)
                rb->SetPosition(transform.position);
            else
            {
                auto cc = GetComponent<CharacterController>();
                if (cc)
                    cc->SetFootPosition(transform.position);
            }
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
