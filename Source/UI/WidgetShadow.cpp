#include "UI/WidgetShadow.h"
#include <ImGuiToolKit/Drag.h>
#include <ImGuiToolKit/Color.h>

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
	ImGuiToolKit::DragPosFloat2(u8"影の位置", &offset.x, 0.5f);
	ImGuiToolKit::ColorEdit4(u8"影の色", &color.x);
}
