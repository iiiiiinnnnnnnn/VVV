#pragma once

#include <memory>

#include "UI/GaugeHUD.h"

class Entity;
class Actor;
class Texture;

// ボスの検知状態とHP表示をひとつにまとめた専用HUD。
class BossBar final : public GaugeHUD
{
public:
	BossBar();

	void Show(Entity* value);
	void Hide(const Entity* value = nullptr);
	Entity* GetBoss() const { return boss; }

protected:
	void OnUpdate() override;
	void OnDrawGUI() override;

private:
	Entity* boss = nullptr;
	std::weak_ptr<Actor> bossLifetime;
	bool showing = false;
	bool visibilityAnimating = false;
	bool filling = false;
	float visibilityTime = 0.0f;
	float visibilityStart = 1.0f;
	float fillTime = 0.0f;
	float showDuration = 0.45f;
	float hideDuration = 0.55f;
	float fillDuration = 0.9f;
};
