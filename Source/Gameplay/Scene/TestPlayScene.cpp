#include "Gameplay/Scene/TestPlayScene.h"

#include <algorithm>

#include "Application/Time/GameTime.h"
#include "Audio/SoundSystem.h"
#include "Gameplay/Actor/Aracore.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Gameplay/Player/LocalPlayer.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/TitleScene.h"
#include "Gameplay/Stage/Stage01.h"
#include "Rendering/Core/Graphics.h"
#include "UI/BossBar.h"
#include "UI/GaugeHUD.h"
#include "UI/WidgetGroup.h"
#include "UI/PauseMenu.h"
#include "Gameplay/Actor/Deer.h"
#include "Gameplay/Actor/Crystal.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "UI/WidgetShadow.h"

TestPlayScene::TestPlayScene() = default;

void TestPlayScene::OnInit()
{
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(true);
	graphics.SetWindowMovementLocked(true);
	showGameEditorGUI = false;

	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;

	player = std::make_shared<LocalPlayer>();
	player->Init();
	currentStage = std::make_unique<Stage01>(player.get());

	Stage& stage = *currentStage;
	ActorManager& actorManager = stage.GetActorManager();
	Camera* camera = stage.GetActiveCamera();
	Actor* cameraActor = stage.GetDefaultCameraActor();

	actorManager.Register(player);
	camera->SetPerspectiveFov(
		DirectX::XMConvertToRadians(45), screenWidth / screenHeight, 0.1f, 1000.0f);
	camera->SetLookAt({0, 3, 5}, {0, 0, 0}, {0, 1, 0});

	thirdPersonCamera = cameraActor->AddComponent<ThirdPersonCameraController>(player.get());
	player->SetCameraController(thirdPersonCamera);
	FreeCameraController* freeCamera = cameraActor->AddComponent<FreeCameraController>();
	freeCamera->SetActive(false);

	// 画面単位のWidgetだけをManagerへ登録する
	gameHud = std::make_shared<WidgetGroup>("HUD / Gameplay");
	playerHudGroup = std::make_shared<WidgetGroup>("Player UI");
	bossHudGroup = std::make_shared<WidgetGroup>("Boss UI");
	gameHud->AddChild(playerHudGroup);
	gameHud->AddChild(bossHudGroup);

	playerHealthGauge = std::make_shared<GaugeHUD>("Player Health Gauge");
	playerHealthGauge->SetFrameTexture("Resources/UI/window.png");
	playerHealthGauge->SetFillRect({0.15f, 0.25f}, {0.75f, 0.52f});
	playerHealthGauge->rect.position = {screenWidth / 2.0f, 999.600f};
	playerHealthGauge->rect.size = {518.400f, 51.840f};
	playerHealthGauge->rect.anchor = {0.5f, 0.0f};
	playerHealthGauge->SetValueSmoothingSpeed(4.5f);
	playerHudGroup->AddChild(playerHealthGauge);

	bossBar = std::make_shared<BossBar>();
	bossHudGroup->AddChild(bossBar);
	widgetManager.Register(gameHud);
	for (Actor* actor : actorManager.GetActors())
	{
		Aracore* boss = dynamic_cast<Aracore*>(actor);
		if (boss) boss->SetBossBar(bossBar.get());
	}

	// ポーズ画面の内部状態とWidgetはPauseMenu自身が管理する
	auto ownedPauseMenu = std::make_shared<PauseMenu>(
		[this](bool paused) { OnPauseChanged(paused); },
		[this] { ReturnToTitle(); });
	pauseMenu = ownedPauseMenu.get();
	widgetManager.Register(std::move(ownedPauseMenu));

#ifndef _RELEASE // ちょうデバッグ
	developText = std::make_shared<TextWidget>("DevHelp",
		(const char*)u8"※デバッグ/デベロップビルドです\r\n"
					 u8"\r\n"
					 u8"[ゲーム操作]\r\n"
					 u8"[WASD] 移動\r\n"
					 u8"[スペース] 回避（ジャスト回避時クリックで必殺技＋2コンボ）\r\n"
					 u8"[LSHIFT] ダッシュ\r\n"
					 u8"[LCTRL] しゃがみ\r\n"
					 u8"[マウス] 視点操作\r\n"
					 u8"[左クリック] 攻撃＋4コンボ\r\n"
					 u8"[右クリック] キャラにカメラを合わせる\r\n"
					 u8"\r\n"
					 u8"[デバッグコマンド]\r\n"
					 u8"[1] Deerを出現させる\r\n"
					 u8"[2] クリスタルを出現させる\r\n"
					 u8"[3] Aracoreを出現させる\r\n"
					 u8"[4] プレイヤーボスの間にTP\r\n"
					 u8"[5] プレイヤーにダメージ\r\n"
					 u8"[6] プレイヤーを回復\r\n"
					 u8"[7] すべての敵をキル\r\n"
					 u8"[8] 現在のシーンをリロード\r\n"
					 u8"\r\n"
					 u8"[ゲーム内エディタ操作]\r\n"
					 u8"[F1] カーソルをロック/解除\r\n"
					 u8"[F2] ゲーム内エディタ表示/非表示\r\n"
					 u8"[F3] デバッグ表示/非表示\r\n"
					 u8"[F4] ゲーム時間を停止（右クリック＋WASDQEで移動）\r\n"
					 u8"[F5] ゲーム時間を再開\r\n"
					 u8"[F6] (予備)\r\n"
					 u8"[F7] この文字を表示/非表示",
		25.0f, "NotoSansJP");
	developText->AddComponent<WidgetShadow>(
		Vector2(2.0f, 2.0f), Color(0.0f, 0.0f, 0.0f, 1.0f));
	developText->rect.position = {1414.400f, 113.000f};
	developText->rect.size = {499.200f, 864.000f};
	developText->rect.anchor = Vector2::Zero;
	widgetManager.Register(developText);
#endif
}

TestPlayScene::~TestPlayScene()
{
	Uninit();
}

void TestPlayScene::OnUninit()
{
	Game::Time::paused = false;
	SoundSystem::Instance().SetPaused(false);
	if (currentStage)
	{
		for (Actor* actor : currentStage->GetActorManager().GetActors())
		{
			Aracore* boss = dynamic_cast<Aracore*>(actor);
			if (boss) boss->SetBossBar(nullptr);
		}
	}
	Game::Graphics::Instance().SetWindowMovementLocked(false);
	player.reset();
	gameHud.reset();
	playerHudGroup.reset();
	bossHudGroup.reset();
	playerHealthGauge.reset();
	bossBar.reset();
	thirdPersonCamera = nullptr;
	pauseMenu = nullptr;
#ifndef _RELEASE
	developText.reset();
#endif
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
#ifndef _RELEASE // ちょうデバッグ
	if (ImGui::IsKeyPressed(ImGuiKey_F7))
	{
		showDevelopText = !showDevelopText;
		developText->SetActive(showDevelopText);
	}
	// F2デバッグ画面操作中は不可
	if (!showGameEditorGUI)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_1))
		{
			currentStage->GetActorManager().Register(std::make_shared<Deer>(
				Transform::FromPosition(player->transform.position + Vector3(0, 3.0f, 0)))
			);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_2))
		{
			currentStage->GetActorManager().Register(std::make_shared<Crystal>(
				Transform::FromPosition(player->transform.position + Vector3(0, 0.0f, 0)))
			);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_3))
		{
			currentStage->GetActorManager().Register(
				std::make_shared<Aracore>(
				player.get(),
				Transform::FromPosition(player->transform.position + Vector3(0, 3.0f, 0)),
				currentStage->GetComponent<Terrain>())
			);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_4))
		{
			player->GetComponent<CharacterController>()->SetPosition(7.0f, 10.0f, 7.0f);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_5))
		{
			player->TakeDamage(DamageData{.damage = 50.0f});
		}
		if (ImGui::IsKeyPressed(ImGuiKey_6))
		{
			player->Heal(player->GetMaxLife());
		}
		if (ImGui::IsKeyPressed(ImGuiKey_7))
		{
			auto& acts = currentStage->GetActorManager();
			for (auto& enem : acts.GetActorsByTag("Enemy"))
			{
				reinterpret_cast<Entity*>(enem)->TakeDamage(DamageData{.damage = 1000000.0f});
			}
		}
		if (ImGui::IsKeyPressed(ImGuiKey_8))
		{
			if (ImGui::Button("ReloadScene")) SceneManager::Instance().LoadScene<TestPlayScene>();
		}
	}
#endif

	// スポナーが後から生成したボスにもHUDを接続する
	if (currentStage && bossBar)
	{
		for (Actor* actor : currentStage->GetActorManager().GetActors())
		{
			Aracore* boss = dynamic_cast<Aracore*>(actor);
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

	const float maximumLife = player->GetMaxLife();
	playerHealthGauge->SetValue(maximumLife > 0 ?
		std::clamp(player->GetLife() / maximumLife, 0.0f, 1.0f) : 0.0f);
}

void TestPlayScene::OnDrawGUI()
{
	if (ImGui::CollapsingHeader("SceneManager"))
	{
		if (ImGui::Button("ReloadScene"))
			SceneManager::Instance().LoadScene<TestPlayScene>();
	}
}
