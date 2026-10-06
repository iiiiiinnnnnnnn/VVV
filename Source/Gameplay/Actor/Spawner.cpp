#include "Gameplay/Actor/Spawner.h"

#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Actor/ActorManager.h"

Spawner::Spawner(Object* owner, std::string entityName)
	: Component(owner), entityName(std::move(entityName))
{}

Spawner::~Spawner()
{
	ClearSummonedActors();
}

Actor* Spawner::Summon()
{
	return Summon(summonTransform);
}

Actor* Spawner::Summon(const Transform& transform)
{
	if (!actorManager || !factory) return nullptr;

	std::shared_ptr<Actor> summoned = factory(transform);
	if (!summoned) return nullptr;

	Actor* result = summoned.get();
	summonedActors.emplace_back(summoned);
	actorManager->Register(std::move(summoned));
	return result;
}

void Spawner::ClearSummonedActors()
{
	for (const std::weak_ptr<Actor>& reference : summonedActors)
	{
		if (std::shared_ptr<Actor> actor = reference.lock()) actor->Destroy();
	}
	summonedActors.clear();
}

void Spawner::DrawGUI()
{
	ImGui::Text((const char*)u8"生成対象: %s", entityName.c_str());
	ImGui::Text((const char*)u8"生成数: %d", static_cast<int>(summonedActors.size()));
}
