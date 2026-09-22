#pragma once

#include "Gameplay/Scene/Scene.h"
#include "Rendering/Effect/ParticleSystem.h"

class LocalPlayer;
class BossBar;
class ThirdPersonCameraController;
class GaugeHUD;
class HUDWidget;
class WidgetGroup;
class PauseMenu;

class TestPlayScene : public Scene
{
public:
	TestPlayScene();

	~TestPlayScene() override;

	void OnUpdate() override;
	void OnRender(RenderContext& rc) override;
	void OnDrawGUI() override;
	MouseCursorMode GetMouseCursorMode() const override { return MouseCursorMode::HiddenLocked; }

private:
	void OnPauseChanged(bool paused);
	void ReturnToTitle();
	bool ShouldUpdateWorld() const override;

	std::shared_ptr<LocalPlayer> player;
	std::shared_ptr<HUDWidget> gameHud;
	std::shared_ptr<WidgetGroup> playerHudGroup;
	std::shared_ptr<WidgetGroup> bossHudGroup;
	std::shared_ptr<GaugeHUD> playerHealthGauge;
	std::shared_ptr<BossBar> bossBar;
	ThirdPersonCameraController* thirdPersonCamera = nullptr;
	PauseMenu* pauseMenu = nullptr;
};
