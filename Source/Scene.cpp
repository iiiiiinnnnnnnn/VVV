// Scene.cpp

#include "Scene.h"
#include "GameTime.h"
#include "Player.h"

Scene::Scene(const std::string& name) : name(name)
{
	// ライト設定
	DirectionalLight directionalLight;
	directionalLight.direction = { 0, -1, -1 };
	directionalLight.color = { 1, 1, 1 };
	lightData.SetDirectionalLight(directionalLight);
	lightData.SetAmbientColor({ 1, 1, 1, 1 });
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

	RenderTarget* displayBuffer = graphics.GetFrameBuffer(Game::FrameBufferId::Display);
	RenderTarget* sceneBuffer = graphics.GetFrameBuffer(Game::FrameBufferId::Scene);
	RenderTarget* luminanceBuffer = graphics.GetFrameBuffer(Game::FrameBufferId::Luminance);

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
	}

	// ---- シーン描画 → sceneBuffer ----------------------------------------
	sceneBuffer->Clear(dc);
	sceneBuffer->Activate(dc);
	{
		// スカイボックス
		graphics.GetSkyBoxRenderer()->Render(
			rc.deviceContext, renderState, *rc.camera,
			graphics.GetIBLSpecularPMREM());

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

		// 剣の軌跡描画（sceneBufferアクティブ中・LINELIST後に行う）
		for (auto& actor : actors.data)
		{
			if (auto* player = dynamic_cast<Player*>(actor.get()))
				player->DrawSwordTrail(rc);
		}
	}
	sceneBuffer->Deactivate(dc);

	// ---- PostEffect -------------------------------------------------------

	// 輝度抽出: sceneBuffer → luminanceBuffer
	luminanceBuffer->Clear(dc);
	luminanceBuffer->Activate(dc);
	{
		postEffect.Begin(rc);
		postEffect.LuminanceExtraction(rc, sceneBuffer->GetSRV());
		postEffect.End(rc);
	}
	luminanceBuffer->Deactivate(dc);

	// Bloom合成: (sceneBuffer + luminanceBuffer) → sceneBuffer に戻す
	sceneBuffer->Clear(dc);
	sceneBuffer->Activate(dc);
	{
		postEffect.Begin(rc);
		postEffect.Bloom(rc, sceneBuffer->GetSRV(), luminanceBuffer->GetSRV());
		postEffect.End(rc);
	}
	sceneBuffer->Deactivate(dc);

	// トーンマッピング: sceneBuffer → displayBuffer
	displayBuffer->Activate(dc);
	{
		postEffect.Begin(rc);
		postEffect.ToneMapping(rc, sceneBuffer->GetSRV());
		postEffect.End(rc);
	}

	// ---- スプライト・GUIはdisplayBufferのまま描画 ------------------------
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

		if (ImGui::CollapsingHeader("Skybox"))
		{
			Game::Graphics::Instance().GetSkyBoxRenderer()->DrawGUI();
		}

		// PostEffect
		if (ImGui::CollapsingHeader("PostEffect"))
		{
			postEffect.DrawGUI();
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