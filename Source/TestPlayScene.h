// TestPlayScene.h

#pragma once

#include "Scene.h"

class TestPlayScene : public Scene
{
public:
	TestPlayScene(SceneMessage message = nullptr);

	~TestPlayScene() override = default;

	void OnUpdate() override;
	void OnDrawGUI() override;

private:

};