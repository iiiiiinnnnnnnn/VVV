// LoadingScene.h

#pragma once

#include "Gameplay/Scene/Scene.h"

class LoadingScene : public Scene
{
public:
	LoadingScene(SceneMessage message = nullptr);
	~LoadingScene() override = default;

	void OnDrawGUI() override;
};
