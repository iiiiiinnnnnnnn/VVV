#include "UI/WidgetGroup.h"
#include <ImGuiToolKit/Widgets.h>

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
				if (!child) return true;
				if (!child->IsPendingDestroy()) return false;
				child->parent = nullptr;
				return true;
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
	ImGuiToolKit::TextDisabled(u8"子Widget: %zu", children.size());
	for (const auto& child : children)
	{
		if (!child) continue;
		bool active = child->IsActive();
		ImGui::PushID(child.get());
		if (ImGui::Checkbox(child->GetName().c_str(), &active)) child->SetActive(active);
		ImGui::PopID();
	}
}

WidgetGroup::~WidgetGroup()
{
	for (const auto& child : children)
		if (child) child->parent = nullptr;
}

void WidgetGroup::RemoveChild(Widget* child)
{
	std::erase_if(children, [child](const std::shared_ptr<Widget>& value)
	{
		if (value.get() != child) return false;
		value->parent = nullptr;
		return true;
	});
}
