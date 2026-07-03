// WidgetManager.h

#pragma once
#include <algorithm>
#include <memory>
#include <vector>

#include "Widget.h"

class WidgetManager
{
public:
	void Register(std::shared_ptr<Widget> widget)
	{
		widget->widgetManager = this;
		data.push_back(widget);
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
		for (auto& d : data)
		{
			if (!d->IsPendingDestroy())
			{
				d->DrawGUI();
			}
		}
	}

	std::vector<std::shared_ptr<Widget>>& GetWidgets() { return data; }
	const std::vector<std::shared_ptr<Widget>>& GetWidgets() const { return data; }

private:
	std::vector<std::shared_ptr<Widget>> data;
};
