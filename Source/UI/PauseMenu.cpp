#include "UI/PauseMenu.h"

#include <algorithm>
#include <cmath>

#include "Application/Input/Input.h"
#include "Application/Time/GameTime.h"
#include "Audio/SoundSystem.h"
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Rendering/Component/SpriteRenderComponent.h"
#include "Rendering/Core/Graphics.h"
#include "UI/Button.h"
#include "UI/GaugeHUD.h"
#include "UI/TextWidget.h"

PauseMenu::PauseMenu(
	ThirdPersonCameraController* cameraController,
	PauseChangedCallback pauseChanged,
	ReturnToTitleCallback returnToTitle)
	: HUDWidget("HUD / Pause Menu"),
	  cameraController(cameraController),
	  pauseChanged(std::move(pauseChanged)),
	  returnToTitle(std::move(returnToTitle))
{
	const auto makeFill = [](const char* name, const Color& color)
	{
		auto widget = std::make_shared<Widget>(name);
		widget->AddComponent<SpriteRenderComponent>(
			std::make_shared<Texture>(Color(1.0f, 1.0f, 1.0f, 1.0f)),
			SpriteShaderId::Basic,
			color);
		widget->SetAffectedByPostProcess(false);
		return widget;
	};
	const auto addText = [](const std::shared_ptr<HUDWidget>& parent,
		const char* name, const char* text, float fontSize)
	{
		auto widget = std::make_shared<TextWidget>(name, text, fontSize);
		parent->AddChild(widget);
		return widget;
	};

	mainPage = std::make_shared<HUDWidget>("Main");
	controlsPage = std::make_shared<HUDWidget>("Controls");
	optionsPage = std::make_shared<HUDWidget>("Options");
	titlePage = std::make_shared<HUDWidget>("Title Confirmation");
	dimmer = makeFill("Background Dimmer", Color(0.01f, 0.02f, 0.04f, 0.52f));
	AddChild(dimmer);
	AddChild(mainPage);
	AddChild(controlsPage);
	AddChild(optionsPage);
	AddChild(titlePage);

	header = addText(mainPage, "Header", "PAUSE MENU", 58.0f);
	header->SetColor(Color(0.88f, 0.94f, 1.0f, 1.0f));

	const char* labels[] = {"RESUME", "CONTROLS", "OPTIONS", "RETURN TO TITLE"};
	for (const char* label : labels)
	{
		auto button = std::make_shared<Button>(std::string("Button / ") + label, label);
		button->SetSelectionTexture("Resources/UI/pause_selection.png");
		button->SetSelectionSourceRect({100.0f, 260.0f}, {1900.0f, 240.0f});
		button->SetFontSize(44.0f);
		mainPage->AddChild(button);
		mainButtons.push_back(button);
	}
	mainButtons[0]->SetOnClick([this] { SetPaused(false); });
	mainButtons[1]->SetOnClick([this] { SetPage(Page::Controls); });
	mainButtons[2]->SetOnClick([this] { SetPage(Page::Options); });
	mainButtons[3]->SetOnClick([this] { SetPage(Page::TitleConfirm); });

	addText(controlsPage, "Controls Header", "CONTROLS", 52.0f);
	const char* controlRows[] = {
		"MOVE                 W A S D  /  LEFT STICK",
		"CAMERA               MOUSE  /  RIGHT STICK",
		"SPRINT               SHIFT  /  L3",
		"ATTACK               LEFT CLICK  /  A",
		"DODGE                SPACE  /  LT",
		"CROUCH               CTRL  /  R3"
	};
	for (int index = 0; index < 6; ++index)
	{
		const std::string rowName = "Control Row " + std::to_string(index);
		addText(controlsPage, rowName.c_str(), controlRows[index], 29.0f);
	}

	addText(optionsPage, "Options Header", "OPTIONS", 52.0f);
	addText(optionsPage, "Volume Label", "MASTER VOLUME", 31.0f);
	volumeGauge = std::make_shared<GaugeHUD>("Master Volume Gauge");
	volumeGauge->SetFillRect({0.0f, 0.35f}, {1.0f, 0.3f});
	volumeGauge->SetSmoothingSpeed(12.0f);
	volumeGauge->SetInteractive([](float ratio)
	{
		SoundSystem::Instance().SetMasterVolume(ratio);
	});
	optionsPage->AddChild(volumeGauge);

	addText(optionsPage, "Sensitivity Label", "CAMERA SENSITIVITY", 31.0f);
	sensitivityGauge = std::make_shared<GaugeHUD>("Camera Sensitivity Gauge");
	sensitivityGauge->SetFillRect({0.0f, 0.35f}, {1.0f, 0.3f});
	sensitivityGauge->SetSmoothingSpeed(12.0f);
	sensitivityGauge->SetInteractive([this](float ratio)
	{
		if (!this->cameraController) return;
		const float sensitivity = 0.4f + ratio * 1.6f;
		this->cameraController->SetSensitivityScale(sensitivity);
	});
	optionsPage->AddChild(sensitivityGauge);
	addText(optionsPage, "Options Help",
		"UP / DOWN  SELECT       LEFT / RIGHT  ADJUST", 23.0f);

	addText(titlePage, "Title Header", "RETURN TO TITLE", 52.0f);
	addText(titlePage, "Title Warning",
		"CURRENT BATTLE PROGRESS WILL BE LOST.", 27.0f);
	for (const char* label : {"CANCEL", "RETURN"})
	{
		auto button = std::make_shared<Button>(std::string("Button / ") + label, label);
		button->SetSelectionTexture("Resources/UI/pause_selection.png");
		button->SetSelectionSourceRect({100.0f, 260.0f}, {1900.0f, 240.0f});
		button->SetFontSize(31.0f);
		titlePage->AddChild(button);
		confirmButtons.push_back(button);
	}
	confirmButtons[0]->SetOnClick([this] { SetPage(Page::Main); });
	confirmButtons[1]->SetOnClick([this]
	{
		if (this->returnToTitle) this->returnToTitle();
	});

	dimmer->SetActive(false);
	SetPage(Page::Main);
}

void PauseMenu::SetPaused(bool value)
{
	if (paused == value) return;
	if (value && !canPause) return;

	paused = value;
	transition = 0.0f;
	inputDelay = 0.0f;
	if (paused) inputDelay = 0.12f;
	mainSelection = 0;
	SetPage(Page::Main);
	dimmer->SetActive(paused);

	if (pauseChanged) pauseChanged(paused);
}

void PauseMenu::SetPage(Page value)
{
	page = value;
	const bool showMain = paused && page == Page::Main;
	const bool showControls = paused && page == Page::Controls;
	const bool showOptions = paused && page == Page::Options;
	const bool showTitle = paused && page == Page::TitleConfirm;
	mainPage->SetActive(showMain);
	controlsPage->SetActive(showControls);
	optionsPage->SetActive(showOptions);
	titlePage->SetActive(showTitle);

	for (size_t index = 0; index < mainButtons.size(); ++index)
	{
		const bool selected = showMain && static_cast<int>(index) == mainSelection;
		mainButtons[index]->SetSelected(selected);
	}
	for (size_t index = 0; index < confirmButtons.size(); ++index)
	{
		const bool selected = showTitle && static_cast<int>(index) == confirmSelection;
		confirmButtons[index]->SetSelected(selected);
	}

	if (!showOptions) return;
	volumeGauge->SetTargetValue(SoundSystem::Instance().GetMasterVolume());
	volumeGauge->SnapToTarget();

	float sensitivityRatio = 0.5f;
	if (cameraController)
	{
		const float sensitivity = cameraController->GetSensitivityScale();
		sensitivityRatio = (sensitivity - 0.4f) / 1.6f;
	}
	sensitivityGauge->SetTargetValue(sensitivityRatio);
	sensitivityGauge->SnapToTarget();
}

void PauseMenu::OnUpdate()
{
	if (!canPause && paused) SetPaused(false);

	const GamePadButton buttonDown = Game::Input::Instance().GetGamePad().GetButtonDown();
	if (buttonDown & GamePad::BTN_ESCAPE)
	{
		if (!paused)
		{
			SetPaused(true);
		}
		else if (inputDelay <= 0.0f)
		{
			if (page == Page::Main) SetPaused(false);
			else
			{
				SetPage(Page::Main);
				inputDelay = 0.12f;
			}
		}
	}

	if (paused)
	{
		UpdateInput();
		UpdateLayout();
	}
	WidgetGroup::OnUpdate();
}

void PauseMenu::UpdateInput()
{
	transition = std::min(transition + Game::Time::unscaledDeltaTime * 6.5f, 1.0f);
	inputDelay = std::max(inputDelay - Game::Time::unscaledDeltaTime, 0.0f);
	if (inputDelay > 0.0f) return;

	GamePad& gamePad = Game::Input::Instance().GetGamePad();
	Mouse& mouse = Game::Input::Instance().GetMouse();
	const GamePadButton buttonDown = gamePad.GetButtonDown();
	bool confirm = (buttonDown & GamePad::BTN_A) != 0;
	if (GetAsyncKeyState(VK_RETURN) & 1) confirm = true;
	if (GetAsyncKeyState(VK_SPACE) & 1) confirm = true;
	const bool back = (buttonDown & GamePad::BTN_B) != 0;
	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;
	const Vector2 cursor(
		static_cast<float>(mouse.GetPositionX()),
		static_cast<float>(mouse.GetPositionY()));
	const auto hovered = [&cursor](float left, float top, float right, float bottom)
	{
		return cursor.x >= left && cursor.x <= right &&
			cursor.y >= top && cursor.y <= bottom;
	};

	if (back)
	{
		if (page == Page::Main) SetPaused(false);
		else SetPage(Page::Main);
		return;
	}

	if (page == Page::Main)
	{
		if (buttonDown & GamePad::BTN_UP) mainSelection = (mainSelection + 3) % 4;
		if (buttonDown & GamePad::BTN_DOWN) mainSelection = (mainSelection + 1) % 4;
		for (size_t index = 0; index < mainButtons.size(); ++index)
		{
			const bool selected = static_cast<int>(index) == mainSelection;
			mainButtons[index]->SetSelected(selected);
		}
		if (confirm && mainSelection >= 0 &&
			mainSelection < static_cast<int>(mainButtons.size()))
		{
			mainButtons[mainSelection]->Activate();
		}
	}
	else if (page == Page::Options)
	{
		if (buttonDown & GamePad::BTN_UP) optionSelection = (optionSelection + 1) % 2;
		if (buttonDown & GamePad::BTN_DOWN) optionSelection = (optionSelection + 1) % 2;
		if (hovered(screenWidth * 0.49f, screenHeight * 0.31f,
			screenWidth * 0.95f, screenHeight * 0.43f)) optionSelection = 0;
		if (hovered(screenWidth * 0.49f, screenHeight * 0.45f,
			screenWidth * 0.95f, screenHeight * 0.57f)) optionSelection = 1;

		float direction = 0.0f;
		if (buttonDown & GamePad::BTN_LEFT) direction = -1.0f;
		if (buttonDown & GamePad::BTN_RIGHT) direction = 1.0f;
		if (direction != 0.0f)
		{
			if (optionSelection == 0)
			{
				const float currentVolume = SoundSystem::Instance().GetMasterVolume();
				const float newVolume = std::clamp(currentVolume + direction * 0.05f, 0.0f, 1.0f);
				SoundSystem::Instance().SetMasterVolume(newVolume);
			}
			else if (cameraController)
			{
				const float currentSensitivity = cameraController->GetSensitivityScale();
				cameraController->SetSensitivityScale(currentSensitivity + direction * 0.1f);
			}
		}

		volumeGauge->SetTargetValue(SoundSystem::Instance().GetMasterVolume());
		float sensitivityRatio = 0.5f;
		if (cameraController)
		{
			const float sensitivity = cameraController->GetSensitivityScale();
			sensitivityRatio = (sensitivity - 0.4f) / 1.6f;
		}
		sensitivityGauge->SetTargetValue(sensitivityRatio);
	}
	else if (page == Page::TitleConfirm)
	{
		if (buttonDown & (GamePad::BTN_LEFT | GamePad::BTN_RIGHT)) confirmSelection ^= 1;
		for (size_t index = 0; index < confirmButtons.size(); ++index)
		{
			const bool selected = static_cast<int>(index) == confirmSelection;
			confirmButtons[index]->SetSelected(selected);
		}
		if (confirm && confirmSelection >= 0 &&
			confirmSelection < static_cast<int>(confirmButtons.size()))
		{
			confirmButtons[confirmSelection]->Activate();
		}
	}
}

void PauseMenu::UpdateLayout()
{
	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;
	const float remainingTransition = 1.0f - transition;
	const float ease = 1.0f - std::pow(remainingTransition, 3.0f);
	const float uiScale = std::clamp(screenHeight / 1080.0f, 0.65f, 1.5f);
	const float slide = (1.0f - ease) * screenWidth * 0.05f;

	dimmer->rect.position = Vector2::Zero;
	dimmer->rect.anchor = Vector2::Zero;
	dimmer->rect.size = {screenWidth, screenHeight};
	SpriteRenderComponent* dimmerSprite = dimmer->GetComponent<SpriteRenderComponent>();
	if (dimmerSprite)
	{
		const float dimmerAlpha = 0.54f * ease;
		dimmerSprite->SetColor(Color(0.01f, 0.02f, 0.04f, dimmerAlpha));
	}

	header->rect.position = {screenWidth * 0.045f - slide, screenHeight * 0.055f};
	header->rect.anchor = Vector2::Zero;
	header->rect.size = {screenWidth * 0.34f, screenHeight * 0.105f};
	header->SetFontSize(58.0f * uiScale);
	for (size_t index = 0; index < mainButtons.size(); ++index)
	{
		const float row = static_cast<float>(index);
		mainButtons[index]->rect.position = {
			screenWidth * 0.035f - slide,
			screenHeight * (0.225f + row * 0.09f)};
		mainButtons[index]->rect.anchor = Vector2::Zero;
		mainButtons[index]->rect.size = {screenWidth * 0.31f, screenHeight * 0.075f};
		mainButtons[index]->SetFontSize(44.0f * uiScale);
	}

	const auto& controlWidgets = controlsPage->GetChildren();
	for (size_t index = 0; index < controlWidgets.size(); ++index)
	{
		float rowTop = 0.15f;
		float rowHeight = 0.09f;
		float fontSize = 52.0f;
		if (index > 0)
		{
			rowTop = 0.26f + static_cast<float>(index - 1) * 0.082f;
			rowHeight = 0.062f;
			fontSize = 29.0f;
		}
		controlWidgets[index]->rect.position = {
			screenWidth * 0.47f + slide,
			screenHeight * rowTop};
		controlWidgets[index]->rect.anchor = Vector2::Zero;
		controlWidgets[index]->rect.size = {screenWidth * 0.48f, screenHeight * rowHeight};
		TextWidget* text = dynamic_cast<TextWidget*>(controlWidgets[index].get());
		if (text) text->SetFontSize(fontSize * uiScale);
	}

	const auto& optionWidgets = optionsPage->GetChildren();
	const float optionsLeft = screenWidth * 0.50f + slide;
	if (optionWidgets.size() >= 6)
	{
		optionWidgets[0]->rect = RectTransform({optionsLeft, screenHeight * 0.17f}, 0.0f,
			{screenWidth * 0.42f, screenHeight * 0.09f});
		optionWidgets[1]->rect = RectTransform({optionsLeft, screenHeight * 0.34f}, 0.0f,
			{screenWidth * 0.22f, screenHeight * 0.065f});
		optionWidgets[2]->rect = RectTransform({screenWidth * 0.72f + slide, screenHeight * 0.34f}, 0.0f,
			{screenWidth * 0.22f, screenHeight * 0.065f});
		optionWidgets[3]->rect = RectTransform({optionsLeft, screenHeight * 0.48f}, 0.0f,
			{screenWidth * 0.25f, screenHeight * 0.065f});
		optionWidgets[4]->rect = RectTransform({screenWidth * 0.72f + slide, screenHeight * 0.48f}, 0.0f,
			{screenWidth * 0.22f, screenHeight * 0.065f});
		optionWidgets[5]->rect = RectTransform({optionsLeft, screenHeight * 0.66f}, 0.0f,
			{screenWidth * 0.44f, screenHeight * 0.06f});

		const size_t textIndices[] = {0, 1, 3, 5};
		for (size_t index : textIndices)
		{
			float fontSize = 31.0f;
			if (index == 0) fontSize = 52.0f;
			if (index == 5) fontSize = 23.0f;
			TextWidget* text = dynamic_cast<TextWidget*>(optionWidgets[index].get());
			if (text) text->SetFontSize(fontSize * uiScale);
		}
	}

	const auto& titleWidgets = titlePage->GetChildren();
	if (titleWidgets.size() >= 4)
	{
		titleWidgets[0]->rect = RectTransform(
			{screenWidth * 0.50f + slide, screenHeight * 0.20f}, 0.0f,
			{screenWidth * 0.44f, screenHeight * 0.09f});
		titleWidgets[1]->rect = RectTransform(
			{screenWidth * 0.50f + slide, screenHeight * 0.36f}, 0.0f,
			{screenWidth * 0.44f, screenHeight * 0.07f});
		for (size_t index = 0; index < 2; ++index)
		{
			const float buttonLeft = 0.50f + static_cast<float>(index) * 0.23f;
			confirmButtons[index]->rect.position = {
				screenWidth * buttonLeft + slide,
				screenHeight * 0.52f};
			confirmButtons[index]->rect.anchor = Vector2::Zero;
			confirmButtons[index]->rect.size = {screenWidth * 0.20f, screenHeight * 0.075f};
			confirmButtons[index]->SetFontSize(31.0f * uiScale);
		}
		TextWidget* titleText = dynamic_cast<TextWidget*>(titleWidgets[0].get());
		if (titleText) titleText->SetFontSize(52.0f * uiScale);
		TextWidget* warningText = dynamic_cast<TextWidget*>(titleWidgets[1].get());
		if (warningText) warningText->SetFontSize(27.0f * uiScale);
	}
}
