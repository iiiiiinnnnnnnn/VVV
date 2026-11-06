#include "UI/WidgetAlpha.h"

WidgetAlpha::WidgetAlpha(Object* owner, float alpha) : Component(owner)
{
	SetAlpha(alpha);
}

void WidgetAlpha::OnDrawGUI()
{
	if (ImGui::SliderFloat((const char*)u8"透明度", &alpha, 0.0f, 1.0f)) SetAlpha(alpha);
}
