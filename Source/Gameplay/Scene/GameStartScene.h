#pragma once

#include "Gameplay/Scene/Scene.h"

class GameStartScene : public Scene
{
public:
	GameStartScene();
	~GameStartScene() override = default;

	void OnUpdate() override;
	void OnDrawGUI() override;

private:
	void ConfigureWindow();

	bool loadRequested = false;
	bool windowConfigured = false;
};
