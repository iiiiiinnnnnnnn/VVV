#pragma once

#include "Application/Time/Timer.h"
#include "UI/WidgetGroup.h"

#include <functional>
#include <memory>
#include <vector>

class LabelButton;
class SpriteWidget;

class PauseMenuWidget : public WidgetGroup
{
public:
	void OnAwake() override;
	using PauseChangedCallback = std::function<void(bool)>;
	using ReturnToTitleCallback = std::function<void()>;

	PauseMenuWidget(const std::string& name, PauseChangedCallback pauseChanged, ReturnToTitleCallback returnToTitle);

	void SetPaused(bool value);
	bool IsPaused() const { return paused; }
	void SetCanPause(bool value) { canPause = value; }

protected:
	void OnUpdate() override;

private:
	void UpdateLayout();

	enum class Page
	{
		Main, Control
	};

	void SetPage(Page page);

	PauseChangedCallback pauseChanged;
	ReturnToTitleCallback returnToTitle;
	std::shared_ptr<SpriteWidget> dimmer;
	std::vector<std::shared_ptr<LabelButton>> mainButtons;
	float transition = 0.0f;
	Game::Timer inputDelay{0.0f};
	bool paused = false;
	bool canPause = true;

	std::shared_ptr<WidgetGroup> mainPage;
	std::shared_ptr<WidgetGroup> controlPage;

	Page currentPage{Page::Main};
};
