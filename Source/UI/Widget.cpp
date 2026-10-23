#include "UI/Widget.h"
#include "IconsFontAwesome5.h"
#include <cmath>

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

bool Widget::ContainsPoint(const Vector2& point) const
{
	if (!IsActive() || IsPendingDestroy() || rect.size.x <= 0 || rect.size.y <= 0) return false;
	const Vector2 topLeft = rect.position - rect.size * rect.anchor;
	const Vector2 center = topLeft + rect.size * 0.5f;
	const Vector2 offset = point - center;
	// SpriteRendererと同じく、矩形の中心を回転の原点にする。
	const float angle = DirectX::XMConvertToRadians(rect.angle);
	const float cosine = std::cos(angle);
	const float sine = std::sin(angle);
	const Vector2 local(offset.x * cosine + offset.y * sine, -offset.x * sine + offset.y * cosine);
	return std::abs(local.x) <= rect.size.x * 0.5f && std::abs(local.y) <= rect.size.y * 0.5f;
}