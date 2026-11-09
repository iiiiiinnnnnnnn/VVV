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
#include "UI/SpriteWidget.h"
#include "Gameplay/Stage/StageTitle.h"

TitleScene::TitleScene() = default;

TitleScene::~TitleScene()
{
	Uninit();
}

void TitleScene::OnInit()
{
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(true);
	graphics.SetWindowMovementLocked(true);
	showGameEditorGUI = false;
	CreateTitleWorld();
	titleHud = std::make_shared<TitleMenuWidget>([this](int index) { OnMenuSelected(index); });
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
	auto stageTitle = std::make_unique<StageTitle>();
	stageTitle->Awake();

	// dummy player
	dummyPlayer = std::make_shared<DummyPlayer>();

	stageTitle->GetDefaultCameraActor()->AddComponent<TitleCamera>(
		stageTitle->GetShowcasePosition()
	);
	dummyPlayer->transform.SetPosition(stageTitle->GetShowcasePosition());
	stageTitle->GetActorManager().Register(dummyPlayer);
	currentStage = std::move(stageTitle);
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
	if (prevMenuPage == TitleMenuWidget::MenuPage::Prompt &&
		titleHud->GetMenuPage() == TitleMenuWidget::MenuPage::Main)
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
