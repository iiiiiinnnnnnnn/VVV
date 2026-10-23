#pragma once

#include "Application/Time/Timer.h"

#include <memory>

#include "Gameplay/Scene/Scene.h"

class SpriteWidget;
class Widget;
class TextWidget;

class LoadingScene : public Scene
{
  public:
	LoadingScene();
	~LoadingScene() override;

	void OnUpdate() override;
	void OnDrawGUI() override;
	MouseCursorMode GetMouseCursorMode() const override;

  protected:
	void OnInit() override;
	void OnUninit() override;

  private:
	void UpdateLayout();
	std::shared_ptr<TextWidget> resourceFileIsLoading = nullptr;
	std::shared_ptr<SpriteWidget> background = nullptr;
	std::shared_ptr<SpriteWidget> vignetteOverlay = nullptr;
	std::shared_ptr<Widget> progressTrack = nullptr;
	std::shared_ptr<Widget> progressFill = nullptr;
	Game::Timer elapsedTime{0.0f};
};
