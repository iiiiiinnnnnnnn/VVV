#include "UI/WidgetAlpha.h"
#include <imgui.h>
#include <imgui_stdlib.h>

WidgetAlpha::WidgetAlpha(Object* owner, float alpha) : Component(owner)
{
	SetAlpha(alpha);
}

void WidgetAlpha::OnDrawGUI()
{
	if (ImGui::SliderFloat("透明度", &alpha, 0.0f, 1.0f)) SetAlpha(alpha);
}
