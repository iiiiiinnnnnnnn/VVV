// Scene.cpp

#include "Gameplay/Scene/Scene.h"
#include "Application/Time/GameTime.h"
#include "Gameplay/Lighting/Light.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Rendering/Component/TrailRenderComponent.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Gameplay/Scene/PostProcessController.h"
#include "Application/SettingsAndDebug/PhysicsLayerManager.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Gameplay/Scene/GameStartScene.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Application/Tools/Dialog.h"

Scene::Scene(SceneMessage message) : message(message)
{
}

void Scene::SwitchToDebugMode()
{
	if (!currentStage) return;

	Stage& stage = *currentStage;
	Camera* sourceCamera = stage.GetActiveCamera();
	if (Camera* debugCamera = stage.GetDebugCamera())
	{
		if (sourceCamera && sourceCamera != debugCamera)
		{
			debugCamera->SetLookAt(
				sourceCamera->GetEye(),
				sourceCamera->GetFocus(),
				sourceCamera->GetUp());
			if (FreeCameraController* controller = dynamic_cast<FreeCameraController*>(stage.GetCameraController(debugCamera)))
				controller->SyncCameraToController(*sourceCamera);
		}
		debugCamera->SetActive(true);
		if (CameraController* controller = stage.GetCameraController(debugCamera))
			controller->SetActive(true);
		stage.PromoteCamera(debugCamera);
	}
	else
	{
		Actor* cameraActor = stage.GetDefaultCameraActor();
		if (!cameraActor) return;
		if (ThirdPersonCameraController* controller = cameraActor->GetComponent<ThirdPersonCameraController>())
			controller->SetActive(false);
		if (FreeCameraController* controller = cameraActor->GetComponent<FreeCameraController>())
		{
			if (sourceCamera) controller->SyncCameraToController(*sourceCamera);
			controller->SetActive(true);
		}
	}

	isCursorReleased = false;
	Game::Time::scale = 0.0f;
}

void Scene::SwitchToPlayMode()
{
	if (!currentStage) return;

	Stage& stage = *currentStage;
	if (Camera* debugCamera = stage.GetDebugCamera())
	{
		if (CameraController* controller = stage.GetCameraController(debugCamera))
			controller->SetActive(false);
		debugCamera->SetActive(false);
	}

	Actor* cameraActor = stage.GetDefaultCameraActor();
	if (FreeCameraController* controller = cameraActor->GetComponent<FreeCameraController>())
		controller->SetActive(false);
	if (ThirdPersonCameraController* controller = cameraActor->GetComponent<ThirdPersonCameraController>())
		controller->SetActive(true);

	isCursorReleased = false;
	Game::Time::scale = 1.0f;
}

void Scene::Update()
{
	// ESCキーで起動画面戻る
	if (Game::Input::Instance().GetGamePad().GetButtonDown() & GamePad::BTN_ESCAPE)
	{
		if (OnRequestExit())
		{
			SceneManager::Instance().LoadScene<GameStartScene>();
			return;
		}
	}

	OnUpdate();
	if (!pendingStagePath.empty())
	{
		auto openedStage = std::make_unique<Stage>();
		if (openedStage->LoadVSTG(pendingStagePath)) currentStage = std::move(openedStage);
		pendingStagePath.clear();
	}
	if (!currentStage) return;

	Stage& stage = *currentStage;

	#ifdef _DEBUG
	if (UsesGameDebugGUI())
	{
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
			isCursorReleased = !isCursorReleased;
		}

		if (gamePad.GetButtonDown() & GamePad::BTN_F6)
		{
			if (Game::Time::scale > 0.0f) SwitchToDebugMode();
			else SwitchToPlayMode();
		}
	}
	#endif

	if (CameraController* controller = stage.GetActiveCameraController())
		controller->SetInputEnabled(!isCursorReleased);

	stage.Update();

	widgetManager.Update();

	PostProcessController::Instance().Update();
}

void Scene::Render()
{
	if (!currentStage)
	{
		OnDrawGUI();
		Game::Graphics& graphics = Game::Graphics::Instance();
		RenderContext rc{};
		rc.deviceContext = graphics.GetDeviceContext();
		rc.renderState = graphics.GetRenderState();

		widgetManager.Render(rc, true);
		graphics.GetSpriteRenderer()->Render(rc);
		widgetManager.Render(rc, false);
		graphics.GetSpriteRenderer()->Render(rc);
		return;
	}

	Stage& stage = *currentStage;
	Camera* activeCamera = stage.GetActiveCamera();
	if (!activeCamera) return;
	Camera& camera = *activeCamera;
	ActorManager& actorManager = stage.GetActorManager();
	LightManager& lightManager = stage.GetLightManager();
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
		if (Terrain* terrain = stage.GetComponent<Terrain>())
		{
			graphics.GetShadowMapRenderer()->Draw(terrain);
		}

		for (Actor* actor : actorManager.GetActors())
		{
			if (!actor || actor->IsPendingDestroy()) continue;

			auto* mrc = actor->GetComponent<VMDLModelComponent>();
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
			500.0f
		);

		for (int cascadeIndex = 0;
			 cascadeIndex < ShadowMapData::CascadeCount;
			 ++cascadeIndex)
		{
			shadowMapData.shadowMaps[cascadeIndex] =
				graphics.GetShadowMapRenderer()->GetDepthSRV(cascadeIndex);
			shadowMapData.lightViewProjections[cascadeIndex] =
				graphics.GetShadowMapRenderer()->GetLightViewProjection(cascadeIndex);
		}
		shadowMapData.cascadeSplits = graphics.GetShadowMapRenderer()->GetCascadeSplits();
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

		stage.Render(rc);

		graphics.GetModelRenderer()->Render(rc);

		for (Actor* actor : actorManager.GetActors())
		{
			if (!actor || actor->IsPendingDestroy()) continue;

			for (TrailRenderComponent* trail : actor->GetComponents<TrailRenderComponent>())
				trail->RenderTrail(rc);
		}

		OnRender(rc);
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
	if (!currentStage) return;
	if (!UsesGameDebugGUI())
	{
		OnDrawGUI();
		return;
	}

	Stage& stage = *currentStage;
	Camera* activeCamera = stage.GetActiveCamera();
	if (!activeCamera) return;
	Camera& camera = *activeCamera;
	ActorManager& actorManager = stage.GetActorManager();
	LightManager& lightManager = stage.GetLightManager();
	#ifdef _DEBUG
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Open Stage"))
				{
					char filename[MAX_PATH]{};
					if (Dialog::OpenFileName(filename, MAX_PATH, "VSTG (*.vstg)\0*.vstg\0", "Open Stage") == DialogResult::OK)
						pendingStagePath = filename;
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Exit"))
				{
					if (OnRequestExit())
					{
						SceneManager::Instance().LoadScene<GameStartScene>();
					}
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Display"))
			{
				ImGui::Checkbox("Colliders", &renderSettings.showColliderDebug);
				ImGui::Checkbox("Components", &renderSettings.showComponentDebug);
				ImGui::Checkbox("NavMesh Move Area", &renderSettings.showNavMeshDebug);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Window"))
			{
				if (ImGui::MenuItem("Physics Layer")) showPhysicsLayerWindow = true;
				if (ImGui::MenuItem("Dynamic Animation Editor")) showDynamicAnimationEditorWindow = true;
				ImGui::EndMenu();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Play", "F5", false, Game::Time::scale <= 0.0f))
				SwitchToPlayMode();
			if (ImGui::MenuItem("Pause", "F6", false, Game::Time::scale > 0.0f))
				SwitchToDebugMode();
			ImGui::EndMainMenuBar();
		}

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		const float panelTop = viewport->WorkPos.y;
		const float panelHeight = std::max(120.0f, viewport->WorkSize.y / 3.0f);
		constexpr float leftWidth = 600.0f;
		constexpr float rightWidth = 680.0f;

		// オブジェクト系統デバッグ
		{
			ImGui::SetNextWindowPos({viewport->WorkPos.x, panelTop}, ImGuiCond_Always);
			ImGui::SetNextWindowSize({leftWidth, panelHeight}, ImGuiCond_Always);
			const bool actorsWindowOpen = ImGui::Begin("Actors");
			actorManager.DrawGUI(actorsWindowOpen);
			ImGui::End();

			ImGui::SetNextWindowPos({viewport->WorkPos.x, panelTop + panelHeight}, ImGuiCond_Always);
			ImGui::SetNextWindowSize({leftWidth, panelHeight}, ImGuiCond_Always);
			if (ImGui::Begin("Widgets"))
			{
				widgetManager.DrawGUI();
			}
			ImGui::End();

			ImGui::SetNextWindowPos({viewport->WorkPos.x, panelTop + panelHeight * 2.0f}, ImGuiCond_Always);
			ImGui::SetNextWindowSize({leftWidth, panelHeight}, ImGuiCond_Always);
			if (ImGui::Begin("Lights"))
			{
				lightManager.DrawGUI();
			}
			ImGui::End();
		}

		// ユーザー設定
		if (showPhysicsLayerWindow)
			PhysicsLayerManager::Instance().DrawGUI(&showPhysicsLayerWindow);

		// シーン設定
		ImGui::SetNextWindowPos(
			{viewport->WorkPos.x + viewport->WorkSize.x - rightWidth, panelTop},
			ImGuiCond_Always);
		ImGui::SetNextWindowSize(
			{rightWidth, viewport->WorkSize.y},
			ImGuiCond_Always);
		if (ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_None))
		{
			// カメラ
			if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("Priority: %d", camera.GetPriority());
				if (CameraController* controller = stage.GetActiveCameraController())
					controller->DrawGUI();
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

			OnDrawGUI();
		}
		ImGui::End();

		dynamicAnimationEditorWindow.Draw(&showDynamicAnimationEditorWindow);
	}
	#endif
}

CameraController* Scene::GetActiveCameraController() const
{
	return currentStage ? currentStage->GetActiveCameraController() : nullptr;
}
