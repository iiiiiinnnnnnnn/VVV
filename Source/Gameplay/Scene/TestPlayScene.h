#pragma once

#include "Gameplay/Scene/Scene.h"
#include "Rendering/Effect/ParticleSystem.h"
#include "UI/TextWidget.h"

class LocalPlayer;
class BossBar;
class ThirdPersonCameraController;
class GaugeHUD;
class WidgetGroup;
class PauseMenu;
class MovieCameraController;
class Aracore;
class CmdActionUI;

class TestPlayScene : public Scene
{
public:
	TestPlayScene();

	~TestPlayScene() override;

	void OnUpdate() override;
	void OnDrawGUI() override;
	MouseCursorMode GetMouseCursorMode() const override;
	bool UsesGameDebugGUI() const override;
	bool ShouldRenderWorldEffects() const override;
	void OnDrawOverlayGUI() override;
	void ConfigurePostProcess(Game::PostProcess& value) override;
	bool ShouldApplyGameplayPostEffects() const override;

protected:
	void OnInit() override;
	void OnUninit() override;

private:
	void OnMovieQteResult(bool success);
	void OnPauseChanged(bool paused);
	void ReturnToTitle();
	bool ShouldUpdateWorld() const override;

#ifndef _RELEASE // ちょうデバッグ
	std::shared_ptr<TextWidget> developText;
#endif
	std::shared_ptr<LocalPlayer> player;
	std::shared_ptr<WidgetGroup> gameHud;
	std::shared_ptr<WidgetGroup> playerHudGroup;
	std::shared_ptr<WidgetGroup> bossHudGroup;
	std::shared_ptr<GaugeHUD> playerHealthGauge;
	std::shared_ptr<BossBar> bossBar;
	std::shared_ptr<CmdActionUI> cmdBtnWidget;
	std::shared_ptr<CmdActionUI> movieCmdWidget;
	ThirdPersonCameraController* thirdPersonCamera = nullptr;
	PauseMenu* pauseMenu = nullptr;
	MovieCameraController* movieCamera = nullptr;
	bool moviePreviewTransition = false;
	Aracore* movieBoss = nullptr;

	bool showDevelopText = true;
};
