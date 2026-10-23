#include "UI/PauseMenu.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "Application/Input/Input.h"
#include "Application/Time/GameTime.h"
#include "Rendering/Component/SpriteRenderComponent.h"
#include "Rendering/Core/Graphics.h"
#include "UI/Button.h"
#include "UI/EventTrigger.h"
#include "UI/UILayout.h"

PauseMenu::PauseMenu(PauseChangedCallback pauseChanged, ReturnToTitleCallback returnToTitle)
	: WidgetGroup("HUD / Pause Menu"), pauseChanged(std::move(pauseChanged)),
	  returnToTitle(std::move(returnToTitle))
{
	dimmer = std::make_shared<Widget>("Background Dimmer");
	dimmer->AddComponent<SpriteRenderComponent>(
		std::make_shared<Texture>(Color(1, 1, 1, 1)), SpriteShaderId::Basic,
		Color(0.01f, 0.02f, 0.04f, 0.54f));
	dimmer->SetAffectedByPostProcess(false);
	AddChild(dimmer);

	for (const char* label : {"Resume", "Return to Title"})
	{
		auto button = std::make_shared<Button>(std::string("Button / ") + label, label);
		button->SetMenuStyle();
		auto* trigger = button->AddComponent<EventTrigger>();
		trigger->SetNavigation(UINavigation::Vertical);
		trigger->SetWrapNavigation(true);
		AddChild(button);
		mainButtons.push_back(button);
	}
	mainButtons[0]->GetComponent<EventTrigger>()->AddActivationListener([this] { SetPaused(false); });
	mainButtons[1]->GetComponent<EventTrigger>()->AddActivationListener([this]
	{
		if (this->returnToTitle) this->returnToTitle();
	});
	SetChildrenActive(false);
}

void PauseMenu::SetPaused(bool value)
{
	if (paused == value || (value && !canPause)) return;

	paused = value;
	transition = 0.0f;
	inputDelay.Reset(paused ? 0.12f : 0.0f);
	SetChildrenActive(paused);
	if (paused)
	{
		for (const auto& button : mainButtons)
			button->GetComponent<EventTrigger>()->SetInputDelay(0.12f);
		mainButtons.front()->GetComponent<EventTrigger>()->Select();
		UpdateLayout();
	}
	if (pauseChanged) pauseChanged(paused);
}

void PauseMenu::OnUpdate()
{
	if (!canPause && paused) SetPaused(false);

	const GamePadButton buttonDown = Game::Input::Instance().GetGamePad().GetButtonDown();
	if (buttonDown & GamePad::BTN_ESCAPE)
	{
		if (!paused) SetPaused(true);
		else if (!inputDelay.IsRunning()) SetPaused(false);
	}
	if (paused)
	{
		inputDelay.Update(Game::Time::unscaledDeltaTime);
		if (!inputDelay.IsRunning() && (buttonDown & GamePad::BTN_B)) SetPaused(false);
	}
	if (paused)
	{
		transition = std::min(transition + Game::Time::unscaledDeltaTime * 6.5f, 1.0f);
		UpdateLayout();
	}
	WidgetGroup::OnUpdate();
}

void PauseMenu::UpdateLayout()
{
	const float width = Game::Graphics::ScreenWidth;
	if (width <= 0.0f || Game::Graphics::ScreenHeight <= 0.0f) return;
	const Vector2 offset(std::pow(1.0f - transition, 3.0f) * width * 0.05f, 0);

	UILayout::Apply(dimmer, {0, 0}, {1, 1});
	if (auto* sprite = dimmer->GetComponent<SpriteRenderComponent>())
		sprite->SetColor(Color(0.01f, 0.02f, 0.04f, 0.54f * transition));

	UILayout::MenuButton(mainButtons[0], {0.5f, 0.45f}, 38, offset);
	UILayout::MenuButton(mainButtons[1], {0.5f, 0.61f}, 32, offset);
}
