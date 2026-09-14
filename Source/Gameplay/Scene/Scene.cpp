#include "Gameplay/Scene/Scene.h"
#include "Application/Time/GameTime.h"
#include "Gameplay/Lighting/Light.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Resource/MeshCache.h"
#include "Rendering/Component/TrailRenderComponent.h"
#include "Rendering/Component/VMDLParticleEmitterComponent.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Gameplay/Scene/PostProcessController.h"
#include "Application/SettingsAndDebug/PhysicsLayerManager.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Gameplay/Scene/GameStartScene.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Application/Tools/Dialog.h"
#include "Physics/Core/PhysicsManager.h"
#include "Physics/Collider/CharacterController.h"

#include <format>

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
				sourceCamera->GetEye(), sourceCamera->GetFocus(), sourceCamera->GetUp());
			if (FreeCameraController* controller =
					dynamic_cast<FreeCameraController*>(stage.GetCameraController(debugCamera)))
				controller->SyncCameraToController(*sourceCamera);
		}
		// 停止中のデバッグカメラは周囲を確認しやすい広めの視野角にする。
		const float width = std::max(Game::Graphics::ScreenWidth, 1.0f);
		const float height = std::max(Game::Graphics::ScreenHeight, 1.0f);
		debugCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(75.0f), width / height, 0.1f, 1000.0f);
		debugCamera->SetActive(true);
		if (CameraController* controller = stage.GetCameraController(debugCamera))
			controller->SetActive(true);
		stage.PromoteCamera(debugCamera);
	}
	else
	{
		Actor* cameraActor = stage.GetDefaultCameraActor();
		if (!cameraActor) return;
		if (ThirdPersonCameraController* controller =
				cameraActor->GetComponent<ThirdPersonCameraController>())
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
	if (ThirdPersonCameraController* controller =
			cameraActor->GetComponent<ThirdPersonCameraController>())
		controller->SetActive(true);

	isCursorReleased = false;
	Game::Time::scale = 1.0f;
}

// F3に対応するデバッグ表示を切り替える
void Scene::ToggleDebugDisplay()
{
	renderSettings.showDebug = !renderSettings.showDebug;
}

void Scene::Update()
{
	OnUpdate();

	// ステージを持たないツールシーンでもF3を受け取れるよう、早期returnより前で処理する
	if (Game::Input::Instance().GetGamePad().GetButtonDown() & GamePad::BTN_F3)
	{
		ToggleDebugDisplay();
	}

	if (!pendingStagePath.empty())
	{
		auto openedStage = std::make_unique<Stage>();
		if (openedStage->LoadVSTG(pendingStagePath)) currentStage = std::move(openedStage);
		pendingStagePath.clear();
	}
	if (!currentStage) return;

	Stage& stage = *currentStage;

	if (UsesGameDebugGUI())
	{
		GamePad& gamePad = Game::Input::Instance().GetGamePad();
		if (gamePad.GetButtonDown() & GamePad::BTN_F2)
		{
			showGameEditorGUI = !showGameEditorGUI;
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

	const bool shouldUpdateWorld = ShouldUpdateWorld();
	if (CameraController* controller = stage.GetActiveCameraController())
		controller->SetInputEnabled(!isCursorReleased && shouldUpdateWorld);

	SelectPausedActor();
	if (!shouldUpdateWorld)
	{
		widgetManager.Update();
		return;
	}

	if (Game::Time::deltaTime > 0.0f)
	{
		class ControllerInteractionFilter final : public CCFilterCallback
		{
		  public:
			bool filter(const PxController& a, const PxController& b) override
			{
				PxShape* shapeA = nullptr;
				PxShape* shapeB = nullptr;
				a.getActor()->getShapes(&shapeA, 1);
				b.getActor()->getShapes(&shapeB, 1);
				auto* controllerA = static_cast<CharacterController*>(shapeA->userData);
				auto* controllerB = static_cast<CharacterController*>(shapeB->userData);
				if (!controllerA->IsPushable() || !controllerB->IsPushable()) return false;
				return CCFilterCallback::filter(a, b);
			}
		};
		static ControllerInteractionFilter controllerFilter;
		PhysicsManager::Instance().GetSceneContext().GetControllerManager()->computeInteractions(
			Game::Time::deltaTime, &controllerFilter);
	}
	stage.Update();

	widgetManager.Update();

	PostProcessController::Instance().Update();
}

void Scene::SelectPausedActor()
{
	auto& io = ImGui::GetIO();
	if (!UsesGameDebugGUI() || !showGameEditorGUI || !currentStage || Game::Time::scale > 0.0f)
		return;
	if (!Game::Input::IsFocusedWindow(true) || io.WantCaptureMouse) return;

	Mouse& mouse = Game::Input::Instance().GetMouse();
	if ((mouse.GetButtonDown() & Mouse::BTN_LEFT) == 0) return;

	Camera* camera = currentStage->GetActiveCamera();
	if (!camera || Game::Graphics::ScreenWidth <= 0.0f || Game::Graphics::ScreenHeight <= 0.0f)
		return;

	const Vector2 mouseNdc = Game::Graphics::Instance().GetMouseNDC(
		static_cast<float>(mouse.GetPositionX()),
		static_cast<float>(mouse.GetPositionY()));
	const Matrix inverseViewProjection = (camera->GetView() * camera->GetProjection()).Invert();
	const DirectX::XMVECTOR nearPointValue = DirectX::XMVector3TransformCoord(
		DirectX::XMVectorSet(mouseNdc.x, mouseNdc.y, 0.0f, 1.0f), inverseViewProjection);
	const DirectX::XMVECTOR farPointValue = DirectX::XMVector3TransformCoord(
		DirectX::XMVectorSet(mouseNdc.x, mouseNdc.y, 1.0f, 1.0f), inverseViewProjection);
	Vector3 nearPoint;
	Vector3 farPoint;
	DirectX::XMStoreFloat3(&nearPoint, nearPointValue);
	DirectX::XMStoreFloat3(&farPoint, farPointValue);

	PhysicsManager::PhysicsRaycastHit hit;
	if (!PhysicsManager::Instance().Raycast(nearPoint, farPoint - nearPoint, 10000.0f, hit) ||
		!hit.actor)
		return;

	currentStage->GetActorManager().SetSelectedActor(hit.actor);
	if (FreeCameraController* controller =
			dynamic_cast<FreeCameraController*>(currentStage->GetActiveCameraController()))
	{
		controller->FocusOn(hit.actor->transform.position);
	}
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
	if (Terrain* terrain = stage.GetComponent<Terrain>())
		terrain->ApplyDistanceFogSettings(renderSettings);
	ConfigureRenderSettings(renderSettings);
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
			if (mrc && mrc->IsActive())
			{
				graphics.GetShadowMapRenderer()->Draw(mrc->GetModel());
				for (const auto& [slot, meshCache] : mrc->GetMeshCaches())
					graphics.GetShadowMapRenderer()->Draw(mrc->GetModel(), &meshCache->GetMeshes());
				for (const auto& [groupIndex, meshCache] : mrc->GetExternalMeshCaches())
					graphics.GetShadowMapRenderer()->Draw(mrc->GetModel(), &meshCache->GetMeshes());
			}

			auto* terrain = actor->GetComponent<Terrain>();
			if (terrain)
			{
				graphics.GetShadowMapRenderer()->Draw(terrain);
			}
		}

		graphics.GetShadowMapRenderer()->Render(
			rc, lightManager.GetDirectionalLight().GetDirection(), 500.0f);

		for (int cascadeIndex = 0; cascadeIndex < ShadowMapData::CascadeCount; ++cascadeIndex)
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
		graphics.GetSkyBoxRenderer()->Render(rc.deviceContext, renderState, *rc.camera,
			graphics.GetIBLSpecularPMREM(), rc.renderSettings);

		stage.Render(rc);

		graphics.GetModelRenderer()->Render(rc);
		stage.RenderEffects(rc);

		for (Actor* actor : actorManager.GetActors())
		{
			if (!actor || actor->IsPendingDestroy()) continue;

			for (TrailRenderComponent* trail : actor->GetComponents<TrailRenderComponent>())
				trail->RenderTrail(rc);
			for (VMDLParticleEmitterComponent* emitter :
				actor->GetComponents<VMDLParticleEmitterComponent>())
				emitter->RenderParticles(rc);
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
		rc, postProcessBuffer->GetSRV(), postProcessBuffer2, postProcessBuffer, displayBuffer);

	// ShapeRenderer描画
	graphics.GetShapeRenderer()->Render(dc, camera.GetView(), camera.GetProjection());

	// PrimitiveRenderer描画
	primitiveRenderer->RenderTriangles(dc, camera.GetView(), camera.GetProjection(), renderState);

	primitiveRenderer->Render(
		dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// PostProcessなしのウィジェット
	widgetManager.Render(rc, false);
	graphics.GetSpriteRenderer()->Render(rc);

	// GUI
	DrawGUI(rc);
}

void Scene::DrawGUI(RenderContext& rc)
{
	auto& io = ImGui::GetIO();
	if (!currentStage) return;
	if (!UsesGameDebugGUI())
	{
		OnDrawGUI();
		return;
	}
	if (!showGameEditorGUI)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(
			{viewport->Pos.x + viewport->Size.x - 10.0f, viewport->Pos.y + 10.0f}, ImGuiCond_Always,
			{1.0f, 0.0f});
		ImGui::SetNextWindowBgAlpha(0.7f);
		constexpr ImGuiWindowFlags fpsWindowFlags =
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoInputs;
		if (ImGui::Begin("##Game FPS", nullptr, fpsWindowFlags))
			ImGui::TextColored(
				ImVec4(0.45f, 1.0f, 0.55f, 1.0f), "FPS: %.0f", io.Framerate);
		ImGui::End();
		return;
	}

	Stage& stage = *currentStage;
	Camera* activeCamera = stage.GetActiveCamera();
	if (!activeCamera) return;
	Camera& camera = *activeCamera;
	ActorManager& actorManager = stage.GetActorManager();
	LightManager& lightManager = stage.GetLightManager();
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu((const char*)u8"ファイル"))
			{
				if (ImGui::MenuItem((const char*)u8"ステージを開く"))
				{
					std::string filename;
					if (Dialog::OpenFileName(filename, "VSTG (*.vstg)\0*.vstg\0",
							"Open Stage") == DialogResult::OK)
						pendingStagePath = filename;
				}
				ImGui::Separator();
				if (ImGui::MenuItem((const char*)u8"終了"))
				{
					if (OnRequestExit())
					{
						SceneManager::Instance().LoadScene<GameStartScene>();
					}
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu((const char*)u8"表示"))
			{
				ImGui::MenuItem((const char*)u8"デバッグ表示", "F3", &renderSettings.showDebug);
				ImGui::Checkbox((const char*)u8"コライダー", &renderSettings.showColliderDebug);
				ImGui::Checkbox(
					(const char*)u8"コンポーネント", &renderSettings.showComponentDebug);
				ImGui::Checkbox(
					(const char*)u8"ナビメッシュ移動範囲", &renderSettings.showNavMeshDebug);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu((const char*)u8"ウィンドウ"))
			{
				if (ImGui::MenuItem((const char*)u8"物理レイヤー")) showPhysicsLayerWindow = true;
				if (ImGui::MenuItem((const char*)u8"動的アニメーションエディタ"))
					showDynamicAnimationEditorWindow = true;
				ImGui::EndMenu();
			}
			ImGui::Separator();
			if (ImGui::MenuItem((const char*)u8"再生", "F5", false, Game::Time::scale <= 0.0f))
				SwitchToPlayMode();
			if (ImGui::MenuItem((const char*)u8"停止", "F6", false, Game::Time::scale > 0.0f))
				SwitchToDebugMode();

			const char* shortcutText = (const char*)u8"F1: カーソル  F2: エディタ表示  F3: "
												u8"デバッグ  F5: 再生  F6: 再生／停止";
			const std::string fpsText = std::format("FPS: {:.0f}", io.Framerate);
			const float shortcutWidth = ImGui::CalcTextSize(shortcutText).x;
			const float fpsWidth = ImGui::CalcTextSize(fpsText.c_str()).x;
			ImGui::SetCursorPosX(std::max(
				ImGui::GetCursorPosX() + 20.0f, ImGui::GetWindowWidth() - shortcutWidth - fpsWidth -
													20.0f - ImGui::GetStyle().WindowPadding.x));
			ImGui::TextUnformatted(shortcutText);
			ImGui::SameLine(0.0f, 20.0f);
			ImGui::TextColored(
				ImVec4(0.45f, 1.0f, 0.55f, 1.0f), "%s", fpsText.c_str());
			ImGui::EndMainMenuBar();
		}

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		constexpr float windowMargin = 10.0f;
		const ImVec2 contentPosition = {
			viewport->WorkPos.x + windowMargin, viewport->WorkPos.y + windowMargin};
		const ImVec2 contentSize = {std::max(1.0f, viewport->WorkSize.x - windowMargin * 2.0f),
			std::max(1.0f, viewport->WorkSize.y - windowMargin * 2.0f)};
		const float panelHeight = contentSize.y / 3.0f;
		const float minimumPanelWidth = std::min(240.0f, contentSize.x * 0.4f);
		const float minimumCenterWidth = std::min(240.0f,
			std::max(1.0f, contentSize.x - minimumPanelWidth * 2.0f));
		const float maximumPanelTotal = std::max(
			minimumPanelWidth * 2.0f, contentSize.x - minimumCenterWidth);
		gameEditorLeftWidth = std::clamp(gameEditorLeftWidth, minimumPanelWidth,
			maximumPanelTotal - minimumPanelWidth);
		gameEditorRightWidth = std::clamp(gameEditorRightWidth, minimumPanelWidth,
			maximumPanelTotal - gameEditorLeftWidth);

		constexpr ImGuiWindowFlags panelWindowFlags = ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoSavedSettings;

		// オブジェクト系統デバッグ
		{
			ImGui::SetNextWindowPos(contentPosition, ImGuiCond_Always);
			ImGui::SetNextWindowSize({gameEditorLeftWidth, panelHeight}, ImGuiCond_Always);
			const bool actorsWindowOpen =
				ImGui::Begin((const char*)u8"アクター", nullptr, panelWindowFlags);
			actorManager.DrawGUI(actorsWindowOpen);
			ImGui::End();

			ImGui::SetNextWindowPos(
				{contentPosition.x, contentPosition.y + panelHeight}, ImGuiCond_Always);
			ImGui::SetNextWindowSize({gameEditorLeftWidth, panelHeight}, ImGuiCond_Always);
			if (ImGui::Begin((const char*)u8"ウィジェット", nullptr, panelWindowFlags))
			{
				widgetManager.DrawGUI();
			}
			ImGui::End();

			ImGui::SetNextWindowPos(
				{contentPosition.x, contentPosition.y + panelHeight * 2.0f}, ImGuiCond_Always);
			ImGui::SetNextWindowSize({gameEditorLeftWidth, panelHeight}, ImGuiCond_Always);
			if (ImGui::Begin((const char*)u8"ライト", nullptr, panelWindowFlags))
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
			{contentPosition.x + contentSize.x - gameEditorRightWidth, contentPosition.y},
			ImGuiCond_Always);
		ImGui::SetNextWindowSize({gameEditorRightWidth, contentSize.y}, ImGuiCond_Always);
		if (ImGui::Begin((const char*)u8"シーン", nullptr, panelWindowFlags))
		{
			// カメラ
			if (ImGui::CollapsingHeader((const char*)u8"カメラ", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text((const char*)u8"優先度: %d", camera.GetPriority());
				if (CameraController* controller = stage.GetActiveCameraController())
					controller->DrawGUI();
			}

			// Time
			if (ImGui::CollapsingHeader((const char*)u8"時間", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text((const char*)u8"経過時間: %.4f", Game::Time::time);
				ImGui::Text(
					(const char*)u8"非スケール差分時間: %.4f", Game::Time::unscaledDeltaTime);
				ImGui::Text((const char*)u8"差分時間: %.4f", Game::Time::deltaTime);
				ImGui::DragFloat((const char*)u8"時間倍率", &Game::Time::scale, 0.01f, 0.0f, 10.0f);
			}

			if (ImGui::CollapsingHeader(
					(const char*)u8"スカイボックス", ImGuiTreeNodeFlags_DefaultOpen))
			{
				Game::Graphics& graphics = Game::Graphics::Instance();
				graphics.GetSkyBoxRenderer()->DrawGUI();
				graphics.DrawSkyMapGUI();
			}

			// PostProcess
			if (ImGui::CollapsingHeader(
					(const char*)u8"ポストプロセス", ImGuiTreeNodeFlags_DefaultOpen))
			{
				postProcess.DrawGUI();

				ImGui::Separator();

				ImGui::Text((const char*)u8"コントローラー");

				PostProcessController::Instance().DrawGUI();
			}

			OnDrawGUI();
		}
		ImGui::End();

		// パネル幅調整
		constexpr float resizeHandleWidth = 8.0f;
		constexpr ImGuiWindowFlags resizeHandleFlags = ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoBackground;

		ImGui::SetNextWindowPos(
			{contentPosition.x + gameEditorLeftWidth - resizeHandleWidth * 0.5f,
				contentPosition.y},
			ImGuiCond_Always);
		ImGui::SetNextWindowSize({resizeHandleWidth, contentSize.y}, ImGuiCond_Always);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
		ImGui::Begin("##GameEditorLeftResize", nullptr, resizeHandleFlags);
		ImGui::InvisibleButton("##Resize", {resizeHandleWidth, contentSize.y});
		const bool leftResizeHovered = ImGui::IsItemHovered();
		const bool leftResizeActive = ImGui::IsItemActive();
		if (leftResizeHovered || leftResizeActive)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		if (leftResizeActive)
		{
			gameEditorLeftWidth = std::clamp(
				gameEditorLeftWidth + io.MouseDelta.x, minimumPanelWidth,
				maximumPanelTotal - gameEditorRightWidth);
		}
		const ImU32 leftResizeColor = ImGui::GetColorU32(leftResizeActive
			? ImGuiCol_SeparatorActive
			: leftResizeHovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
		const ImVec2 leftResizePosition = ImGui::GetWindowPos();
		ImGui::GetWindowDrawList()->AddLine(
			{leftResizePosition.x + resizeHandleWidth * 0.5f, leftResizePosition.y},
			{leftResizePosition.x + resizeHandleWidth * 0.5f,
				leftResizePosition.y + contentSize.y},
			leftResizeColor,
			leftResizeHovered || leftResizeActive ? 3.0f : 1.0f);
		ImGui::End();
		ImGui::PopStyleVar();

		const float rightResizeX = contentPosition.x + contentSize.x - gameEditorRightWidth;
		ImGui::SetNextWindowPos(
			{rightResizeX - resizeHandleWidth * 0.5f, contentPosition.y}, ImGuiCond_Always);
		ImGui::SetNextWindowSize({resizeHandleWidth, contentSize.y}, ImGuiCond_Always);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
		ImGui::Begin("##GameEditorRightResize", nullptr, resizeHandleFlags);
		ImGui::InvisibleButton("##Resize", {resizeHandleWidth, contentSize.y});
		const bool rightResizeHovered = ImGui::IsItemHovered();
		const bool rightResizeActive = ImGui::IsItemActive();
		if (rightResizeHovered || rightResizeActive)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		if (rightResizeActive)
		{
			gameEditorRightWidth = std::clamp(
				gameEditorRightWidth - io.MouseDelta.x, minimumPanelWidth,
				maximumPanelTotal - gameEditorLeftWidth);
		}
		const ImU32 rightResizeColor = ImGui::GetColorU32(rightResizeActive
			? ImGuiCol_SeparatorActive
			: rightResizeHovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
		const ImVec2 rightResizePosition = ImGui::GetWindowPos();
		ImGui::GetWindowDrawList()->AddLine(
			{rightResizePosition.x + resizeHandleWidth * 0.5f, rightResizePosition.y},
			{rightResizePosition.x + resizeHandleWidth * 0.5f,
				rightResizePosition.y + contentSize.y},
			rightResizeColor,
			rightResizeHovered || rightResizeActive ? 3.0f : 1.0f);
		ImGui::End();
		ImGui::PopStyleVar();

		dynamicAnimationEditorWindow.Draw(&showDynamicAnimationEditorWindow);
	}
}

CameraController* Scene::GetActiveCameraController() const
{
	return currentStage ? currentStage->GetActiveCameraController() : nullptr;
}
