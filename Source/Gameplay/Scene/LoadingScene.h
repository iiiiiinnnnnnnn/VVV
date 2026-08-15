#pragma once

#include "Gameplay/Scene/Scene.h"

class LoadingScene : public Scene
{
public:
	LoadingScene();
	~LoadingScene() override = default;

	void OnDrawGUI() override;
};
