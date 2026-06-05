// Scene.cpp

#include "Scene.h"
#include "GameTime.h"

Scene::Scene(const std::string& name) : name(name)
{
	// ライト設定
	DirectionalLight directionalLight;
	directionalLight.direction = {0, -1, -1};
	directionalLight.color = {1, 1, 1};
	lightData.SetDirectionalLight(directionalLight);
	lightData.SetAmbientColor({1, 1, 1, 1});
}

void Scene::Update()
{
	OnUpdate();

	_ASSERT_EXPR(!cameraControllers.empty(), "CameraController is empty.");

	// カメラ更新処理
	cameraControllers[nowCameraControllerIndex]->Update();

	// カメラコントローラーからカメラへ反映
	cameraControllers[nowCameraControllerIndex]->SyncControllerToCamera(camera);

	actors.Update();
	widgets.Update();
}

void Scene::Render()
{
	Game::Graphics& graphics = Game::Graphics::Instance();
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
		if (Game::Input::Instance().GetGamePad().GetButtonDown() & GamePad::BTN_F3)
			renderSettings.showDebug = !renderSettings.showDebug;
		#endif
	}

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
	actors.Render(rc);

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
			Vector3(0, 0, 0), 20.0f, 100.0f, 0.01f, 200.0f
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
	graphics.GetModelRenderer()->Render(rc);

	// ShapeRenderer描画
	graphics.GetShapeRenderer()->Render(
		dc,
		camera.GetView(),
		camera.GetProjection()
	);

	// PrimitiveRenderer描画
	primitiveRenderer->Render(
		dc,
		camera.GetView(),
		camera.GetProjection(),
		D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// スプライト
	widgets.Render(rc);
	graphics.GetSpriteRenderer()->Render(rc);

	// GUI
	DrawGUI(rc);
}

void Scene::DrawGUI(RenderContext& rc)
{
	#ifdef _DEBUG
	if (renderSettings.showDebug)
	{
		if (!actors.data.empty())
		{
			ImGui::Begin("Actors");
			actors.DrawGUI();
			ImGui::End();
		}

		if (!widgets.data.empty())
		{
			ImGui::Begin("Widgets");
			widgets.DrawGUI();
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
				nowCameraController->DrawGUI();
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

		// Time
		if (ImGui::CollapsingHeader("Time"))
		{
			ImGui::Text("Time: %.4f", Game::Time::time);
			ImGui::Text("Unscaled Delta Time: %.4f", Game::Time::unscaledDeltaTime);
			ImGui::Text("Delta Time: %.4f", Game::Time::deltaTime);
			ImGui::DragFloat("Time Scale", &Game::Time::scale, 0.01f, 0.0f, 10.0f);
		}

		OnDrawGUI();

		ImGui::End();
	}
	#endif
}

CameraController* Scene::GetNowCameraController() const
{
	if (cameraControllers.empty()) return nullptr;
	return cameraControllers[nowCameraControllerIndex].get();
}