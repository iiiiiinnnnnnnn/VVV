#pragma once

#include "Gameplay/Scene/Scene.h"
#include "Rendering/Effect/ParticleSystem.h"

#include <vector>

class LocalPlayer;
class BossBar;
class ThirdPersonCameraController;
class Button;
class GaugeHUD;
class HUDWidget;
class TextWidget;

class TestPlayScene : public Scene
{
public:
	TestPlayScene();

	~TestPlayScene() override;

	void OnUpdate() override;
	void OnRender(RenderContext& rc) override;
	void OnDrawGUI() override;

private:
	enum class PausePage { Main, Controls, Options, TitleConfirm };

	void SetPaused(bool paused);
	void SetPausePage(PausePage page);
	void UpdatePauseMenu();
	void UpdatePauseLayout();
	bool ShouldUpdateWorld() const override { return !paused; }

	std::shared_ptr<LocalPlayer> player;
	std::shared_ptr<HUDWidget> gameHud;
	std::shared_ptr<GaugeHUD> playerHealthGauge;
	std::shared_ptr<BossBar> bossBar;
	std::shared_ptr<HUDWidget> pauseHud;
	std::shared_ptr<HUDWidget> pauseMainHud;
	std::shared_ptr<HUDWidget> pauseControlsHud;
	std::shared_ptr<HUDWidget> pauseOptionsHud;
	std::shared_ptr<HUDWidget> pauseTitleHud;
	std::shared_ptr<Widget> pauseDimmer;
	std::shared_ptr<TextWidget> pauseHeader;
	std::vector<std::shared_ptr<Button>> pauseButtons;
	std::vector<std::shared_ptr<Button>> pauseConfirmButtons;
	std::shared_ptr<GaugeHUD> pauseVolumeGauge;
	std::shared_ptr<GaugeHUD> pauseSensitivityGauge;
	ThirdPersonCameraController* thirdPersonCamera = nullptr;
	PausePage pausePage = PausePage::Main;
	int pauseSelection = 0;
	int pauseOptionSelection = 0;
	int pauseConfirmSelection = 0;
	float pauseTransition = 0.0f;
	float pauseInputDelay = 0.0f;
	bool paused = false;
	bool cursorWasLocked = false;
	bool cursorWasVisible = true;
};
