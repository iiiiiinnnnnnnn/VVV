#include "UI/Widget.h"
#include "IconsFontAwesome5.h"

void Widget::Update()
{
    if (!isActive) return;

    rect.Update();
    Object::Update();
}

void Widget::DrawGUI()
{
    ImGui::PushID(this);
	const std::string label = std::string(ICON_FA_WINDOW_MAXIMIZE " ") +
		(name.empty() ? "Unnamed Object" : name) + "###WidgetInspector";
    if (ImGui::CollapsingHeader(label.c_str()))
    {
		Object::DrawGUI();

        if(ImGui::TreeNode(ICON_FA_INFO_CIRCLE " Widget Info"))
        {
			ImGui::TextDisabled("Widget info is void");
            ImGui::TreePop();
		}

        if (ImGui::TreeNode(ICON_FA_VECTOR_SQUARE " RectTransform"))
        {
            ImGui::DragFloat2("Position", &rect.position.x);
            ImGui::DragFloat("Angle", &rect.angle);
            ImGui::DragFloat2("Size", &rect.size.x);
            ImGui::DragFloat2("Anchor", &rect.anchor.x, 0.01f, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode(ICON_FA_SLIDERS_H " User param"))
        {
            OnDrawGUI();
            ImGui::TreePop();
        }
    }
    ImGui::PopID();

    ImGui::Separator();
}
