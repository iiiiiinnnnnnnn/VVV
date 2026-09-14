#pragma once

#include <memory>

#include "Gameplay/Scene/Scene.h"

class SpriteWidget;
class Widget;

class LoadingScene : public Scene
{
public:
	LoadingScene();
	~LoadingScene() override = default;

	void OnUpdate() override;

private:
	void UpdateLayout();

	std::shared_ptr<SpriteWidget> background;
	std::shared_ptr<SpriteWidget> vignetteOverlay;
	std::shared_ptr<Widget> progressTrack;
	std::shared_ptr<Widget> progressFill;
	float elapsedTime = 0.0f;
};
