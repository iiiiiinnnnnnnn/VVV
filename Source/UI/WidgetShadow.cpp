#include "UI/WidgetShadow.h"

#include "UI/Widget.h"

WidgetShadow::WidgetShadow(Object* owner, const Vector2& offset, const Color& color)
	: Component(owner), offset(offset), color(color)
{
}

void WidgetShadow::OnRender(const RenderContext& rc)
{
	Widget* widget = dynamic_cast<Widget*>(owner);
	if (widget == nullptr)
	{
		return;
	}

	widget->DrawShadow(rc, offset, color);
}

void WidgetShadow::OnDrawGUI()
{
	ImGui::DragFloat2((const char*)u8"影の位置", &offset.x, 0.5f);
	ImGui::ColorEdit4((const char*)u8"影の色", &color.x);
}
