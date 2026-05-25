// Widget.cpp

#include "Widget.h"

void Widget::Update(float elapsedTime)
{
    rect.Update();

    OnUpdate(elapsedTime);
}

void Widget::DrawGUI(float elapsedTime)
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