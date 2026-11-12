#include <ProfileKit/ProfileKit.h>
#include <ImGuiToolKit/Widgets.h>
#include <ImGuiToolKit/Drag.h>
#include <ImGuiToolKit/Color.h>
#include "IconsFontAwesome5.h"
#include "Gameplay/Scene/Scene.h"
#include "Rendering/Renderer/ImGuiRenderer.h"
#include <cmath>
#include "Application/Time/GameTime.h"
#include "Animation/Animator.h"
#include "Gameplay/Lighting/Light.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Resource/MeshCache.h"
#include "Resource/ResourceManager.h"
#include "Rendering/Component/TrailRenderComponent.h"
#include "Rendering/Component/VMDLParticleEmitterComponent.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Rendering/Effect/EffectManager.h"
#include "Rendering/Renderer/WaterRenderer.h"
#include "Gameplay/Scene/PostProcessController.h"
#include "Application/SettingsAndDebug/PhysicsLayerManager.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Gameplay/Scene/TitleScene.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Application/Tools/Dialog.h"
#include "Physics/Core/PhysicsManager.h"
#include "Physics/Collider/CharacterController.h"

#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>

Scene::Scene() = default;
Scene::~Scene()
{
	Uninit();
}

void Scene::Uninit()
{
	PROFILE_FUNCTION();
	if (uninitialized) return;
	uninitialized = true;
	if (initialized) OnUninit();
	initialized = false;
	animatorEditor.reset();
	pendingAnimatorModel.reset();
	liveAnimatorOwner.reset();
	pendingLiveAnimatorOwner.reset();
	currentStage.reset();
	widgetManager = WidgetManager{};
	postProcess.Uninit();
}

void Scene::Init()
{
	PROFILE_FUNCTION();
	if (initialized || uninitialized) return;
	try
	{
		postProcess.Init();
		initialized = true;
		OnInit();
		if (currentStage)
		{
			currentStage->Awake();
			currentStage->Start();
		}
	}
	catch (...)
	{
		Uninit();
		throw;
	}
}

void Scene::OpenAnimatorEditor(const std::string& path, std::shared_ptr<VMDLModel> model,
	std::weak_ptr<Actor> liveOwner)
{
	// エディター自身の「開く」からも呼ばれるため、描画中には作り直さない。
	pendingAnimatorPath = path;
	pendingAnimatorModel = std::move(model);
	pendingLiveAnimatorOwner = std::move(liveOwner);
	animatorOpenRequested = true;
}

void Scene::DrawAnimatorEditor()
{
	if (!initialized) return;
	if (animatorOpenRequested)
	{
		animatorOpenRequested = false;
		liveAnimatorOwner = std::move(pendingLiveAnimatorOwner);
		const auto liveOwner = liveAnimatorOwner.lock();
		Animator* liveAnimator = liveOwner && !liveOwner->IsPendingDestroy()
			? liveOwner->GetComponent<Animator>() : nullptr;
		if (liveAnimator)
		{
			if (animatorEditor) animatorEditor->editorOpen = false;
			liveAnimator->OpenEditorWindow();
		}
		else
		{
			if (!animatorEditor || !pendingAnimatorPath.empty() ||
				(pendingAnimatorModel && animatorEditor->GetModel() != pendingAnimatorModel))
			{
				auto model = std::move(pendingAnimatorModel);
				if (!model && !pendingAnimatorPath.empty())
				{
					const auto modelPath = ResourceManager::FindSourceResourceRoot() / "Model" /
						(std::filesystem::path(pendingAnimatorPath).stem().string() + ".vmdl");
					// ソースの絶対パスはマニフェストの検索キーと一致しないため、直接読み込む
					if (std::filesystem::is_regular_file(modelPath))
						model = std::make_shared<VMDLModel>(modelPath.string().c_str());
				}

				// 再生中のコンポーネントではなく、Scene専用の編集データを保持する。
				animatorEditor = std::make_unique<Animator>(nullptr, std::move(model), true);
				if (!pendingAnimatorPath.empty())
					animatorEditor->Load(pendingAnimatorPath);
				else
					animatorEditor->AddLayer("Base Layer");
			}
			animatorEditor->OpenEditorWindow();
		}
		pendingAnimatorPath.clear();
		pendingAnimatorModel.reset();
	}

	if (const auto liveOwner = liveAnimatorOwner.lock())
	{
		if (liveOwner->IsPendingDestroy()) return;
		if (Animator* liveAnimator = liveOwner->GetComponent<Animator>())
		{
			if (liveAnimator->editorOpen)
				liveAnimator->DrawEditor(&liveAnimator->editorOpen);
			return;
		}
	}

	if (animatorEditor && animatorEditor->editorOpen)
		animatorEditor->DrawEditor(&animatorEditor->editorOpen);
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
	PROFILE_FUNCTION();
	if (!initialized) return;
	OnUpdate();
	ApplyMouseCursorMode();

	// ステージを持たないツールシーンでもF3を受け取れるよう、早期returnより前で処理する
	// F3でデバッグ表示切り替え
	if (InputKit::InputSystem::Instance().IsDown("DebugInput/ToggleDebugDisplay", true))
	{
		ToggleDebugDisplay();
	}

	if (!pendingStagePath.empty())
	{
		auto openedStage = std::make_unique<Stage>();
		if (openedStage->LoadVSTG(pendingStagePath))
		{
			currentStage = std::move(openedStage);
			selectedLight = nullptr;
			editorSelectionType = -1;
		}
		pendingStagePath.clear();
	}
	if (!currentStage)
	{
		widgetManager.Update();
		return;
	}

	Stage& stage = *currentStage;

	if (UsesGameDebugGUI())
	{
		const auto& input = InputKit::InputSystem::Instance();

		// F1でマウスカーソル解放切り替え
		if (input.IsDown("DebugInput/ToggleCursor", true))
		{
			isCursorReleased = !isCursorReleased;
		}

		// F2でゲーム内エディタ表示切り替え
		if (input.IsDown("DebugInput/ToggleEditor", true))
		{
			showGameEditorGUI = !showGameEditorGUI;
		}

		// F3でデバッグ表示切り替え => 95行目～

		// F4でゲーム止める
		if (input.IsDown("DebugInput/TogglePause", true))
		{
			if (Game::Time::scale > 0.0f) SwitchToDebugMode();
			else SwitchToPlayMode();
		}

		// F5でゲーム再開
		if (input.IsDown("DebugInput/Resume", true))
		{
			SwitchToPlayMode();
		}
	}

	const bool shouldUpdateWorld = ShouldUpdateWorld();
	if (CameraController* controller = stage.GetActiveCameraController())
		controller->SetInputEnabled(!isCursorReleased && shouldUpdateWorld);

	if (Camera* camera = stage.GetActiveCamera())
		UpdateCamFov(*camera, !ImGui::GetIO().WantCaptureMouse);
	if (auto* controller = dynamic_cast<FreeCameraController*>(stage.GetActiveCameraController()))
	{
		float speed = controller->GetMoveSpeed();
		UpdateCamSpeed(speed, !isCursorReleased && shouldUpdateWorld && !ImGui::GetIO().WantCaptureMouse);
		controller->SetMoveSpeed(speed);
	}

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

void Scene::ApplyMouseCursorMode()
{
	MouseCursorMode mode = GetMouseCursorMode();
	if (hudMouseCursorMode.has_value())
	{
		// ポーズHUDなどの一時的な指定はシーン既定値より優先する
		mode = hudMouseCursorMode.value();
	}
	else if (UsesGameDebugGUI() && isCursorReleased)
	{
		mode = MouseCursorMode::VisibleFree;
	}

	bool visible = true;
	bool locked = false;
	if (mode == MouseCursorMode::HiddenFree)
	{
		visible = false;
	}
	else if (mode == MouseCursorMode::HiddenLocked)
	{
		visible = false;
		locked = true;
	}

	InputKit::Mouse& mouse = InputKit::InputSystem::Instance().GetMouse();
	mouse.SetCursorLock(locked);
	mouse.SetCursorVisible(visible);
}

void Scene::SelectPausedActor()
{
	auto& io = ImGui::GetIO();
	if (!UsesGameDebugGUI() || !showGameEditorGUI || !currentStage || Game::Time::scale > 0.0f)
		return;
	if (!InputKit::InputSystem::IsFocusedWindow(true) || io.WantCaptureMouse) return;

	InputKit::Mouse& mouse = InputKit::InputSystem::Instance().GetMouse();
	if ((mouse.GetButtonDown() & InputKit::Mouse::BTN_LEFT) == 0) return;

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
	editorSelectionType = 0;
	requestInspectorTab = true;
	if (FreeCameraController* controller =
			dynamic_cast<FreeCameraController*>(currentStage->GetActiveCameraController()))
	{
		controller->FocusOn(hit.actor->transform.position);
	}
}

void Scene::Render()
{
	PROFILE_FUNCTION();
	if (!initialized) return;
	DrawToolWindows();
	if (!currentStage)
	{
		OnDrawGUI();
		Game::Graphics& graphics = Game::Graphics::Instance();
		RenderContext rc{};
		rc.deviceContext = graphics.GetDeviceContext();
		rc.graphics = &graphics;

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
	renderSettings.distanceFogEnabled &= renderSettings.showFog;
	Game::Graphics& graphics = Game::Graphics::Instance();
	ID3D11DeviceContext* dc = graphics.GetDeviceContext();
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
		rc.depthTarget = graphics.GetSceneDepthTarget();
		rc.graphics = &graphics;
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
		shadowMapData.shadowBias = graphics.GetShadowMapRenderer()->GetShadowBias();
		rc.shadowMapData = shadowMapData;
	}

	// ---- シーン描画 → sceneBuffer ----------------------------------------
	sceneBuffer->Clear(dc);
	rc.depthTarget->Clear(dc, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	const auto sceneOutput = RenderOutput::CaptureOutput(dc);
	RenderOutput::Bind(dc, sceneBuffer, rc.depthTarget, false);
	{
		graphics.GetSkyBoxRenderer()->Render(rc.deviceContext, &graphics, *rc.camera,
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
		EffectManager::Instance().Render(rc);

		OnRender(rc);
	}
	RenderOutput::RestoreOutput(dc, sceneOutput);

	for (WaterRenderer* water : stage.GetComponents<WaterRenderer>())
		water->ApplyUnderwaterContext(rc);
	for (Actor* actor : actorManager.GetActors())
	{
		if (!actor || !actor->IsActive() || actor->IsPendingDestroy()) continue;
		for (WaterRenderer* water : actor->GetComponents<WaterRenderer>())
			water->ApplyUnderwaterContext(rc);
	}

	ConfigurePostProcess(postProcess);

	postProcess.PrepareSceneColor(rc, sceneBuffer, luminanceBuffer, bloomWorkBuffer,
		ssaoBuffer, postProcessBuffer, postProcessBuffer2);

	// PostProcessありのウィジェット
	rc.depthTarget->Clear(dc, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	const auto widgetOutput = RenderOutput::CaptureOutput(dc);
	RenderOutput::Bind(dc, postProcessBuffer, rc.depthTarget, false);
	{
		widgetManager.Render(rc, true);
		graphics.GetSpriteRenderer()->Render(rc);
		EffectManager::Instance().Render2D(rc, true);
	}
	RenderOutput::RestoreOutput(dc, widgetOutput);

	postProcess.ClearRuntimeEffects();

	if (ShouldApplyGameplayPostEffects())
		PostProcessController::Instance().ApplyTo(postProcess);

	// ---- Final PostProcess: postProcessBuffer → displayBuffer -------------
	postProcess.RenderFinal(
		rc, postProcessBuffer->GetSRV(), postProcessBuffer2, postProcessBuffer, displayBuffer);
	rc.depthTarget->Clear(dc, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	RenderOutput::Bind(dc, displayBuffer, rc.depthTarget, false);

	// ShapeRenderer描画
	graphics.GetShapeRenderer()->Render(dc, camera.GetView(), camera.GetProjection());

	// PrimitiveRenderer描画
	primitiveRenderer->RenderTriangles(dc, camera.GetView(), camera.GetProjection(), &graphics);

	primitiveRenderer->Render(
		dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// PostProcessなしのウィジェット
	widgetManager.Render(rc, false);
	graphics.GetSpriteRenderer()->Render(rc);
	EffectManager::Instance().Render2D(rc, false);

	// GUI
	DrawGUI(rc);
	OnDrawOverlayGUI();
}

// 共通の補助ウィンドウ
void Scene::DrawWindowMenu()
{
	if (!ImGuiToolKit::BeginMenu(ICON_FA_WINDOW_MAXIMIZE " " u8"ウィンドウ")) return;
	if (ImGuiToolKit::MenuItem(ICON_FA_PROJECT_DIAGRAM " " u8"アニメーター")) OpenAnimatorEditor();
	ImGuiToolKit::MenuItem(ICON_FA_KEYBOARD " " u8"入力アクション", nullptr, &showInputSystemWindow);
	ImGuiToolKit::MenuItem(ICON_FA_LAYER_GROUP " " u8"物理レイヤー", nullptr, &showPhysicsLayerWindow);
	ImGuiToolKit::MenuItem(ICON_FA_WAVE_SQUARE " " u8"負荷プロファイラ", nullptr, &showProfilerWindow);
	ImGui::EndMenu();
}

void Scene::DrawEditorText(const std::string& title, bool alignRight)
{
	const std::string fpsText = std::format("FPS: {:.0f}", ImGui::GetIO().Framerate);
	constexpr float spacing = 20.0f;
	if (alignRight)
	{
		float width = ImGui::CalcTextSize(fpsText.c_str()).x;
		if (!title.empty()) width += spacing + ImGui::CalcTextSize(title.c_str()).x;
		ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX() + spacing,
			ImGui::GetWindowWidth() - width - ImGui::GetStyle().WindowPadding.x));
	}
	ImGui::TextColored(ImVec4(0.45f, 1.0f, 0.55f, 1.0f), "%s", fpsText.c_str());
	if (title.empty()) return;
	ImGui::SameLine(0.0f, spacing);
	ImGui::TextUnformatted(title.c_str());
}

void Scene::DrawToolWindows()
{
	if (showPhysicsLayerWindow)
		PhysicsLayerManager::Instance().DrawGUI(&showPhysicsLayerWindow);
	if (showInputSystemWindow)
	{
		ImGui::SetNextWindowSize({1100.0f, 650.0f}, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints({760.0f, 400.0f}, {10000.0f, 10000.0f});
		ImGui::SetNextWindowBgAlpha(1.0f);
		if (ImGuiToolKit::Begin(u8"入力アクション", &showInputSystemWindow))
			InputKit::InputSystemGUI::Instance().DrawGUI(InputKit::InputSystem::Instance());
		ImGui::End();
	}
	if (showProfilerWindow)
	{
		ImGui::SetNextWindowSize({1100.0f, 650.0f}, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints({760.0f, 400.0f}, {10000.0f, 10000.0f});
		ImGui::SetNextWindowBgAlpha(1.0f);
		if (ImGuiToolKit::Begin(u8"負荷プロファイラ", &showProfilerWindow))
			Profiler::DrawGUI();
		ImGui::End();
	}
}

void Scene::DrawGUI(RenderContext& rc)
{
	PROFILE_FUNCTION();
	ImGui::SetNextWindowBgAlpha(1.0f);

	auto& io = ImGui::GetIO();
	if (!currentStage) return;
	if (!UsesGameDebugGUI())
	{
		OnDrawGUI();
		return;
	}
	if (!io.WantTextInput && !io.KeyCtrl && !io.KeyAlt && !io.KeySuper &&
		!ImGui::IsMouseDown(ImGuiMouseButton_Right) && !ImGui::IsMouseDown(ImGuiMouseButton_Middle))
	{
		if (ImGui::IsKeyPressed(ImGuiKey_F, false)) renderSettings.showFog = !renderSettings.showFog;
		if (ImGui::IsKeyPressed(ImGuiKey_G, false)) renderSettings.showGrass = !renderSettings.showGrass;
	}
	if (!showGameEditorGUI)
	{
		gameEditorResizeEdge = 0;
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(
			{viewport->Pos.x + viewport->Size.x - 10.0f, viewport->Pos.y + 10.0f}, ImGuiCond_Always,
			{1.0f, 0.0f});
		constexpr ImGuiWindowFlags fpsWindowFlags =
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoInputs;
		if (ImGui::Begin("##Game FPS", nullptr, fpsWindowFlags))
			DrawEditorText({}, false);
		ImGui::End();
		return;
	}

	const bool restoreLayoutTabs = !gameEditorLayoutLoaded;
	auto& layout = ImGuiRenderer::GetGameEditorLayoutSettings();
	if (!gameEditorLayoutLoaded)
	{
		if (layout.loaded)
		{
			if (std::isfinite(layout.leftWidth) && layout.leftWidth > 0.0f) gameEditorLeftWidth = layout.leftWidth;
			if (std::isfinite(layout.rightWidth) && layout.rightWidth > 0.0f) gameEditorRightWidth = layout.rightWidth;
			if (std::isfinite(layout.panelHeight) && layout.panelHeight > 0.0f) gameEditorPanelHeight = layout.panelHeight;
			hierarchyTab = std::clamp(layout.hierarchyTab, 0, 2);
			inspectorTab = std::clamp(layout.inspectorTab, 0, 1);
			sceneSettingsPage = std::clamp(layout.sceneSettingsPage, 0, 4);
		}
		gameEditorLayoutLoaded = true;
	}
	const int restoredHierarchyTab = hierarchyTab;
	const int restoredInspectorTab = inspectorTab;
	Stage& stage = *currentStage;
	Camera* activeCamera = stage.GetActiveCamera();
	if (!activeCamera) return;
	Camera& camera = *activeCamera;
	ActorManager& actorManager = stage.GetActorManager();
	LightManager& lightManager = stage.GetLightManager();
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGuiToolKit::BeginMenu(ICON_FA_FILE " " u8"ファイル"))
			{
				if (ImGuiToolKit::MenuItem(ICON_FA_FOLDER_OPEN " " u8"ステージを開く"))
				{
					std::string filename;
					if (Dialog::OpenFileName(filename, "VSTG (*.vstg)\0*.vstg\0",
							"Open Stage") == DialogResult::OK)
						pendingStagePath = filename;
				}
				ImGui::Separator();
				if (ImGuiToolKit::MenuItem(ICON_FA_SIGN_OUT_ALT " " u8"終了"))
				{
					if (OnRequestExit())
					{
						SceneManager::Instance().LoadScene<TitleScene>();
					}
				}
				ImGui::EndMenu();
			}
			if (ImGuiToolKit::BeginMenu(ICON_FA_EYE " " u8"表示"))
			{
				ImGuiToolKit::MenuItem(ICON_FA_LEAF " " u8"草", "G", &renderSettings.showGrass);
				ImGuiToolKit::MenuItem(ICON_FA_SMOG " " u8"フォグ", "F", &renderSettings.showFog);
				ImGui::Separator();
				ImGuiToolKit::MenuItem(ICON_FA_BUG " " u8"デバッグ表示", "F3", &renderSettings.showDebug);
				ImGuiToolKit::Checkbox(ICON_FA_SHAPES " " u8"コライダー", &renderSettings.showColliderDebug);
				ImGuiToolKit::Checkbox(ICON_FA_PUZZLE_PIECE " " u8"コンポーネント", &renderSettings.showComponentDebug);
				ImGuiToolKit::Checkbox(ICON_FA_PROJECT_DIAGRAM " " u8"ナビメッシュ移動範囲", &renderSettings.showNavMeshDebug);
				ImGui::EndMenu();
			}
			DrawWindowMenu();
			ImGui::Separator();
			if (ImGui::MenuItem(ICON_FA_PLAY "##PlayGame", nullptr, false, Game::Time::scale <= 0.0f))
				SwitchToPlayMode();
			if (ImGui::IsItemHovered()) ImGuiToolKit::SetTooltip(u8"再生 (F5)");
			if (ImGui::MenuItem(ICON_FA_STOP "##StopGame", nullptr, false, Game::Time::scale > 0.0f))
				SwitchToDebugMode();
			if (ImGui::IsItemHovered()) ImGuiToolKit::SetTooltip(u8"停止 (F4)");

			DrawEditorText();
			ImGui::EndMainMenuBar();
		}

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		constexpr float windowMargin = 10.0f;
		const ImVec2 contentPosition = {
			viewport->WorkPos.x + windowMargin, viewport->WorkPos.y + windowMargin};
		const ImVec2 contentSize = {std::max(1.0f, viewport->WorkSize.x - windowMargin * 2.0f),
			std::max(1.0f, viewport->WorkSize.y - windowMargin * 2.0f)};

		const float minimumPanelWidth = std::min(240.0f, contentSize.x * 0.4f);
		const float minimumCenterWidth = std::min(240.0f,
			std::max(1.0f, contentSize.x - minimumPanelWidth * 2.0f));
		const float maximumPanelTotal = std::max(
			minimumPanelWidth * 2.0f, contentSize.x - minimumCenterWidth);
		gameEditorLeftWidth = std::clamp(gameEditorLeftWidth, minimumPanelWidth,
			maximumPanelTotal - minimumPanelWidth);
		gameEditorRightWidth = std::clamp(gameEditorRightWidth, minimumPanelWidth,
			maximumPanelTotal - gameEditorLeftWidth);

		if (gameEditorPanelHeight <= 0.0f) gameEditorPanelHeight = contentSize.y;
		gameEditorPanelHeight = std::clamp(gameEditorPanelHeight, std::min(240.0f, contentSize.y), contentSize.y);

		// 透明ウィンドウではなく境界の座標で判定し、パネルとの重なりを避ける
		constexpr float resizeHitRadius = 6.0f;
		int hoveredResizeEdge = 0;
		if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
		{
			const auto hitsVerticalEdge = [&](float x) {
				return ImGui::IsMouseHoveringRect({x - resizeHitRadius, contentPosition.y},
					{x + resizeHitRadius, contentPosition.y + gameEditorPanelHeight}, false);
			};
			if (hitsVerticalEdge(contentPosition.x + gameEditorLeftWidth)) hoveredResizeEdge = 1;
			else if (hitsVerticalEdge(contentPosition.x + contentSize.x - gameEditorRightWidth)) hoveredResizeEdge = 2;
			else if (ImGui::IsMouseHoveringRect(
				{contentPosition.x, contentPosition.y + gameEditorPanelHeight - resizeHitRadius},
				{contentPosition.x + contentSize.x, contentPosition.y + gameEditorPanelHeight + resizeHitRadius}, false))
				hoveredResizeEdge = 3;
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) gameEditorResizeEdge = 0;
		if (hoveredResizeEdge && !ImGui::IsAnyItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			gameEditorResizeEdge = hoveredResizeEdge;
		const int cursorResizeEdge = gameEditorResizeEdge ? gameEditorResizeEdge : hoveredResizeEdge;
		if (cursorResizeEdge)
		{
			ImGui::SetMouseCursor(cursorResizeEdge == 3 ? ImGuiMouseCursor_ResizeNS : ImGuiMouseCursor_ResizeEW);
			ImGui::SetNextFrameWantCaptureMouse(true);
		}
		if (gameEditorResizeEdge == 1)
			gameEditorLeftWidth = std::clamp(gameEditorLeftWidth + io.MouseDelta.x,
				minimumPanelWidth, maximumPanelTotal - gameEditorRightWidth);
		else if (gameEditorResizeEdge == 2)
			gameEditorRightWidth = std::clamp(gameEditorRightWidth - io.MouseDelta.x,
				minimumPanelWidth, maximumPanelTotal - gameEditorLeftWidth);
		else if (gameEditorResizeEdge == 3)
			gameEditorPanelHeight = std::clamp(gameEditorPanelHeight + io.MouseDelta.y,
				std::min(240.0f, contentSize.y), contentSize.y);

		const ImGuiWindowFlags panelWindowFlags = ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoSavedSettings |
			(cursorResizeEdge ? ImGuiWindowFlags_NoMouseInputs : 0);

		// 一覧は選択だけを担当し、詳細は右側のInspectorへ表示する。
		ImGui::SetNextWindowPos(contentPosition, ImGuiCond_Always);
		ImGui::SetNextWindowSize({gameEditorLeftWidth, gameEditorPanelHeight}, ImGuiCond_Always);
		if (ImGui::Begin("ヒエラルキー##GameEditor", nullptr, panelWindowFlags))
		{
			if (ImGui::BeginTabBar("##HierarchyTabs"))
			{
				if (ImGui::BeginTabItem("Actor", nullptr, restoreLayoutTabs && restoredHierarchyTab == 0 ? ImGuiTabItemFlags_SetSelected : 0)) { hierarchyTab = 0; ImGui::EndTabItem(); }
				if (ImGui::BeginTabItem("Widget", nullptr, restoreLayoutTabs && restoredHierarchyTab == 1 ? ImGuiTabItemFlags_SetSelected : 0)) { hierarchyTab = 1; ImGui::EndTabItem(); }
				if (ImGui::BeginTabItem("Light", nullptr, restoreLayoutTabs && restoredHierarchyTab == 2 ? ImGuiTabItemFlags_SetSelected : 0)) { hierarchyTab = 2; ImGui::EndTabItem(); }
				ImGui::EndTabBar();
			}
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##HierarchySearch", JText(u8"名前・タグ・種類で検索"), hierarchySearch, sizeof(hierarchySearch));
			const ImGuiTextFilter filter(hierarchySearch);
			const auto select = [&](int type) {
				editorSelectionType = type;
				inspectorTab = 0;
				requestInspectorTab = true;
			};
			if (hierarchyTab == 2)
			{
				ImGui::BeginDisabled(!lightManager.CanAddLight());
				if (ImGuiToolKit::Button(u8"+ ライトを追加")) ImGui::OpenPopup("AddLight");
				if (ImGui::BeginPopup("AddLight"))
				{
					Light* added = nullptr;
					if (ImGui::MenuItem("Directional")) added = lightManager.AddDirectionalLight();
					if (ImGui::MenuItem("Point")) added = lightManager.AddPointLight();
					if (ImGui::MenuItem("Spot")) added = lightManager.AddSpotLight();
					if (ImGui::MenuItem("Area")) added = lightManager.AddAreaLight();
					if (added) { selectedLight = added; select(2); }
					ImGui::EndPopup();
				}
				ImGui::EndDisabled();
			}
			ImGui::Separator();
			if (ImGui::BeginChild("##HierarchyItems"))
			{
				if (hierarchyTab == 1)
				{
					if (widgetManager.DrawGUI(hierarchySearch)) select(1);
				}
				else if (ImGui::BeginTable("ObjectList", 2,
					ImGuiTableFlags_RowBg))
				{
					ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 28.0f);
					ImGuiToolKit::TableSetupColumn(u8"名前", ImGuiTableColumnFlags_WidthStretch, 2.0f);
					const auto row = [&](Object& object, bool selected) {
						ImGui::TableNextRow(0, ImGui::GetFrameHeight() + 4.0f);
						ImGui::TableSetColumnIndex(0);
						bool active = object.IsActive();
						if (ImGui::Checkbox("##Active", &active)) object.SetActive(active);
						ImGui::TableSetColumnIndex(1);
						const char* name = object.GetName().empty() ? "(Unnamed)" : object.GetName().c_str();
						ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, {0.0f, 0.5f});
						const bool clicked = ImGui::Selectable(name, selected, 0, {0, ImGui::GetFrameHeight()});
						ImGui::PopStyleVar();
						if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", name);
						return clicked;
					};
					if (hierarchyTab == 0)
					{
						for (Actor* actor : actorManager.GetActors())
						{
							if (!actor || actor->IsPendingDestroy()) continue;
							const std::string searchText = actor->GetName() + " " + actor->GetTag();
							if (!filter.PassFilter(searchText.c_str())) continue;
							ImGui::PushID(actor);
							if (row(*actor, editorSelectionType == 0 && actor == actorManager.GetSelectedActor()))
							{ actorManager.SetSelectedActor(actor); select(0); }
							ImGui::PopID();
						}
					}
					else
					{
						const auto drawLight = [&](Light& light, const char* type) {
							if (light.IsPendingDestroy()) return;
							const std::string searchText = light.GetName() + " " + type;
							if (!filter.PassFilter(searchText.c_str())) return;
							ImGui::PushID(&light);
							if (row(light, editorSelectionType == 2 && selectedLight == &light))
							{ selectedLight = &light; select(2); }
							ImGui::PopID();
						};
						for (auto& light : lightManager.GetDirectionalLights()) drawLight(light, "Dir");
						for (auto& light : lightManager.GetPointLights()) drawLight(light, "Point");
						for (auto& light : lightManager.GetSpotLights()) drawLight(light, "Spot");
						for (auto& light : lightManager.GetAreaLights()) drawLight(light, "Area");
					}
					ImGui::EndTable();
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();

		// ユーザー設定


		// シーン設定
		ImGui::SetNextWindowPos(
			{contentPosition.x + contentSize.x - gameEditorRightWidth, contentPosition.y},
			ImGuiCond_Always);
		ImGui::SetNextWindowSize({gameEditorRightWidth, gameEditorPanelHeight}, ImGuiCond_Always);
		if (ImGui::Begin("インスペクタ##GameEditor", nullptr, panelWindowFlags))
		{
			if (ImGui::BeginTabBar("##InspectorTabs"))
			{
				if (ImGui::BeginTabItem("Inspector", nullptr, (requestInspectorTab || (restoreLayoutTabs && restoredInspectorTab == 0)) ? ImGuiTabItemFlags_SetSelected : 0)) { inspectorTab = 0; ImGui::EndTabItem(); }
				if (ImGui::BeginTabItem("Scene Settings", nullptr, restoreLayoutTabs && restoredInspectorTab == 1 ? ImGuiTabItemFlags_SetSelected : 0)) { inspectorTab = 1; ImGui::EndTabItem(); }
				ImGui::EndTabBar();
				requestInspectorTab = false;
			}
			if (inspectorTab == 0)
			{
				const auto deleteButton = [](Object& object) {
					ImGui::PushID(&object);
					ImGui::Separator();
					if (ImGuiToolKit::Button(u8"削除", {120.0f, 0.0f})) object.Destroy();
					ImGui::PopID();
				};
				if (editorSelectionType == 0)
				{
					Actor* selected = actorManager.GetSelectedActor();
					if (selected && actorManager.Contains(selected) && !selected->IsPendingDestroy())
					{
						deleteButton(*selected);
						selected->DrawGUI();
					}
				}
				else if (editorSelectionType == 1)
				{
					auto selected = widgetManager.GetSelectedWidget();
					if (selected)
					{
						deleteButton(*selected);
						selected->DrawGUI();
					}
				}
				else if (editorSelectionType == 2)
				{
					bool valid = false;
					const auto check = [&](Light& light) { if (&light == selectedLight && !light.IsPendingDestroy()) valid = true; };
					for (auto& light : lightManager.GetDirectionalLights()) check(light);
					for (auto& light : lightManager.GetPointLights()) check(light);
					for (auto& light : lightManager.GetSpotLights()) check(light);
					for (auto& light : lightManager.GetAreaLights()) check(light);
					if (valid)
					{ ImGui::PushID(selectedLight); selectedLight->DrawGUI(); ImGui::PopID(); deleteButton(*selectedLight); }
					else { selectedLight = nullptr; }
				}
			}
			else
			{
				ImGuiToolKit::Combo(u8"設定カテゴリ", &sceneSettingsPage,
					JText(u8"カメラ\0時間\0環境\0画面効果\0シーン固有\0"));
				ImGui::Separator();
				if (sceneSettingsPage == 2)
					ImGuiToolKit::ColorEdit3(u8"環境光", &lightManager.GetAmbientColor().x);
				
				// カメラ
				if (sceneSettingsPage == 0 && ImGuiToolKit::CollapsingHeader(ICON_FA_CAMERA " " u8"カメラ", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGuiToolKit::Text(u8"優先度: %d", camera.GetPriority());
					if (CameraController* controller = stage.GetActiveCameraController())
						controller->DrawGUI();
				}

				// 時間
				if (sceneSettingsPage == 1 && ImGuiToolKit::CollapsingHeader(ICON_FA_CLOCK " " u8"時間", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGuiToolKit::Text(u8"経過時間: %.4f", Game::Time::time);
					ImGuiToolKit::Text(u8"非スケール差分時間: %.4f", Game::Time::unscaledDeltaTime);
					ImGuiToolKit::Text(u8"差分時間: %.4f", Game::Time::deltaTime);
					ImGuiToolKit::DragSpeedFloat(u8"時間倍率", &Game::Time::scale, 0.01f, 0.0f, 10.0f);
				}

				// 環境
				if (sceneSettingsPage == 2)
				{
					if (ImGuiToolKit::CollapsingHeader(ICON_FA_SUN " " u8"シャドウ", ImGuiTreeNodeFlags_DefaultOpen))
					{
						Game::Graphics::Instance().GetShadowMapRenderer()->DrawGUI();
					}
					if (ImGuiToolKit::CollapsingHeader(ICON_FA_CLOUD " " u8"スカイボックス",
							ImGuiTreeNodeFlags_DefaultOpen))
					{
						Game::Graphics& graphics = Game::Graphics::Instance();
						graphics.GetSkyBoxRenderer()->DrawGUI();
						graphics.DrawSkyMapGUI();
					}
				}

				// 画面効果
				if (sceneSettingsPage == 3 && ImGuiToolKit::CollapsingHeader(ICON_FA_MAGIC " " u8"ポストプロセス", ImGuiTreeNodeFlags_DefaultOpen))
				{
					postProcess.DrawGUI();

					ImGui::Separator();

					ImGuiToolKit::Text(u8"コントローラー");

					PostProcessController::Instance().DrawGUI();
				}

				// シーン固有
				if (sceneSettingsPage == 4) OnDrawGUI();
			}
		}
		ImGui::End();

		// 描画順に左右されず、パネルの境界を画面上で示す
		ImDrawList* resizeDrawList = ImGui::GetForegroundDrawList(const_cast<ImGuiViewport*>(viewport));
		const auto drawResizeEdge = [&](int edge, const ImVec2& from, const ImVec2& to) {
			const bool active = gameEditorResizeEdge == edge;
			const bool hovered = hoveredResizeEdge == edge;
			resizeDrawList->AddLine(from, to, ImGui::GetColorU32(active
				? ImGuiCol_SeparatorActive : hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator),
				active || hovered ? 3.0f : 1.0f);
		};
		drawResizeEdge(1, {contentPosition.x + gameEditorLeftWidth, contentPosition.y},
			{contentPosition.x + gameEditorLeftWidth, contentPosition.y + gameEditorPanelHeight});
		drawResizeEdge(2, {contentPosition.x + contentSize.x - gameEditorRightWidth, contentPosition.y},
			{contentPosition.x + contentSize.x - gameEditorRightWidth, contentPosition.y + gameEditorPanelHeight});
		drawResizeEdge(3, {contentPosition.x, contentPosition.y + gameEditorPanelHeight},
			{contentPosition.x + contentSize.x, contentPosition.y + gameEditorPanelHeight});

		const bool layoutChanged = !layout.loaded || layout.leftWidth != gameEditorLeftWidth ||
			layout.rightWidth != gameEditorRightWidth || layout.panelHeight != gameEditorPanelHeight ||
			layout.hierarchyTab != hierarchyTab || layout.inspectorTab != inspectorTab ||
			layout.sceneSettingsPage != sceneSettingsPage;
		if (layoutChanged && !gameEditorResizeEdge)
		{
			layout.loaded = true;
			layout.leftWidth = gameEditorLeftWidth;
			layout.rightWidth = gameEditorRightWidth;
			layout.panelHeight = gameEditorPanelHeight;
			layout.hierarchyTab = hierarchyTab;
			layout.inspectorTab = inspectorTab;
			layout.sceneSettingsPage = sceneSettingsPage;
			ImGuiRenderer::SaveSettings();
		}

	}
}

CameraController* Scene::GetActiveCameraController() const
{
	return currentStage ? currentStage->GetActiveCameraController() : nullptr;
}

std::unique_ptr<LightManager> Scene::previewLighting;
std::unique_ptr<Game::PostProcess> Scene::previewPostProcess;
RenderSettings Scene::previewRenderSettings;

void Scene::CapturePreviewEnvironment()
{
	if (!currentStage) return;
	previewLighting = std::make_unique<LightManager>();
	previewLighting->SetDirectionalLight(currentStage->GetLightManager().GetDirectionalLight());
	previewLighting->SetAmbientColor(currentStage->GetLightManager().GetAmbientColor());
	if (!previewPostProcess) previewPostProcess = std::make_unique<Game::PostProcess>();
	previewPostProcess->CopySettingsFrom(postProcess);
	previewRenderSettings = renderSettings;
}
