// Scene.cpp

#include "Scene.h"

Scene::Scene()
{
	// ライト設定
	DirectionalLight directionalLight;
	directionalLight.direction = { 0, -1, -1 };
	directionalLight.color = { 1, 1, 1 };
	lightManager.SetDirectionalLight(directionalLight);
}

void Scene::Update(float elapsedTime)
{
	OnUpdate(elapsedTime);

	_ASSERT_EXPR(!cameraControllers.empty(), "CameraController is empty.");

	// カメラ更新処理
	cameraControllers[nowCameraControllerIndex]->Update(elapsedTime);

	// カメラコントローラーからカメラへ反映
	cameraControllers[nowCameraControllerIndex]->SyncControllerToCamera(camera);

	actors.Update(elapsedTime);
	widgets.Update(elapsedTime);

	// 物理シミュレーション
	PhysicsManager::Instance().GetSceneContext().Simulate(elapsedTime);
}

void Scene::Render(float elapsedTime)
{
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

void Scene::DrawGUI(float elapsedTime)
{
	if (!renderSettings.showDebug) return;

	if (!actors.data.empty()) {
		ImGui::Begin("Actors");
		actors.DrawGUI(elapsedTime);
		ImGui::End();
	}

	if (!widgets.data.empty()) {
		ImGui::Begin("Widgets");
		widgets.DrawGUI(elapsedTime);
		ImGui::End();
	}

	ImGui::Begin("ModelViewerScene", nullptr, ImGuiWindowFlags_None);
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

	ImGui::Separator();
	ImGui::SliderInt("CameraController", &nowCameraControllerIndex, 0, static_cast<int>(cameraControllers.size()) - 1);
	GetNowCameraController()->DrawGUI(elapsedTime);

	OnDrawGUI(elapsedTime);

	ImGui::End();
}

CameraController* Scene::GetNowCameraController() const
{
	if (cameraControllers.empty()) return nullptr;
	return cameraControllers[nowCameraControllerIndex].get();
}
