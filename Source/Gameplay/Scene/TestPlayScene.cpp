#include "Gameplay/Scene/TestPlayScene.h"

#include <algorithm>

#include "Application/Time/GameTime.h"
#include "Audio/SoundSystem.h"
#include "Gameplay/Actor/AracoreQueen.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Gameplay/Player/LocalPlayer.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/TitleScene.h"
#include "Gameplay/Stage/Stage01.h"
#include "Rendering/Core/Graphics.h"
#include "UI/BossBar.h"
#include "UI/GaugeHUD.h"
#include "UI/HUDWidget.h"
#include "UI/PauseMenu.h"

TestPlayScene::TestPlayScene()
{
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(true);
	graphics.SetWindowMovementLocked(true);
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

	thirdPersonCamera = cameraActor->AddComponent<ThirdPersonCameraController>(player.get());
	player->SetCameraController(thirdPersonCamera);
	FreeCameraController* freeCamera = cameraActor->AddComponent<FreeCameraController>();
	freeCamera->SetActive(false);

	// 画面単位のWidgetだけをManagerへ登録する
	gameHud = std::make_shared<HUDWidget>("HUD / Gameplay");
	playerHudGroup = std::make_shared<WidgetGroup>("Player UI");
	bossHudGroup = std::make_shared<WidgetGroup>("Boss UI");
	gameHud->AddChild(playerHudGroup);
	gameHud->AddChild(bossHudGroup);

	playerHealthGauge = std::make_shared<GaugeHUD>("Player Health Gauge");
	playerHealthGauge->SetFrameTexture("Resources/UI/window.png");
	playerHealthGauge->SetEmblemTexture("Resources/UI/circle.png");
	playerHealthGauge->SetFillRect({0.108f, 0.43f}, {0.82f, 0.31f});
	playerHealthGauge->SetEmblemRect({-0.12f, 0.08f}, {0.285f, 0.9f});
	playerHealthGauge->SetSmoothingSpeed(4.5f);
	playerHudGroup->AddChild(playerHealthGauge);

	bossBar = std::make_shared<BossBar>();
	bossHudGroup->AddChild(bossBar);
	widgetManager.Register(gameHud);
	for (Actor* actor : actorManager.GetActors())
	{
		AracoreQueen* boss = dynamic_cast<AracoreQueen*>(actor);
		if (boss) boss->SetBossBar(bossBar.get());
	}

	// ポーズ画面の内部状態とWidgetはPauseMenu自身が管理する
	auto ownedPauseMenu = std::make_shared<PauseMenu>(
		thirdPersonCamera,
		[this](bool paused) { OnPauseChanged(paused); },
		[this] { ReturnToTitle(); });
	pauseMenu = ownedPauseMenu.get();
	widgetManager.Register(std::move(ownedPauseMenu));
}

TestPlayScene::~TestPlayScene()
{
	Game::Time::paused = false;
	SoundSystem::Instance().SetPaused(false);
	if (currentStage)
	{
		for (Actor* actor : currentStage->GetActorManager().GetActors())
		{
			AracoreQueen* boss = dynamic_cast<AracoreQueen*>(actor);
			if (boss) boss->SetBossBar(nullptr);
		}
	}
	Game::Graphics::Instance().SetWindowMovementLocked(false);
}

void TestPlayScene::OnPauseChanged(bool paused)
{
	Game::Time::paused = paused;
	SoundSystem::Instance().SetPaused(paused);
	if (!paused) Game::Input::Instance().SuppressGameplayInput(2);

	if (paused)
	{
		SetHudMouseCursorMode(MouseCursorMode::VisibleFree);
	}
	else
	{
		ClearHudMouseCursorMode();
	}
}

void TestPlayScene::ReturnToTitle()
{
	if (pauseMenu) pauseMenu->SetPaused(false);
	SceneManager::Instance().LoadScene<TitleScene>();
}

bool TestPlayScene::ShouldUpdateWorld() const
{
	if (!pauseMenu) return true;
	return !pauseMenu->IsPaused();
}

void TestPlayScene::OnUpdate()
{
	// スポナーが後から生成したボスにもHUDを接続する
	if (currentStage && bossBar)
	{
		for (Actor* actor : currentStage->GetActorManager().GetActors())
		{
			AracoreQueen* boss = dynamic_cast<AracoreQueen*>(actor);
			if (boss) boss->SetBossBar(bossBar.get());
		}
	}

	const bool playerDead = player && player->IsDead();
	if (pauseMenu)
	{
		pauseMenu->SetCanPause(!playerDead);
		if (playerDead && pauseMenu->IsPaused()) pauseMenu->SetPaused(false);
	}

	if (!player || !playerHealthGauge) return;

	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;
	const float screenScale = std::min(screenWidth / 1920.0f, screenHeight / 1080.0f);
	const float hudScale = std::clamp(screenScale, 0.68f, 1.35f) * 0.72f;

	const float playerHudTop = screenHeight - 146.0f * hudScale;
	float playerLifeRatio = 0.0f;
	if (player->GetMaxLife() > 0.0f)
	{
		playerLifeRatio = player->GetLife() / player->GetMaxLife();
		playerLifeRatio = std::clamp(playerLifeRatio, 0.0f, 1.0f);
	}
	playerHealthGauge->rect.position = {78.0f * hudScale, playerHudTop + 26.0f * hudScale};
	playerHealthGauge->rect.anchor = Vector2::Zero;
	playerHealthGauge->rect.size = {500.0f * hudScale, 92.0f * hudScale};
	playerHealthGauge->SetTargetValue(playerLifeRatio);
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
