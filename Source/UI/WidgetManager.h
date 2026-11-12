#pragma once
#include <ProfileKit/ProfileKit.h>
#include <ImGuiToolKit/Utf8.h>
#include "IconsFontAwesome5.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <memory>
#include <vector>

#include "UI/Widget.h"
#include "UI/WidgetGroup.h"
#include "UI/UIEventSystem.h"

class WidgetManager
{
public:
	void Register(std::shared_ptr<Widget> widget)
	{
		if (!widget || widget->GetParent()) return;
		if (widget->IsPendingDestroy()) return;
		widget->widgetManager = this;

		widget->Awake();
		widget->Start();
		data.push_back(std::move(widget));
	}

	void Update()
	{
		PROFILE_FUNCTION();
		for (auto& d : data)
		{
			if (d && !d->GetParent()) d->Update();
		}

		// 削除フラグありのオブジェクトを削除
		data.erase(
			std::remove_if(
			data.begin(),
			data.end(),
			[](const std::shared_ptr<Widget>& widget)
		{
			return !widget || widget->IsPendingDestroy() || widget->GetParent();
		}),
			data.end());

		// 全Widgetのレイアウト更新後に、イベントを1回だけ配送する。
		eventSystem.Update(data);
	}

	void Render(const RenderContext& rc, bool affectedByPostProcess)
	{
		PROFILE_FUNCTION();
		for (auto& d : data)
		{
			if (!d)
			{
				continue;
			}

			if (d->IsPendingDestroy() || d->GetParent())
			{
				continue;
			}

			if (d->GetAffectedByPostProcess() != affectedByPostProcess)
			{
				continue;
			}

			d->Render(rc);
		}
	}

	bool DrawGUI(const char* search = "")
	{
		selectionChanged = false;
		for (const std::shared_ptr<Widget>& widget : data) DrawWidgetNode(widget, search);
		return selectionChanged;
	}

	std::shared_ptr<Widget> GetSelectedWidget() const
	{
		auto selected = selectedWidget.lock();
		return selected && !selected->IsPendingDestroy() ? selected : nullptr;
	}

	std::vector<std::shared_ptr<Widget>>& GetWidgets() { return data; }
	const std::vector<std::shared_ptr<Widget>>& GetWidgets() const { return data; }

private:
	void DrawWidgetNode(const std::shared_ptr<Widget>& widget, const char* search)
	{
		if (!widget || widget->IsPendingDestroy()) return;
		// Keep matching parents visible so filtered children remain reachable.
		if (search && *search)
		{
			const auto contains = [&](const std::string& value) {
				std::string name = value, filter = search;
				std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				return name.find(filter) != std::string::npos;
			};
			const auto matchesDescendant = [&](const auto& self, const std::shared_ptr<Widget>& node) -> bool {
				if (!node || node->IsPendingDestroy()) return false;
				if (contains(node->GetName())) return true;
				if (auto* group = dynamic_cast<WidgetGroup*>(node.get()))
					for (const auto& child : group->GetChildren()) if (self(self, child)) return true;
				return false;
			};
			if (!matchesDescendant(matchesDescendant, widget)) return;
		}

		ImGui::PushID(widget.get());
		bool active = widget->IsActive();
		if (ImGui::Checkbox("##Active", &active)) widget->SetActive(active);
		ImGui::SameLine();

		WidgetGroup* group = dynamic_cast<WidgetGroup*>(widget.get());
		bool hasChildren = false;
		if (group) hasChildren = !group->GetChildren().empty();

		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
		std::shared_ptr<Widget> selected = selectedWidget.lock();
		if (selected.get() == widget.get()) flags |= ImGuiTreeNodeFlags_Selected;

		const std::string& widgetName = widget->GetName();
		const char* label = widgetName.c_str();
		if (widgetName.empty()) label = JText(u8"名前なし");
		if (search && *search) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		const bool open = ImGui::TreeNodeEx("##WidgetNode", flags, ICON_FA_WINDOW_MAXIMIZE " %s", label);
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		{
			selectedWidget = widget;
			selectionChanged = true;
		}
		if (open)
		{
			if (group)
			{
				for (const std::shared_ptr<Widget>& child : group->GetChildren())
					DrawWidgetNode(child, search);
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	bool selectionChanged = false;
	UIEventSystem eventSystem;
	std::vector<std::shared_ptr<Widget>> data;
	std::weak_ptr<Widget> selectedWidget;
};
