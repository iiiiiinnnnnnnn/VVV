// ActorManager.h

#pragma once

#include "Actor.h"

struct ActorManager
{
public:
	std::vector<std::shared_ptr<Actor>> data;

	void Register(std::shared_ptr<Actor> actor)
	{
		data.push_back(actor);
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
			[](const std::shared_ptr<Actor>& actor)
		{
			return actor->IsPendingDestroy();
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