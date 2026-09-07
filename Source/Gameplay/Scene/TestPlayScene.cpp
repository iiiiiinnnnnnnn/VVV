// TestPlayScene.cpp
#include "Gameplay/Scene/TestPlayScene.h"

#include <algorithm>
#include "Gameplay/Actor/AracoreQueen.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Gameplay/Player/LocalPlayer.h"
#include "Gameplay/Scene/GameStartScene.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Stage/Stage01.h"
#include "Rendering/Component/SpriteRenderComponent.h"
#include "Rendering/Core/Graphics.h"
#include "UI/BossBar.h"
#include "UI/SpriteWidget.h"
#include "UI/Widget.h"

TestPlayScene::TestPlayScene()
{
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(true);
	graphics.SetWindowMovementLocked(true);
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

	ThirdPersonCameraController* third =
		cameraActor->AddComponent<ThirdPersonCameraController>(player.get());
	player->SetCameraController(third);
	FreeCameraController* freeCamera = cameraActor->AddComponent<FreeCameraController>();
	freeCamera->SetActive(false);

	const auto makeSprite = [this](const char* name, const char* path, const Color& color) {
		auto widget = std::make_shared<SpriteWidget>(path, SpriteShaderId::Basic, color);
		widget->SetName(name);
		widget->SetAffectedByPostProcess(false);
		widgetManager.Register(widget);
		return widget;
	};
	const auto makeFill = [this](const char* name, const Color& color) {
		auto widget = std::make_shared<Widget>(name);
		widget->AddComponent<SpriteRenderComponent>(
			std::make_shared<Texture>(Color(1.0f, 1.0f, 1.0f, 1.0f)),
			SpriteShaderId::Basic, color);
		widget->SetAffectedByPostProcess(false);
		widgetManager.Register(widget);
		return widget;
	};

	// 左上：プレイヤー状態。装飾枠はResources/UIの素材をそのまま使う。
	playerHudBack = makeSprite("HUD Player Frame", "Resources/UI/window.png",
		Color(0.28f, 0.34f, 0.46f, 0.9f));
	playerHudLife = makeFill("HUD Player Life", Color(0.82f, 0.12f, 0.09f, 0.94f));
	playerHudCircle = makeSprite("HUD Player Emblem", "Resources/UI/circle.png",
		Color(0.62f, 0.68f, 0.78f, 0.96f));

	// 上中央：ボスからの通知でON/OFFする専用HPバー。
	bossBar = std::make_shared<BossBar>();
	widgetManager.Register(bossBar);
	for (Actor* actor : actorManager.GetActors())
		if (auto* boss = dynamic_cast<AracoreQueen*>(actor))
			boss->SetBossBar(bossBar.get());
}

TestPlayScene::~TestPlayScene()
{
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

void TestPlayScene::OnUpdate()
{
	if (Game::Input::Instance().GetGamePad().GetButtonDown() & GamePad::BTN_ESCAPE)
	{
		Mouse& mouse = Game::Input::Instance().GetMouse();
		mouse.SetCursorLock(false);
		mouse.SetCursorVisible(true);
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
		SceneManager::Instance().LoadScene<GameStartScene>();
#else
		PostQuitMessage(0);
#endif
		return;
	}

	if (!player || !playerHudBack || !playerHudLife || !playerHudCircle) return;

	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;
	const float hudScale = std::clamp(
		std::min(screenWidth / 1920.0f, screenHeight / 1080.0f), 0.68f, 1.35f);

	// プレイヤーHPは左から右へクロップし、枠の比率を崩さない。
	const float playerLifeRatio = player->GetMaxLife() > 0.0f
		? std::clamp(player->GetLife() / player->GetMaxLife(), 0.0f, 1.0f) : 0.0f;
	playerHudBack->rect.position = {78.0f * hudScale, 26.0f * hudScale};
	playerHudBack->rect.anchor = {0.0f, 0.0f};
	playerHudBack->rect.size = {500.0f * hudScale, 92.0f * hudScale};
	playerHudLife->rect.position = {132.0f * hudScale, 52.0f * hudScale};
	playerHudLife->rect.anchor = {0.0f, 0.0f};
	playerHudLife->rect.size = {410.0f * hudScale, 38.0f * hudScale};
	playerHudCircle->rect.position = {18.0f * hudScale, 18.0f * hudScale};
	playerHudCircle->rect.anchor = {0.0f, 0.0f};
	playerHudCircle->rect.size = {142.0f * hudScale, 110.0f * hudScale};
	if (auto* life = playerHudLife->GetComponent<SpriteRenderComponent>())
	{
		life->SetHorizontalFill(playerLifeRatio);
		life->SetColor(playerLifeRatio < 0.3f
			? Color(1.0f, 0.035f, 0.02f, 0.98f)
			: Color(0.82f, 0.12f, 0.09f, 0.94f));
	}

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
