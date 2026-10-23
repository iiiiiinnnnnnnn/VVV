#pragma once

#include "Application/Time/Timer.h"

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

	// 通常時の色は共通API、HP低下時の変化先はボス固有の設定。
	void SetColor(const Color& value) override;
	void SetPhaseColor(const Color& value) { phaseColor = value; }

	void Show(Entity* value);
	void Hide(const Entity* value = nullptr);
	Entity* GetBoss() const { return boss; }

protected:
	void OnUpdate() override;
	void OnDrawGUI() override;

private:
	Color baseColor = Color(0.04f, 0.48f, 0.95f, 0.98f);
	Color phaseColor = Color(1.0f, 0.28f, 0.25f, 0.98f);
	Entity* boss = nullptr;
	std::weak_ptr<Actor> bossLifetime;
	bool showing = false;
	bool visibilityAnimating = false;
	bool filling = false;
	Game::Timer visibilityTime{0.0f};
	float visibilityStart = 1.0f;
	Game::Timer fillTime{0.0f};
	Game::Timer phaseColorElapsed{3.0f};
	float showDuration = 0.45f;
	float hideDuration = 0.55f;
	float fillDuration = 0.9f;
};
