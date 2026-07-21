// GameStartScene.h

#pragma once

#include "Gameplay/Scene/Scene.h"

class GameStartScene : public Scene
{
public:
	GameStartScene(SceneMessage message = nullptr);
	~GameStartScene() override = default;

	void OnUpdate() override;
	void OnDrawGUI() override;

private:
	bool resourcesReady = false;
	bool loadRequested = false;
};
