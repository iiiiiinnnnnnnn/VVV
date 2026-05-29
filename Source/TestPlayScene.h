// TestPlayScene.h

#pragma once

#include "Scene.h"
#include "RenderTarget.h"

class TestPlayScene : public Scene
{
public:
	TestPlayScene();
	~TestPlayScene() override = default;

	// 更新処理
	void OnUpdate(float elapsedTime) override;

	// GUI描画処理
	void OnDrawGUI(float elapsedTime) override;

private:

};
