#pragma once

#include <memory>
#include "Scene.h"
#include "Camera.h"
#include "FreeCameraController.h"
#include "Model.h"
#include "Light.h"
#include "Player.h"
#include "Stage.h"

// モデルビューアシーン
class ModelViewerScene : public Scene
{
public:
	ModelViewerScene();
	~ModelViewerScene() override = default;

	// 更新処理
	void Update(float elapsedTime) override;

	// 描画処理
	void Render(float elapsedTime) override;

	// GUI描画処理
	void DrawGUI() override;

private:
	// ヒエラルキーGUI描画
	void DrawHierarchyGUI();

	// プロパティGUI描画
	void DrawPropertyGUI();

	// アニメーションGUI描画
	void DrawAnimationGUI();

	// マテリアルGUI描画
	void DrawMaterialGUI();

private:
	Camera								camera;
	FreeCameraController				cameraController;
	LightManager						lightManager;
	RenderSettings 						renderSettings;
	std::shared_ptr<Player>				player;
	std::shared_ptr<Stage>				stage;
	std::vector<Model*> 				debug_models;
	Model::Node*						selectionNode = nullptr;
	std::vector<Model::NodePose>		nodePoses;
};
