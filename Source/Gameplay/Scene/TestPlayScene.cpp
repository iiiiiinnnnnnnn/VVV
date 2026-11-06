#include "Application/SettingsAndDebug/ProfileScope.h"
#include "Gameplay/Scene/TestPlayScene.h"

#include <algorithm>

#include "Application/Time/GameTime.h"
#include "Application/Input/Input.h"
#include "Audio/SoundSystem.h"
#include "Gameplay/Actor/Aracore.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Camera/MovieCameraController.h"
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Gameplay/Player/LocalPlayer.h"
#include "Gameplay/Player/LocalPlayerInput.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/TitleScene.h"
#include "Gameplay/Scene/TimeScaleController.h"
#include "Gameplay/Scene/PostProcessController.h"
#include "Gameplay/Stage/Stage01.h"
#include "Rendering/Core/Graphics.h"
#include "UI/BossBarWidget.h"
#include "UI/GaugeWidget.h"
#include "UI/WidgetGroup.h"
#include "UI/TextWidget.h"
#include "UI/PauseMenuWidget.h"
#include "Gameplay/Actor/Deer.h"
#include "Gameplay/Actor/Crystal.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "UI/WidgetShadow.h"
#include "UI/CmdActionWidget.h"
#include "Physics/Collider/CharacterController.h"
#include "UI/UILayout.h"
#include "UI/ColorWidget.h"

TestPlayScene::TestPlayScene() = default;

void TestPlayScene::OnInit()
{
	PROFILE_FUNCTION();
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(true);
	graphics.SetWindowMovementLocked(true);
	showGameEditorGUI = false;

	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;

	player = std::make_shared<LocalPlayer>();
	player->Awake();
	currentStage = std::make_unique<Stage01>(player.get());
	currentStage->Awake();

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
	movieCamera = cameraActor->AddComponent<MovieCameraController>();

	// 画面単位のWidgetだけをManagerへ登録する
	gameHud = std::make_shared<WidgetGroup>("HUD / Gameplay");
	playerHudGroup = std::make_shared<WidgetGroup>("Player UI");
	bossHudGroup = std::make_shared<WidgetGroup>("Boss UI");
	gameHud->AddChild(playerHudGroup);
	gameHud->AddChild(bossHudGroup);

	// ジャスト回避したときのボタン
	cmdBtnWidget =
		std::make_shared<CmdActionWidget>(CmdActionWidget::ActionBtns::B, player.get(), camera);
	gameHud->AddChild(cmdBtnWidget);

	// ムービー用
	movieCmdWidget = std::make_shared<CmdActionWidget>(
		CmdActionWidget::ActionBtns::B, player.get(), camera, Vector2{140, 140});
	movieCmdWidget->SetName("Movie QTE");
	movieCmdWidget->SetExternalPrompt(0.0f, 0.0f);
	widgetManager.Register(movieCmdWidget);

	playerHealthGauge = std::make_shared<GaugeWidget>("Player Health Gauge");
	playerHealthGauge->SetFrameTexture("Resources/UI/window.png");
	playerHealthGauge->SetFillRect({0.15f, 0.25f}, {0.75f, 0.52f});
	playerHealthGauge->rect.position = {screenWidth / 2.0f, 999.600f};
	playerHealthGauge->rect.size = {518.400f, 51.840f};
	playerHealthGauge->rect.anchor = {0.5f, 0.0f};
	playerHealthGauge->SetValueSmoothingSpeed(4.5f);
	playerHudGroup->AddChild(playerHealthGauge);

	// ボスバー
	bossBar = std::make_shared<BossBarWidget>();
	bossHudGroup->AddChild(bossBar);
	widgetManager.Register(gameHud);
	for (Actor* actor : actorManager.GetActors())
	{
		Aracore* boss = dynamic_cast<Aracore*>(actor);
		if (boss) boss->SetBossBar(bossBar.get());
	}

	// ポーズ画面の内部状態とWidgetはPauseMenu自身が管理する
	auto ownedPauseMenu = std::make_shared<PauseMenuWidget>(
		[this](bool paused) { OnPauseChanged(paused); }, [this] { ReturnToTitle(); });
	pauseMenu = ownedPauseMenu.get();
	widgetManager.Register(std::move(ownedPauseMenu));

#ifndef _RELEASE // ちょうデバッグ
	developText = std::make_shared<TextWidget>("DevHelp",
		(const char*)u8"※デバッグ/デベロップビルドです\r\n"
					 u8"\r\n"
					 u8"[1] Deerを出現させる\r\n"
					 u8"[2] クリスタルを出現させる\r\n"
					 u8"[3] Aracoreを出現させる\r\n"
					 u8"[4] プレイヤーボスの間にTP\r\n"
					 u8"[5] プレイヤーにダメージ\r\n"
					 u8"[6] プレイヤーを回復\r\n"
					 u8"[7] すべての敵に致死ダメージ\r\n"
					 u8"[8] すべてのクリスタルを破壊\r\n"
					 u8"[9] 現在のシーンをリロード\r\n"
					 u8"[0] 空中浮遊\r\n"
					 u8"[F] フォグを表示/非表示\r\n"
					 u8"[G] 草マップを表示/非表示\r\n"
					 u8"[F1] カーソルをロック/解除\r\n"
					 u8"[F2] ゲーム内エディタ表示/非表示\r\n"
					 u8"[F3] デバッグ表示/非表示\r\n"
					 u8"[F4] ゲーム時間を停止（右クリック＋WASDQEで移動）\r\n"
					 u8"[F5] ゲーム時間を再開\r\n"
					 u8"[F6] ムービーカメラ・タイムライン編集\r\n"
					 u8"[F7] この文字を表示/非表示",
		22.0f, "NotoSansJP");
	developText->AddComponent<WidgetShadow>(Vector2(2.0f, 2.0f), Color(0.0f, 0.0f, 0.0f, 1.0f));
	developText->rect.position = {48.1f, -143.0f};
	developText->rect.size = {499.200f, 864.000f};
	developText->rect.anchor = Vector2::Zero;
	gameHud->AddChild(developText);
	widgetManager.Register(developText);
#endif

	// 操作確認画面
	xboxControllerWidget = std::make_shared<XboxControllerWidget>(
		player, DefaultXboxControllerPos + Vector2(0, 400.0f), Vector2{0.4f, 0.4f});
	gameHud->AddChild(xboxControllerWidget);
}

TestPlayScene::~TestPlayScene()
{
	Uninit();
}

void TestPlayScene::OnUninit()
{
	if (movieCmdWidget) movieCmdWidget->ResetPrompt();
	if (movieCamera) movieCamera->End();
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
}

void TestPlayScene::OnMovieQteResult(bool success)
{
	// キーン
	if (success) SoundSystem::Instance().Play("SE/Player/pl_justdodge");
	// QTEの確定結果をシーン側で受け取り、表示やゲーム固有の演出を行う。
	if (!movieCmdWidget) return;
	movieCmdWidget->SetExternalPrompt(0.0f, 0.0f);
	if (success) movieCmdWidget->PlaySuccess();
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
	if (moviePreviewTransition || (movieCamera && movieCamera->IsEditing())) return false;
	if (!pauseMenu) return true;
	return !pauseMenu->IsPaused();
}

void TestPlayScene::OnUpdate()
{
	PROFILE_FUNCTION();
	moviePreviewTransition = false;

	if (movieCmdWidget && (!movieCamera || !movieCamera->IsEditing())) movieCmdWidget->SetExternalPrompt(0.0f, 0.0f);
	if (movieCamera && !movieCamera->IsEditing() && currentStage && player &&
		(!pauseMenu || !pauseMenu->IsPaused()))
	{
		for (Actor* actor : currentStage->GetActorManager().GetActors())
		{
			auto* boss = dynamic_cast<Aracore*>(actor);
			if (!boss || !boss->IsActive() || boss->IsPendingDestroy() || !boss->IsDefeatMovieRequested()) continue;
			boss->ClearDefeatMovieRequest();
			if (movieCamera->BeginPlayback(*currentStage, *player, *boss, postProcess))
			{
				movieBoss = boss;
				if (movieCmdWidget) movieCmdWidget->ResetPrompt();
				if (gameHud) gameHud->SetActive(false);
#ifndef _RELEASE
				if (developText) developText->SetActive(false);
#endif
				if (pauseMenu) pauseMenu->SetCanPause(false);
				return;
			}
			break;
		}
	}
#ifndef _RELEASE
	if (movieCamera && !movieCamera->IsPlayback() && ImGui::IsKeyPressed(ImGuiKey_F6, false) && (!pauseMenu || !pauseMenu->IsPaused()))
	{
		if (movieCmdWidget) movieCmdWidget->ResetPrompt();
		if (movieCamera->IsEditing()) movieCamera->End();
		else if (currentStage && player)
		{
			Actor* boss = nullptr;
			for (Actor* actor : currentStage->GetActorManager().GetActors())
				if (dynamic_cast<Aracore*>(actor) && !actor->IsPendingDestroy())
				{
					boss = actor;
					break;
				}
			movieCamera->Begin(*currentStage, *player, boss, postProcess);
		}
		if (gameHud) gameHud->SetActive(!movieCamera->IsEditing());
		if (developText) developText->SetActive(!movieCamera->IsEditing() && showDevelopText);
		moviePreviewTransition = true;
		if (pauseMenu) pauseMenu->SetCanPause(false);
		return;
	}
#endif
	if (movieCamera && movieCamera->IsEditing())
	{
		auto* input = player->GetComponent<LocalPlayerInput>();
		const bool qtePressed = input && input->PollAttackButton();
		movieCamera->Tick(qtePressed);
		if (movieCmdWidget)
		{
			const float scale = std::max(Game::Time::scale, 0.01f);
			movieCmdWidget->SetExternalPrompt(movieCamera->GetQteRemaining() / scale,
				movieCamera->GetQteDuration() / scale);
		}
		const auto qteResult = movieCamera->ConsumeQteResult();
		if (qteResult != MovieCameraController::QteResult::None)
			OnMovieQteResult(qteResult == MovieCameraController::QteResult::Success);
		if (!movieCamera->IsEditing() && movieBoss)
		{
			player->FinishMovie();
			TimeScaleController::CancelAll();
			PostProcessController::Instance().Reset();
			if (movieCamera->GetPlaybackResult() > 0) movieBoss->CompleteDefeatMovie();
			movieBoss = nullptr;
		}
		moviePreviewTransition = !movieCamera->IsEditing();
		if (gameHud) gameHud->SetActive(!movieCamera->IsEditing());
#ifndef _RELEASE
		if (developText) developText->SetActive(!movieCamera->IsEditing() && showDevelopText);
#endif
		if (pauseMenu) pauseMenu->SetCanPause(false);
		return;
	}
#ifndef _RELEASE // ちょうデバッグ
	if (ImGui::IsKeyPressed(ImGuiKey_F7))
	{
		showDevelopText = !showDevelopText;
		developText->SetActive(showDevelopText);
	}
	// F2デバッグ画面操作中は不可
	if (!showGameEditorGUI)
	{
		// deer
		if (ImGui::IsKeyPressed(ImGuiKey_1))
		{
			if (Random::Range(0, 10) != 0)
			{
				currentStage->GetActorManager().Register(std::make_shared<Deer>(
					Transform::FromPosition(player->transform.position + Vector3(0, 3.0f, 0))));
			}
			else
			{
				currentStage->GetActorManager().Register(std::make_shared<Deer>(
					Transform::FromPosition(player->transform.position + Vector3(0, 3.0f, 0)), "GoldenDeer"));
			}
		}
		// crystal
		if (ImGui::IsKeyPressed(ImGuiKey_2))
		{
			currentStage->GetActorManager().Register(std::make_shared<Crystal>(
				Transform::FromPosition(player->transform.position + Vector3(0, 0.0f, 0))));
		}
		// aracore
		if (ImGui::IsKeyPressed(ImGuiKey_3))
		{
			currentStage->GetActorManager().Register(
				std::make_shared<Aracore>(
					player.get(),
					Transform::FromPosition(player->transform.position + Vector3(0, 3.0f, 0)),
					currentStage->GetComponent<Terrain>()));
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
			auto& acts = currentStage->GetActorManager();
			for (auto& enem : acts.GetActorsByTag("Crystal"))
			{
				reinterpret_cast<Entity*>(enem)->TakeDamage(DamageData{.damage = 1000000.0f});
			}
		}
		if (ImGui::IsKeyPressed(ImGuiKey_9))
		{
			if (ImGui::Button("ReloadScene")) SceneManager::Instance().LoadScene<TestPlayScene>();
		}
		if (ImGui::IsKeyDown(ImGuiKey_0))
		{
			if (auto c = player->GetComponent<CharacterController>())
			{
				c->Move({0, 50.0f * Game::Time::deltaTime, 0});
			}
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
	playerHealthGauge->SetValue(maximumLife > 0 ? std::clamp(player->GetLife() / maximumLife, 0.0f, 1.0f) : 0.0f);

	// とある地点でxboxコントローラーの操作UIを隠す

	// とあるエリアのベクトル
	Transform hideArea = Transform::FromPosition({50.685f, 3.295f, 2.631f});
	hideArea.SetAngle(9.213f, -103.898f, 0.0f);
	auto areaVec = hideArea.GetMatrix();

	// プレイヤーのベクトル
	auto plVec = player->GetTransform()->GetMatrix();

	// 裏表計算
	float omote = (plVec * areaVec.Invert()).m[3][2];
	const float HideSpeed = 4.0f;
	const float blend = std::clamp(Game::Time::unscaledDeltaTime * HideSpeed, 0.0f, 1.0f);
	if (omote > 0)
	{
		// 隠す
		xboxControllerWidget->rect.position =
			Vector2::Lerp(xboxControllerWidget->rect.position,
				DefaultXboxControllerPos + Vector2(0, 400.0f), blend);
	}
	else
	{
		// 出す
		xboxControllerWidget->rect.position =
			Vector2::Lerp(xboxControllerWidget->rect.position,
				DefaultXboxControllerPos + Vector2(0, 0), blend);
	}
}

void TestPlayScene::OnDrawGUI()
{
	PROFILE_FUNCTION();
	if (movieCamera && movieCamera->IsEditing()) return;
	if (ImGui::CollapsingHeader("SceneManager"))
	{
		if (ImGui::Button("ReloadScene"))
			SceneManager::Instance().LoadScene<TestPlayScene>();
	}
}

Scene::MouseCursorMode TestPlayScene::GetMouseCursorMode() const
{
	return movieCamera && movieCamera->IsEditing() ? MouseCursorMode::VisibleFree : MouseCursorMode::HiddenLocked;
}

bool TestPlayScene::UsesGameDebugGUI() const
{
	return !movieCamera || !movieCamera->IsEditing();
}

bool TestPlayScene::ShouldRenderWorldEffects() const
{
	return !movieCamera || !movieCamera->IsEditing();
}

void TestPlayScene::OnDrawOverlayGUI()
{
	if (movieCamera) movieCamera->DrawEditor();
}

void TestPlayScene::ConfigurePostProcess(Game::PostProcess& value)
{
	if (movieCamera) movieCamera->ApplyPostEffects(value);
}

bool TestPlayScene::ShouldApplyGameplayPostEffects() const
{
	return !movieCamera || !movieCamera->IsEditing();
}
