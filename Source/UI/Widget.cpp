// Widget.cpp

#include "UI/Widget.h"

void Widget::Update()
{
    if (!isActive) return;

    rect.Update();
    Object::Update();
}

void Widget::DrawGUI()
{
    ImGui::PushID(this);
    if (ImGui::CollapsingHeader(name.empty() ? "Unnamed Object" : name.c_str()))
    {
		Object::DrawGUI();

        if(ImGui::TreeNode("Widget Info"))
        {
			ImGui::TextDisabled("Widget info is void");
            ImGui::TreePop();
		}

        if (ImGui::TreeNode("RectTransform"))
        {
            ImGui::DragFloat2("Position", &rect.position.x);
            ImGui::DragFloat("Angle", &rect.angle);
            ImGui::DragFloat2("Size", &rect.size.x);
            ImGui::DragFloat2("Anchor", &rect.anchor.x, 0.01f, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("User param"))
        {
            OnDrawGUI();
            ImGui::TreePop();
        }
    }
    ImGui::PopID();

    ImGui::Separator();
}