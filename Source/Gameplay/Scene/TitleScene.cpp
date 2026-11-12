#include "Gameplay/Scene/TitleScene.h"

#include <windows.h>
#include "Core/Foundation/Common.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/TestPlayScene.h"
#include "Gameplay/Scene/VmdlEditorScene.h"
#include "Gameplay/Scene/VstgEditorScene.h"
#include "Gameplay/Stage/Stage.h"
#include "Rendering/Core/Graphics.h"
#include "UI/WidgetGroup.h"
#include "UI/TitleLogoWidget.h"
#include "UI/MenuButtonWidget.h"
#include "UI/EventTrigger.h"
#include "UI/WidgetShadow.h"
#include <ImGuiToolKit/Widgets.h>
#include "Resource/ResourceManager.h"
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
	CreateTitleUI();
}

void TitleScene::CreateTitleUI()
{
	menuButtons.clear();
	titleHud = std::make_shared<WidgetGroup>(
		"HUD / Title Menu");

	mainHud = std::make_shared<WidgetGroup>("MainHUD");
	titleHud->AddChild(mainHud);

	titleLogo = titleHud->AddChild(std::make_shared<TitleLogoWidget>(
		"TitleLogoWidget", "Resources/UI/titleLogo.png"));

	// ゲーム用ボタン
	struct ButtonDefinition
	{
		const char* label;
		MenuAction action;
	};
	const ButtonDefinition menuItems[] = {
		{"PLAY",		MenuAction::Play},
		{"CONTROL",		MenuAction::Control},
		{"VMDL EDITOR", MenuAction::VmdlEditor},
		{"VSTG EDITOR", MenuAction::VstgEditor},
		{"RESOURCES",	MenuAction::Resources},
		{"EXIT",		MenuAction::Exit},
	};
	for (const auto& item : menuItems)
	{
		auto button = std::make_shared<MenuButtonWidget>(item.label, item.label, 42.0f, "",
			[this, action = item.action] { OnMenuSelected(action); });
		button->AddComponent<WidgetShadow>();
		mainHud->AddChild(button);
		menuButtons.push_back(button);
	}

	SetMenuPage(MenuPage::Main);

	// 通常ボタンは項目数に合わせて上から順に並べる。
	for (size_t i = 0; i < menuButtons.size(); ++i)
	{
		const auto& button = menuButtons[i];
		if (!button->GetAutoLayout()) continue;
		const float y = 760.0f + static_cast<float>(i) * 44.0f * 0.8f;
		button->rect.position = Vector2(1580.0f, y);
		button->rect.angle = 0.0f;
		button->rect.size = {593.0f, 44.0f};
		button->rect.scale = {0.8f, 0.8f};
		button->rect.anchor = {0.5f, 0.5f};
		button->SetFontSize(38.0f);
		button->SetAlignment(UITextAlignment::Left);
	}
	widgetManager.Register(titleHud);
}

void TitleScene::SetButtonsInteractable(bool interactable)
{
	for (const auto& button : menuButtons)
		button->GetComponent<EventTrigger>()->SetInteractable(interactable);
}

void TitleScene::SetMenuPage(MenuPage page)
{
	menuPage = page;
	titleLogo->SetActive(page != MenuPage::Resources);
	mainHud->SetActive(page == MenuPage::Main);
	SetButtonsInteractable(!showGameEditorGUI && !loadRequested && !exitRequested && page == MenuPage::Main);
	if (page == MenuPage::Main && !menuButtons.empty()) menuButtons.front()->GetComponent<EventTrigger>()->Select();
}

void TitleScene::DrawResources()
{
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	if (menuPage != MenuPage::Resources) return;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	// リソース設定ウィンドウ：作業領域からの相対位置とサイズ比率。
	ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
	constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
	if (ImGui::Begin("Resources", nullptr, flags))
	{
		if (ImGuiToolKit::Button(u8"戻る")) SetMenuPage(MenuPage::Main);
		else ResourceManager::Instance().DrawGUI();
	}
	ImGui::End();
#endif
}

void TitleScene::OnMenuSelected(MenuAction action)
{
	if (showGameEditorGUI || loadRequested || exitRequested || menuPage != MenuPage::Main) return;

	switch (action)
	{
		case MenuAction::Play:
		{
			loadRequested = SceneManager::Instance().LoadScene<TestPlayScene>();
			break;
		}
		case MenuAction::Control:
		{
			break;
		}
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
		case MenuAction::Resources:
		{
			ResourceManager::Instance().ReloadResourceList();
			SetMenuPage(MenuPage::Resources);
			break;
		}
		case MenuAction::VmdlEditor:
		{
			loadRequested = SceneManager::Instance().LoadScene<VmdlEditorScene>();
			break;
		}
		case MenuAction::VstgEditor:
		{
			loadRequested = SceneManager::Instance().LoadScene<VstgEditorScene>();
			break;
		}
#endif
		case MenuAction::Exit:
		{
			RequestExit();
			break;
		}
	}
}

void TitleScene::CreateTitleWorld()
{
	auto stageTitle = std::make_unique<StageTitle>();
	stageTitle->Awake();

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
	SetButtonsInteractable(!showGameEditorGUI && !loadRequested && !exitRequested && menuPage == MenuPage::Main);
}

void TitleScene::OnDrawGUI()
{
	DrawResources();
}

bool TitleScene::UsesGameDebugGUI() const
{
	return menuPage != MenuPage::Resources;
}
