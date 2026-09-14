// TestPlayScene.cpp
#include "Gameplay/Scene/TestPlayScene.h"

#include <algorithm>
#include <cmath>
#include "Audio/SoundSystem.h"
#include "Gameplay/Actor/AracoreQueen.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Gameplay/Player/LocalPlayer.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/TitleScene.h"
#include "Application/Time/GameTime.h"
#include "Gameplay/Stage/Stage01.h"
#include "Rendering/Component/SpriteRenderComponent.h"
#include "Rendering/Core/Graphics.h"
#include "UI/BossBar.h"
#include "UI/Button.h"
#include "UI/GaugeHUD.h"
#include "UI/HUDWidget.h"
#include "UI/TextWidget.h"
#include "UI/Widget.h"

TestPlayScene::TestPlayScene()
{
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(true);
	graphics.SetWindowMovementLocked(true);
	Mouse& mouse = Game::Input::Instance().GetMouse();
	mouse.SetCursorLock(true);
	mouse.ForceCursorVisible(false);
	// ゲーム起動時はアクションHUDを前面にし、編集パネルはF2で開く。
	showGameEditorGUI = false;

	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;

	player = std::make_shared<LocalPlayer>();
	currentStage = std::make_unique<Stage01>(player.get());

	Stage& stage = *currentStage;
	ActorManager& actorManager = stage.GetActorManager();
	Camera* camera = stage.GetActiveCamera();
	Actor* cameraActor = stage.GetDefaultCameraActor();

	actorManager.Register(player);
	camera->SetPerspectiveFov(
		DirectX::XMConvertToRadians(45),
		screenWidth / screenHeight,
		0.1f,
		1000.0f);
	camera->SetLookAt({0, 3, 5}, {0, 0, 0}, {0, 1, 0});

	thirdPersonCamera =
		cameraActor->AddComponent<ThirdPersonCameraController>(player.get());
	player->SetCameraController(thirdPersonCamera);
	FreeCameraController* freeCamera = cameraActor->AddComponent<FreeCameraController>();
	freeCamera->SetActive(false);

	const auto makeFill = [](const char* name, const Color& color) {
		auto widget = std::make_shared<Widget>(name);
		widget->AddComponent<SpriteRenderComponent>(
			std::make_shared<Texture>(Color(1.0f, 1.0f, 1.0f, 1.0f)),
			SpriteShaderId::Basic, color);
		widget->SetAffectedByPostProcess(false);
		return widget;
	};
	const auto addText = [](const std::shared_ptr<HUDWidget>& parent, const char* name,
		const char* text, float fontSize) {
		auto widget = std::make_shared<TextWidget>(name, text, fontSize);
		parent->AddChild(widget);
		return widget;
	};

	// 画面単位のHUDWidgetだけをManagerへ登録し、内部要素は親のフォルダーで管理する。
	gameHud = std::make_shared<HUDWidget>("HUD / Gameplay");
	playerHealthGauge = std::make_shared<GaugeHUD>("Player Health Gauge");
	playerHealthGauge->SetFrameTexture("Resources/UI/window.png");
	playerHealthGauge->SetEmblemTexture("Resources/UI/circle.png");
	playerHealthGauge->SetFillRect({0.108f, 0.43f}, {0.82f, 0.31f});
	playerHealthGauge->SetEmblemRect({-0.12f, 0.08f}, {0.285f, 0.9f});
	playerHealthGauge->SetSmoothingSpeed(4.5f);
	gameHud->AddChild(playerHealthGauge);

	// 上中央：ボスからの通知でON/OFFする専用HPバー。
	bossBar = std::make_shared<BossBar>();
	gameHud->AddChild(bossBar);
	widgetManager.Register(gameHud);
	for (Actor* actor : actorManager.GetActors())
		if (auto* boss = dynamic_cast<AracoreQueen*>(actor))
			boss->SetBossBar(bossBar.get());

	// ポーズ画面もCanvas相当のHUDWidget一個にまとめる。文字はTTFから描画する。
	pauseHud = std::make_shared<HUDWidget>("HUD / Pause Menu");
	pauseMainHud = std::make_shared<HUDWidget>("Main");
	pauseControlsHud = std::make_shared<HUDWidget>("Controls");
	pauseOptionsHud = std::make_shared<HUDWidget>("Options");
	pauseTitleHud = std::make_shared<HUDWidget>("Title Confirmation");
	pauseDimmer = makeFill("Background Dimmer", Color(0.01f, 0.02f, 0.04f, 0.52f));
	pauseHud->AddChild(pauseDimmer);
	pauseHud->AddChild(pauseMainHud);
	pauseHud->AddChild(pauseControlsHud);
	pauseHud->AddChild(pauseOptionsHud);
	pauseHud->AddChild(pauseTitleHud);
	pauseHeader = addText(pauseMainHud, "Header", "PAUSE MENU", 58.0f);
	pauseHeader->SetColor(Color(0.88f, 0.94f, 1.0f, 1.0f));

	const char* labels[] = {"RESUME", "CONTROLS", "OPTIONS", "RETURN TO TITLE"};
	for (int i = 0; i < 4; ++i)
	{
		auto button = std::make_shared<Button>(std::string("Button / ") + labels[i], labels[i]);
		button->SetSelectionTexture("Resources/UI/pause_selection.png");
		button->SetSelectionSourceRect({100.0f, 260.0f}, {1900.0f, 240.0f});
		button->SetFontSize(44.0f);
		pauseMainHud->AddChild(button);
		pauseButtons.push_back(button);
	}
	pauseButtons[0]->SetOnClick([this] { SetPaused(false); });
	pauseButtons[1]->SetOnClick([this] { SetPausePage(PausePage::Controls); });
	pauseButtons[2]->SetOnClick([this] { SetPausePage(PausePage::Options); });
	pauseButtons[3]->SetOnClick([this] { SetPausePage(PausePage::TitleConfirm); });

	addText(pauseControlsHud, "Controls Header", "CONTROLS", 52.0f);
	const char* controlRows[] = {
		"MOVE                 W A S D  /  LEFT STICK",
		"CAMERA               MOUSE  /  RIGHT STICK",
		"SPRINT               SHIFT  /  L3",
		"ATTACK               LEFT CLICK  /  A",
		"DODGE                SPACE  /  LT",
		"CROUCH               CTRL  /  R3"
	};
	for (int i = 0; i < 6; ++i)
		addText(pauseControlsHud, (std::string("Control Row ") + std::to_string(i)).c_str(),
			controlRows[i], 29.0f);

	addText(pauseOptionsHud, "Options Header", "OPTIONS", 52.0f);
	addText(pauseOptionsHud, "Volume Label", "MASTER VOLUME", 31.0f);
	pauseVolumeGauge = std::make_shared<GaugeHUD>("Master Volume Gauge");
	pauseVolumeGauge->SetFillRect({0.0f, 0.35f}, {1.0f, 0.3f});
	pauseVolumeGauge->SetSmoothingSpeed(12.0f);
	pauseVolumeGauge->SetInteractive([](float ratio)
	{
		SoundSystem::Instance().SetMasterVolume(ratio);
	});
	pauseOptionsHud->AddChild(pauseVolumeGauge);
	addText(pauseOptionsHud, "Sensitivity Label", "CAMERA SENSITIVITY", 31.0f);
	pauseSensitivityGauge = std::make_shared<GaugeHUD>("Camera Sensitivity Gauge");
	pauseSensitivityGauge->SetFillRect({0.0f, 0.35f}, {1.0f, 0.3f});
	pauseSensitivityGauge->SetSmoothingSpeed(12.0f);
	pauseSensitivityGauge->SetInteractive([this](float ratio)
	{
		if (thirdPersonCamera)
			thirdPersonCamera->SetSensitivityScale(0.4f + ratio * 1.6f);
	});
	pauseOptionsHud->AddChild(pauseSensitivityGauge);
	addText(pauseOptionsHud, "Options Help", "UP / DOWN  SELECT       LEFT / RIGHT  ADJUST", 23.0f);

	addText(pauseTitleHud, "Title Header", "RETURN TO TITLE", 52.0f);
	addText(pauseTitleHud, "Title Warning", "CURRENT BATTLE PROGRESS WILL BE LOST.", 27.0f);
	for (const char* label : {"CANCEL", "RETURN"})
	{
		auto button = std::make_shared<Button>(std::string("Button / ") + label, label);
		button->SetSelectionTexture("Resources/UI/pause_selection.png");
		button->SetSelectionSourceRect({100.0f, 260.0f}, {1900.0f, 240.0f});
		button->SetFontSize(31.0f);
		pauseTitleHud->AddChild(button);
		pauseConfirmButtons.push_back(button);
	}
	pauseConfirmButtons[0]->SetOnClick([this] { SetPausePage(PausePage::Main); });
	pauseConfirmButtons[1]->SetOnClick([this] {
		SetPaused(false);
		SceneManager::Instance().LoadScene<TitleScene>();
	});

	pauseHud->SetActive(false);
	widgetManager.Register(pauseHud);
}

TestPlayScene::~TestPlayScene()
{
	Game::Time::paused = false;
	SoundSystem::Instance().SetPaused(false);
	// ステージより先にHUDのshared_ptrが破棄されるため、非所有参照を先に外す。
	if (currentStage)
	{
		for (Actor* actor : currentStage->GetActorManager().GetActors())
			if (auto* boss = dynamic_cast<AracoreQueen*>(actor))
				boss->SetBossBar(nullptr);
	}
	Mouse& mouse = Game::Input::Instance().GetMouse();
	mouse.SetCursorLock(false);
	mouse.SetCursorVisible(true);
	Game::Graphics::Instance().SetWindowMovementLocked(false);
}

void TestPlayScene::SetPaused(bool value)
{
	if (paused == value) return;
	paused = value;
	Game::Time::paused = paused;
	SoundSystem::Instance().SetPaused(paused);
	pauseTransition = 0.0f;
	pauseInputDelay = paused ? 0.12f : 0.0f;
	pauseSelection = 0;
	SetPausePage(PausePage::Main);
	if (pauseHud) pauseHud->SetActive(paused);
	if (!paused) Game::Input::Instance().SuppressGameplayInput(2);

	Mouse& mouse = Game::Input::Instance().GetMouse();
	if (paused)
	{
		cursorWasLocked = mouse.IsCursorLocked();
		cursorWasVisible = mouse.IsCursorVisible();
		mouse.SetCursorLock(false);
		mouse.SetCursorVisible(true);
	}
	else
	{
		mouse.SetCursorLock(cursorWasLocked);
		mouse.SetCursorVisible(cursorWasVisible);
	}
}

void TestPlayScene::SetPausePage(PausePage page)
{
	pausePage = page;
	if (pauseMainHud) pauseMainHud->SetActive(paused && page == PausePage::Main);
	if (pauseControlsHud) pauseControlsHud->SetActive(paused && page == PausePage::Controls);
	if (pauseOptionsHud) pauseOptionsHud->SetActive(paused && page == PausePage::Options);
	if (pauseTitleHud) pauseTitleHud->SetActive(paused && page == PausePage::TitleConfirm);
	for (size_t i = 0; i < pauseButtons.size(); ++i)
		pauseButtons[i]->SetSelected(page == PausePage::Main && static_cast<int>(i) == pauseSelection);
	for (size_t i = 0; i < pauseConfirmButtons.size(); ++i)
		pauseConfirmButtons[i]->SetSelected(
			page == PausePage::TitleConfirm && static_cast<int>(i) == pauseConfirmSelection);
	if (page == PausePage::Options)
	{
		if (pauseVolumeGauge)
		{
			pauseVolumeGauge->SetTargetValue(SoundSystem::Instance().GetMasterVolume());
			pauseVolumeGauge->SnapToTarget();
		}
		if (pauseSensitivityGauge)
		{
			pauseSensitivityGauge->SetTargetValue(thirdPersonCamera
				? (thirdPersonCamera->GetSensitivityScale() - 0.4f) / 1.6f : 0.5f);
			pauseSensitivityGauge->SnapToTarget();
		}
	}
}

void TestPlayScene::UpdatePauseMenu()
{
	pauseTransition = std::min(pauseTransition + Game::Time::unscaledDeltaTime * 6.5f, 1.0f);
	pauseInputDelay = std::max(pauseInputDelay - Game::Time::unscaledDeltaTime, 0.0f);
	if (pauseInputDelay > 0.0f) return;

	GamePad& pad = Game::Input::Instance().GetGamePad();
	Mouse& mouse = Game::Input::Instance().GetMouse();
	const GamePadButton down = pad.GetButtonDown();
	const bool confirm = (down & GamePad::BTN_A) ||
		(GetAsyncKeyState(VK_RETURN) & 1) || (GetAsyncKeyState(VK_SPACE) & 1);
	const bool back = (down & GamePad::BTN_B) != 0;
	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;
	const Vector2 cursor(static_cast<float>(mouse.GetPositionX()), static_cast<float>(mouse.GetPositionY()));
	const auto hovered = [&cursor](float x0, float y0, float x1, float y1) {
		return cursor.x >= x0 && cursor.x <= x1 && cursor.y >= y0 && cursor.y <= y1;
	};

	if (back)
	{
		if (pausePage == PausePage::Main) SetPaused(false);
		else SetPausePage(PausePage::Main);
		return;
	}

	if (pausePage == PausePage::Main)
	{
		if (down & GamePad::BTN_UP) pauseSelection = (pauseSelection + 3) % 4;
		if (down & GamePad::BTN_DOWN) pauseSelection = (pauseSelection + 1) % 4;
		for (size_t i = 0; i < pauseButtons.size(); ++i)
			pauseButtons[i]->SetSelected(static_cast<int>(i) == pauseSelection);
		if (confirm && pauseSelection >= 0 && pauseSelection < static_cast<int>(pauseButtons.size()))
			pauseButtons[pauseSelection]->Activate();
	}
	else if (pausePage == PausePage::Options)
	{
		if (down & GamePad::BTN_UP) pauseOptionSelection = (pauseOptionSelection + 1) % 2;
		if (down & GamePad::BTN_DOWN) pauseOptionSelection = (pauseOptionSelection + 1) % 2;
		if (hovered(screenWidth * 0.49f, screenHeight * 0.31f, screenWidth * 0.95f,
			screenHeight * 0.43f)) pauseOptionSelection = 0;
		if (hovered(screenWidth * 0.49f, screenHeight * 0.45f, screenWidth * 0.95f,
			screenHeight * 0.57f)) pauseOptionSelection = 1;
		float direction = 0.0f;
		if (down & GamePad::BTN_LEFT) direction = -1.0f;
		if (down & GamePad::BTN_RIGHT) direction = 1.0f;
		if (direction != 0.0f)
		{
			if (pauseOptionSelection == 0)
				SoundSystem::Instance().SetMasterVolume(std::clamp(
					SoundSystem::Instance().GetMasterVolume() + direction * 0.05f, 0.0f, 1.0f));
			else if (thirdPersonCamera)
				thirdPersonCamera->SetSensitivityScale(
					thirdPersonCamera->GetSensitivityScale() + direction * 0.1f);
		}
		if (pauseVolumeGauge)
			pauseVolumeGauge->SetTargetValue(SoundSystem::Instance().GetMasterVolume());
		if (pauseSensitivityGauge)
			pauseSensitivityGauge->SetTargetValue(thirdPersonCamera
				? (thirdPersonCamera->GetSensitivityScale() - 0.4f) / 1.6f : 0.5f);
	}
	else if (pausePage == PausePage::TitleConfirm)
	{
		if (down & (GamePad::BTN_LEFT | GamePad::BTN_RIGHT)) pauseConfirmSelection ^= 1;
		for (size_t i = 0; i < pauseConfirmButtons.size(); ++i)
			pauseConfirmButtons[i]->SetSelected(static_cast<int>(i) == pauseConfirmSelection);
		if (confirm && pauseConfirmSelection >= 0 &&
			pauseConfirmSelection < static_cast<int>(pauseConfirmButtons.size()))
			pauseConfirmButtons[pauseConfirmSelection]->Activate();
	}
}

void TestPlayScene::UpdatePauseLayout()
{
	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;
	const float ease = 1.0f - std::pow(1.0f - pauseTransition, 3.0f);
	const float uiScale = std::clamp(screenHeight / 1080.0f, 0.65f, 1.5f);
	const float slide = (1.0f - ease) * screenWidth * 0.05f;
	if (pauseDimmer)
	{
		pauseDimmer->rect.position = Vector2::Zero;
		pauseDimmer->rect.anchor = Vector2::Zero;
		pauseDimmer->rect.size = {screenWidth, screenHeight};
		if (auto* sprite = pauseDimmer->GetComponent<SpriteRenderComponent>())
			sprite->SetColor(Color(0.01f, 0.02f, 0.04f, 0.54f * ease));
	}

	if (pauseHeader)
	{
		pauseHeader->rect.position = {screenWidth * 0.045f - slide, screenHeight * 0.055f};
		pauseHeader->rect.anchor = Vector2::Zero;
		pauseHeader->rect.size = {screenWidth * 0.34f, screenHeight * 0.105f};
		pauseHeader->SetFontSize(58.0f * uiScale);
	}
	for (size_t i = 0; i < pauseButtons.size(); ++i)
	{
		pauseButtons[i]->rect.position = {screenWidth * 0.035f - slide,
			screenHeight * (0.225f + static_cast<float>(i) * 0.09f)};
		pauseButtons[i]->rect.anchor = Vector2::Zero;
		pauseButtons[i]->rect.size = {screenWidth * 0.31f, screenHeight * 0.075f};
		pauseButtons[i]->SetFontSize(44.0f * uiScale);
	}

	if (pauseControlsHud)
	{
		const auto& children = pauseControlsHud->GetChildren();
		for (size_t i = 0; i < children.size(); ++i)
		{
			children[i]->rect.position = {screenWidth * 0.47f + slide,
				screenHeight * (i == 0 ? 0.15f : 0.26f + static_cast<float>(i - 1) * 0.082f)};
			children[i]->rect.anchor = Vector2::Zero;
			children[i]->rect.size = {screenWidth * 0.48f, screenHeight * (i == 0 ? 0.09f : 0.062f)};
			if (auto* text = dynamic_cast<TextWidget*>(children[i].get()))
				text->SetFontSize((i == 0 ? 52.0f : 29.0f) * uiScale);
		}
	}

	if (pauseOptionsHud)
	{
		const auto& children = pauseOptionsHud->GetChildren();
		const float x = screenWidth * 0.50f + slide;
		if (children.size() >= 6)
		{
			children[0]->rect = RectTransform({x, screenHeight * 0.17f}, 0.0f,
				{screenWidth * 0.42f, screenHeight * 0.09f});
			children[1]->rect = RectTransform({x, screenHeight * 0.34f}, 0.0f,
				{screenWidth * 0.22f, screenHeight * 0.065f});
			children[2]->rect = RectTransform({screenWidth * 0.72f + slide, screenHeight * 0.34f}, 0.0f,
				{screenWidth * 0.22f, screenHeight * 0.065f});
			children[3]->rect = RectTransform({x, screenHeight * 0.48f}, 0.0f,
				{screenWidth * 0.25f, screenHeight * 0.065f});
			children[4]->rect = RectTransform({screenWidth * 0.72f + slide, screenHeight * 0.48f}, 0.0f,
				{screenWidth * 0.22f, screenHeight * 0.065f});
			children[5]->rect = RectTransform({x, screenHeight * 0.66f}, 0.0f,
				{screenWidth * 0.44f, screenHeight * 0.06f});
			for (size_t i : {size_t(0), size_t(1), size_t(3), size_t(5)})
				if (auto* text = dynamic_cast<TextWidget*>(children[i].get()))
					text->SetFontSize((i == 0 ? 52.0f : i == 5 ? 23.0f : 31.0f) * uiScale);
		}
	}

	if (pauseTitleHud)
	{
		const auto& children = pauseTitleHud->GetChildren();
		if (children.size() >= 4)
		{
			children[0]->rect = RectTransform({screenWidth * 0.50f + slide, screenHeight * 0.20f}, 0.0f,
				{screenWidth * 0.44f, screenHeight * 0.09f});
			children[1]->rect = RectTransform({screenWidth * 0.50f + slide, screenHeight * 0.36f}, 0.0f,
				{screenWidth * 0.44f, screenHeight * 0.07f});
			for (size_t i = 0; i < 2; ++i)
			{
				pauseConfirmButtons[i]->rect.position = {screenWidth * (0.50f + i * 0.23f) + slide,
					screenHeight * 0.52f};
				pauseConfirmButtons[i]->rect.anchor = Vector2::Zero;
				pauseConfirmButtons[i]->rect.size = {screenWidth * 0.20f, screenHeight * 0.075f};
				pauseConfirmButtons[i]->SetFontSize(31.0f * uiScale);
			}
			if (auto* text = dynamic_cast<TextWidget*>(children[0].get())) text->SetFontSize(52.0f * uiScale);
			if (auto* text = dynamic_cast<TextWidget*>(children[1].get())) text->SetFontSize(27.0f * uiScale);
		}
	}
}

void TestPlayScene::OnUpdate()
{
	const bool playerDead = player && player->IsDead();
	if (playerDead && paused)
	{
		SetPaused(false);
	}
	else if (!playerDead &&
		(Game::Input::Instance().GetGamePad().GetButtonDown() & GamePad::BTN_ESCAPE))
	{
		if (!paused) SetPaused(true);
		else if (pauseInputDelay <= 0.0f)
		{
			if (pausePage == PausePage::Main) SetPaused(false);
			else
			{
				SetPausePage(PausePage::Main);
				pauseInputDelay = 0.12f;
			}
		}
	}
	if (paused) UpdatePauseMenu();
	UpdatePauseLayout();
	if (!player || !playerHealthGauge) return;

	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;
	const float hudScale = std::clamp(
		std::min(screenWidth / 1920.0f, screenHeight / 1080.0f), 0.68f, 1.35f) *
		0.72f;

	// GaugeHUDが実値へ補間するため、回復時も段差なく滑らかに伸びる。
	const float playerHudTop = screenHeight - 146.0f * hudScale;
	const float playerLifeRatio = player->GetMaxLife() > 0.0f
		? std::clamp(player->GetLife() / player->GetMaxLife(), 0.0f, 1.0f) : 0.0f;
	playerHealthGauge->rect.position = {78.0f * hudScale, playerHudTop + 26.0f * hudScale};
	playerHealthGauge->rect.anchor = Vector2::Zero;
	playerHealthGauge->rect.size = {500.0f * hudScale, 92.0f * hudScale};
	playerHealthGauge->SetTargetValue(playerLifeRatio);

	if (paused) return;
}

void TestPlayScene::OnRender(RenderContext& rc)
{
}

void TestPlayScene::OnDrawGUI()
{
	if (ImGui::CollapsingHeader("SceneManager"))
	{
		if (ImGui::Button("ReloadScene"))
			SceneManager::Instance().LoadScene<TestPlayScene>();
	}
}
