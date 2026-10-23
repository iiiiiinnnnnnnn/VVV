#include "Gameplay/Scene/TitleScene.h"

#include <windows.h>
#include "Core/Foundation/Common.h"
#include "Gameplay/Player/DummyPlayer.h"
#include "Gameplay/Camera/TitleCamera.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/TestPlayScene.h"
#include "Gameplay/Scene/VmdlEditorScene.h"
#include "Gameplay/Scene/VstgEditorScene.h"
#include "Gameplay/Stage/Component/StageLoader.h"
#include "Gameplay/Stage/Stage.h"
#include "Rendering/Core/Graphics.h"

TitleScene::TitleScene() = default;

TitleScene::~TitleScene()
{
	Uninit();
}

void TitleScene::OnUninit()
{
	dummyPlayer.reset();
	titleHud.reset();
}

void TitleScene::OnInit()
{
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(true);
	graphics.SetWindowMovementLocked(true);
	showGameEditorGUI = false;
	CreateTitleWorld();
	titleHud = std::make_shared<TitleHUD>([this](int index) { OnMenuSelected(index); });
	widgetManager.Register(titleHud);
}

void TitleScene::OnMenuSelected(int index)
{
	if (showGameEditorGUI || loadRequested || exitRequested) return;
	switch (index)
	{
	case 0: loadRequested = SceneManager::Instance().LoadScene<TestPlayScene>(); break;
	case 1: RequestExit(); break;
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	case 2: loadRequested = SceneManager::Instance().LoadScene<VmdlEditorScene>(); break;
	case 3: loadRequested = SceneManager::Instance().LoadScene<VstgEditorScene>(); break;
#endif
	}
}

void TitleScene::CreateTitleWorld()
{
	auto titleStage = std::make_unique<Stage>();
	if (!titleStage->LoadVSTG("cave_title")) return;

	StageLoader* loader = titleStage->GetComponent<StageLoader>();
	const Vector3 showcasePosition = loader && loader->HasPlayerStart()
		? loader->GetPlayerStartTransforms().front().position
		: Vector3::Zero;

	// dummy player
	dummyPlayer = std::make_shared<DummyPlayer>();

	titleStage->GetDefaultCameraActor()->AddComponent<TitleCamera>(showcasePosition);
	dummyPlayer->transform.SetPosition(showcasePosition);
	titleStage->GetActorManager().Register(dummyPlayer);
	currentStage = std::move(titleStage);

	// ゲーム本編と同じくVSTGのライトだけを使い、共有Skybox光を持ち越さない
	Game::Graphics::Instance().GetSkyBoxRenderer()->SetIntensity(0.0f);
}

void TitleScene::RequestExit()
{
	if (exitRequested) return;
	exitRequested = true;

	// quit
	PostMessageW(Game::Graphics::Instance().GetWindowHandle(), WM_CLOSE, 0, 0);
}

void TitleScene::OnUpdate()
{
	if (currentStage)
		if (auto* camera = currentStage->GetDefaultCameraActor()->GetComponent<TitleCamera>())
			camera->SetMotionEnabled(!showGameEditorGUI);
	titleHud->SetInputEnabled(!showGameEditorGUI && !loadRequested && !exitRequested);

	// 遷移状況監視してプレイヤー動かす
	if (prevMenuPage == TitleHUD::MenuPage::Prompt &&
		titleHud->GetMenuPage() == TitleHUD::MenuPage::Main)
	{
		dummyPlayer->GetAnimator()->SetTrigger("trigger");
	}
	prevMenuPage = titleHud->GetMenuPage();
}

void TitleScene::OnDrawGUI()
{
	titleHud->DrawResources();
}

bool TitleScene::UsesGameDebugGUI() const
{
	return !titleHud || !titleHud->IsResourcesPage();
}
