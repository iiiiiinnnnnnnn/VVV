// ModelViewerScene.h

#pragma once

#include "Common.h"
#include "Scene.h"
#include "Camera.h"
#include "FreeCameraController.h"
#include "Model.h"
#include "Light.h"
#include "Actor.h"
#include "RenderContext.h"

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
	void DrawGUI(float elapsedTime) override;

private:
	Camera								camera;
	FreeCameraController				cameraController;
	LightManager						lightManager;
	RenderSettings 						renderSettings;
	struct Actors
	{
		std::vector<std::shared_ptr<Actor>> data;
		void push_back(std::shared_ptr<Actor> actor) { data.push_back(actor); }
		void Update(float elapsedTime) {
			for (auto& a : data) {
				a->Update(elapsedTime);
			}
		}
		void Render(const RenderContext& rc, float elapsedTime, ModelRenderer* renderer) {
			for (auto& a : data) {
				a->Render(rc, elapsedTime, renderer);
			}
		}
		void DrawGUI(float elapsedTime) {
			for(auto& a : data) {
				a->DrawGUI(elapsedTime);
			}
		}
	} actors;
	Model::Node*						selectionNode = nullptr;
	std::vector<Model::NodePose>		nodePoses;
};
