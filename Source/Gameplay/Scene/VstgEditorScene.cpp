#include <ImGuiToolKit/ImGuiToolKit.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <ProfileKit/ProfileKit.h>
#include "Gameplay/Scene/VstgEditorScene.h"

#include <algorithm>
#include <cfloat>
#include <cctype>
#include <format>
#include <limits>

#include <imguizmo/ImGuizmo.h>

#include "Application/Tools/Dialog.h"
#include <InputKit/InputKit.h>
#include "Core/Object/Object.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Scene/TitleScene.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/VmdlEditorScene.h"
#include "Gameplay/Stage/Component/StageLoader.h"
#include "Rendering/Component/Terrain.h"
#include "Gameplay/Stage/Stage.h"
#include "Physics/Collider/TerrainMeshCollider.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Physics/Core/PhysicsManager.h"
#include "Physics/RigidBody/Rigidbody.h"
#include "Resource/ResourceManager.h"
#include "Resource/Texture.h"
#include "Resource/ResourcePath.h"
#include "Resource/VMDLModel.h"
#include "Rendering/Core/Graphics.h"
#include <Dx11Kit/Dx11Kit.h>
#include "Rendering/Renderer/ShapeRenderer.h"
#include "Rendering/Renderer/ImGuiRenderer.h"
#include "IconsFontAwesome5.h"
#include "Rendering/Effect/EffectManager.h"

// 現在の向きを保ちながら、上方向だけを接地面に合わせる。
static Quaternion AlignPropRotationToSurface(const Quaternion& rotation, const Vector3& normal)
{
	if (normal.LengthSquared() < 0.000001f) return rotation;

	Vector3 targetUp = normal;
	targetUp.Normalize();
	const Matrix currentRotation = Matrix::CreateFromQuaternion(rotation);
	const Vector3 currentUp = Vector3::TransformNormal(Vector3::Up, currentRotation);
	const float dot = std::clamp(currentUp.Dot(targetUp), -1.0f, 1.0f);
	if (dot > 0.999999f) return rotation;

	Vector3 axis = currentUp.Cross(targetUp);
	if (dot < -0.999999f)
		axis = Vector3::TransformNormal(Vector3::UnitX, currentRotation);
	axis.Normalize();
	const Quaternion adjustment = Quaternion::CreateFromAxisAngle(axis, std::acos(dot));
	Quaternion aligned = Quaternion::CreateFromRotationMatrix(
		currentRotation * Matrix::CreateFromQuaternion(adjustment));
	aligned.Normalize();
	return aligned;
}

VstgEditorScene::VstgEditorScene() = default;

void VstgEditorScene::OnInit()
{
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(false);
	graphics.SetWindowMovementLocked(false);
	propPreviewTarget =
		std::make_unique<RenderTarget>(graphics.GetDevice(), 256, 256, DXGI_FORMAT_R8G8B8A8_UNORM);
	propPreviewDepthTarget = std::make_unique<DepthTarget>(graphics.GetDevice(), 256, 256,
		DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
	propPreviewCameraOwner = std::make_unique<Object>("VSTG Prop Preview Camera");
	propPreviewCamera = propPreviewCameraOwner->AddComponent<Camera>();
	propPreviewLights.SetAmbientColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
	propPreviewRenderParams.unlit = true;
	dragPreviewRenderParams.unlit = true;
	DirectionalLight& previewLight = propPreviewLights.GetDirectionalLight();
	previewLight.transform.rotation =
		Quaternion::CreateFromYawPitchRoll(RAD(-30.0f), RAD(35.0f), 0.0f);
	CreateStage();
	RefreshPropModels();
	RefreshTerrainAssets();
	grassDraftInitialized = false;
	use_brush = false;
	brushViewportInputAllowed = false;
	currentTerrainLayerIndex = 0;
	terrainBrushPreview = {};
	for (const auto& layer : terrainLayerImages)
	{
		if (terrain->GetTerrainLayerCount() >= Terrain::MaxTerrainLayers) break;
		terrain->AddTerrainLayer(layer.baseColorPath, layer.normalPath);
	}
	cleanStateHash = data.BuildEditorStateHash(*terrain, *navMesh, *stageLoader, currentStage->GetLightManager());
	LoadEditorSettings();
	std::error_code error;
	if (!recentStagePath.empty() && std::filesystem::is_regular_file(recentStagePath, error))
		LoadStage(recentStagePath);
	terrain->PrepareRenderResources();
}

VstgEditorScene::~VstgEditorScene()
{
	Uninit();
}

void VstgEditorScene::OnUninit()
{
	propPreviewCameraOwner.reset();
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(false);
	graphics.SetWindowMovementLocked(false);
}

// 通常ウィンドウの最大化と、3Dビューのアスペクト比同期を行う
void VstgEditorScene::OnUpdate()
{
	PROFILE_FUNCTION();
	Game::Graphics& graphics = Game::Graphics::Instance();
	if (maximizeWindowPending && !graphics.IsBorderlessFullscreen())
	{
		// ボーダーレス解除後に通常ウィンドウとして最大化する。
		HWND window = graphics.GetWindowHandle();
		SetWindowLongPtr(window, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
		SetWindowPos(window, HWND_TOP, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		ShowWindow(window, SW_MAXIMIZE);
		maximizeWindowPending = false;
	}

	// 最大化や手動リサイズ後も、モデルが横方向へ潰れない投影比率を維持する。
	if (currentStage)
	{
		if (Camera* camera = currentStage->GetActiveCamera())
		{
			const float width = std::max(Game::Graphics::ScreenWidth, 1.0f);
			const float height = std::max(Game::Graphics::ScreenHeight, 1.0f);
			const float fov = camera->GetVerticalFovDegrees();
			camera->SetPerspectiveFov(RAD(fov), width / height, 0.1f, 2000.0f);
		}
	}
}

void VstgEditorScene::CreateStage()
{
	effectPopupPending = false;
	selectedLightType = EditorLightType::None;
	selectedLightIndex = -1;
	lightingSettingsSelected = false;
	currentStage = std::make_unique<Stage>();
	DirectionalLight& directionalLight = currentStage->GetLightManager().GetDirectionalLight();
	directionalLight.transform.rotation =
		Quaternion::CreateFromYawPitchRoll(RAD(-35.0f), RAD(35.0f), 0.0f);
	currentStage->GetLightManager().SetAmbientColor(ColorFromRGBA(0x2A4C7DFF));
	auto* rigidbody = currentStage->AddComponent<RigidbodyStatic>();
	terrain = currentStage->AddComponent<Terrain>();
	currentStage->AddComponent<TerrainMeshCollider>(
		Layers::Get("Terrain"), rigidbody, TerrainMeshCollider::CollisionArea{}, nullptr, false);
	navMesh = currentStage->AddComponent<NavMeshActor>();
	stageLoader =
		currentStage->AddComponent<StageLoader>(currentStage.get(), std::string("{}"), true);
	stageLoader->SetEditorModels(&propModels);
	terrain->Awake();
	grassDraftInitialized = false;
	use_brush = false;
	brushViewportInputAllowed = false;
	currentTerrainLayerIndex = 0;
	terrainBrushPreview = {};
	for (const auto& layer : terrainLayerImages)
	{
		if (terrain->GetTerrainLayerCount() >= Terrain::MaxTerrainLayers) break;
		terrain->AddTerrainLayer(layer.baseColorPath, layer.normalPath);
	}
	navMesh->Awake();
	stageLoader->Awake();
	Camera* camera = currentStage->GetActiveCamera();
	camera->SetPerspectiveFov(
		RAD(FreeCameraController::VerticalFovDegrees), Game::Graphics::ScreenWidth / Game::Graphics::ScreenHeight, 0.1f, 2000.0f);
	camera->SetLookAt({0.0f, 20.0f, -30.0f}, Vector3::Zero, Vector3::Up);
	freeCameraController =
		currentStage->GetDefaultCameraActor()->AddComponent<FreeCameraController>();
	freeCameraController->SetMoveSpeed(10.0f);
	cleanStateHash = data.BuildEditorStateHash(
		*terrain, *navMesh, *stageLoader, currentStage->GetLightManager());
}

void VstgEditorScene::ConfigureRenderSettings(RenderSettings& settings)
{
	UpdateTerrainBrush();
	settings.terrainBrushPreview = &terrainBrushPreview;
	settings.distanceFogEnabled = settings.distanceFogEnabled && settings.showFog;
}

void VstgEditorScene::OnRender(RenderContext& rc)
{
	if (currentStage && rc.renderSettings.showDebug && rc.renderSettings.showLightDebug)
		currentStage->GetLightManager().DrawDebug();
	if (stageLoader && stageLoader->HasPlayerStart())
	{
		for (const Transform& start : stageLoader->GetPlayerStartTransforms())
		{
			const Matrix markerTransform = Matrix::CreateFromQuaternion(start.rotation) *
				Matrix::CreateTranslation(start.position + Vector3(0.0f, 0.85f, 0.0f));
			Game::Graphics::Instance().GetShapeRenderer()->DrawCapsule(
				markerTransform, 0.3f, 1.1f, Color(1.0f, 0.68f, 0.08f, 0.9f));
		}
	}
	if (showPlayerStartDragPreview)
	{
		const Matrix markerTransform = Matrix::CreateTranslation(
			playerStartDragPreviewPosition + Vector3(0.0f, 0.85f, 0.0f));
		Game::Graphics::Instance().GetShapeRenderer()->DrawCapsule(
			markerTransform, 0.3f, 1.1f, Color(0.2f, 0.85f, 1.0f, 0.75f));
	}
	if (showLightDragPreview)
		Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(
			lightDragPreviewPosition, 0.25f, Color(1.0f, 0.85f, 0.25f, 0.8f));
	RenderPropPreview(rc);
	RenderDragPreview(rc);
}

void VstgEditorScene::RenderPropPreview(const RenderContext& rc)
{
	if (propPreviewRequestPath.empty() || !propPreviewTarget || !propPreviewCamera || !currentStage)
		return;
	if (propPreviewLoadedPath != propPreviewRequestPath)
	{
		const auto found = propModels.find(propPreviewRequestPath);
		propPreviewModel =
			found == propModels.end() || !found->second ? nullptr : found->second->CloneRuntimeInstance(true);
		propPreviewLoadedPath = propPreviewRequestPath;
		propPreviewRenderParams.materials.clear();
	}
	if (!propPreviewModel) return;

	propPreviewModel->UpdateTransform(Matrix::Identity);
	Vector3 boundsMin(FLT_MAX, FLT_MAX, FLT_MAX);
	Vector3 boundsMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	const Matrix renderScale = propPreviewModel->GetRenderScaleTransform();
	for (const VMDLModel::Mesh& mesh : propPreviewModel->GetMeshes())
	{
		if (!mesh.isDraw || !mesh.node) continue;

		const Matrix transform = mesh.node->worldTransform * renderScale;
		for (const VMDLModel::Vertex& vertex : mesh.vertices)
		{
			const Vector3 position = Vector3::Transform(vertex.position, transform);
			boundsMin = Vector3::Min(boundsMin, position);
			boundsMax = Vector3::Max(boundsMax, position);
		}
	}
	if (boundsMin.x == FLT_MAX) return;

	const Vector3 center = (boundsMin + boundsMax) * 0.5f;
	const float radius = std::max(0.1f, (boundsMax - boundsMin).Length() * 0.5f);
	propPreviewCamera->SetLookAt(
		center + Vector3(radius * 1.4f, radius * 0.7f, -radius * 2.5f), center, Vector3::Up);
	propPreviewCamera->SetPerspectiveFov(
		RAD(FreeCameraController::VerticalFovDegrees), 1.0f, std::max(0.01f, radius * 0.01f), radius * 10.0f);

	Game::Graphics& graphics = Game::Graphics::Instance();
	ID3D11DeviceContext* dc = graphics.GetDeviceContext();
	propPreviewTarget->Clear(dc, 0.035f, 0.035f, 0.04f, 1.0f);
	propPreviewDepthTarget->Clear(dc, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	const auto previewOutput = RenderOutput::CaptureOutput(dc);
	RenderOutput::Bind(dc, propPreviewTarget.get(), propPreviewDepthTarget.get(), false);
	RenderContext previewContext = rc;
	previewContext.camera = propPreviewCamera;
	previewContext.depthTarget = propPreviewDepthTarget.get();
	previewContext.lightManager = &propPreviewLights;
	previewContext.shadowMapData = {};
	previewContext.renderSettings.showDebug = false;
	graphics.GetModelRenderer()->Draw(
		ModelShaderId::PBR, propPreviewModel, &propPreviewRenderParams);
	graphics.GetModelRenderer()->Render(previewContext);
	RenderOutput::RestoreOutput(dc, previewOutput);
}

void VstgEditorScene::RenderDragPreview(const RenderContext& rc)
{
	if (!showDragPreview || !dragPreviewModel) return;

	const Matrix modelTransform =
		Matrix::CreateTranslation(dragPreviewModel->GetVmdlExtensionData().rootOffset) *
		dragPreviewTransform.GetMatrix();
	dragPreviewModel->UpdateTransform(modelTransform);
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.GetModelRenderer()->Draw(
		ModelShaderId::PBR, dragPreviewModel, &dragPreviewRenderParams);
	graphics.GetModelRenderer()->Render(rc);
}

void VstgEditorScene::HandleShortcuts()
{
	HandleVisibilityShortcuts();
	auto& io = ImGui::GetIO();
	if (!io.WantTextInput && !io.KeyCtrl && !io.KeyAlt && !io.KeySuper &&
		!ImGui::IsMouseDown(ImGuiMouseButton_Right) && !ImGui::IsMouseDown(ImGuiMouseButton_Middle))
	{
		if (terrain && ImGui::IsKeyPressed(ImGuiKey_B, false))
			use_brush = !use_brush;
	}

	// ファイル操作のショートカット
	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false))
	{
		New();
	}
	else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false))
	{
		Open();
	}
	else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
	{
		if (io.KeyShift) SaveAs();
		else Save();
	}
}

void VstgEditorScene::OnDrawGUI()
{
	PROFILE_FUNCTION();
	HandleShortcuts();
	auto& io = ImGui::GetIO();
	propPreviewRequestPath.clear();
	showDragPreview = false;
	showPlayerStartDragPreview = false;
	showLightDragPreview = false;
	ImGuizmo::BeginFrame();

	// 3Dビューを縮小せず、不透明なパネルを重ねる。
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);

	// メインメニューとファイル情報
	UpdateTitle();
	float mainMenuBarHeight = ImGui::GetFrameHeight();
	if (ImGui::BeginMainMenuBar())
	{
		mainMenuBarHeight = ImGui::GetWindowHeight();
		if (ImGui::BeginMenu(ICON_FA_FILE " ファイル"))
		{
			if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " 開く", "Ctrl+O")) Open();
			if (ImGui::MenuItem(ICON_FA_SAVE " 保存", "Ctrl+S")) Save();
			if (ImGui::MenuItem(ICON_FA_COPY " 名前を付けて保存", "Ctrl+Shift+S")) SaveAs();
			ImGui::Separator();
			if (ImGui::MenuItem(ICON_FA_FILE " 新規作成", "Ctrl+N")) New();
			ImGui::Separator();
			if (ImGui::MenuItem(ICON_FA_SIGN_OUT_ALT " 終了"))
			{
				if (OnRequestExit())
				{
					SceneManager::Instance().LoadScene<TitleScene>();
				}
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(ICON_FA_EYE " 表示"))
		{
			DrawCommonVisibilityGUI();
			ImGui::MenuItem(ICON_FA_BUG " デバッグ表示", "F3", &renderSettings.showDebug);
			ImGui::Checkbox(ICON_FA_SHAPES " コライダー", &renderSettings.showColliderDebug);
			ImGui::Checkbox(ICON_FA_PUZZLE_PIECE " コンポーネント", &renderSettings.showComponentDebug);
			ImGui::Checkbox(ICON_FA_LIGHTBULB " ライト", &renderSettings.showLightDebug);
			ImGui::Checkbox(ICON_FA_PROJECT_DIAGRAM " ナビメッシュ", &renderSettings.showNavMeshDebug);
			ImGui::EndMenu();
		}
		DrawWindowMenu();
		DrawDocumentTitle(path, dirty);
		ImGui::EndMainMenuBar();
	}

	// 左インスペクタ・右ヒエラルキー・下アセット/Terrain。描画自体は全画面のまま。
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	constexpr float gap = 6.0f;
	const ImVec2 workPosition = {viewport->Pos.x + gap, viewport->Pos.y + mainMenuBarHeight + gap};
	const ImVec2 workSize = {std::max(1.0f, viewport->Size.x - gap * 2.0f),
		std::max(1.0f, viewport->Pos.y + viewport->Size.y - workPosition.y - gap)};
	auto& panelLayout = ImGuiRenderer::GetVstgEditorSettings();
	float& leftWidth = panelLayout.inspectorWidth;
	float& rightWidth = panelLayout.hierarchyWidth;
	const float bottomHeight = workSize.y * 0.30f;
	const float topHeight = workSize.y - bottomHeight - gap;
	if (UpdatePanelWidths(workPosition, workSize.x, topHeight, leftWidth, rightWidth))
	{
		panelLayout.loaded = true;
		ImGuiRenderer::SaveSettings();
	}
	const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
		(panelResizeHovered ? ImGuiWindowFlags_NoMouseInputs : 0);

	ImGui::SetNextWindowPos(workPosition, ImGuiCond_Always);
	ImGui::SetNextWindowSize({leftWidth, topHeight}, ImGuiCond_Always);
	if (ImGui::Begin(ICON_FA_COG " インスペクタ###VSTG Inspector", nullptr, windowFlags))
		DrawInspector();
	ImGui::End();

	ImGui::SetNextWindowPos({workPosition.x + workSize.x - rightWidth, workPosition.y}, ImGuiCond_Always);
	ImGui::SetNextWindowSize({rightWidth, topHeight}, ImGuiCond_Always);
	if (ImGui::Begin(ICON_FA_LIST " ヒエラルキー###VSTG Hierarchy", nullptr, windowFlags))
		DrawHierarchy();
	ImGui::End();

	ImGui::SetNextWindowPos({workPosition.x, workPosition.y + topHeight + gap}, ImGuiCond_Always);
	ImGui::SetNextWindowSize({workSize.x, bottomHeight}, ImGuiCond_Always);
	if (ImGui::Begin("ステージ編集###VSTG Bottom", nullptr, windowFlags))
	{
		if (ImGui::BeginTabBar("VSTG Bottom Tabs"))
		{
			if (ImGui::BeginTabItem(ICON_FA_CUBES " アセット"))
			{
				DrawPropBrowser(0.0f);
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem(ICON_FA_MOUNTAIN " テライン地形"))
			{
				if (terrain) DrawTerrainGUI();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem(ICON_FA_WALKING "NavMesh設定"))
			{
				if (navMesh) navMesh->DrawGUI();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem(ICON_FA_STAMP " デカール画像"))
			{
				DrawDecalPalette();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();

	if (InputKit::InputSystem::IsFocusedWindow(true) && !io.WantTextInput && !ImGui::IsAnyItemActive() &&
		!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && !ImGuizmo::IsUsing() &&
		ImGui::IsKeyPressed(ImGuiKey_Delete, false))
	{
		if (GetSelectedLight()) DeleteSelectedLight();
		else if (stageLoader && stageLoader->DeleteSelectedEditorObject()) dirty = true;
	}

	HandleGizmoShortcuts(gizmoOperation);

	// 3Dビュー上部のギズモ切り替え
	const ImVec2 viewportMin = {workPosition.x + leftWidth + gap, workPosition.y};
	const ImVec2 viewportMax = {
		workPosition.x + workSize.x - rightWidth - gap, workPosition.y + topHeight};
	constexpr ImGuiWindowFlags toolbarFlags =
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoCollapse;
	const float toolbarHeight = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;
	ImGui::SetNextWindowPos(viewportMin, ImGuiCond_Always);
	ImGui::SetNextWindowSize({viewportMax.x - viewportMin.x, toolbarHeight}, ImGuiCond_Always);
	if (ImGui::Begin("###VSTG Viewport Toolbar", nullptr, toolbarFlags))
	{
		ImGui::TextUnformatted(ICON_FA_CUBE " 3Dビュー");
		DrawGizmoButtons(gizmoOperation);
		ImGui::SameLine();
		ImGui::TextUnformatted("|");
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_SITEMAP "##VstgSerialViewer"))
		{
			showSerialViewer = true;
			serialViewerDocument = nullptr;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("シリアルビューアー");
	}
	ImGui::End();

	// 不透明パネル上では地形を編集しない。ドラッグ終了後は通常のブラシ操作へ戻す。
	const ImVec2 mouse = ImGui::GetMousePos();
	if (terrain) brushViewportInputAllowed = (
		!panelResizeHovered && mouse.x >= viewportMin.x && mouse.x <= viewportMax.x &&
		mouse.y >= viewportMin.y + toolbarHeight && mouse.y <= viewportMax.y &&
		!ImGui::GetDragDropPayload() && !ImGuizmo::IsUsing() && !ImGui::IsAnyItemActive());

	// 3Dビュー上の配置と選択操作
	DrawPlacementDropTarget({viewportMin.x, viewportMin.y + toolbarHeight}, viewportMax);
	if (effectPopupPending)
	{
		ImGui::OpenPopup("VSTG Effect Placement");
		effectPopupPending = false;
	}
	if (ImGui::BeginPopup("VSTG Effect Placement"))
	{
		if (ImGui::BeginCombo("エフェクト", placementEffectName.empty()
			? "選択してください" : placementEffectName.c_str()))
		{
			for (const auto& candidate : EffectManager::GetEffectPaths())
				if (ImGui::Selectable(candidate.c_str(), candidate == placementEffectName))
					placementEffectName = candidate;
			ImGui::EndCombo();
		}
		ImGui::BeginDisabled(placementEffectName.empty());
		if (ImGui::Button("配置") && stageLoader)
		{
			stageLoader->AddEditorEffect(placementEffectName, effectPlacementPosition);
			if (terrain) use_brush = false;
			selectedLightType = EditorLightType::None;
			selectedLightIndex = -1;
			lightingSettingsSelected = false;
			dirty = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("キャンセル")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	DrawLightMarkers({viewportMin.x, viewportMin.y + toolbarHeight}, viewportMax);
	DrawObjectGizmo({viewportMin.x, viewportMin.y + toolbarHeight}, viewportMax);
	if (stageLoader)
		stageLoader->DrawSpawnTags({viewportMin.x, viewportMin.y + toolbarHeight},
			{viewportMax.x, viewportMax.y});

	// UI編集後の変更検出
	if (!dirty && (ImGui::IsAnyItemActive() || ImGui::IsMouseReleased(ImGuiMouseButton_Left)) &&
		cleanStateHash != data.BuildEditorStateHash(
							  *terrain, *navMesh, *stageLoader, currentStage->GetLightManager()))
		dirty = true;
	ImGui::PopStyleVar();
	DrawSerialViewer();
}

void VstgEditorScene::DrawDecalPalette()
{
	const bool reload = ImGui::Button(ICON_FA_SYNC_ALT " 再読み込み##DecalTextures");
	if (!decalTexturesLoaded || reload)
	{
		if (reload && !ResourceManager::Instance().RefreshResources())
			ImGuiToolKit::Alert("デカール画像の再読み込みに失敗しました。");
		else
		{
			decalTextures.clear();
			const auto resourceRoot = ResourceManager::GetResourceRoot();
			std::vector<std::filesystem::path> paths;
			std::error_code error;
			for (std::filesystem::directory_iterator file(resourceRoot / "Terrain/Decals", error), end;
				file != end && !error; file.increment(error))
			{
				if (!file->is_regular_file(error)) continue;
				std::string extension = file->path().extension().string();
				std::transform(extension.begin(), extension.end(), extension.begin(),
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
					extension == ".tga" || extension == ".bmp" || extension == ".dds")
					paths.push_back(file->path());
			}
			std::sort(paths.begin(), paths.end());
			for (const auto& file : paths)
			{
				auto source = file;
				source.replace_extension(".png");
				if (file.extension() == ".dds" && std::filesystem::exists(source)) continue;

				const std::string imagePath = (std::filesystem::path("Resources") /
					file.lexically_relative(resourceRoot)).generic_string();
				try
				{
					auto texture = ResourceManager::Instance().LoadTexture(imagePath);
					if (texture) decalTextures.emplace_back(imagePath, std::move(texture));
				}
				catch (const std::exception&)
				{
					ImGuiToolKit::Alert("デカール画像を読み込めませんでした: " + imagePath);
				}
			}
			if (std::none_of(decalTextures.begin(), decalTextures.end(), [this](const auto& entry) {
				return entry.first == selectedDecalTexturePath;
			})) selectedDecalTexturePath.clear();
			decalTexturesLoaded = true;
		}
	}

	const float thumbnailSize = 72.0f;
	const float childHeight = thumbnailSize + ImGui::GetStyle().ScrollbarSize +
		ImGui::GetStyle().WindowPadding.y * 2.0f;
	ImGui::BeginChild("decal texture palette", ImVec2(0.0f, childHeight), true,
		ImGuiWindowFlags_HorizontalScrollbar);
	for (size_t i = 0; i < decalTextures.size(); ++i)
	{
		const auto& [imagePath, texture] = decalTextures[i];
		ImGui::PushID(imagePath.c_str());
		const bool selected = imagePath == selectedDecalTexturePath;
		if (selected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.95f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.65f, 1.0f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.85f, 1.0f));
		}
		if (ImGui::ImageButton("decal texture", texture->GetShaderResourceView().Get(),
			ImVec2(thumbnailSize, thumbnailSize))) selectedDecalTexturePath = imagePath;
		if (selected) ImGui::PopStyleColor(3);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%d x %d",
			imagePath.c_str(), texture->GetWidth(), texture->GetHeight());
		ImGui::PopID();
		if (i + 1 < decalTextures.size()) ImGui::SameLine();
	}
	ImGui::EndChild();
}

Light* VstgEditorScene::GetSelectedLight()
{
	if (!currentStage || selectedLightIndex < 0) return nullptr;

	auto& lights = currentStage->GetLightManager();
	const auto get = [this](auto&& values) -> Light* {
		if (selectedLightIndex >= static_cast<int>(values.size())) return nullptr;
		Light* light = &values[selectedLightIndex];
		return light->IsPendingDestroy() ? nullptr : light;
	};
	switch (selectedLightType)
	{
	case EditorLightType::Directional:
		return get(lights.GetDirectionalLights());
	case EditorLightType::Point:
		return get(lights.GetPointLights());
	case EditorLightType::Spot:
		return get(lights.GetSpotLights());
	case EditorLightType::Area:
		return get(lights.GetAreaLights());
	default:
		return nullptr;
	}
}

void VstgEditorScene::SelectLight(EditorLightType type, int index)
{
	selectedLightType = type;
	selectedLightIndex = index;
	lightingSettingsSelected = false;
	if (stageLoader) stageLoader->ClearEditorSelection();
	if (terrain) use_brush = false;
}

void VstgEditorScene::DrawHierarchy()
{
	PROFILE_FUNCTION();
	if (!currentStage) return;
	if (ImGui::Selectable(ICON_FA_SUN " 環境・ライト設定", lightingSettingsSelected))
	{
		lightingSettingsSelected = true;
		selectedLightType = EditorLightType::None;
		selectedLightIndex = -1;
		if (stageLoader) stageLoader->ClearEditorSelection();
	}
	ImGui::Separator();
	if (stageLoader && stageLoader->DrawEditorHierarchy())
	{
		if (terrain) use_brush = false;
		selectedLightType = EditorLightType::None;
		selectedLightIndex = -1;
		lightingSettingsSelected = false;
	}
	auto& lights = currentStage->GetLightManager();
	ImGui::Text("ライト数: %d / %d", lights.GetLightCount(), CbLightData::MaxLights);
	if (!lights.CanAddLight()) ImGui::TextDisabled("ライト数の限界です。");
	const auto group = [this](const char* icon, const char* label, auto&& values, EditorLightType type) {
		int count = 0;
		for (const auto& light : values)
			if (!light.IsPendingDestroy()) ++count;
		const std::string category = std::string(icon) + " " + label + " (" +
			std::to_string(count) + ")###" + label;
		if (!ImGui::TreeNodeEx(category.c_str(),
			ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) return;
		for (int index = 0; index < static_cast<int>(values.size()); ++index)
		{
			if (values[index].IsPendingDestroy()) continue;
			ImGui::PushID(index);
			const std::string name = std::string(icon) + " " + values[index].GetName();
			if (ImGui::Selectable(name.c_str(), selectedLightType == type && selectedLightIndex == index))
				SelectLight(type, index);
			ImGui::PopID();
		}
		ImGui::TreePop();
	};
	group(ICON_FA_SUN, "ディレクショナルライト",
		lights.GetDirectionalLights(), EditorLightType::Directional);
	group(ICON_FA_LIGHTBULB, "ポイントライト",
		lights.GetPointLights(), EditorLightType::Point);
	group(ICON_FA_LIGHTBULB, "スポットライト",
		lights.GetSpotLights(), EditorLightType::Spot);
	group(ICON_FA_LIGHTBULB, "エリアライト",
		lights.GetAreaLights(), EditorLightType::Area);
}

void VstgEditorScene::DrawInspector()
{
	PROFILE_FUNCTION();
	if (!currentStage) return;
	if (lightingSettingsSelected)
	{
		currentStage->GetLightManager().DrawGUI();
		ImGui::SeparatorText("IBL・スカイボックス");
		Game::Graphics::Instance().DrawSkyMapGUI();
		Game::Graphics::Instance().GetSkyBoxRenderer()->DrawGUI();
		return;
	}
	if (Light* light = GetSelectedLight())
	{
		ImGui::PushID(light);
		std::string name = light->GetName();
		if (ImGui::InputText("名前", &name)) { light->SetName(name); dirty = true; }
		light->DrawGUI();
		if (ImGui::Button(ICON_FA_TRASH " 削除")) DeleteSelectedLight();
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_COPY " 複製")) DuplicateSelectedLight();
		ImGui::PopID();
		return;
	}
	if (stageLoader) stageLoader->DrawSelectedEditorInspector();
}

void VstgEditorScene::DuplicateSelectedLight()
{
	Light* source = GetSelectedLight();
	if (!source || !currentStage) return;

	auto& lights = currentStage->GetLightManager();
	Light* copy = nullptr;
	int index = -1;
	if (selectedLightType == EditorLightType::Directional)
	{
		copy = lights.AddDirectionalLight();
		index = static_cast<int>(lights.GetDirectionalLights().size()) - 1;
	}
	else if (selectedLightType == EditorLightType::Point)
	{
		const float range = static_cast<PointLight*>(source)->GetRange();
		auto* addedLight = lights.AddPointLight();
		if (!addedLight) return;

		auto& light = *addedLight;
		light.SetRange(range);
		copy = &light;
		index = static_cast<int>(lights.GetPointLights().size()) - 1;
	}
	else if (selectedLightType == EditorLightType::Spot)
	{
		const auto* original = static_cast<SpotLight*>(source);
		auto* addedLight = lights.AddSpotLight();
		if (!addedLight) return;

		auto& light = *addedLight;
		light.SetRange(original->GetRange());
		light.SetInnerConeAngle(original->GetInnerConeAngle());
		light.SetOuterConeAngle(original->GetOuterConeAngle());
		copy = &light;
		index = static_cast<int>(lights.GetSpotLights().size()) - 1;
	}
	else if (selectedLightType == EditorLightType::Area)
	{
		const auto* original = static_cast<AreaLight*>(source);
		auto* addedLight = lights.AddAreaLight();
		if (!addedLight) return;

		auto& light = *addedLight;
		light.SetRange(original->GetRange());
		light.SetWidth(original->GetWidth());
		light.SetHeight(original->GetHeight());
		copy = &light;
		index = static_cast<int>(lights.GetAreaLights().size()) - 1;
	}
	if (!copy) return;
	copy->SetName(source->GetName() + " コピー");
	copy->SetTag(source->GetTag());
	copy->SetActive(source->IsActive());
	copy->SetColor(source->GetColor());
	copy->SetIntensity(source->GetIntensity());
	copy->transform = source->transform;
	SelectLight(selectedLightType, index);
	dirty = true;
}

void VstgEditorScene::DeleteSelectedLight()
{
	Light* light = GetSelectedLight();
	if (!light) return;

	// 他のObjectと同じ破棄予約を使い、コンポーネントを持つライトを移動代入しない。
	light->Destroy();
	selectedLightType = EditorLightType::None;
	selectedLightIndex = -1;
	dirty = true;
}

// ライトはコライダーを持たないため、常にクリックできる画面上のマーカーを表示する。
void VstgEditorScene::DrawLightMarkers(const ImVec2& viewportMin, const ImVec2& viewportMax)
{
	if (!currentStage) return;
	Camera* camera = currentStage->GetActiveCamera();
	if (!camera) return;

	const Matrix viewProjection = camera->GetView() * camera->GetProjection();
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImDrawList* draw = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
	draw->PushClipRect(viewportMin, viewportMax, true);
	const auto markers = [&](auto&& values, EditorLightType type) {
		for (int index = 0; index < static_cast<int>(values.size()); ++index)
		{
			const auto& light = values[index];
			if (light.IsPendingDestroy()) continue;

			const Vector3 projected = Vector3::Transform(light.transform.position, viewProjection);
			if (projected.z < 0.0f || projected.z > 1.0f) continue;

			const ImVec2 screen = {viewport->Pos.x + (projected.x + 1.0f) * 0.5f * Game::Graphics::ScreenWidth,
				viewport->Pos.y + (1.0f - projected.y) * 0.5f * Game::Graphics::ScreenHeight};
			const bool selected = selectedLightType == type && selectedLightIndex == index;
			draw->AddCircleFilled(screen, selected ? 8.0f : 6.0f, selected ? IM_COL32(70, 190, 255, 255) : IM_COL32(255, 205, 65, 255));
			draw->AddText(ImVec2(screen.x + 10.0f, screen.y - 8.0f), IM_COL32(255, 225, 120, 255), ICON_FA_LIGHTBULB);
		}
	};
	auto& lights = currentStage->GetLightManager();
	markers(lights.GetDirectionalLights(), EditorLightType::Directional);
	markers(lights.GetPointLights(), EditorLightType::Point);
	markers(lights.GetSpotLights(), EditorLightType::Spot);
	markers(lights.GetAreaLights(), EditorLightType::Area);
	draw->PopClipRect();
}

void VstgEditorScene::RefreshPropModels()
{
	if (!ResourceManager::Instance().RefreshResources())
	{
		ImGuiToolKit::Alert("Runtime resource refresh failed.");
		return;
	}
	propPreviewModel.reset();
	propPreviewLoadedPath.clear();
	propModelPaths.clear();
	propModels.clear();
	const std::filesystem::path resourceRoot = ResourceManager::GetResourceRoot();
	const std::filesystem::path modelRoot = resourceRoot / "Model";
	std::error_code error;
	for (std::filesystem::recursive_directory_iterator file(modelRoot, error), end;
		file != end && !error; file.increment(error))
	{
		if (!file->is_regular_file()) continue;
		std::string extension = file->path().extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(),
			[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		if (extension != ".vmdl") continue;

		const std::filesystem::path relativePath = file->path().lexically_relative(resourceRoot);
		const std::string modelPath =
			(std::filesystem::path("Resources") / relativePath).generic_string();
		std::shared_ptr<VMDLModel> model;
		try
		{
			model = ResourceManager::Instance().LoadModel(modelPath, true);
		}
		catch (const std::exception&)
		{
			continue;
		}
		if (!model) continue;
		propModelPaths.push_back(modelPath);
		propModels.emplace(modelPath, std::move(model));
	}
	std::sort(propModelPaths.begin(), propModelPaths.end());
}

void VstgEditorScene::DrawPropBrowser(float height)
{
	// VMDLの検索と再読み込み
	ImGui::SetNextItemWidth(-38.0f);
	ImGui::InputTextWithHint("##VSTG Prop Search", "検索", &propSearch);
	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_SYNC_ALT "##Refresh VSTG Props")) RefreshPropModels();
	ImGui::Separator();
	ImGui::BeginChild("##VSTG Prop Browser List", ImVec2(0.0f, height), true);

	constexpr ImVec2 tileSize(144.0f, 84.0f);
	const auto nextTile = [&] {
		const float nextRight = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + tileSize.x;
		const float contentRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
		if (nextRight <= contentRight) ImGui::SameLine();
	};
	const auto specialTile = [&](const char* label, const char* payloadType, int value) {
		ImGui::Button(label, tileSize);
		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload(payloadType, &value, sizeof(value));
			ImGui::TextUnformatted(label);
			ImGui::TextDisabled("ステージ上へドロップして配置");
			ImGui::EndDragDropSource();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("ステージ上へドラッグして配置");
		nextTile();
	};
	specialTile(ICON_FA_MAP_MARKER_ALT "\n" "プレイヤー初期位置", "VSTG_PLAYER_START", 0);
	specialTile(ICON_FA_SUN "\n" "ディレクショナルライト", "VSTG_LIGHT", static_cast<int>(EditorLightType::Directional));
	specialTile(ICON_FA_LIGHTBULB "\n" "ポイントライト", "VSTG_LIGHT", static_cast<int>(EditorLightType::Point));
	specialTile(ICON_FA_LIGHTBULB "\n" "スポットライト", "VSTG_LIGHT", static_cast<int>(EditorLightType::Spot));
	specialTile(ICON_FA_LIGHTBULB "\n" "エリアライト", "VSTG_LIGHT", static_cast<int>(EditorLightType::Area));
	specialTile(ICON_FA_MAGIC "\n" "エフェクト", "VSTG_EFFECT", 0);
	specialTile(ICON_FA_COG "\n" "侵入不可エリア", "VSTG_BLOCKED_AREA", 0);

	// 検索結果のVMDL一覧
	std::string search = propSearch;
	std::transform(search.begin(), search.end(), search.begin(),
		[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
	int visibleIndex = 0;
	for (const std::string& modelPath : propModelPaths)
	{
		std::string searchable = modelPath;
		std::transform(searchable.begin(), searchable.end(), searchable.begin(),
			[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		if (!search.empty() && searchable.find(search) == std::string::npos) continue;

		ImGui::PushID(modelPath.c_str());
		const std::string name = std::filesystem::path(modelPath).stem().string();
		const std::string label = std::string(ICON_FA_CUBE "\n") + name;
		ImGui::Button(label.c_str(), tileSize);
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			ImGui::PopID();
			ImGui::EndChild();
			OpenVmdlEditor(modelPath);
			return;
		}

		// ステージへ配置するドラッグデータ
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
		{
			ImGui::SetDragDropPayload("VSTG_PROP", modelPath.c_str(), modelPath.size() + 1);
			ImGui::TextUnformatted(name.c_str());
			ImGui::EndDragDropSource();
		}

		// VMDLのプレビューツールチップ
		if (ImGui::IsItemHovered())
		{
			propPreviewRequestPath = modelPath;
			if (ImGui::BeginTooltip())
			{
				ImGui::TextUnformatted(name.c_str());
				if (propPreviewLoadedPath == modelPath && propPreviewModel && propPreviewTarget)
					ImGui::Image(propPreviewTarget->GetSRV(), ImVec2(192.0f, 192.0f));
				else ImGui::TextDisabled("プレビューを読み込み中");
				ImGui::TextDisabled("%s", modelPath.c_str());
				ImGui::EndTooltip();
			}
		}
		ImGui::PopID();
		nextTile();
		++visibleIndex;
	}
	ImGui::NewLine();
	if (visibleIndex == 0) ImGui::TextDisabled("該当するVMDLはありません");
	ImGui::EndChild();
}

bool VstgEditorScene::OpenVmdlEditor(const std::string& modelPath)
{
	if (!OnRequestExit()) return false;

	const auto editorPath = ResourceManager::Instance().ResolveEditPath(modelPath, ResourceManager::ResourceType::Model);
	return SceneManager::Instance().LoadScene<VmdlEditorScene>(editorPath);
}

void VstgEditorScene::DrawPlacementDropTarget(
	const ImVec2& viewportMin, const ImVec2& viewportMax)
{
	// ドラッグ中の接地面と配置プレビュー
	const ImGuiPayload* draggingPayload = ImGui::GetDragDropPayload();
	const bool draggingProp = draggingPayload && draggingPayload->IsDataType("VSTG_PROP");
	const bool draggingPlayerStart =
		draggingPayload && draggingPayload->IsDataType("VSTG_PLAYER_START");
	const bool draggingLight = draggingPayload && draggingPayload->IsDataType("VSTG_LIGHT");
	const bool draggingBlockedArea = draggingPayload && draggingPayload->IsDataType("VSTG_BLOCKED_AREA");
	const bool draggingEffect = draggingPayload && draggingPayload->IsDataType("VSTG_EFFECT");
	if (!draggingProp && !draggingPlayerStart && !draggingLight && !draggingBlockedArea && !draggingEffect) return;

	const std::string modelPath = draggingProp
		? static_cast<const char*>(draggingPayload->Data) : std::string{};
	Vector3 surfacePoint;
	Vector3 surfaceNormal;
	const ImVec2 mouse = ImGui::GetMousePos();
	const bool insideViewport = mouse.x >= viewportMin.x && mouse.x <= viewportMax.x &&
		mouse.y >= viewportMin.y && mouse.y <= viewportMax.y;
	const bool hasSurfacePoint =
		insideViewport && stageLoader && ScreenToPlacementSurface(mouse, surfacePoint, surfaceNormal);
	if (hasSurfacePoint)
	{
		if (draggingPlayerStart)
		{
			playerStartDragPreviewPosition = surfacePoint;
			showPlayerStartDragPreview = true;
		}
		else if (draggingLight || draggingEffect)
		{
			lightDragPreviewPosition = surfacePoint + Vector3::Up;
			showLightDragPreview = true;
		}
		else if (draggingProp && dragPreviewModelPath != modelPath)
		{
			const auto found = propModels.find(modelPath);
			dragPreviewModel =
				found == propModels.end() || !found->second ? nullptr : found->second->CloneRuntimeInstance(true);
			dragPreviewModelPath = modelPath;
			dragPreviewRenderParams.materials.clear();
			if (dragPreviewModel)
			{
				for (const VMDLModel::Material& material : dragPreviewModel->GetMaterials())
				{
					PBRMaterialParams& params = dragPreviewRenderParams.materials[material.name];
					params.baseColor = Color(0.15f, 0.65f, 1.0f, 0.45f);
					params.emissionColor = Color(0.05f, 0.2f, 0.35f, 1.0f);
					params.metalness = 0.0f;
					params.roughness = 1.0f;
					params.shadowStrength = 0.0f;
					params.useBaseColorTexture = false;
				}
			}
		}
		if (draggingProp)
		{
			dragPreviewTransform.rotation =
				AlignPropRotationToSurface(Quaternion::Identity, surfaceNormal);
			showDragPreview =
				dragPreviewModel && stageLoader->BuildEditorPropTransform(
					*dragPreviewModel, surfacePoint, dragPreviewTransform);
		}
	}

	if (terrain) brushViewportInputAllowed = false;

	// 3Dビュー全体をドロップ領域として受け付ける
	ImGui::SetNextWindowPos(viewportMin, ImGuiCond_Always);
	ImGui::SetNextWindowSize(
		{viewportMax.x - viewportMin.x, viewportMax.y - viewportMin.y}, ImGuiCond_Always);
	ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
	constexpr ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	if (ImGui::Begin("##VSTG Prop Drop Target", nullptr, flags))
	{
		ImGui::InvisibleButton("##VSTG Surface Drop", ImGui::GetContentRegionAvail());
		if (terrain) brushViewportInputAllowed = false;
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("VSTG_PROP"))
			{
				if (payload->IsDelivery() && showDragPreview)
				{
					if (stageLoader->AddEditorProp(static_cast<const char*>(payload->Data),
						surfacePoint, dragPreviewTransform.rotation))
					{
						if (terrain) use_brush = false;
						selectedLightType = EditorLightType::None;
						selectedLightIndex = -1;
						lightingSettingsSelected = false;
						dirty = true;
					}
					else if (!stageLoader->GetError().empty()) ImGuiToolKit::Alert(stageLoader->GetError());
				}
			}
			if (const ImGuiPayload* payload =
					ImGui::AcceptDragDropPayload("VSTG_PLAYER_START"))
			{
				if (payload->IsDelivery() && hasSurfacePoint && stageLoader)
				{
					stageLoader->SetEditorPlayerStart(surfacePoint);
					if (terrain) use_brush = false;
					selectedLightType = EditorLightType::None;
					selectedLightIndex = -1;
					lightingSettingsSelected = false;
					dirty = true;
				}
			}
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("VSTG_EFFECT"))
			{
				if (payload->IsDelivery() && hasSurfacePoint)
				{
					effectPlacementPosition = surfacePoint;
					placementEffectName.clear();
					effectPopupPending = true;
				}
			}
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("VSTG_LIGHT"))
			{
				if (payload->IsDelivery() && hasSurfacePoint && currentStage && payload->DataSize == sizeof(int))
				{
					const auto type = static_cast<EditorLightType>(*static_cast<const int*>(payload->Data));
					auto& lights = currentStage->GetLightManager();
					Light* light = nullptr;
					int index = -1;
					if (type == EditorLightType::Directional)
					{
						light = lights.AddDirectionalLight();
						index = static_cast<int>(lights.GetDirectionalLights().size()) - 1;
					}
					else if (type == EditorLightType::Point)
					{
						light = lights.AddPointLight();
						index = static_cast<int>(lights.GetPointLights().size()) - 1;
					}
					else if (type == EditorLightType::Spot)
					{
						light = lights.AddSpotLight();
						index = static_cast<int>(lights.GetSpotLights().size()) - 1;
					}
					else if (type == EditorLightType::Area)
					{
						light = lights.AddAreaLight();
						index = static_cast<int>(lights.GetAreaLights().size()) - 1;
					}
					if (light)
					{
						light->transform.position = lightDragPreviewPosition;
						SelectLight(type, index);
						dirty = true;
					}
				}
			}
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("VSTG_BLOCKED_AREA"))
			{
				if (payload->IsDelivery() && hasSurfacePoint && stageLoader)
				{
					stageLoader->AddEditorBlockedArea();
					if (terrain) use_brush = false;
					if (Transform* transform = stageLoader->GetSelectedEditorTransform())
					{
						transform->position = surfacePoint;
						stageLoader->RefreshSelectedEditorObject();
					}
					selectedLightType = EditorLightType::None;
					selectedLightIndex = -1;
					lightingSettingsSelected = false;
					dirty = true;
				}
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::GetWindowDrawList()->AddRect(viewportMin, viewportMax,
			ImGui::ColorConvertFloat4ToU32(ImGuiToolKit::Theme::SelectedBorder), 0.0f, 0, 2.0f);
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

bool VstgEditorScene::ScreenToPlacementSurface(const ImVec2& mousePosition,
	Vector3& point, Vector3& normal, int ignoredPropIndex) const
{
	if (!currentStage || !stageLoader) return false;
	Camera* camera = currentStage->GetActiveCamera();
	if (!camera) return false;

	const Vector2 ndc = Game::Graphics::Instance().GetMouseNDC(mousePosition.x, mousePosition.y);
	const Matrix inverse = (camera->GetView() * camera->GetProjection()).Invert();
	const Vector3 origin = Vector3::Transform(Vector3(ndc.x, ndc.y, 0.0f), inverse);
	Vector3 direction = Vector3::Transform(Vector3(ndc.x, ndc.y, 1.0f), inverse) - origin;
	if (direction.LengthSquared() < 0.000001f) return false;
	direction.Normalize();
	bool found = ScreenToTerrainPoint(mousePosition, point, &normal);
	float distance = found ? (point - origin).Length() : 10000.0f;
	if (stageLoader->RaycastEditorProps(origin, direction, distance, normal, ignoredPropIndex) >= 0)
	{
		point = origin + direction * distance;
		found = true;
	}
	return found;
}

bool VstgEditorScene::ScreenToTerrainPoint(
	const ImVec2& mousePosition, Vector3& point, Vector3* normal) const
{
	if (!currentStage || !terrain) return false;

	Camera* camera = currentStage->GetActiveCamera();
	if (!camera) return false;

	// マウス座標からレイを生成
	const Vector2 mouseNdc =
		Game::Graphics::Instance().GetMouseNDC(
			mousePosition.x, mousePosition.y);

	const Matrix inverseViewProjection =
		(camera->GetView() * camera->GetProjection()).Invert();

	const Vector3 nearPoint = Vector3::Transform(
		Vector3(mouseNdc.x, mouseNdc.y, 0.0f), inverseViewProjection);
	const Vector3 farPoint = Vector3::Transform(
		Vector3(mouseNdc.x, mouseNdc.y, 1.0f), inverseViewProjection);

	// 地形のローカル座標へレイを移し、拡大や回転を判定に反映する。
	const Matrix terrainWorld =
		currentStage->GetTransform()->GetMatrix();

	const Matrix inverseTerrainWorld = terrainWorld.Invert();

	const Vector3 localOrigin =
		Vector3::Transform(nearPoint, inverseTerrainWorld);

	Vector3 localDirection =
		Vector3::TransformNormal(
			farPoint - nearPoint, inverseTerrainWorld);

	if (localDirection.LengthSquared() < 0.000001f)
		return false;

	localDirection.Normalize();

	// XZ平面上でテラインへの進入・退出距離を求める
	const float terrainSize = terrain->GetTerrainSize();
	const float halfSize = terrainSize * 0.5f;

	float entryDistance = 0.0f;
	float exitDistance = 10000.0f;

	const float axisOrigins[] = {
		localOrigin.x, localOrigin.z};

	const float axisDirections[] = {
		localDirection.x, localDirection.z};

	for (int axis = 0; axis < 2; ++axis)
	{
		if (axisDirections[axis] == 0.0f)
		{
			if (axisOrigins[axis] < -halfSize ||
				axisOrigins[axis] > halfSize)
			{
				return false;
			}

			continue;
		}

		float nearDistance =
			(-halfSize - axisOrigins[axis]) /
			axisDirections[axis];

		float farDistance =
			(halfSize - axisOrigins[axis]) /
			axisDirections[axis];

		if (nearDistance > farDistance)
			std::swap(nearDistance, farDistance);

		entryDistance = std::max(entryDistance, nearDistance);
		exitDistance = std::min(exitDistance, farDistance);

		if (entryDistance > exitDistance)
			return false;
	}

	// テッセレーションを考慮したグリッド分割数
	const int tessellationFactor = std::max(
		static_cast<int>(std::ceil(std::max(
			terrain->GetTessellationEdgeFactor(),
			terrain->GetTessellationInnerFactor()))),
		1);

	const int segments = std::max(
		terrain->GetGridResolution() * tessellationFactor, 1);

	const float cellSize = terrainSize / segments;

	// レイの進入位置から最初のグリッドを特定
	const Vector3 entryPoint =
		localOrigin + localDirection * entryDistance;

	const float gridX =
		(entryPoint.x + halfSize) / cellSize;

	const float gridZ =
		(entryPoint.z + halfSize) / cellSize;

	int x = static_cast<int>(std::floor(gridX));
	int z = static_cast<int>(std::floor(gridZ));

	// 境界上から負方向に進む場合は隣のセルを選ぶ
	if (localDirection.x < 0.0f && gridX == std::floor(gridX))
		--x;

	if (localDirection.z < 0.0f && gridZ == std::floor(gridZ))
		--z;

	x = std::clamp(x, 0, segments - 1);
	z = std::clamp(z, 0, segments - 1);

	int stepX = 0;
	int stepZ = 0;

	if (localDirection.x > 0.0f) stepX = 1;
	if (localDirection.x < 0.0f) stepX = -1;

	if (localDirection.z > 0.0f) stepZ = 1;
	if (localDirection.z < 0.0f) stepZ = -1;

	const float infinity =
		std::numeric_limits<float>::infinity();

	float nextX = infinity;
	float nextZ = infinity;

	float deltaX = infinity;
	float deltaZ = infinity;

	// 次のグリッド境界までの距離と、1セル分の距離
	if (stepX != 0)
	{
		int boundaryX = x;

		if (stepX > 0)
			++boundaryX;

		const float edgeX =
			-halfSize + boundaryX * cellSize;

		nextX =
			(edgeX - localOrigin.x) / localDirection.x;

		deltaX = cellSize / std::abs(localDirection.x);
	}

	if (stepZ != 0)
	{
		int boundaryZ = z;

		if (stepZ > 0)
			++boundaryZ;

		const float edgeZ =
			-halfSize + boundaryZ * cellSize;

		nextZ =
			(edgeZ - localOrigin.z) / localDirection.z;

		deltaZ = cellSize / std::abs(localDirection.z);
	}

	const DirectX::SimpleMath::Ray localRay(localOrigin, localDirection);
	float cellEntry = entryDistance;

	// 2D DDAでレイが通過するグリッドのみ探索
	while (x >= 0 && x < segments &&
		   z >= 0 && z < segments &&
		   cellEntry <= exitDistance)
	{
		const float nextBoundary = std::min(nextX, nextZ);

		const float cellExit =
			std::min(nextBoundary, exitDistance);

		const float u0 = static_cast<float>(x) / segments;
		const float u1 = static_cast<float>(x + 1) / segments;
		const float v0 = static_cast<float>(z) / segments;
		const float v1 = static_cast<float>(z + 1) / segments;

		const float h00 = terrain->GetHeightByUV(u0, v0);
		const float h10 = terrain->GetHeightByUV(u1, v0);
		const float h01 = terrain->GetHeightByUV(u0, v1);
		const float h11 = terrain->GetHeightByUV(u1, v1);

		// グリッド内の地形の高さの範囲
		const float terrainMinY =
			std::min({h00, h10, h01, h11});

		const float terrainMaxY =
			std::max({h00, h10, h01, h11});

		// グリッド通過中のレイの高さの範囲
		const float enterY =
			localOrigin.y + localDirection.y * cellEntry;

		const float exitY =
			localOrigin.y + localDirection.y * cellExit;

		const float rayMinY = std::min(enterY, exitY);
		const float rayMaxY = std::max(enterY, exitY);

		// 高さの範囲が重なる場合のみ三角形との交差判定
		if (rayMinY <= terrainMaxY &&
			rayMaxY >= terrainMinY)
		{
			const Vector3 p00(
				(u0 - 0.5f) * terrainSize,
				h00,
				(v0 - 0.5f) * terrainSize);

			const Vector3 p10(
				(u1 - 0.5f) * terrainSize,
				h10,
				(v0 - 0.5f) * terrainSize);

			const Vector3 p01(
				(u0 - 0.5f) * terrainSize,
				h01,
				(v1 - 0.5f) * terrainSize);

			const Vector3 p11(
				(u1 - 0.5f) * terrainSize,
				h11,
				(v1 - 0.5f) * terrainSize);

			float distance0 = 0.0f;
			float distance1 = 0.0f;

			bool hit0 = localRay.Intersects(
				p00, p01, p10, distance0);

			bool hit1 = localRay.Intersects(
				p10, p01, p11, distance1);

			constexpr float hitEpsilon = 0.0001f;

			hit0 = hit0 &&
				   distance0 >= cellEntry - hitEpsilon &&
				   distance0 <= cellExit + hitEpsilon;

			hit1 = hit1 &&
				   distance1 >= cellEntry - hitEpsilon &&
				   distance1 <= cellExit + hitEpsilon;

			if (hit0 || hit1)
			{
				float hitDistance = distance0;

				if (!hit0 || (hit1 && distance1 < distance0))
					hitDistance = distance1;

				const Vector3 localPoint =
					localOrigin + localDirection * hitDistance;

				point = Vector3::Transform(
					localPoint, terrainWorld);

				if (normal)
				{
					const float u =
						localPoint.x / terrainSize + 0.5f;

					const float v =
						localPoint.z / terrainSize + 0.5f;

					*normal = Vector3::TransformNormal(
						terrain->GetSurfaceNormalByUV(u, v),
						inverseTerrainWorld.Transpose());

					normal->Normalize();
				}

				return true;
			}
		}

		if (nextBoundary >= exitDistance)
			break;

		// 先に到達する境界の方向へグリッドを移動
		const bool moveX = nextX <= nextZ;
		const bool moveZ = nextZ <= nextX;

		cellEntry = nextBoundary;

		if (moveX)
		{
			x += stepX;
			nextX += deltaX;
		}

		if (moveZ)
		{
			z += stepZ;
			nextZ += deltaZ;
		}
	}

	return false;
}

void VstgEditorScene::DrawObjectGizmo(const ImVec2& viewportMin, const ImVec2& viewportMax)
{
	if (terrain && use_brush) return;
	if (ImGui::GetDragDropPayload()) return;
	if (!stageLoader || !currentStage) return;

	// ImGuizmo自身もWantCaptureMouseを設定するため、実際のUI操作だけを調べる。
	const bool uiCapturesMouse = ImGui::IsAnyItemActive() ||
		ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
		ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
	if (uiCapturesMouse && !ImGuizmo::IsUsing())
	{
		terrainSnapActive = false;
		return;
	}
	bool gizmoHovered = false;
	Camera* camera = currentStage->GetActiveCamera();
	Light* selectedLight = GetSelectedLight();
	Transform* transform = selectedLight ? &selectedLight->transform : stageLoader->GetSelectedEditorTransform();
	if (camera && transform)
	{
		Matrix view = camera->GetView();
		Matrix projection = camera->GetProjection();
		Matrix world = transform->GetMatrix();
		ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList(ImGui::GetMainViewport()));
		const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
		ImGuizmo::SetRect(mainViewport->Pos.x, mainViewport->Pos.y,
			Game::Graphics::ScreenWidth, Game::Graphics::ScreenHeight);
		const ImVec2 mouse = ImGui::GetMousePos();
		const bool insideViewport = mouse.x >= viewportMin.x && mouse.x <= viewportMax.x &&
			mouse.y >= viewportMin.y && mouse.y <= viewportMax.y;
		ImGuizmo::Enable(insideViewport || ImGuizmo::IsUsing());
		ImDrawList* draw = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
		draw->PushClipRect(viewportMin, viewportMax, true);
		const bool manipulated = ImGuizmo::Manipulate(&view._11, &projection._11,
			static_cast<ImGuizmo::OPERATION>(gizmoOperation), ImGuizmo::LOCAL, &world._11);
		draw->PopClipRect();
		gizmoHovered = ImGuizmo::IsOver(ImGuizmo::UNIVERSAL) ||
			ImGuizmo::IsOver(&world._41, 24.0f);
		ImGuizmo::Enable(true);
		if (!selectedLight && gizmoOperation == ImGuizmo::TRANSLATE &&
			ImGuizmo::GetActiveHandleType() == ImGuizmo::MT_MOVE_SCREEN)
		{
			terrainSnapActive = true;
		}
		if (manipulated)
		{
			world.Decompose(transform->scale, transform->rotation, transform->position);
			if (!selectedLight && terrainSnapActive)
			{
				Vector3 surfacePoint;
				Vector3 surfaceNormal;
				const int ignoredPropIndex =
					stageLoader->GetSelectedEditorObjectType() == StageLoader::EditorObjectType::Prop
					? stageLoader->GetSelectedEditorObjectIndex() : -1;
				if (ScreenToPlacementSurface(ImGui::GetMousePos(), surfacePoint, surfaceNormal, ignoredPropIndex))
				{
					transform->position = surfacePoint;
					if (ignoredPropIndex >= 0)
						transform->rotation = AlignPropRotationToSurface(transform->rotation, surfaceNormal);
				}
			}
			if (!selectedLight) stageLoader->RefreshSelectedEditorObject();
			dirty = true;
		}
	}
	if (!ImGuizmo::IsUsing()) terrainSnapActive = false;

	if (uiCapturesMouse || gizmoHovered || ImGuizmo::IsUsingAny() || ImGuizmo::IsOver() ||
		!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		return;
	SelectObjectAt(ImGui::GetMousePos(), viewportMin, viewportMax);
}

void VstgEditorScene::SelectObjectAt(
	const ImVec2& mousePosition, const ImVec2& viewportMin, const ImVec2& viewportMax)
{
	if (!stageLoader || !currentStage || mousePosition.x < viewportMin.x ||
		mousePosition.x > viewportMax.x || mousePosition.y < viewportMin.y ||
		mousePosition.y > viewportMax.y)
		return;

	Camera* camera = currentStage->GetActiveCamera();
	if (!camera) return;

	const Vector2 mouseNdc =
		Game::Graphics::Instance().GetMouseNDC(mousePosition.x, mousePosition.y);
	const Matrix inverseViewProjection = (camera->GetView() * camera->GetProjection()).Invert();

	// 画面座標を逆投影し、手前と奥の点から選択用レイを作る。
	const Vector3 nearPoint = Vector3::Transform(
		Vector3(mouseNdc.x, mouseNdc.y, 0.0f), inverseViewProjection);
	const Vector3 farPoint = Vector3::Transform(
		Vector3(mouseNdc.x, mouseNdc.y, 1.0f), inverseViewProjection);

	const Matrix viewProjection = camera->GetView() * camera->GetProjection();
	const ImGuiViewport* viewport = ImGui::GetMainViewport();

	// ライトは画面上のマーカーとの距離で選択する。
	float lightDistanceSquared = 18.0f * 18.0f;
	EditorLightType hitLightType = EditorLightType::None;
	int hitLightIndex = -1;
	const auto pickLights = [&](auto&& values, EditorLightType type) {
		for (int index = 0; index < static_cast<int>(values.size()); ++index)
		{
			if (values[index].IsPendingDestroy()) continue;

			const Vector3 projected = Vector3::Transform(values[index].transform.position, viewProjection);
			if (projected.z < 0.0f || projected.z > 1.0f) continue;

			const float x = viewport->Pos.x + (projected.x + 1.0f) * 0.5f * Game::Graphics::ScreenWidth;
			const float y = viewport->Pos.y + (1.0f - projected.y) * 0.5f * Game::Graphics::ScreenHeight;
			const float dx = x - mousePosition.x, dy = y - mousePosition.y;
			const float distance = dx * dx + dy * dy;
			if (distance >= lightDistanceSquared) continue;
			lightDistanceSquared = distance;
			hitLightType = type;
			hitLightIndex = index;
		}
	};
	auto& lights = currentStage->GetLightManager();
	pickLights(lights.GetDirectionalLights(), EditorLightType::Directional);
	pickLights(lights.GetPointLights(), EditorLightType::Point);
	pickLights(lights.GetSpotLights(), EditorLightType::Spot);
	pickLights(lights.GetAreaLights(), EditorLightType::Area);
	if (hitLightIndex >= 0)
	{
		SelectLight(hitLightType, hitLightIndex);
		return;
	}

	selectedLightType = EditorLightType::None;
	selectedLightIndex = -1;
	lightingSettingsSelected = false;

	// エフェクトは画面上の位置で選択する。
	StageLoader::EditorObjectReference effectHit;
	float effectDistanceSquared = 18.0f * 18.0f;
	for (const auto& object : stageLoader->GetEditorObjects())
	{
		if (object.type != StageLoader::EditorObjectType::Effect || !object.transform) continue;

		const Vector2 viewportSize(Game::Graphics::ScreenWidth, Game::Graphics::ScreenHeight);
		Vector2 screenPosition;
		if (!WorldToScreen(object.transform->position, viewProjection, viewportSize, screenPosition)) continue;

		const float x = viewport->Pos.x + screenPosition.x;
		const float y = viewport->Pos.y + screenPosition.y;
		const float dx = x - mousePosition.x, dy = y - mousePosition.y;
		const float distance = dx * dx + dy * dy;
		if (distance >= effectDistanceSquared) continue;
		effectDistanceSquared = distance;
		effectHit = object;
	}
	if (effectHit.transform)
	{
		stageLoader->SelectEditorObject(effectHit.type, effectHit.index);
		return;
	}

	// 物理レイとモデルのレイ判定で配置オブジェクトを選択する。
	PhysicsManager::PhysicsRaycastHit hit;
	bool selected =
		PhysicsManager::Instance().Raycast(nearPoint, farPoint - nearPoint, 10000.0f, hit) &&
		stageLoader->SelectEditorActor(hit.actor);
	if (!selected)
		selected = stageLoader->SelectEditorObjectAtRay(nearPoint, farPoint - nearPoint);

	if (!selected)
	{
		float bestDistanceSquared = 36.0f * 36.0f;
		StageLoader::EditorObjectReference best;
		const float width = std::max(Game::Graphics::ScreenWidth, 1.0f);
		const float height = std::max(Game::Graphics::ScreenHeight, 1.0f);
		const Matrix viewProjection = camera->GetView() * camera->GetProjection();
		for (const StageLoader::EditorObjectReference& object : stageLoader->GetEditorObjects())
		{
			if (!object.transform) continue;

			const Vector3 projected = Vector3::Transform(object.transform->position, viewProjection);
			if (projected.z < 0.0f || projected.z > 1.0f) continue;

			const float screenX = (projected.x + 1.0f) * 0.5f * width;
			const float screenY = (1.0f - projected.y) * 0.5f * height;
			const float dx = screenX - mousePosition.x;
			const float dy = screenY - mousePosition.y;
			const float distanceSquared = dx * dx + dy * dy;
			if (distanceSquared >= bestDistanceSquared) continue;
			bestDistanceSquared = distanceSquared;
			best = object;
		}
		selected = best.transform && stageLoader->SelectEditorObject(best.type, best.index);
	}

	if (!selected)
	{
		stageLoader->ClearEditorSelection();
		return;
	}
}

bool VstgEditorScene::OnRequestExit()
{
	if (dirty)
	{
		int result = MessageBoxW(Game::Graphics::Instance().GetWindowHandle(),
			L"変更を保存しますか？", L"VEER", MB_YESNOCANCEL | MB_ICONQUESTION);
		if (result == IDYES)
		{
			Save();
			if (dirty) return false; // Save failed or canceled
		}
		else if (result == IDCANCEL)
		{
			return false; // Cancel exit
		}
	}
	return true;
}

void VstgEditorScene::New()
{
	serialViewerDocument = nullptr;
	if (!OnRequestExit()) return;

	auto& graphics = Game::Graphics::Instance();
	if (!graphics.LoadSkyMap("Default"))
	{
		ImGuiToolKit::Alert("デフォルトのライティング環境を読み込めませんでした。");
		return;
	}
	graphics.GetSkyBoxRenderer()->SetIntensity(1.0f);

	showDragPreview = false;
	showPlayerStartDragPreview = false;
	showLightDragPreview = false;
	dragPreviewModel.reset();
	dragPreviewModelPath.clear();
	terrainSnapActive = false;
	path.clear();
	recentStagePath.clear();
	data = VSTG{};
	CreateStage();
	dirty = false;
	SaveEditorSettings();
}

void VstgEditorScene::Open()
{
	const std::string initialDirectory =
		(ResourceManager::GetResourceDirectory(ResourceManager::ResourceType::Stage)).string();
	std::string filename;
	if (Dialog::OpenFileName(filename, "VSTG (*.vstg)\0*.vstg\0",
			"VSTGを開く", initialDirectory.c_str()) != DialogResult::OK)
		return;
	LoadStage(filename);
}

bool VstgEditorScene::LoadStage(const std::filesystem::path& stagePath)
{
	serialViewerDocument = nullptr;
	PROFILE_FUNCTION();
	VSTG loaded;
	if (!loaded.Load(stagePath))
	{
		ImGuiToolKit::Alert(loaded.GetError());
		return false;
	}
	CreateStage();
	if (!loaded.Apply(*terrain, *navMesh, *stageLoader, currentStage->GetLightManager()))
	{
		ImGuiToolKit::Alert(loaded.GetError());
		return false;
	}

	// 現在のスキーマで扱う項目を確定し、それ以外は未知のまま保持する。
	if (!loaded.Capture(*terrain, *navMesh, *stageLoader, currentStage->GetLightManager()))
	{
		ImGuiToolKit::Alert(loaded.GetError());
		return false;
	}
	loaded.GetSerialDocument();
	const bool recoveredMissingModels = ResolveMissingModels();
	data = std::move(loaded);
	path = ResourcePath::Path(ResourceManager::Instance().ResolveResourcePath(ResourcePath::Text(stagePath), ResourceManager::ResourceType::Stage));
	recentStagePath = path;
	SaveEditorSettings();
	cleanStateHash = data.BuildEditorStateHash(
		*terrain, *navMesh, *stageLoader, currentStage->GetLightManager());
	dirty = recoveredMissingModels;
	terrain->PrepareRenderResources();
	return true;
}

bool VstgEditorScene::ResolveMissingModels()
{
	if (!stageLoader) return false;

	bool changed = false;
	for (;;)
	{
		const std::vector<std::string> missingPaths = stageLoader->GetMissingModelPaths();
		if (missingPaths.empty()) break;
		const std::string& missingPath = missingPaths.front();
		const std::wstring message =
			L"VMDLリソースが見つかりません。\n\n" + Utf8ToWide(missingPath) +
			L"\n\n代わりとなるファイルを探しますか？\n"
			L"はい: 代替VMDLを選択\nいいえ: このVMDLの配置物をすべて破棄";
		const int choice = MessageBoxW(Game::Graphics::Instance().GetWindowHandle(),
			message.c_str(), L"VSTG リソースの復旧", MB_YESNO | MB_ICONWARNING);
		if (choice == IDNO)
		{
			stageLoader->RemovePropsWithModelPath(missingPath);
			changed = true;
			continue;
		}

		const std::filesystem::path resourceRoot = ResourceManager::GetResourceRoot();
		const std::filesystem::path modelRoot = resourceRoot / "Model";
		std::string replacementFile;
		if (Dialog::OpenFileName(replacementFile, "VMDL (*.vmdl)\0*.vmdl\0\0",
				"代わりとなるVMDLを選択", modelRoot.string().c_str()) !=
			DialogResult::OK)
			continue;

		const std::string replacementPath = ResourceManager::PortableResourcePath(replacementFile);
		if (!replacementPath.starts_with("Resources/"))
		{
			ImGuiToolKit::Alert("Resourcesフォルダ内のVMDLを選択してください。");
			continue;
		}
		RefreshPropModels();
		if (!stageLoader->ReplaceMissingModelPath(missingPath, replacementPath))
		{
			ImGuiToolKit::Alert(stageLoader->GetError().empty()
				? "選択したVMDLを読み込めませんでした。" : stageLoader->GetError());
			continue;
		}
		changed = true;
	}
	return changed;
}

void VstgEditorScene::Save()
{
	PROFILE_FUNCTION();
	if (path.empty())
	{
		SaveAs();
		return;
	}
	path = ResourceManager::ResolveSavePath(path);
	terrain->BakeCollider();
	if (!data.Capture(*terrain, *navMesh, *stageLoader, currentStage->GetLightManager()) ||
		!data.Save(path))
	{
		ImGuiToolKit::Alert(data.GetError());
		return;
	}
	serialViewerDocument = nullptr;
	recentStagePath = path;
	SaveEditorSettings();
	cleanStateHash = data.BuildEditorStateHash(
		*terrain, *navMesh, *stageLoader, currentStage->GetLightManager());
	dirty = false;
}

void VstgEditorScene::SaveAs()
{
	std::string filename;
	if (Dialog::SaveFileName(filename, "VSTG (*.vstg)\0*.vstg\0",
			"VSTGを保存", "vstg") != DialogResult::OK)
		return;
	path = filename;
	if (path.extension() != ".vstg") path.replace_extension(".vstg");
	Save();
}

void VstgEditorScene::LoadEditorSettings()
{
	const auto& settings = ImGuiRenderer::GetVstgEditorSettings();
	if (!settings.loaded || settings.recentStagePath.empty()) return;

	const std::string& recentPathUtf8 = settings.recentStagePath;
	recentStagePath = std::filesystem::path(std::u8string(
		reinterpret_cast<const char8_t*>(recentPathUtf8.data()), recentPathUtf8.size()));
	recentStagePath = ResourceManager::ResolveSourcePath(recentStagePath);
}

void VstgEditorScene::SaveEditorSettings() const
{
	const std::u8string recentPathUtf8 = recentStagePath.u8string();
	auto& settings = ImGuiRenderer::GetVstgEditorSettings();
	settings.loaded = true;
	settings.recentStagePath.assign(
		reinterpret_cast<const char*>(recentPathUtf8.data()), recentPathUtf8.size());
	ImGuiRenderer::SaveSettings();
}

void VstgEditorScene::UpdateTitle()
{
	SetWindowTextW(Game::Graphics::Instance().GetWindowHandle(), L"VEER");
}

SerialDocument* VstgEditorScene::PrepareSerialDocument()
{
	if (!terrain || !navMesh || !stageLoader || !currentStage) return nullptr;
	if (!data.Capture(*terrain, *navMesh, *stageLoader, currentStage->GetLightManager()))
		throw std::runtime_error(data.GetError());
	return &data.GetSerialDocument();
}

void VstgEditorScene::OnSerialDocumentChanged()
{
	dirty = true;
}
