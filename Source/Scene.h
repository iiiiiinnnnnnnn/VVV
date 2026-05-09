// Scene.h

#pragma once

#include "Common.h"
#include "Camera.h"
#include "Light.h"
#include "Actor.h"
#include "Widget.h"
#include "CameraController.h"
#include "RenderContext.h"
#include "PhysicsManager.h"
#include "Graphics.h"
#include "Input.h"

// シーン基底
class Scene
{
public:
	Scene() {
		// ライト設定
		DirectionalLight directionalLight;
		directionalLight.direction = { 0, -1, -1 };
		directionalLight.color = { 1, 1, 1 };
		lightManager.SetDirectionalLight(directionalLight);
	}

	virtual ~Scene() = default;

	// 更新処理
	virtual void Update(float elapsedTime) {
		OnUpdate(elapsedTime);

		_ASSERT_EXPR(!cameraControllers.empty(), "CameraController is empty.");

		// カメラ更新処理
		cameraControllers[nowCameraControllerIndex]->Update(elapsedTime);

		// カメラコントローラーからカメラへ反映
		cameraControllers[nowCameraControllerIndex]->SyncControllerToCamera(camera);

		actors.Update(elapsedTime);
		widgets.Update(elapsedTime);

		// 物理シミュレーション更新
		GetPhysicsSceneContext().Simulate(elapsedTime);
	}

	// 描画処理
	virtual void Render(float elapsedTime) {
		ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
		RenderState* renderState = Graphics::Instance().GetRenderState();
		PrimitiveRenderer* primitiveRenderer = Graphics::Instance().GetPrimitiveRenderer();

		// 描画コンテキスト設定
		RenderContext rc;
		rc.deviceContext = dc;
		rc.renderState = renderState;
		rc.camera = &camera;
		rc.lightManager = &lightManager;
		rc.renderSettings = &renderSettings;

		// グリッド描画
#ifdef _DEBUG
		if (Input::Instance().GetGamePad().GetButtonDown() & GamePad::BTN_F3) renderSettings.showDebug = !renderSettings.showDebug;
		if (renderSettings.showDebug) {
			primitiveRenderer->DrawGrid(100, 1);
			primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		}
#endif
		OnRender(rc, elapsedTime);

		actors.Render(rc, elapsedTime);
		widgets.Render(rc, elapsedTime);
	}

	// GUI描画処理
	virtual void DrawGUI(float elapsedTime) {
		if (!renderSettings.showDebug) return;
		OnDrawGUI(elapsedTime);

		actors.DrawGUI(elapsedTime);

		ImGui::Begin("ModelViewerScene", nullptr, ImGuiWindowFlags_None);
		ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
		ImGui::SliderInt("CameraController", &nowCameraControllerIndex, 0, static_cast<int>(cameraControllers.size()) - 1);
		ImGui::End();
	}

	const PhysicsSceneContext& GetPhysicsSceneContext() const { return psc; }

protected:
	virtual void OnUpdate(float elapsedTime) {}
	virtual void OnRender(RenderContext& rc, float elapsedTime) {}
	virtual void OnDrawGUI(float elapsedTime) {}

	PhysicsSceneContext psc;

	Camera											camera;
	std::vector<std::unique_ptr<CameraController>>	cameraControllers;
	int 											nowCameraControllerIndex = 0;
	LightManager									lightManager;
	RenderSettings 									renderSettings;

	struct Actors
	{
		std::vector<std::shared_ptr<Actor>> data;
		void push_back(std::shared_ptr<Actor> actor) { data.push_back(actor); }
		void Update(float elapsedTime) {
			for (auto& a : data) {
				a->Update(elapsedTime);
			}
		}
		void Render(const RenderContext& rc, float elapsedTime) {
			for (auto& a : data) {
				a->Render(rc, elapsedTime);
			}
		}
		void DrawGUI(float elapsedTime) {
			for (auto& a : data) {
				a->DrawGUI(elapsedTime);
			}
		}
	} actors;
	struct Widgets
	{
		std::vector<std::shared_ptr<Widget>> data;
		void push_back(std::shared_ptr<Widget> widget) { data.push_back(widget); }
		void Update(float elapsedTime) {
			for (auto& a : data) {
				a->Update(elapsedTime);
			}
		}
		void Render(const RenderContext& rc, float elapsedTime) {
			for (auto& a : data) {
				a->Render(elapsedTime);
			}
		}
		void DrawGUI(float elapsedTime) {
			for (auto& a : data) {
				a->DrawGUI(elapsedTime);
			}
		}
	} widgets;
};
