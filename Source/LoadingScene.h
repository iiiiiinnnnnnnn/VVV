// LoadingScene.h

#pragma once

#include "Scene.h"
#include "SpriteWidget.h"

class LoadingScene : public Scene
{
public:
	LoadingScene(SceneMessage message = nullptr);
	~LoadingScene() override = default;

	void OnUpdate() override;
	void OnDrawGUI() override;

private:
	Vector2 oldCursorPos{};
	Vector2 cursorMoveVec{};
	Vector2 bgVelocity{};
	bool cursorInit = false;

	std::shared_ptr<SpriteWidget> BG;
};
