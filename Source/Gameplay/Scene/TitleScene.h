#pragma once

#include "Gameplay/Scene/Scene.h"

class SpriteWidget;
class HUDWidget;
class Button;
class TextWidget;
class GaugeHUD;
class Actor;

class TitleScene : public Scene
{
public:
	TitleScene();
	~TitleScene() override = default;

protected:
	void OnUpdate() override;
	bool UsesGameDebugGUI() const override { return false; }

private:
	enum class MenuPage
	{
		Main,
		Controls,
		Options,
	};

	void UpdateLayout();
	void UpdateMenu();
	void CreateTitleWorld();
	void UpdateTitleCamera();
	void SetMenuPage(MenuPage page);
	void RequestExit();

	std::shared_ptr<SpriteWidget> background;
	std::shared_ptr<Actor> showcasePlayer;
	Vector3 showcasePosition = Vector3::Zero;
	std::shared_ptr<HUDWidget> titleHud;
	std::shared_ptr<HUDWidget> mainHud;
	std::shared_ptr<HUDWidget> controlsHud;
	std::shared_ptr<HUDWidget> optionsHud;
	std::shared_ptr<Widget> dimmer;
	std::shared_ptr<TextWidget> menuHeader;
	std::shared_ptr<GaugeHUD> volumeGauge;
	std::vector<std::shared_ptr<Button>> menuButtons;
	std::shared_ptr<Widget> fadeOverlay;
	MenuPage menuPage = MenuPage::Main;
	int menuSelection = 0;
	float fadeAlpha = 1.0f;
	float menuTransition = 0.0f;
	float cameraTime = 0.0f;
	bool titleWorldReady = false;
	bool inputArmed = false;
	bool confirmWasHeld = false;
	bool loadRequested = false;
	bool exitRequested = false;
};
