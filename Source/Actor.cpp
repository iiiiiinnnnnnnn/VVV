// Actor.cpp

#include "Actor.h"
#include "imgui.h"
#include "Components.h"

void Actor::Update(float elapsedTime)
{
    transform.Update();

    Object::Update(elapsedTime);
}

void Actor::DrawGUI(float elapsedTime)
{
    ImGui::PushID(this);
    if (ImGui::CollapsingHeader(name.empty() ? "Unnamed Object" : name.c_str()))
    {
        if (ImGui::TreeNode("Transform"))
        {
            if (ImGui::DragFloat3("Position", &transform.position.x)) {
                auto rb = GetComponent<Rigidbody>();
                if (rb)
                    rb->SetPosition(transform.position);
                else {
                    auto cc = GetComponent<CharacterController>();
                    if (cc)
                        cc->SetPosition(transform.position);
                    else
						transform.position = transform.position;
                }
            }
            if (ImGui::DragFloat4("Rotation", &transform.rotation.x)) {
                auto rb = GetComponent<Rigidbody>();
                if (rb)
                    rb->SetPosition(transform.position);
                else {
                    auto cc = GetComponent<CharacterController>();
                    if (cc)
                        cc->SetPosition(transform.position);
                    else
						transform.rotation = transform.rotation;
                }
            }
            if (ImGui::DragFloat3("Scale", &transform.scale.x)) {
                auto rb = GetComponent<Rigidbody>();
                if (rb)
                    rb->SetPosition(transform.position);
                else {
                    auto cc = GetComponent<CharacterController>();
                    if (cc)
                        cc->SetPosition(transform.position);
                    else
						transform.scale = transform.scale;
                }
            }
            ImGui::TreePop();
        }

        componentList.DrawGUI(elapsedTime);

        if (ImGui::TreeNode("User param"))
        {
            OnDrawGUI(elapsedTime);
            ImGui::TreePop();
        }
    }
    ImGui::PopID();

    ImGui::Separator();
}
