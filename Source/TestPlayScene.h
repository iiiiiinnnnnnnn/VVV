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
	void OnUpdate() override;

	// GUI描画処理
	void OnDrawGUI() override;

private:

};
