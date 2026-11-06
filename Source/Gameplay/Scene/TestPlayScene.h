#pragma once

#include "Gameplay/Scene/Scene.h"
#include "Rendering/Effect/ParticleSystem.h"
#include "UI/XboxControllerWidget.h"

class LocalPlayer;
class TextWidget;
class WidgetGroup;
class BossBarWidget;
class ThirdPersonCameraController;
class GaugeWidget;
class PauseMenuWidget;
class MovieCameraController;
class Aracore;
class CmdActionWidget;

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
	std::shared_ptr<GaugeWidget> playerHealthGauge;
	std::shared_ptr<BossBarWidget> bossBar;
	std::shared_ptr<CmdActionWidget> cmdBtnWidget;
	std::shared_ptr<CmdActionWidget> movieCmdWidget;
	ThirdPersonCameraController* thirdPersonCamera = nullptr;
	PauseMenuWidget* pauseMenu = nullptr;
	MovieCameraController* movieCamera = nullptr;
	bool moviePreviewTransition = false;
	Aracore* movieBoss = nullptr;

	bool showDevelopText = true;

	const Vector2 DefaultXboxControllerPos = Vector2{-48.0f, 686.0f};
	std::shared_ptr<XboxControllerWidget> xboxControllerWidget;

};
