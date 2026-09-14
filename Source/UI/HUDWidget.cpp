#include "UI/HUDWidget.h"

#include <imgui.h>

HUDWidget::HUDWidget(const std::string& name) : Widget(name)
{
	SetAffectedByPostProcess(false);
}

void HUDWidget::SetChildrenActive(bool active)
{
	for (const auto& child : children)
		if (child) child->SetActive(active);
}

void HUDWidget::OnUpdate()
{
	for (const auto& child : children)
		if (child) child->Update();
}

void HUDWidget::OnRender(const RenderContext& rc)
{
	for (const auto& child : children)
		if (child) child->Render(rc);
}

void HUDWidget::OnDrawGUI()
{
	ImGui::TextDisabled("HUD children: %zu", children.size());
	for (const auto& child : children)
	{
		if (!child) continue;
		bool active = child->IsActive();
		ImGui::PushID(child.get());
		if (ImGui::Checkbox(child->GetName().c_str(), &active)) child->SetActive(active);
		ImGui::PopID();
	}
}
