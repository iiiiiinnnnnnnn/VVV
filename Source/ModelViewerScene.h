// ModelViewerScene.h

#pragma once

#include "Scene.h"

// モデルビューアシーン
class ModelViewerScene : public Scene
{
public:
	ModelViewerScene();
	~ModelViewerScene() override = default;

	// 更新処理
	void OnUpdate(float elapsedTime) override;

	// 描画処理
	void OnRender(RenderContext& rc, float elapsedTime) override;

	// GUI描画処理
	void OnDrawGUI(float elapsedTime) override;

private:

};
