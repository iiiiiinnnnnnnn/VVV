// ActorManager.h

#pragma once

#include "Actor.h"

class ActorManager
{
public:
	void Register(std::shared_ptr<Actor> actor)
	{
		if (!actor) return;

		actor->actorManager = this;
		pendingActors.push_back(actor);
		actor->OnRegistered(this);
	}

	void Clear()
	{
		data.clear();
		pendingActors.clear();
	}

	void FlushPendingActors()
	{
		if (pendingActors.empty())
		{
			return;
		}

		for (std::shared_ptr<Actor>& actor : pendingActors)
		{
			if (!actor)
			{
				continue;
			}

			data.push_back(actor);
		}

		pendingActors.clear();
	}

	void Update()
	{
		FlushPendingActors();

		const size_t count = data.size();

		for (size_t i = 0; i < count; ++i)
		{
			if (!data[i])
			{
				continue;
			}

			if (data[i]->IsPendingDestroy())
			{
				continue;
			}

			data[i]->Update();
		}

		data.erase(
			std::remove_if(
			data.begin(),
			data.end(),
			[](const std::shared_ptr<Actor>& actor)
		{
			return !actor || actor->IsPendingDestroy();
		}),
			data.end());

		FlushPendingActors();
	}

	void Render(const RenderContext& rc)
	{
		const size_t count = data.size();

		for (size_t i = 0; i < count; ++i)
		{
			if (!data[i])
			{
				continue;
			}

			if (data[i]->IsPendingDestroy())
			{
				continue;
			}

			data[i]->Render(rc);
		}
	}

	void DrawGUI()
	{
		const size_t count = data.size();

		for (size_t i = 0; i < count; ++i)
		{
			if (!data[i])
			{
				continue;
			}

			if (data[i]->IsPendingDestroy())
			{
				continue;
			}

			data[i]->DrawGUI();
		}
	}
	std::vector<std::shared_ptr<Actor>>& GetActors() { return data; }
	const std::vector<std::shared_ptr<Actor>>& GetActors() const { return data; }

private:
	std::vector<std::shared_ptr<Actor>> data;
	std::vector<std::shared_ptr<Actor>> pendingActors;
};
