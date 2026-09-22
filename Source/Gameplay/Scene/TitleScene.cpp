#include "Gameplay/Scene/TitleScene.h"

#include <algorithm>
#include <cmath>
#include <windows.h>

#include "Application/Time/GameTime.h"
#include "Audio/SoundSystem.h"
#include "Animation/Animator.h"
#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/TestPlayScene.h"
#include "Gameplay/Stage/Component/StageLoader.h"
#include "Gameplay/Stage/Stage.h"
#include "Rendering/Component/SpriteRenderComponent.h"
#include "Rendering/Component/VMDL.h"
#include "Rendering/Core/Graphics.h"
#include "UI/SpriteWidget.h"
#include "UI/Button.h"
#include "UI/GaugeHUD.h"
#include "UI/HUDWidget.h"
#include "UI/TextWidget.h"
#include "UI/Widget.h"

TitleScene::TitleScene()
{
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(true);
	graphics.SetWindowMovementLocked(true);
	// タイトルでは通常UIを優先し、F2が押されたときだけエディタを開く
	showGameEditorGUI = false;

	background = std::make_shared<SpriteWidget>(
		"Resources/UI/title.png", SpriteShaderId::Basic, Color(1, 1, 1, 1));
	background->SetName("Title Background");
	background->SetAffectedByPostProcess(false);
	widgetManager.Register(background);
	CreateTitleWorld();

	const auto makeFill = [](const char* name, const Color& color)
	{
		auto widget = std::make_shared<Widget>(name);
		widget->AddComponent<SpriteRenderComponent>(
			std::make_shared<Texture>(Color(1, 1, 1, 1)),
			SpriteShaderId::Basic, color);
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

	// タイトル画面もポーズ画面と同じCanvas構成と見た目に統一する。
	titleHud = std::make_shared<HUDWidget>("HUD / Title Menu");
	mainHud = std::make_shared<HUDWidget>("Main Menu");
	controlsHud = std::make_shared<HUDWidget>("Controls");
	optionsHud = std::make_shared<HUDWidget>("Options");
	dimmer = makeFill("Background Dimmer", Color(0.01f, 0.02f, 0.04f, 0.42f));
	titleHud->AddChild(dimmer);
	titleHud->AddChild(mainHud);
	titleHud->AddChild(controlsHud);
	titleHud->AddChild(optionsHud);

	menuHeader = addText(mainHud, "Header", "MAIN MENU", 58.0f);
	menuHeader->SetColor(Color(0.88f, 0.94f, 1.0f, 1.0f));
	const char* labels[] = {"PLAY", "CONTROLS", "OPTIONS", "EXIT"};
	for (const char* label : labels)
	{
		auto button = std::make_shared<Button>(std::string("Button / ") + label, label);
		button->SetSelectionTexture("Resources/UI/pause_selection.png");
		button->SetSelectionSourceRect({100.0f, 260.0f}, {1900.0f, 240.0f});
		button->SetFontSize(44.0f);
		mainHud->AddChild(button);
		menuButtons.push_back(button);
	}
	menuButtons[0]->SetOnClick([this]
	{
		if (!loadRequested)
			loadRequested = SceneManager::Instance().LoadScene<TestPlayScene>();
	});
	menuButtons[1]->SetOnClick([this]
	{
		menuSelection = 1;
		SetMenuPage(MenuPage::Controls);
	});
	menuButtons[2]->SetOnClick([this]
	{
		menuSelection = 2;
		SetMenuPage(MenuPage::Options);
	});
	menuButtons[3]->SetOnClick([this]
	{
		menuSelection = 3;
		RequestExit();
	});

	addText(controlsHud, "Controls Header", "CONTROLS", 52.0f);
	const char* controlRows[] = {
		"MOVE                 W A S D  /  LEFT STICK",
		"CAMERA               MOUSE  /  RIGHT STICK",
		"SPRINT               SHIFT  /  L3",
		"ATTACK               LEFT CLICK  /  A",
		"DODGE                SPACE  /  LT",
		"CROUCH               CTRL  /  R3"
	};
	for (int i = 0; i < 6; ++i)
		addText(controlsHud, (std::string("Control Row ") + std::to_string(i)).c_str(),
			controlRows[i], 29.0f);

	addText(optionsHud, "Options Header", "OPTIONS", 52.0f);
	addText(optionsHud, "Volume Label", "MASTER VOLUME", 31.0f);
	volumeGauge = std::make_shared<GaugeHUD>("Master Volume Gauge");
	volumeGauge->SetFillRect({0.0f, 0.35f}, {1.0f, 0.3f});
	volumeGauge->SetSmoothingSpeed(12.0f);
	volumeGauge->SetInteractive([](float ratio)
	{
		SoundSystem::Instance().SetMasterVolume(ratio);
	});
	volumeGauge->SetTargetValue(SoundSystem::Instance().GetMasterVolume());
	volumeGauge->SnapToTarget();
	optionsHud->AddChild(volumeGauge);
	addText(optionsHud, "Options Help", "LEFT / RIGHT  ADJUST", 23.0f);

	widgetManager.Register(titleHud);
	SetMenuPage(MenuPage::Main);

	fadeOverlay = std::make_shared<Widget>("Title Fade");
	fadeOverlay->AddComponent<SpriteRenderComponent>(
		std::make_shared<Texture>(Color(1, 1, 1, 1)),
		SpriteShaderId::Basic,
		Color(0, 0, 0, 1));
	fadeOverlay->SetAffectedByPostProcess(false);
	widgetManager.Register(fadeOverlay);

}

void TitleScene::CreateTitleWorld()
{
	auto titleStage = std::make_unique<Stage>();
	if (!titleStage->LoadVSTG("Resources/Stage/cave_01.vstg")) return;

	StageLoader* loader = titleStage->GetComponent<StageLoader>();
	showcasePosition = loader && loader->HasPlayerStart()
		? loader->GetPlayerStartTransforms().front().position
		: Vector3::Zero;

	showcasePlayer = std::make_shared<Actor>("Title Player", "TitlePreview");
	showcasePlayer->transform.SetPosition(showcasePosition);
	showcasePlayer->transform.SetRotation(Quaternion::Identity);
	VMDL* playerVmdl = showcasePlayer->AddComponent<VMDL>(
		"Resources/Model/Player/CombatGirls_Sword_Shield");
	playerVmdl->SetModelYawOffset(RAD(180.0f));
	if (Animator* animator = playerVmdl->GetAnimator())
		animator->Load("Resources/Animator/TitlePlayer.animator");

	Camera* camera = titleStage->GetActiveCamera();
	if (!camera) return;
	const float width = (std::max)(Game::Graphics::ScreenWidth, 1.0f);
	const float height = (std::max)(Game::Graphics::ScreenHeight, 1.0f);
	camera->SetPerspectiveFov(
		DirectX::XMConvertToRadians(39.0f), width / height, 0.1f, 1000.0f);
	titleStage->GetActorManager().Register(showcasePlayer);
	currentStage = std::move(titleStage);
	titleWorldReady = true;
	background->SetActive(false);
	// ゲーム本編と同じくVSTGのライトだけを使い、共有Skybox光を持ち越さない。
	Game::Graphics::Instance().GetSkyBoxRenderer()->SetIntensity(0.0f);
	UpdateTitleCamera();
}

void TitleScene::UpdateTitleCamera()
{
	if (!titleWorldReady || !currentStage) return;
	Camera* camera = currentStage->GetActiveCamera();
	if (!camera) return;

	// プレイヤーを画面中央より少し右へ置き、静止画に見えない程度にゆっくり漂わせる。
	const float driftX = std::sin(cameraTime * 0.23f) * 0.18f;
	const float driftY = std::sin(cameraTime * 0.31f) * 0.06f;
	const Vector3 eye = showcasePosition + Vector3(3.15f + driftX, 1.55f + driftY, 5.0f);
	const Vector3 focus = showcasePosition + Vector3(0.25f, 0.92f, 0.0f);
	camera->SetLookAt(eye, focus, Vector3::Up);
}

void TitleScene::UpdateLayout()
{
	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;
	if (screenWidth <= 0.0f || screenHeight <= 0.0f) return;

	if (background)
	{
		background->rect.position = Vector2::Zero;
		background->rect.anchor = Vector2::Zero;
		background->rect.size = {screenWidth, screenHeight};

		// 画面比率が変わっても画像を引き伸ばさず、中央をクロップして全面表示する。
		if (auto* sprite = background->GetComponent<SpriteRenderComponent>())
		{
			if (Texture* texture = sprite->GetTexture();
				texture && texture->GetWidth() > 0 && texture->GetHeight() > 0)
			{
				const float textureWidth = static_cast<float>(texture->GetWidth());
				const float textureHeight = static_cast<float>(texture->GetHeight());
				const float textureAspect = textureWidth / textureHeight;
				const float screenAspect = screenWidth / screenHeight;
				Vector2 sourcePosition = Vector2::Zero;
				Vector2 sourceSize(textureWidth, textureHeight);
				if (textureAspect > screenAspect)
				{
					sourceSize.x = textureHeight * screenAspect;
					sourcePosition.x = (textureWidth - sourceSize.x) * 0.5f;
				}
				else
				{
					sourceSize.y = textureWidth / screenAspect;
					sourcePosition.y = (textureHeight - sourceSize.y) * 0.5f;
				}
				sprite->SetSourceRect(sourcePosition, sourceSize);
			}
		}
	}

	if (fadeOverlay)
	{
		fadeOverlay->rect.position = Vector2::Zero;
		fadeOverlay->rect.anchor = Vector2::Zero;
		fadeOverlay->rect.size = {screenWidth, screenHeight};
	}

	const float uiScale = std::clamp(screenHeight / 1080.0f, 0.65f, 1.5f);
	const float ease = 1.0f - std::pow(1.0f - menuTransition, 3.0f);
	const float leftSlide = (1.0f - ease) * screenWidth * 0.05f;
	const float rightSlide = (1.0f - ease) * screenWidth * 0.04f;
	if (dimmer)
	{
		dimmer->rect.position = Vector2::Zero;
		dimmer->rect.anchor = Vector2::Zero;
		dimmer->rect.size = {screenWidth, screenHeight};
		if (auto* sprite = dimmer->GetComponent<SpriteRenderComponent>())
			sprite->SetColor(Color(0.01f, 0.02f, 0.04f, 0.42f * ease));
	}
	if (menuHeader)
	{
		menuHeader->rect = RectTransform(
			{screenWidth * 0.045f - leftSlide, screenHeight * 0.055f}, 0.0f,
			{screenWidth * 0.34f, screenHeight * 0.105f});
		menuHeader->SetFontSize(58.0f * uiScale);
	}
	for (size_t i = 0; i < menuButtons.size(); ++i)
	{
		menuButtons[i]->rect = RectTransform(
			{screenWidth * 0.035f - leftSlide,
			 screenHeight * (0.225f + static_cast<float>(i) * 0.09f)}, 0.0f,
			{screenWidth * 0.31f, screenHeight * 0.075f});
		menuButtons[i]->SetFontSize(44.0f * uiScale);
	}
	if (controlsHud)
	{
		const auto& children = controlsHud->GetChildren();
		for (size_t i = 0; i < children.size(); ++i)
		{
			children[i]->rect = RectTransform(
				{screenWidth * 0.47f + rightSlide,
				 screenHeight * (i == 0 ? 0.15f : 0.26f + static_cast<float>(i - 1) * 0.082f)},
				0.0f, {screenWidth * 0.48f, screenHeight * (i == 0 ? 0.09f : 0.062f)});
			if (auto* text = dynamic_cast<TextWidget*>(children[i].get()))
				text->SetFontSize((i == 0 ? 52.0f : 29.0f) * uiScale);
		}
	}
	if (optionsHud)
	{
		const auto& children = optionsHud->GetChildren();
		const float x = screenWidth * 0.50f + rightSlide;
		if (children.size() >= 4)
		{
			children[0]->rect = RectTransform({x, screenHeight * 0.20f}, 0.0f,
				{screenWidth * 0.42f, screenHeight * 0.09f});
			children[1]->rect = RectTransform({x, screenHeight * 0.38f}, 0.0f,
				{screenWidth * 0.22f, screenHeight * 0.065f});
			children[2]->rect = RectTransform({screenWidth * 0.72f + rightSlide, screenHeight * 0.38f}, 0.0f,
				{screenWidth * 0.22f, screenHeight * 0.065f});
			children[3]->rect = RectTransform({x, screenHeight * 0.55f}, 0.0f,
				{screenWidth * 0.42f, screenHeight * 0.06f});
			if (auto* text = dynamic_cast<TextWidget*>(children[0].get())) text->SetFontSize(52.0f * uiScale);
			if (auto* text = dynamic_cast<TextWidget*>(children[1].get())) text->SetFontSize(31.0f * uiScale);
			if (auto* text = dynamic_cast<TextWidget*>(children[3].get())) text->SetFontSize(23.0f * uiScale);
		}
	}
}

void TitleScene::SetMenuPage(MenuPage page)
{
	menuPage = page;
	if (controlsHud) controlsHud->SetActive(page == MenuPage::Controls);
	if (optionsHud) optionsHud->SetActive(page == MenuPage::Options);
	for (size_t i = 0; i < menuButtons.size(); ++i)
		menuButtons[i]->SetSelected(static_cast<int>(i) == menuSelection);
}

void TitleScene::RequestExit()
{
	if (exitRequested) return;
	exitRequested = true;
	PostMessageW(Game::Graphics::Instance().GetWindowHandle(), WM_CLOSE, 0, 0);
}

void TitleScene::UpdateMenu()
{
	GamePad& gamePad = Game::Input::Instance().GetGamePad();
	const GamePadButton down = gamePad.GetButtonDown();
	if (down & GamePad::BTN_UP) menuSelection = (menuSelection + 3) % 4;
	if (down & GamePad::BTN_DOWN) menuSelection = (menuSelection + 1) % 4;
	for (size_t i = 0; i < menuButtons.size(); ++i)
		menuButtons[i]->SetSelected(static_cast<int>(i) == menuSelection);

	const bool confirmHeld =
		(GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 ||
		(GetAsyncKeyState(VK_SPACE) & 0x8000) != 0 ||
		(gamePad.GetButton() & GamePad::BTN_A) != 0;
	if (!inputArmed)
	{
		inputArmed = !confirmHeld;
		confirmWasHeld = confirmHeld;
		return;
	}
	if (confirmHeld && !confirmWasHeld && menuSelection >= 0 &&
		menuSelection < static_cast<int>(menuButtons.size()))
		menuButtons[menuSelection]->Activate();
	confirmWasHeld = confirmHeld;

	if ((down & GamePad::BTN_B) || (down & GamePad::BTN_ESCAPE))
	{
		if (menuPage == MenuPage::Main) return;
		SetMenuPage(MenuPage::Main);
	}

	if (menuPage == MenuPage::Options)
	{
		float direction = 0.0f;
		if (down & GamePad::BTN_LEFT) direction = -1.0f;
		if (down & GamePad::BTN_RIGHT) direction = 1.0f;
		if (direction != 0.0f)
			SoundSystem::Instance().SetMasterVolume(std::clamp(
				SoundSystem::Instance().GetMasterVolume() + direction * 0.05f, 0.0f, 1.0f));

		if (volumeGauge)
			volumeGauge->SetTargetValue(SoundSystem::Instance().GetMasterVolume());
	}
}

void TitleScene::OnUpdate()
{
	cameraTime += Game::Time::unscaledDeltaTime;
	UpdateTitleCamera();
	UpdateLayout();
	menuTransition = std::min(
		menuTransition + Game::Time::unscaledDeltaTime * 4.5f, 1.0f);

	fadeAlpha = std::max(0.0f, fadeAlpha - Game::Time::unscaledDeltaTime / 0.8f);
	if (fadeOverlay)
		if (auto* sprite = fadeOverlay->GetComponent<SpriteRenderComponent>())
			sprite->SetColor(Color(0, 0, 0, fadeAlpha));

	if (loadRequested || exitRequested || !Game::Input::IsFocusedWindow(true)) return;
	UpdateMenu();
}
