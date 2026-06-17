// WidgetManager.h

#pragma once

#include "Widget.h"

struct WidgetManager
{
	std::vector<std::shared_ptr<Widget>> data;

	void Register(std::shared_ptr<Widget> widget)
	{
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

	void Render(const RenderContext& rc)
	{
		for (auto& d : data)
		{
			if (!d->IsPendingDestroy())
			{
				d->Render(rc);
			}
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
};