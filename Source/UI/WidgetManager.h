#pragma once
#include <algorithm>
#include <memory>
#include <vector>

#include "UI/Widget.h"
#include "UI/HUDWidget.h"

class WidgetManager
{
public:
	void Register(std::shared_ptr<Widget> widget)
	{
		if (!widget) return;
		widget->widgetManager = this;
		data.push_back(std::move(widget));
	}

	void Update()
	{
		for (auto& d : data)
		{
			d->Update();
		}

		// 削除フラグありのオブジェクトを削除
		data.erase(
			std::remove_if(
			data.begin(),
			data.end(),
			[](const std::shared_ptr<Widget>& widget)
		{
			return widget->IsPendingDestroy();
		}),
			data.end());
	}

	void Render(const RenderContext& rc, bool affectedByPostProcess)
	{
		for (auto& d : data)
		{
			if (!d)
			{
				continue;
			}

			if (d->IsPendingDestroy())
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

	void DrawGUI()
	{
		ImGui::TextDisabled((const char*)u8"ルートWidget: %zu", data.size());
		if (ImGui::BeginChild("##WidgetHierarchy", ImVec2(0.0f, 170.0f), true))
		{
			for (const std::shared_ptr<Widget>& widget : data)
				DrawWidgetNode(widget);
		}
		ImGui::EndChild();

		ImGui::SeparatorText((const char*)u8"選択中のWidget");
		std::shared_ptr<Widget> selected = selectedWidget.lock();
		if (selected && !selected->IsPendingDestroy()) selected->DrawGUI();
		else ImGui::TextDisabled((const char*)u8"ノードを選択すると詳細を表示します");
	}

	std::vector<std::shared_ptr<Widget>>& GetWidgets() { return data; }
	const std::vector<std::shared_ptr<Widget>>& GetWidgets() const { return data; }

private:
	void DrawWidgetNode(const std::shared_ptr<Widget>& widget)
	{
		if (!widget || widget->IsPendingDestroy()) return;

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
		if (widgetName.empty()) label = (const char*)u8"名前なし";
		const bool open = ImGui::TreeNodeEx("##WidgetNode", flags, "%s", label);
		if (ImGui::IsItemClicked()) selectedWidget = widget;
		if (open)
		{
			if (group)
			{
				for (const std::shared_ptr<Widget>& child : group->GetChildren())
					DrawWidgetNode(child);
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	std::vector<std::shared_ptr<Widget>> data;
	std::weak_ptr<Widget> selectedWidget;
};
