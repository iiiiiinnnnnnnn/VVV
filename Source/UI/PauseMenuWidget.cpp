#include "UI/PauseMenuWidget.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <InputKit/InputKit.h>
#include "Application/Time/GameTime.h"
#include "UI/ColorWidget.h"
#include "Rendering/Core/Graphics.h"
#include "UI/EventTrigger.h"
#include "UI/UILayout.h"
#include "UI/XboxControllerWidget.h"
#include "UI/MenuButtonWidget.h"
#include "UI/WidgetShadow.h"

PauseMenuWidget::PauseMenuWidget(const std::string& name, PauseChangedCallback pauseChanged, ReturnToTitleCallback returnToTitle)
	: WidgetGroup(name), pauseChanged(std::move(pauseChanged)),
	  returnToTitle(std::move(returnToTitle))
{
}

void PauseMenuWidget::OnAwake()
{
	WidgetGroup::OnAwake();

	// 背景
	dimmer = std::make_shared<ColorWidget>(
		"Background Dimmer", SpriteShaderId::Basic, Color(0.01f, 0.02f, 0.04f, 0.54f));
	dimmer->SetAffectedByPostProcess(false);
	AddChild(dimmer);

	// メインページ
	mainPage = std::make_shared<WidgetGroup>("MainPage");
	AddChild(mainPage);

	// ゲーム用ボタン
	struct ButtonDefinition
	{
		const char* label;
		std::function<void()> onActivated;
	};
	const ButtonDefinition menuItems[] = {
		{"RESUME", [this] {
			 SetPaused(false);
			 MenuButtonWidget::PlayClickSound();
		 }},
		{"CONTROL", [this] {
			 SetPage(Page::Control);
			 MenuButtonWidget::PlayClickSound();
		 }},
		{"RETURN TO TITLE", [this] {
			 if (returnToTitle) returnToTitle();
			 MenuButtonWidget::PlayClickSound();
		 }}};
	for (const auto& item : menuItems)
	{
		auto button = std::make_shared<MenuButtonWidget>(
			item.label, item.label, 42.0f, "", item.onActivated);
		button->AddComponent<WidgetShadow>();
		
		mainPage->AddChild(button);
		mainButtons.push_back(button);
	}

	// 操作方法ページ
	controlPage = std::make_shared<WidgetGroup>("ControlPage");
	controlPage->AddChild(std::make_shared<XboxControllerWidget>(
		"XboxControllerWidget", std::weak_ptr<Object>{}, Vector2{50.0f, 73.0f}, Vector2{1, 1}
	));
	backButton = std::make_shared<MenuButtonWidget>("Button / Back", "BACK", 42.0f, "",
		[this] {
			SetPage(Page::Main);
			MenuButtonWidget::PlayClickSound();
	});
	backButton->AddComponent<WidgetShadow>();
	controlPage->AddChild(backButton);
	AddChild(controlPage);

	// 一時停止メニュー全部非アクティブ化
	SetChildrenActive(false);
}

void PauseMenuWidget::SetPaused(bool value)
{
	if (paused == value || (value && !canPause)) return;

	paused = value;
	transition = 0.0f;
	dimmer->SetActive(paused);
	SetPage(Page::Main);
	if (paused) UpdateLayout();
	if (pauseChanged) pauseChanged(paused);
}

void PauseMenuWidget::SetPage(Page page)
{
	currentPage = page;
	const bool mainActive = paused && page == Page::Main;
	const bool controlActive = paused && page == Page::Control;
	mainPage->SetActive(mainActive);
	controlPage->SetActive(controlActive);
	inputDelay.Reset(paused ? 0.12f : 0.0f);
	for (const auto& button : mainButtons)
	{
		auto* trigger = button->GetComponent<EventTrigger>();
		trigger->SetInteractable(mainActive);
		if (mainActive) trigger->SetInputDelay(0.12f);
	}
	auto* backTrigger = backButton->GetComponent<EventTrigger>();
	backTrigger->SetInteractable(controlActive);
	if (mainActive) mainButtons.front()->GetComponent<EventTrigger>()->Select();
	else if (controlActive)
	{
		backTrigger->SetInputDelay(0.12f);
		backTrigger->Select();
	}
}

void PauseMenuWidget::OnUpdate()
{
	if (!canPause && paused) SetPaused(false);

	const auto& input = InputKit::InputSystem::Instance();
	if (input.IsDown("PlayerInput/Pause"))
	{
		if (!paused) SetPaused(true);
		else if (!inputDelay.IsRunning()) SetPaused(false);
	}
	if (paused)
	{
		inputDelay.Update(Game::Time::unscaledDeltaTime);
		if (!inputDelay.IsRunning() && input.IsDown("UIInput/Cancel"))
		{
			if (currentPage == Page::Control) SetPage(Page::Main);
			else SetPaused(false);
		}
	}
	if (paused)
	{
		transition = std::min(transition + Game::Time::unscaledDeltaTime * 6.5f, 1.0f);
		UpdateLayout();
	}
	WidgetGroup::OnUpdate();
}

void PauseMenuWidget::UpdateLayout()
{
	const float width = Game::Graphics::ScreenWidth;
	if (width <= 0.0f || Game::Graphics::ScreenHeight <= 0.0f) return;
	const Vector2 offset(std::pow(1.0f - transition, 3.0f) * width * 0.05f, 0);

	UILayout::Apply(dimmer, {0, 0}, {1, 1});
	dimmer->SetColor(Color(0.01f, 0.02f, 0.04f, 0.54f * transition));

	// タイトルと同じ大きさ・行間で、ボタン列の中心を画面中央に揃える
	const float centerY = Game::Graphics::ScreenHeight * 0.5f;
	const float halfRows = static_cast<float>(mainButtons.size() - 1) * 0.5f;
	for (size_t i = 0; i < mainButtons.size(); ++i)
	{
		const auto& button = mainButtons[i];
		if (!button->GetAutoLayout()) continue;
		button->rect.position = Vector2(width * 0.5f,
			centerY + (static_cast<float>(i) - halfRows) * 44.0f * 0.8f) + offset;
		button->rect.angle = 0.0f;
		button->rect.size = {593.0f, 44.0f};
		button->rect.scale = {0.8f, 0.8f};
		button->rect.anchor = {0.5f, 0.5f};
		button->SetFontSize(38.0f);
		button->SetAlignment(UITextAlignment::Left);
	}
	if (backButton->GetAutoLayout())
	{
		backButton->rect.position = Vector2(width * 0.5f, Game::Graphics::ScreenHeight * 0.9f) + offset;
		backButton->rect.angle = 0.0f;
		backButton->rect.size = {593.0f, 44.0f};
		backButton->rect.scale = {0.8f, 0.8f};
		backButton->rect.anchor = {0.5f, 0.5f};
		backButton->SetFontSize(38.0f);
		backButton->SetAlignment(UITextAlignment::Left);
	}
}
