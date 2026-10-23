#pragma once

#include "Application/Time/Timer.h"
#include "UI/WidgetGroup.h"

#include <functional>
#include <memory>
#include <vector>

class Button;

class PauseMenu : public WidgetGroup
{
public:
	using PauseChangedCallback = std::function<void(bool)>;
	using ReturnToTitleCallback = std::function<void()>;

	PauseMenu(PauseChangedCallback pauseChanged, ReturnToTitleCallback returnToTitle);

	void SetPaused(bool value);
	bool IsPaused() const { return paused; }
	void SetCanPause(bool value) { canPause = value; }

protected:
	void OnUpdate() override;

private:
	void UpdateLayout();

	PauseChangedCallback pauseChanged;
	ReturnToTitleCallback returnToTitle;
	std::shared_ptr<Widget> dimmer;
	std::vector<std::shared_ptr<Button>> mainButtons;
	float transition = 0.0f;
	Game::Timer inputDelay{0.0f};
	bool paused = false;
	bool canPause = true;
};
