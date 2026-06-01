// Widget.cpp

#include "Widget.h"

void Widget::Update()
{
    rect.Update();

    OnUpdate();
}

void Widget::DrawGUI()
{
    ImGui::PushID(this);
    if (ImGui::CollapsingHeader(name.empty() ? "Unnamed Object" : name.c_str()))
    {
        if (ImGui::TreeNode("RectTransform"))
        {
            ImGui::DragFloat3("Position", &rect.position.x);
            ImGui::DragFloat("Angle", &rect.angle);
            ImGui::DragFloat3("Size", &rect.size.x);
            ImGui::TreePop();
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