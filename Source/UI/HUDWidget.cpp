#include "UI/HUDWidget.h"

#include <algorithm>
#include <imgui.h>

WidgetGroup::WidgetGroup(const std::string& name) : Widget(name)
{
	SetAffectedByPostProcess(false);
}

void WidgetGroup::SetChildrenActive(bool active)
{
	for (const auto& child : children)
		if (child) child->SetActive(active);
}

void WidgetGroup::OnUpdate()
{
	for (const auto& child : children)
		if (child && !child->IsPendingDestroy()) child->Update();

	children.erase(
		std::remove_if(children.begin(), children.end(),
			[](const std::shared_ptr<Widget>& child)
			{
				return !child || child->IsPendingDestroy();
			}),
		children.end());
}

void WidgetGroup::OnRender(const RenderContext& rc)
{
	for (const auto& child : children)
		if (child && !child->IsPendingDestroy()) child->Render(rc);
}

void WidgetGroup::OnDrawGUI()
{
	ImGui::TextDisabled((const char*)u8"子Widget: %zu", children.size());
	for (const auto& child : children)
	{
		if (!child) continue;
		bool active = child->IsActive();
		ImGui::PushID(child.get());
		if (ImGui::Checkbox(child->GetName().c_str(), &active)) child->SetActive(active);
		ImGui::PopID();
	}
}
