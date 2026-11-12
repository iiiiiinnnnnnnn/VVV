#include "UI/WidgetAlpha.h"
#include <ImGuiToolKit/Widgets.h>

WidgetAlpha::WidgetAlpha(Object* owner, float alpha) : Component(owner)
{
	SetAlpha(alpha);
}

void WidgetAlpha::OnDrawGUI()
{
	if (ImGuiToolKit::SliderFloat(u8"透明度", &alpha, 0.0f, 1.0f)) SetAlpha(alpha);
}
