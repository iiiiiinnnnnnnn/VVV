// ActorManager.h

#pragma once
#include <cstddef>

#include <algorithm>
#include <memory>
#include <vector>

#include "Gameplay/Actor/Actor.h"

class ActorManager
{
public:
	ActorManager() { active = this; }
	~ActorManager()
	{
		if (active == this)
			active = nullptr;
	}

	static ActorManager* GetActive() { return active; }

	void Register(std::shared_ptr<Actor> actor)
	{
		if (!actor) return;

		actor->Awake();
		actor->Start();
		pendingActors.emplace_back(actor);
	}

	void Clear()
	{
		data.clear();
		pendingActors.clear();
	}

	void FlushPendingActors()
	{
		if (pendingActors.empty()) return;

		for (std::shared_ptr<Actor>& actor : pendingActors)
		{
			if (!actor) continue;

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
			if (!data[i]) continue;

			if (data[i]->IsPendingDestroy()) continue;

			data[i]->Update();
		}

		for (size_t i = 0; i < count; ++i)
		{
			if (!data[i] || data[i]->IsPendingDestroy()) continue;

			data[i]->LateUpdate();
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
			if (!data[i]) continue;

			if (data[i]->IsPendingDestroy()) continue;

			data[i]->Render(rc);
		}
	}

	void DrawGUI()
	{
		const size_t count = data.size();

		for (size_t i = 0; i < count; ++i)
		{
			if (!data[i]) continue;

			if (data[i]->IsPendingDestroy()) continue;

			data[i]->DrawGUI();
		}
	}

	std::vector<Actor*> GetActors() const
	{
		std::vector<Actor*> result;
		result.reserve(data.size());
		for (const auto& actor : data)
		{
			if (!actor) continue;
			result.push_back(actor.get());
		}
		return result;
	}

	std::vector<Actor*> GetActorsByTag(const std::string& tag) const
	{
		std::vector<Actor*> result;
		for (const auto& actor : data)
		{
			if (!actor || !actor->CompareTag(tag)) continue;
			result.push_back(actor.get());
		}
		return result;
	}

	Actor* FindActorByTag(const std::string& tag) const
	{
		for (const auto& actor : data)
			if (actor && actor->CompareTag(tag)) return actor.get();
		return nullptr;
	}

private:
	inline static ActorManager* active = nullptr;
	std::vector<std::shared_ptr<Actor>> data;
	std::vector<std::shared_ptr<Actor>> pendingActors;
};
