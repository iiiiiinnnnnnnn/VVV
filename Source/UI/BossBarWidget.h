#pragma once

#include <memory>

#include "UI/GaugeWidget.h"

class Entity;
class Actor;
class Texture;

// ボスバー。ボス検知兼描画用
class BossBarWidget final : public GaugeWidget
{
  public:
	BossBarWidget(const std::string& name, const std::string& maskPath = "Resources/UI/dissolve_animation.png", bool revealOnStart = false);

	void OnAwake() override;
	void OnUpdate() override;

	void Show(Entity* value);
	void Hide(const Entity* value = nullptr);

	Entity* GetBoss() const
	{
		return boss;
	}

  protected:
	Entity* boss = nullptr;
	std::weak_ptr<Actor> bossLifetime;
	bool showing = false;
};
