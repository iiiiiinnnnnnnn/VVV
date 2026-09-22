#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "UI/HUDWidget.h"

class Button;
class GaugeHUD;
class TextWidget;
class ThirdPersonCameraController;

class PauseMenu : public HUDWidget
{
public:
	using PauseChangedCallback = std::function<void(bool)>;
	using ReturnToTitleCallback = std::function<void()>;

	PauseMenu(
		ThirdPersonCameraController* cameraController,
		PauseChangedCallback pauseChanged,
		ReturnToTitleCallback returnToTitle);

	void SetPaused(bool value);
	bool IsPaused() const { return paused; }
	void SetCanPause(bool value) { canPause = value; }

protected:
	void OnUpdate() override;

private:
	enum class Page
	{
		Main,
		Controls,
		Options,
		TitleConfirm
	};

	void SetPage(Page value);
	void UpdateInput();
	void UpdateLayout();

	ThirdPersonCameraController* cameraController = nullptr;
	PauseChangedCallback pauseChanged;
	ReturnToTitleCallback returnToTitle;
	std::shared_ptr<HUDWidget> mainPage;
	std::shared_ptr<HUDWidget> controlsPage;
	std::shared_ptr<HUDWidget> optionsPage;
	std::shared_ptr<HUDWidget> titlePage;
	std::shared_ptr<Widget> dimmer;
	std::shared_ptr<TextWidget> header;
	std::vector<std::shared_ptr<Button>> mainButtons;
	std::vector<std::shared_ptr<Button>> confirmButtons;
	std::shared_ptr<GaugeHUD> volumeGauge;
	std::shared_ptr<GaugeHUD> sensitivityGauge;
	Page page = Page::Main;
	int mainSelection = 0;
	int optionSelection = 0;
	int confirmSelection = 0;
	float transition = 0.0f;
	float inputDelay = 0.0f;
	bool paused = false;
	bool canPause = true;
};
