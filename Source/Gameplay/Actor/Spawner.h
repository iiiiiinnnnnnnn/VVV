#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Core/Object/Component.h"
#include "Core/Object/Transform.h"

class Actor;
class ActorManager;

class Spawner : public Component
{
  public:
	using Factory = std::function<std::shared_ptr<Actor>(const Transform&)>;

	Spawner(Object* owner, std::string entityName = "EnemySmall");

	// 召喚
	Actor* Summon();
	Actor* Summon(const Transform& transform);
	void ClearSummonedActors();

	// エディタ表示
	void SetEditorPreview(bool enabled);

	// 召喚位置
	void SetSummonTransform(const Transform& transform) { summonTransform = transform; }
	const Transform& GetSummonTransform() const { return summonTransform; }

	// 生成処理
	void SetFactory(Factory factory) { this->factory = std::move(factory); }
	const Factory& GetFactory() const { return factory; }
	void SetActorManager(ActorManager* actorManager) { this->actorManager = actorManager; }
	ActorManager* GetActorManager() const { return actorManager; }

	// 生成対象
	void SetEntityName(const std::string& name) { entityName = name; }
	const std::string& GetEntityName() const { return entityName; }

	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_MAP_MARKED_ALT " Spawner"; }

  private:
	// 召喚設定
	std::string entityName = "EnemySmall";
	Factory factory = {};
	ActorManager* actorManager = nullptr;
	Transform summonTransform = {};

	// 生成状態
	std::vector<std::weak_ptr<Actor>> summonedActors;
	bool editorPreview = false;
};
