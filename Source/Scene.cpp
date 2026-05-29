// Scene.cpp

#include "Scene.h"

Scene::Scene(const std::string& name) : name(name)
{
	// ライト設定
	DirectionalLight directionalLight;
	directionalLight.direction = {0, -1, -1};
	directionalLight.color = {1, 1, 1};
	lightData.SetDirectionalLight(directionalLight);
	lightData.SetAmbientColor({1, 1, 1, 1});
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
	Graphics& graphics = Graphics::Instance();
	ID3D11DeviceContext* dc = graphics.GetDeviceContext();
	RenderState* renderState = graphics.GetRenderState();
	PrimitiveRenderer* primitiveRenderer = graphics.GetPrimitiveRenderer();

	// 描画コンテキスト設定
	RenderContext rc;
	{
		rc.deviceContext = dc;
		rc.renderState = renderState;
		rc.camera = &camera;
		rc.lightData = lightData;
		rc.renderSettings = renderSettings;
		rc.shadowMapData = shadowMapData;
		rc.iblData = iblData;
	}

	// デバッグ切り替え
	{
		#ifdef _DEBUG
		if (Input::Instance().GetGamePad().GetButtonDown() & GamePad::BTN_F3)
			renderSettings.showDebug = !renderSettings.showDebug;
		#endif
	}

	// 描画
	{
		// グリッド
		/*if (renderSettings.showDebug)
		{
			primitiveRenderer->DrawGrid(100, 1);
			primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		}*/

		// IBLデータをRenderContextに詰める
		iblData.ggxLookUpTableMap = graphics.GetIBLGGXLUT();
		iblData.specularPremappingRadianceEnvironmentMap = graphics.GetIBLSpecularPMREM();
		iblData.diffuseIrradianceEnvironmentMap = graphics.GetIBLDiffuseIEM();

		// 先にワールド行列確定させるためにDrawしとく
		actors.Render(rc, elapsedTime);

		// シャドウマップ描画
		{
			for (auto& actor : actors.data)
			{
				auto* mrc = actor->GetComponent<ModelRenderComponent>();
				if (mrc) graphics.GetShadowMapRenderer()->Draw(mrc->GetModel());
			}
			graphics.GetShadowMapRenderer()->Render(
				rc,
				lightData.GetDirectionalLight().direction,
				Vector3(0, 0, 0), 50.0f, 300.0f, 0.1f, 200.0f
			);
			shadowMapData.shadowMap = graphics.GetShadowMapRenderer()->GetDepthSRV();
			shadowMapData.lightViewProjection = graphics.GetShadowMapRenderer()->GetLightViewProjection();
			graphics.SetRenderTargets();
		}

		// スカイボックス
		graphics.GetSkyBoxRenderer()->Render(
			rc.deviceContext, renderState, *rc.camera,
			graphics.GetIBLSpecularPMREM(), 1.0f
		);

		// 通常描画
		graphics.GetModelRenderer()->Render(rc, elapsedTime);

		// スプライト
		widgets.Render(rc, elapsedTime);
		graphics.GetSpriteRenderer()->Render(rc, elapsedTime);
	}

	// GUI
	{
		#ifdef _DEBUG
		if (!renderSettings.showDebug)
		{
			if (!actors.data.empty())
			{
				ImGui::Begin("Actors");
				actors.DrawGUI(elapsedTime);
				ImGui::End();
			}

			if (!widgets.data.empty())
			{
				ImGui::Begin("Widgets");
				widgets.DrawGUI(elapsedTime);
				ImGui::End();
			}

			ImGui::Begin(name.empty() ? "Unnamed Scene" : name.c_str(), nullptr, ImGuiWindowFlags_None);

			// パフォーマンス
			if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
			}

			// カメラ
			if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::SliderInt("CameraController", &nowCameraControllerIndex, 0, static_cast<int>(cameraControllers.size()) - 1);
				auto nowCameraController = GetNowCameraController();
				if (nowCameraController)
					nowCameraController->DrawGUI(elapsedTime);
			}

			// RenderContext
			if (ImGui::CollapsingHeader("RenderContext", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::TreeNode("LightData"))
				{
					rc.lightData.DrawGUI();
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("RenderSettings"))
				{
					ImGui::Checkbox("Show Debug", &renderSettings.showDebug);
					ImGui::Checkbox("Wireframe", &renderSettings.wireframe);
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("ShadowMapData"))
				{
					ImGui::Image(shadowMapData.shadowMap, ImVec2(256, 256), ImVec2(0, 0), ImVec2(1, 1));
					ImGui::ColorEdit4("Shadow Color", &shadowMapData.shadowColor.x);
					ImGui::DragFloat("Shadow Bias", &shadowMapData.shadowBias, 0.001f, 0.0f, 1.0f);
					ImGui::DragInt("PCF Kernel Size", &shadowMapData.pcfKernelSize, 1, 1, 15);
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("IBLData"))
				{
					ImGui::Image(iblData.diffuseIrradianceEnvironmentMap, ImVec2(128, 128), ImVec2(0, 0), ImVec2(1, 1));
					ImGui::Image(iblData.specularPremappingRadianceEnvironmentMap, ImVec2(128, 128), ImVec2(0, 0), ImVec2(1, 1));
					ImGui::Image(iblData.ggxLookUpTableMap, ImVec2(128, 128), ImVec2(0, 0), ImVec2(1, 1));
					ImGui::TreePop();
				}
			}

			OnDrawGUI(elapsedTime);

			ImGui::End();
		}
		#endif
	}
}
CameraController* Scene::GetNowCameraController() const
{
	if (cameraControllers.empty()) return nullptr;
	return cameraControllers[nowCameraControllerIndex].get();
}
