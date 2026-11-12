#include "UI/PauseMenuWidget.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <InputKit/InputKit.h>
#include "Application/Time/GameTime.h"
#include "UI/ColorWidget.h"
#include "Rendering/Core/Graphics.h"
#include "UI/SpriteButton.h"
#include "UI/LabelButton.h"
#include "UI/EventTrigger.h"
#include "UI/UILayout.h"
#include "UI/XboxControllerWidget.h"

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
	for (const char* label : {"Resume", "Control", "Return to Title"})
	{
		auto button = std::make_shared<LabelButton>(std::string("Button / ") + label, label, 42.0f);
		button->SetAlignment(UITextAlignment::Center);
		button->AddComponent<SpriteButton>("Resources/UI/btn_back.png", "Resources/UI/btn_front.png");
		auto* trigger = button->AddComponent<EventTrigger>();
		trigger->SetNavigation(UINavigation::Vertical);
		trigger->SetWrapNavigation(true);
		mainPage->AddChild(button);
		mainButtons.push_back(button);
	}
	mainButtons[0]->GetComponent<EventTrigger>()->AddActivationListener(
		[this] { SetPaused(false); });
	mainButtons[1]->GetComponent<EventTrigger>()->AddActivationListener(
		[this] { SetPage(Page::Control); });
	mainButtons[2]->GetComponent<EventTrigger>()->AddActivationListener([this] {
		if (this->returnToTitle) this->returnToTitle();
	});

	// 操作方法ページ
	controlPage = std::make_shared<WidgetGroup>("ControlPage");
	controlPage->AddChild(std::make_shared<XboxControllerWidget>(
		"XboxControllerWidget", std::weak_ptr<Object>{}, Vector2{50.0f, 73.0f}, Vector2{1, 1}
	));
	auto backBtn = std::make_shared<LabelButton>("Button / Baack", "Back", 42.0f);
	backBtn->SetAlignment(UITextAlignment::Center);
	backBtn->AddComponent<SpriteButton>("Resources/UI/btn_back.png", "Resources/UI/btn_front.png");
	backBtn->rect.anchor = {0.5f, 0.5f};
	UILayout::Apply(backBtn, {0.5f, 0.9f}, {0.3f, 0.08f});
	controlPage->AddChild(backBtn);
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
	mainPage->SetActive(paused && page == Page::Main);
	controlPage->SetActive(paused && page == Page::Control);
	inputDelay.Reset(paused ? 0.12f : 0.0f);
	if (paused && page == Page::Main)
	{
		for (const auto& button : mainButtons)
			button->GetComponent<EventTrigger>()->SetInputDelay(0.12f);
		mainButtons.front()->GetComponent<EventTrigger>()->Select();
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

	UILayout::MenuButton(mainButtons[0], {0.5f, 0.33f}, 38, offset);
	UILayout::MenuButton(mainButtons[1], {0.5f, 0.5f}, 38, offset);
	UILayout::MenuButton(mainButtons[2], {0.5f, 0.67f}, 38, offset);
}
