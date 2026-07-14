// LoadingScene.h

#pragma once
#include <memory>

#include "Gameplay/Scene/Scene.h"
#include "UI/SpriteWidget.h"

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

	float progressBarWidth = 380.0f;

	std::shared_ptr<SpriteWidget> BG_Illust;
	std::shared_ptr<SpriteWidget> progressBarFill;
};
