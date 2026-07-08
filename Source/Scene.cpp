// Scene.cpp

#include "Scene.h"
#include "GameTime.h"
#include "Light.h"
#include "ModelRenderComponent.h"
#include "TrailRenderComponent.h"
#include "Terrain.h"
#include "PostProcessController.h"

Scene::Scene(SceneMessage message) : message(message)
{
	// ライト設定
	DirectionalLight dl{"Sun", "Directional Light", true, {1, 1, 1, 1}};
	dl.transform.rotation = {1, 0, 1, 1};
	lightManager.SetDirectionalLight(dl);

	lightManager.SetAmbientColor(ColorFromRGBA(0xE1FFD6FF));
}

void Scene::SwitchToDebugMode()
{
	if (cameraControllers.size() > 1)
	{
		nowCameraControllerIndex = 1;
		cameraControllers[nowCameraControllerIndex]->SyncCameraToController(camera);
	}

	isCursorReleased = false;
	Game::Time::scale = 0.0f;
}

void Scene::SwitchToPlayMode()
{
	if (cameraControllers.size() > 0)
	{
		nowCameraControllerIndex = 0;
		cameraControllers[nowCameraControllerIndex]->SyncCameraToController(camera);
	}

	isCursorReleased = false;
	Game::Time::scale = 1.0f;
}

void Scene::Update()
{
	OnUpdate();

	#ifdef _DEBUG
	GamePad& gamePad = Game::Input::Instance().GetGamePad();
	if (gamePad.GetButtonDown() & GamePad::BTN_F4)
	{
		SwitchToDebugMode();
	}
	if (gamePad.GetButtonDown() & GamePad::BTN_F5)
	{
		SwitchToPlayMode();
	}

	if (gamePad.GetButtonDown() & GamePad::BTN_F1)
	{
		isCursorReleased = true;
	}

	if (isCursorReleased &&
		Game::Input::IsFocusedWindow() &&
		(Game::Input::Instance().GetMouse().GetButtonDown() & Mouse::BTN_LEFT))
	{
		isCursorReleased = false;
	}
	#endif

	// カメラコントローラーがないとアクターは動けないよ！
	if (cameraControllers.size() > 0)
	{
		// カメラ更新処理
		if (isCursorReleased)
		{
			cameraControllers[nowCameraControllerIndex]->OnFocusLost();
		}
		else
		{
			cameraControllers[nowCameraControllerIndex]->Update();
		}

		// カメラコントローラーからカメラへ反映
		cameraControllers[nowCameraControllerIndex]->SyncControllerToCamera(camera);

		actorManager.Update();
	}

	widgetManager.Update();

	lightManager.Update();

	PostProcessController::Instance().Update();
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
	RenderTarget* bloomWorkBuffer = graphics.GetFrameBuffer(Game::FrameBufferId::BloomWork);
	RenderTarget* ssaoBuffer = graphics.GetFrameBuffer(Game::FrameBufferId::SSAO);
	RenderTarget* postProcessBuffer = graphics.GetFrameBuffer(Game::FrameBufferId::PostProcess);
	RenderTarget* postProcessBuffer2 = graphics.GetFrameBuffer(Game::FrameBufferId::PostProcess2);

	// 描画コンテキスト設定
	RenderContext rc;
	{
		rc.deviceContext = dc;
		rc.renderState = renderState;
		rc.camera = &camera;
		rc.lightManager = &lightManager;
		rc.renderSettings = renderSettings;
		rc.shadowMapData = shadowMapData;
		rc.iblData = iblData;
	}

	// デバッグ切り替え
	{
		#ifdef _DEBUG
		if (Game::Input::Instance().GetGamePad().GetButtonDown() & GamePad::BTN_F3)
		{
			renderSettings.showDebug = !renderSettings.showDebug;
		}
		#endif
	}

	// IBLデータをRenderContextに詰める
	iblData.ggxLookUpTableMap = graphics.GetIBLGGXLUT();
	iblData.specularPremappingRadianceEnvironmentMap = graphics.GetIBLSpecularPMREM();
	iblData.diffuseIrradianceEnvironmentMap = graphics.GetIBLDiffuseIEM();
	rc.iblData = iblData;

	// シャドウマップ描画
	{
		for (Actor* actor : actorManager.GetActors())
		{
			if (!actor || actor->IsPendingDestroy()) continue;

			auto* mrc = actor->GetComponent<ModelRenderComponent>();
			if (mrc)
			{
				graphics.GetShadowMapRenderer()->Draw(mrc->GetModel());
			}

			auto* terrain = actor->GetComponent<Terrain>();
			if (terrain)
			{
				graphics.GetShadowMapRenderer()->Draw(terrain);
			}
		}

		graphics.GetShadowMapRenderer()->Render(
			rc,
			lightManager.GetDirectionalLight().GetDirection(),
			camera.GetFocus(),
			20.0f,
			100.0f,
			0.01f,
			200.0f
		);

		shadowMapData.shadowMap = graphics.GetShadowMapRenderer()->GetDepthSRV();
		shadowMapData.lightViewProjection = graphics.GetShadowMapRenderer()->GetLightViewProjection();
		rc.shadowMapData = shadowMapData;
	}

	// ---- シーン描画 → sceneBuffer ----------------------------------------
	sceneBuffer->Clear(dc);
	sceneBuffer->Activate(dc);
	{
		graphics.GetSkyBoxRenderer()->Render(
			rc.deviceContext,
			renderState,
			*rc.camera,
			graphics.GetIBLSpecularPMREM());

		actorManager.Render(rc);

		graphics.GetModelRenderer()->Render(rc);

		for (Actor* actor : actorManager.GetActors())
		{
			if (!actor || actor->IsPendingDestroy()) continue;

			auto* trail = actor->GetComponent<TrailRenderComponent>();
			if (trail)
			{
				trail->RenderTrail(rc);
			}
		}
	}
	sceneBuffer->Deactivate(dc);

	ID3D11ShaderResourceView* sceneColorMap = sceneBuffer->GetSRV();
	if (postProcess.IsSSAOEnabled())
	{
		ssaoBuffer->Clear(dc, 1, 1, 1, 1);
		ssaoBuffer->Activate(dc);
		{
			postProcess.SSAO(rc, sceneBuffer->GetDepthSRV());
		}
		ssaoBuffer->Deactivate(dc);

		postProcessBuffer2->Clear(dc);
		postProcessBuffer2->Activate(dc);
		{
			postProcess.ApplySSAO(rc, sceneBuffer->GetSRV(), ssaoBuffer->GetSRV());
		}
		postProcessBuffer2->Deactivate(dc);
		sceneColorMap = postProcessBuffer2->GetSRV();
	}

	// ---- 輝度抽出: sceneBuffer → luminanceBuffer --------------------------
	luminanceBuffer->Clear(dc);
	luminanceBuffer->Activate(dc);
	{
		if (postProcess.IsBloomExtractEnabled())
		{
			postProcess.LuminanceExtraction(rc, sceneColorMap);
		}
		else
		{
			postProcess.Copy(rc, sceneColorMap);
		}
	}
	luminanceBuffer->Deactivate(dc);

	if (postProcess.IsBloomBlurEnabled())
	{
		bloomWorkBuffer->Clear(dc);
		bloomWorkBuffer->Activate(dc);
		{
			postProcess.BloomBlur(rc, luminanceBuffer->GetSRV(), true);
		}
		bloomWorkBuffer->Deactivate(dc);

		luminanceBuffer->Clear(dc);
		luminanceBuffer->Activate(dc);
		{
			postProcess.BloomBlur(rc, bloomWorkBuffer->GetSRV(), false);
		}
		luminanceBuffer->Deactivate(dc);
	}

	// ---- Bloom合成 / Merge: sceneBuffer + luminanceBuffer → postProcessBuffer -----
	postProcessBuffer->Clear(dc);
	postProcessBuffer->Activate(dc);
	{
		if (postProcess.IsDualEffectEnabled())
		{
			postProcess.Bloom(rc, sceneColorMap, luminanceBuffer->GetSRV());
		}
		else
		{
			postProcess.Copy(rc, sceneColorMap);
		}
	}
	postProcessBuffer->Deactivate(dc);

	// PostProcessありのウィジェット
	postProcessBuffer->Activate(dc);
	{
		widgetManager.Render(rc, true);
		graphics.GetSpriteRenderer()->Render(rc);
	}
	postProcessBuffer->Deactivate(dc);

	postProcess.ClearRuntimeEffects();

	PostProcessController::Instance().ApplyTo(postProcess);

	// ---- Final PostProcess: postProcessBuffer → displayBuffer -------------
	postProcess.RenderFinal(
		rc,
		postProcessBuffer->GetSRV(),
		postProcessBuffer2,
		postProcessBuffer,
		displayBuffer);

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

	// PostProcessなしのウィジェット
	widgetManager.Render(rc, false);
	graphics.GetSpriteRenderer()->Render(rc);

	// GUI
	DrawGUI(rc);
}

void Scene::DrawGUI(RenderContext& rc)
{
	#ifdef _DEBUG
	if (renderSettings.showDebug)
	{
		// オブジェクト系統デバッグ
		{
			if (ImGui::Begin("Actors"))
			{
				actorManager.DrawGUI();
			}
			ImGui::End();

			if (ImGui::Begin("Widgets"))
			{
				widgetManager.DrawGUI();
			}
			ImGui::End();

			if (ImGui::Begin("Lights"))
			{
				lightManager.DrawGUI();
			}
			ImGui::End();
		}

		// ユーザー設定
		UserSettingsManager::Instance().DrawGUI();

		// シーン設定
		if (ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_None))
		{
			// パフォーマンス
			if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
			}

			// ウィンドウ切替
			if (ImGui::CollapsingHeader("Window", ImGuiTreeNodeFlags_DefaultOpen))
			{
				Game::Graphics& graphics = Game::Graphics::Instance();
				const char* label = graphics.IsBorderlessFullscreen()
					? "Windowed"
					: "Borderless Fullscreen";

				if (ImGui::Button(label, ImVec2(-1.0f, 30.0f)))
				{
					graphics.RequestToggleBorderlessFullscreen();
				}
			}

			// モード切替
			{
				float buttonHeight = 30.0f;
				float spacing = ImGui::GetStyle().ItemSpacing.x;
				float buttonWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;

				// 即デバッグモード
				if (ImGui::Button("Let's Debug!(F4)", ImVec2(buttonWidth, buttonHeight)))
				{
					SwitchToDebugMode();
				}

				ImGui::SameLine();

				// 即プレイモード
				if (ImGui::Button("Let's Play!(F5)", ImVec2(buttonWidth, buttonHeight)))
				{
					SwitchToPlayMode();
				}
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
					ImGui::TextDisabled("Light data is moved");
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
			if (ImGui::CollapsingHeader("Time", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("Time: %.4f", Game::Time::time);
				ImGui::Text("Unscaled Delta Time: %.4f", Game::Time::unscaledDeltaTime);
				ImGui::Text("Delta Time: %.4f", Game::Time::deltaTime);
				ImGui::DragFloat("Time Scale", &Game::Time::scale, 0.01f, 0.0f, 10.0f);
			}

			if (ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen))
			{
				Game::Graphics& graphics = Game::Graphics::Instance();
				graphics.GetSkyBoxRenderer()->DrawGUI();
				graphics.DrawSkyMapGUI();
			}

			// PostProcess
			if (ImGui::CollapsingHeader("PostProcess", ImGuiTreeNodeFlags_DefaultOpen))
			{
				postProcess.DrawGUI();

				ImGui::Separator();

				ImGui::Text("CONTROLLER");

				PostProcessController::Instance().DrawGUI();
			}

			// Editor
			if (ImGui::CollapsingHeader("Editor", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::Button("Dynamic Animation Editor"))
				{
					showDynamicAnimationEditorWindow = true;
				}
			}

			OnDrawGUI();
		}
		ImGui::End();

		dynamicAnimationEditorWindow.Draw(&showDynamicAnimationEditorWindow);
	}
	#endif
}

CameraController* Scene::GetNowCameraController() const
{
	if (cameraControllers.empty()) return nullptr;
	return cameraControllers[nowCameraControllerIndex].get();
}
