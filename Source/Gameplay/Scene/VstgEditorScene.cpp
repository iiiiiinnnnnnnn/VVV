#include "Gameplay/Scene/VstgEditorScene.h"

#include <algorithm>
#include <cfloat>
#include <cctype>
#include <format>
#include <limits>

#include <imguizmo/ImGuizmo.h>

#include "Application/Tools/Dialog.h"
#include "Application/Input/Input.h"
#include "Core/Object/Object.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Scene/TitleScene.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/VmdlEditorScene.h"
#include "Gameplay/Stage/Component/StageLoader.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Gameplay/Stage/Stage.h"
#include "Physics/Collider/TerrainMeshCollider.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Physics/Core/PhysicsManager.h"
#include "Physics/RigidBody/Rigidbody.h"
#include "Resource/ResourceManager.h"
#include "Resource/ResourcePath.h"
#include "Resource/VMDLModel.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Core/RenderTarget.h"
#include "Rendering/Renderer/ImGuiTheme.h"
#include "Rendering/Renderer/ShapeRenderer.h"
#include "Rendering/Renderer/ImGuiRenderer.h"
#include "IconsFontAwesome5.h"
#include "Rendering/Effect/EffectManager.h"

static std::filesystem::path GetVstgEditorResourceRoot()
{
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	const auto sourceRoot = ResourceManager::FindSourceResourceRoot();
	if (!sourceRoot.empty()) return sourceRoot;
#endif
	return std::filesystem::absolute("Resources");
}

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
	propPreviewCameraOwner = std::make_unique<Object>("VSTG Prop Preview Camera");
	propPreviewCamera = propPreviewCameraOwner->AddComponent<Camera>();
	propPreviewLights.SetAmbientColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
	propPreviewRenderParams.unlit = true;
	dragPreviewRenderParams.unlit = true;
	DirectionalLight& previewLight = propPreviewLights.GetDirectionalLight();
	previewLight.transform.rotation =
		Quaternion::CreateFromYawPitchRoll(RAD(-30.0f), RAD(35.0f), 0.0f);
	previewLight.transform.Update();
	CreateStage();
	RefreshPropModels();
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
	propPreviewModel.reset();
	dragPreviewModel.reset();
	propModels.clear();
	propPreviewTarget.reset();
	propPreviewCameraOwner.reset();
	propPreviewCamera = nullptr;
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(false);
	graphics.SetWindowMovementLocked(false);
}

// 通常ウィンドウの最大化と、3Dビューのアスペクト比同期を行う
void VstgEditorScene::OnUpdate()
{
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
			const float fov = freeCameraController ? freeCameraController->GetVerticalFovDegrees()
				: FreeCameraController::VerticalFovDegrees;
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
	directionalLight.transform.Update();
	currentStage->GetLightManager().SetAmbientColor(ColorFromRGBA(0x2A4C7DFF));
	auto* rigidbody = currentStage->AddComponent<RigidbodyStatic>();
	terrain = currentStage->AddComponent<Terrain>();
	currentStage->AddComponent<TerrainMeshCollider>(
		Layers::Get("Terrain"), rigidbody, TerrainMeshCollider::CollisionArea{}, nullptr, false);
	navMesh = currentStage->AddComponent<NavMeshActor>();
	stageLoader =
		currentStage->AddComponent<StageLoader>(currentStage.get(), std::string("{}"), true);
	stageLoader->SetEditorModels(&propModels);
	Camera* camera = currentStage->GetActiveCamera();
	camera->SetPerspectiveFov(
		RAD(FreeCameraController::VerticalFovDegrees), Game::Graphics::ScreenWidth / Game::Graphics::ScreenHeight, 0.1f, 2000.0f);
	camera->SetLookAt({0.0f, 20.0f, -30.0f}, Vector3::Zero, Vector3::Up);
	freeCameraController =
		currentStage->GetDefaultCameraActor()->AddComponent<FreeCameraController>();
	freeCameraController->SetMoveSpeed(10.0f);
	freeCameraController->SetFovWheelEnabled(true);
	cleanStateHash = data.BuildEditorStateHash(
		*terrain, *navMesh, *stageLoader, currentStage->GetLightManager());
}

void VstgEditorScene::ConfigureRenderSettings(RenderSettings& settings)
{
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
	propPreviewTarget->Activate(dc);
	RenderContext previewContext = rc;
	previewContext.camera = propPreviewCamera;
	previewContext.lightManager = &propPreviewLights;
	previewContext.shadowMapData = {};
	previewContext.renderSettings.showDebug = false;
	graphics.GetModelRenderer()->Draw(
		ModelShaderId::VMat, propPreviewModel, &propPreviewRenderParams);
	graphics.GetModelRenderer()->Render(previewContext);
	propPreviewTarget->Deactivate(dc);
}

void VstgEditorScene::RenderDragPreview(const RenderContext& rc)
{
	if (!showDragPreview || !dragPreviewModel) return;
	const Matrix modelTransform =
		Matrix::CreateTranslation(dragPreviewModel->GetVmdlExtensionData().rootOffset) *
		dragPreviewTransform.matrix;
	dragPreviewModel->UpdateTransform(modelTransform);
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.GetModelRenderer()->Draw(
		ModelShaderId::VMat, dragPreviewModel, &dragPreviewRenderParams);
	graphics.GetModelRenderer()->Render(rc);
}

void VstgEditorScene::OnDrawGUI()
{
	auto& io = ImGui::GetIO();
	if (!io.WantTextInput && !io.KeyCtrl && !io.KeyAlt && !io.KeySuper)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_F, false)) renderSettings.showFog = !renderSettings.showFog;
		if (ImGui::IsKeyPressed(ImGuiKey_G, false)) renderSettings.showGrass = !renderSettings.showGrass;
		if (terrain && ImGui::IsKeyPressed(ImGuiKey_B, false))
			terrain->SetBrushEnabled(!terrain->IsBrushEnabled());
	}
	propPreviewRequestPath.clear();
	showDragPreview = false;
	showPlayerStartDragPreview = false;
	showLightDragPreview = false;
	ImGuizmo::BeginFrame();

	// 3Dビューを縮小せず、不透明なパネルを重ねる。
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
	ImVec4 panelColor = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
	panelColor.w = 1.0f;
	ImGui::PushStyleColor(ImGuiCol_WindowBg, panelColor);
	ImGui::PushStyleColor(ImGuiCol_ChildBg, panelColor);
	ImVec4 popupColor = ImGui::GetStyleColorVec4(ImGuiCol_PopupBg);
	popupColor.w = 1.0f;
	ImGui::PushStyleColor(ImGuiCol_PopupBg, popupColor);

	// VSTG Editorの青テーマ
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGuiTheme::BlueButtonHovered);
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImGuiTheme::BlueButtonActive);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImGuiTheme::BlueSelected);
	ImGui::PushStyleColor(ImGuiCol_CheckMark, ImGuiTheme::BlueSelectedBorder);
	ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImGuiTheme::BlueButtonHovered);
	ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImGuiTheme::BlueButtonActive);
	ImGui::PushStyleColor(ImGuiCol_Button, ImGuiTheme::BlueButton);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGuiTheme::BlueButtonHovered);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGuiTheme::BlueButtonActive);
	ImGui::PushStyleColor(ImGuiCol_Header, ImGuiTheme::BlueSelected);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGuiTheme::BlueButtonHovered);
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGuiTheme::BlueButtonActive);
	ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, ImGuiTheme::BlueButtonHovered);
	ImGui::PushStyleColor(ImGuiCol_SeparatorActive, ImGuiTheme::BlueButtonActive);
	ImGui::PushStyleColor(ImGuiCol_ResizeGrip, ImGuiTheme::BlueButton);
	ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, ImGuiTheme::BlueButtonHovered);
	ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, ImGuiTheme::BlueButtonActive);
	const ImVec4 tabColor = ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg);
	ImGui::PushStyleColor(ImGuiCol_Tab, tabColor);
	ImGui::PushStyleColor(ImGuiCol_TabHovered, ImGuiTheme::BlueSelected);
	ImGui::PushStyleColor(ImGuiCol_TabSelected, ImGuiTheme::BlueSelected);
	ImGui::PushStyleColor(ImGuiCol_TabSelectedOverline, ImGuiTheme::BlueSelectedBorder);
	ImGui::PushStyleColor(ImGuiCol_TabDimmed, tabColor);
	ImGui::PushStyleColor(ImGuiCol_TabDimmedSelected, ImGuiTheme::BlueSelected);
	ImGui::PushStyleColor(ImGuiCol_TabDimmedSelectedOverline, ImGuiTheme::BlueSelectedBorder);
	ImGui::PushStyleColor(ImGuiCol_TextLink, ImGuiTheme::BlueSelectedBorder);
	ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImGuiTheme::BlueSelected);
	ImGui::PushStyleColor(ImGuiCol_DockingPreview, ImGuiTheme::BlueButtonActive);
	ImGui::PushStyleColor(ImGuiCol_NavCursor, ImGuiTheme::BlueSelectedBorder);

	// メインメニューとファイル情報
	UpdateTitle();
	float mainMenuBarHeight = ImGui::GetFrameHeight();
	if (ImGui::BeginMainMenuBar())
	{
		mainMenuBarHeight = ImGui::GetWindowHeight();
		if (ImGui::BeginMenu((const char*)(ICON_FA_FILE " " u8"ファイル")))
		{
			if (ImGui::MenuItem((const char*)(ICON_FA_FILE " " u8"VSTGを新規作成"), "Ctrl+N")) New();
			if (ImGui::MenuItem((const char*)(ICON_FA_FOLDER_OPEN " " u8"VSTGを開く"), "Ctrl+O")) Open();
			if (ImGui::MenuItem((const char*)(ICON_FA_SAVE " " u8"VSTGを保存"), "Ctrl+S")) Save();
			if (ImGui::MenuItem((const char*)(ICON_FA_COPY " " u8"名前を付けて保存"), "Ctrl+Shift+S")) SaveAs();
			if (ImGui::MenuItem((const char*)(ICON_FA_SIGN_OUT_ALT " " u8"終了")))
			{
				if (OnRequestExit())
				{
					SceneManager::Instance().LoadScene<TitleScene>();
				}
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu((const char*)(ICON_FA_EYE " " u8"表示")))
		{
			if (ImGui::MenuItem(ICON_FA_ADJUST " PBR", nullptr, renderSettings.shadingMode == ShadingMode::Pbr))
				renderSettings.shadingMode = ShadingMode::Pbr;
			if (ImGui::MenuItem(ICON_FA_SUN " Unlit", nullptr, renderSettings.shadingMode == ShadingMode::Unlit))
				renderSettings.shadingMode = ShadingMode::Unlit;
			if (ImGui::MenuItem((const char*)(ICON_FA_ADJUST " " u8"ソリッド"), nullptr, renderSettings.shadingMode == ShadingMode::Solid))
				renderSettings.shadingMode = ShadingMode::Solid;
			ImGui::Separator();
			ImGui::Checkbox((const char*)(ICON_FA_LEAF " " u8"草マップ"), &renderSettings.showGrass);
			ImGui::Checkbox((const char*)(ICON_FA_SMOG " " u8"フォグ"), &renderSettings.showFog);
			ImGui::Separator();
			ImGui::MenuItem((const char*)(ICON_FA_BUG " " u8"デバッグ表示"), "F3", &renderSettings.showDebug);
			ImGui::Checkbox((const char*)(ICON_FA_SHAPES " " u8"コライダー"), &renderSettings.showColliderDebug);
			ImGui::Checkbox((const char*)(ICON_FA_PUZZLE_PIECE " " u8"コンポーネント"), &renderSettings.showComponentDebug);
			ImGui::Checkbox((const char*)(ICON_FA_LIGHTBULB " " u8"ライト"), &renderSettings.showLightDebug);
			ImGui::Checkbox((const char*)(ICON_FA_PROJECT_DIAGRAM " " u8"ナビメッシュ"), &renderSettings.showNavMeshDebug);
			ImGui::Checkbox((const char*)(ICON_FA_DRAW_POLYGON " " u8"ワイヤーフレーム"), &renderSettings.wireframe);
			ImGui::EndMenu();
		}
		std::string displayPath = (const char*)u8"名称未設定";
		if (!path.empty())
		{
			const std::u8string utf8Path = path.u8string();
			displayPath.assign(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());
		}
		if (dirty) displayPath += " *";
		const std::string fpsText = std::format("FPS: {:.0f}", io.Framerate);
		const float pathWidth = ImGui::CalcTextSize(displayPath.c_str()).x;
		const float fpsWidth = ImGui::CalcTextSize(fpsText.c_str()).x;
		ImGui::SetCursorPosX(std::max(
			ImGui::GetCursorPosX() + 20.0f, ImGui::GetWindowWidth() - fpsWidth - pathWidth - 32.0f -
												ImGui::GetStyle().WindowPadding.x));
		ImGui::TextColored(
			ImVec4(0.45f, 1.0f, 0.55f, 1.0f), "%s", fpsText.c_str());
		ImGui::SameLine(0.0f, 20.0f);
		ImGui::TextUnformatted(displayPath.c_str());
		ImGui::EndMainMenuBar();
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

	// 左インスペクタ・右ヒエラルキー・下アセット/Terrain。描画自体は全画面のまま。
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	constexpr float gap = 6.0f;
	const ImVec2 workPosition = {viewport->Pos.x + gap, viewport->Pos.y + mainMenuBarHeight + gap};
	const ImVec2 workSize = {std::max(1.0f, viewport->Size.x - gap * 2.0f),
		std::max(1.0f, viewport->Pos.y + viewport->Size.y - workPosition.y - gap)};
	const float leftWidth = std::min(340.0f, workSize.x * 0.25f);
	const float rightWidth = std::min(300.0f, workSize.x * 0.23f);
	const float bottomHeight = workSize.y * 0.30f;
	const float topHeight = workSize.y - bottomHeight - gap;
	constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

	ImGui::SetNextWindowPos(workPosition, ImGuiCond_Always);
	ImGui::SetNextWindowSize({leftWidth, topHeight}, ImGuiCond_Always);
	if (ImGui::Begin((const char*)(ICON_FA_COG " " u8"インスペクタ###VSTG Inspector"), nullptr, windowFlags))
		DrawInspector();
	ImGui::End();

	ImGui::SetNextWindowPos({workPosition.x + workSize.x - rightWidth, workPosition.y}, ImGuiCond_Always);
	ImGui::SetNextWindowSize({rightWidth, topHeight}, ImGuiCond_Always);
	if (ImGui::Begin((const char*)(ICON_FA_LIST " " u8"ヒエラルキー###VSTG Hierarchy"), nullptr, windowFlags))
		DrawHierarchy();
	ImGui::End();

	ImGui::SetNextWindowPos({workPosition.x, workPosition.y + topHeight + gap}, ImGuiCond_Always);
	ImGui::SetNextWindowSize({workSize.x, bottomHeight}, ImGuiCond_Always);
	if (ImGui::Begin((const char*)u8"ステージ編集###VSTG Bottom", nullptr, windowFlags))
	{
		if (ImGui::BeginTabBar("VSTG Bottom Tabs"))
		{
			if (ImGui::BeginTabItem((const char*)(ICON_FA_CUBES " " u8"アセット")))
			{
				DrawPropBrowser(0.0f);
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem((const char*)(ICON_FA_MOUNTAIN " Terrain")))
			{
				if (ImGui::BeginChild("VSTG Terrain Settings"))
				{
					if (terrain) terrain->DrawGUI();
					if (navMesh && ImGui::CollapsingHeader((const char*)u8"NavMesh設定")) navMesh->DrawGUI();
				}
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();

	if (Game::Input::IsFocusedWindow(true) && !io.WantTextInput && !ImGui::IsAnyItemActive() &&
		!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && !ImGuizmo::IsUsing() &&
		ImGui::IsKeyPressed(ImGuiKey_Delete, false))
	{
		if (GetSelectedLight()) DeleteSelectedLight();
		else if (stageLoader && stageLoader->DeleteSelectedEditorObject()) dirty = true;
	}

	// ギズモ操作のショートカット
	if (!io.WantTextInput &&
		(Game::Input::Instance().GetMouse().GetButton() & Mouse::BTN_RIGHT) == 0)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_W, false)) gizmoOperation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_E, false)) gizmoOperation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_R, false)) gizmoOperation = ImGuizmo::SCALE;
	}

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
		ImGui::TextUnformatted((const char*)(ICON_FA_CUBE " " u8"3Dビュー"));
		ImGui::SameLine();
		if (ImGui::SmallButton((const char*)(ICON_FA_ARROWS_ALT " " u8"移動"))) gizmoOperation = ImGuizmo::TRANSLATE;
		ImGui::SameLine();
		if (ImGui::SmallButton((const char*)(ICON_FA_SYNC_ALT " " u8"回転"))) gizmoOperation = ImGuizmo::ROTATE;
		ImGui::SameLine();
		if (ImGui::SmallButton((const char*)(ICON_FA_EXPAND_ARROWS_ALT " " u8"サイズ"))) gizmoOperation = ImGuizmo::SCALE;
	}
	ImGui::End();

	// 不透明パネル上では地形を編集しない。ドラッグ終了後は通常のブラシ操作へ戻す。
	const ImVec2 mouse = ImGui::GetMousePos();
	if (terrain) terrain->SetBrushViewportInputAllowed(
		mouse.x >= viewportMin.x && mouse.x <= viewportMax.x &&
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
		if (ImGui::BeginCombo((const char*)u8"エフェクト", placementEffectName.empty()
			? (const char*)u8"選択してください" : placementEffectName.c_str()))
		{
			for (const auto& candidate : EffectManager::GetEffectPaths())
				if (ImGui::Selectable(candidate.c_str(), candidate == placementEffectName))
					placementEffectName = candidate;
			ImGui::EndCombo();
		}
		ImGui::BeginDisabled(placementEffectName.empty());
		if (ImGui::Button((const char*)u8"配置") && stageLoader)
		{
			stageLoader->AddEditorEffect(placementEffectName, effectPlacementPosition);
			if (terrain) terrain->SetBrushEnabled(false);
			selectedLightType = EditorLightType::None;
			selectedLightIndex = -1;
			lightingSettingsSelected = false;
			dirty = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button((const char*)u8"キャンセル")) ImGui::CloseCurrentPopup();
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
	ImGui::PopStyleColor(31);
	ImGui::PopStyleVar();
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
	case EditorLightType::Directional: return get(lights.GetDirectionalLights());
	case EditorLightType::Point: return get(lights.GetPointLights());
	case EditorLightType::Spot: return get(lights.GetSpotLights());
	case EditorLightType::Area: return get(lights.GetAreaLights());
	default: return nullptr;
	}
}

void VstgEditorScene::SelectLight(EditorLightType type, int index)
{
	selectedLightType = type;
	selectedLightIndex = index;
	lightingSettingsSelected = false;
	if (stageLoader) stageLoader->ClearEditorSelection();
	if (terrain) terrain->SetBrushEnabled(false);
}

void VstgEditorScene::DrawHierarchy()
{
	if (!currentStage) return;
	if (ImGui::Selectable((const char*)(ICON_FA_SUN " " u8"環境・ライト設定"), lightingSettingsSelected))
	{
		lightingSettingsSelected = true;
		selectedLightType = EditorLightType::None;
		selectedLightIndex = -1;
		if (stageLoader) stageLoader->ClearEditorSelection();
	}
	ImGui::Separator();
	if (stageLoader && stageLoader->DrawEditorHierarchy())
	{
		if (terrain) terrain->SetBrushEnabled(false);
		selectedLightType = EditorLightType::None;
		selectedLightIndex = -1;
		lightingSettingsSelected = false;
	}
	auto& lights = currentStage->GetLightManager();
	ImGui::Text("Lights: %d / %d", lights.GetLightCount(), CbLightData::MaxLights);
	if (!lights.CanAddLight()) ImGui::TextDisabled("Light limit reached");
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
	group(ICON_FA_SUN, (const char*)u8"ディレクショナルライト",
		lights.GetDirectionalLights(), EditorLightType::Directional);
	group(ICON_FA_LIGHTBULB, (const char*)u8"ポイントライト",
		lights.GetPointLights(), EditorLightType::Point);
	group(ICON_FA_LIGHTBULB, (const char*)u8"スポットライト",
		lights.GetSpotLights(), EditorLightType::Spot);
	group(ICON_FA_LIGHTBULB, (const char*)u8"エリアライト",
		lights.GetAreaLights(), EditorLightType::Area);
}

void VstgEditorScene::DrawInspector()
{
	if (!currentStage) return;
	if (lightingSettingsSelected)
	{
		currentStage->GetLightManager().DrawGUI();
		ImGui::SeparatorText((const char*)u8"IBL・スカイボックス");
		Game::Graphics::Instance().DrawSkyMapGUI();
		Game::Graphics::Instance().GetSkyBoxRenderer()->DrawGUI();
		return;
	}
	if (Light* light = GetSelectedLight())
	{
		ImGui::PushID(light);
		std::string name = light->GetName();
		if (ImGui::InputText((const char*)u8"名前", &name)) { light->SetName(name); dirty = true; }
		light->DrawGUI();
		light->transform.Update();
		if (ImGui::Button((const char*)(ICON_FA_TRASH " " u8"削除"))) DeleteSelectedLight();
		ImGui::SameLine();
		if (ImGui::Button((const char*)(ICON_FA_COPY " " u8"複製"))) DuplicateSelectedLight();
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
	copy->SetName(source->GetName() + " Copy");
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
	ImDrawList* draw = ImGui::GetForegroundDrawList();
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
		ErrorMessage("Runtime resource refresh failed.");
		return;
	}
	propPreviewModel.reset();
	propPreviewLoadedPath.clear();
	propModelPaths.clear();
	propModels.clear();
	const std::filesystem::path resourceRoot = GetVstgEditorResourceRoot();
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
	ImGui::InputTextWithHint("##VSTG Prop Search", (const char*)u8"検索", &propSearch);
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
			ImGui::TextDisabled((const char*)u8"ステージ上へドロップして配置");
			ImGui::EndDragDropSource();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"ステージ上へドラッグして配置");
		nextTile();
	};
	specialTile((const char*)(ICON_FA_MAP_MARKER_ALT "\n" u8"プレイヤー初期位置"), "VSTG_PLAYER_START", 0);
	specialTile((const char*)(ICON_FA_SUN "\n" u8"ディレクショナルライト"), "VSTG_LIGHT", static_cast<int>(EditorLightType::Directional));
	specialTile((const char*)(ICON_FA_LIGHTBULB "\n" u8"ポイントライト"), "VSTG_LIGHT", static_cast<int>(EditorLightType::Point));
	specialTile((const char*)(ICON_FA_LIGHTBULB "\n" u8"スポットライト"), "VSTG_LIGHT", static_cast<int>(EditorLightType::Spot));
	specialTile((const char*)(ICON_FA_LIGHTBULB "\n" u8"エリアライト"), "VSTG_LIGHT", static_cast<int>(EditorLightType::Area));
	specialTile((const char*)(ICON_FA_MAGIC "\n" u8"エフェクト"), "VSTG_EFFECT", 0);
	specialTile((const char*)(ICON_FA_COG "\n" u8"侵入不可エリア"), "VSTG_BLOCKED_AREA", 0);

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
				else ImGui::TextDisabled((const char*)u8"プレビューを読み込み中");
				ImGui::TextDisabled("%s", modelPath.c_str());
				ImGui::EndTooltip();
			}
		}
		ImGui::PopID();
		nextTile();
		++visibleIndex;
	}
	ImGui::NewLine();
	if (visibleIndex == 0) ImGui::TextDisabled((const char*)u8"該当するVMDLはありません");
	ImGui::EndChild();
}

bool VstgEditorScene::OpenVmdlEditor(const std::string& modelPath)
{
	if (!OnRequestExit()) return false;
	std::filesystem::path editorPath = ResourcePath::Path(ResourceManager::Instance().ResolveResourcePath(modelPath, ResourceManager::ResourceType::Model));
	const std::filesystem::path sourceResourceRoot = GetVstgEditorResourceRoot();
	if (!sourceResourceRoot.empty())
	{
		const std::filesystem::path relativePath =
			editorPath.lexically_relative(std::filesystem::path("Resources"));
		editorPath = sourceResourceRoot / relativePath;
	}
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
					VMatMaterialParams& params = dragPreviewRenderParams.materials[material.name];
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

	if (terrain) terrain->SetBrushViewportInputAllowed(false);
	// 3Dビュー全体をドロップ領域として受け付ける
	ImGui::SetNextWindowPos(viewportMin, ImGuiCond_Always);
	ImGui::SetNextWindowSize(
		{viewportMax.x - viewportMin.x, viewportMax.y - viewportMin.y}, ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.0f);
	constexpr ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	if (ImGui::Begin("##VSTG Prop Drop Target", nullptr, flags))
	{
		ImGui::InvisibleButton("##VSTG Surface Drop", ImGui::GetContentRegionAvail());
		if (terrain) terrain->SetBrushViewportInputAllowed(false);
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("VSTG_PROP"))
			{
				if (payload->IsDelivery() && showDragPreview)
				{
					if (stageLoader->AddEditorProp(static_cast<const char*>(payload->Data),
						surfacePoint, dragPreviewTransform.rotation))
					{
						if (terrain) terrain->SetBrushEnabled(false);
						selectedLightType = EditorLightType::None;
						selectedLightIndex = -1;
						lightingSettingsSelected = false;
						dirty = true;
					}
					else if (!stageLoader->GetError().empty()) ErrorMessage(stageLoader->GetError());
				}
			}
			if (const ImGuiPayload* payload =
					ImGui::AcceptDragDropPayload("VSTG_PLAYER_START"))
			{
				if (payload->IsDelivery() && hasSurfacePoint && stageLoader)
				{
					stageLoader->SetEditorPlayerStart(surfacePoint);
					if (terrain) terrain->SetBrushEnabled(false);
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
						light->transform.Update();
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
					if (terrain) terrain->SetBrushEnabled(false);
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
			ImGui::ColorConvertFloat4ToU32(ImGuiTheme::BlueSelectedBorder), 0.0f, 0, 2.0f);
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
	if (!currentStage) return false;
	Camera* camera = currentStage->GetActiveCamera();
	if (!camera) return false;

	const Vector2 mouseNdc =
		Game::Graphics::Instance().GetMouseNDC(mousePosition.x, mousePosition.y);
	const Matrix inverseViewProjection = (camera->GetView() * camera->GetProjection()).Invert();
	const DirectX::XMVECTOR nearValue = DirectX::XMVector3TransformCoord(
		DirectX::XMVectorSet(mouseNdc.x, mouseNdc.y, 0.0f, 1.0f), inverseViewProjection);
	const DirectX::XMVECTOR farValue = DirectX::XMVector3TransformCoord(
		DirectX::XMVectorSet(mouseNdc.x, mouseNdc.y, 1.0f, 1.0f), inverseViewProjection);
	Vector3 nearPoint;
	Vector3 farPoint;
	DirectX::XMStoreFloat3(&nearPoint, nearValue);
	DirectX::XMStoreFloat3(&farPoint, farValue);
	const Matrix terrainWorld = currentStage->GetTransform()->matrix;
	const Matrix inverseTerrainWorld = terrainWorld.Invert();
	const Vector3 localOrigin = Vector3::Transform(nearPoint, inverseTerrainWorld);
	Vector3 localDirection = Vector3::TransformNormal(farPoint - nearPoint, inverseTerrainWorld);
	if (localDirection.LengthSquared() < 0.000001f) return false;
	localDirection.Normalize();

	const float halfSize = terrain->GetTerrainSize() * 0.5f;
	float entryDistance = 0.0f;
	float exitDistance = 10000.0f;
	const float axisOrigins[] = {localOrigin.x, localOrigin.z};
	const float axisDirections[] = {localDirection.x, localDirection.z};
	for (int axis = 0; axis < 2; ++axis)
	{
		if (std::abs(axisDirections[axis]) < 0.000001f)
		{
			if (axisOrigins[axis] < -halfSize || axisOrigins[axis] > halfSize) return false;
			continue;
		}
		float nearDistance = (-halfSize - axisOrigins[axis]) / axisDirections[axis];
		float farDistance = (halfSize - axisOrigins[axis]) / axisDirections[axis];
		if (nearDistance > farDistance) std::swap(nearDistance, farDistance);
		entryDistance = std::max(entryDistance, nearDistance);
		exitDistance = std::min(exitDistance, farDistance);
		if (entryDistance > exitDistance) return false;
	}
	if (exitDistance < 0.0f) return false;
	entryDistance = std::max(0.0f, entryDistance);

	const int tessellationFactor = std::max(
		static_cast<int>(std::ceil(std::max(
			terrain->GetTessellationEdgeFactor(),
			terrain->GetTessellationInnerFactor()))),
		1);
	const int surfaceSegments = terrain->GetGridResolution() * tessellationFactor;
	const int searchSteps = std::clamp(surfaceSegments * 2, 512, 4096);
	float previousDistance = entryDistance;
	Vector3 sample = localOrigin + localDirection * previousDistance;
	float u = sample.x / terrain->GetTerrainSize() + 0.5f;
	float v = sample.z / terrain->GetTerrainSize() + 0.5f;
	float previousDifference = sample.y - terrain->GetSurfaceHeightByUV(u, v);
	for (int step = 1; step <= searchSteps; ++step)
	{
		const float distance =
			entryDistance + (exitDistance - entryDistance) * static_cast<float>(step) / searchSteps;
		sample = localOrigin + localDirection * distance;
		u = sample.x / terrain->GetTerrainSize() + 0.5f;
		v = sample.z / terrain->GetTerrainSize() + 0.5f;
		const float difference = sample.y - terrain->GetSurfaceHeightByUV(u, v);
		if ((previousDifference <= 0.0f && difference >= 0.0f) ||
			(previousDifference >= 0.0f && difference <= 0.0f))
		{
			float low = previousDistance;
			float high = distance;
			for (int iteration = 0; iteration < 16; ++iteration)
			{
				const float middle = (low + high) * 0.5f;
				const Vector3 middleSample = localOrigin + localDirection * middle;
				const float middleU = middleSample.x / terrain->GetTerrainSize() + 0.5f;
				const float middleV = middleSample.z / terrain->GetTerrainSize() + 0.5f;
				const float middleDifference =
					middleSample.y - terrain->GetSurfaceHeightByUV(middleU, middleV);
				if ((previousDifference <= 0.0f && middleDifference <= 0.0f) ||
					(previousDifference >= 0.0f && middleDifference >= 0.0f))
					low = middle;
				else high = middle;
			}
			Vector3 localPoint = localOrigin + localDirection * ((low + high) * 0.5f);
			const float u = localPoint.x / terrain->GetTerrainSize() + 0.5f;
			const float v = localPoint.z / terrain->GetTerrainSize() + 0.5f;
			localPoint.y = terrain->GetSurfaceHeightByUV(u, v);
			point = Vector3::Transform(localPoint, terrainWorld);
			if (normal)
			{
				*normal = Vector3::TransformNormal(terrain->GetSurfaceNormalByUV(u, v), inverseTerrainWorld.Transpose());
				normal->Normalize();
			}
			return true;
		}
		previousDistance = distance;
		previousDifference = difference;
	}
	return false;
}

void VstgEditorScene::DrawObjectGizmo(const ImVec2& viewportMin, const ImVec2& viewportMax)
{
	if (terrain && terrain->IsBrushEnabled()) return;
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
		Matrix world = transform->matrix;
		ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
		const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
		ImGuizmo::SetRect(mainViewport->Pos.x, mainViewport->Pos.y,
			Game::Graphics::ScreenWidth, Game::Graphics::ScreenHeight);
		const ImVec2 mouse = ImGui::GetMousePos();
		const bool insideViewport = mouse.x >= viewportMin.x && mouse.x <= viewportMax.x &&
			mouse.y >= viewportMin.y && mouse.y <= viewportMax.y;
		ImGuizmo::Enable(insideViewport || ImGuizmo::IsUsing());
		ImDrawList* draw = ImGui::GetForegroundDrawList();
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
			if (selectedLight) selectedLight->transform.Update();
			else stageLoader->RefreshSelectedEditorObject();
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
	const DirectX::XMVECTOR nearValue = DirectX::XMVector3TransformCoord(
		DirectX::XMVectorSet(mouseNdc.x, mouseNdc.y, 0.0f, 1.0f), inverseViewProjection);
	const DirectX::XMVECTOR farValue = DirectX::XMVector3TransformCoord(
		DirectX::XMVectorSet(mouseNdc.x, mouseNdc.y, 1.0f, 1.0f), inverseViewProjection);
	Vector3 nearPoint;
	Vector3 farPoint;
	DirectX::XMStoreFloat3(&nearPoint, nearValue);
	DirectX::XMStoreFloat3(&farPoint, farValue);

	const Matrix viewProjection = camera->GetView() * camera->GetProjection();
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
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
	if (hitLightIndex >= 0) { SelectLight(hitLightType, hitLightIndex); return; }
	selectedLightType = EditorLightType::None;
	selectedLightIndex = -1;
	lightingSettingsSelected = false;
	StageLoader::EditorObjectReference effectHit;
	float effectDistanceSquared = 18.0f * 18.0f;
	for (const auto& object : stageLoader->GetEditorObjects())
	{
		if (object.type != StageLoader::EditorObjectType::Effect || !object.transform) continue;
		const Vector4 clip = Vector4::Transform(Vector4(object.transform->position.x,
			object.transform->position.y, object.transform->position.z, 1.0f), viewProjection);
		if (clip.w <= eps || clip.z < 0.0f || clip.z > clip.w) continue;
		const float x = viewport->Pos.x + (clip.x / clip.w + 1.0f) * 0.5f * Game::Graphics::ScreenWidth;
		const float y = viewport->Pos.y + (1.0f - clip.y / clip.w) * 0.5f * Game::Graphics::ScreenHeight;
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
			const DirectX::XMVECTOR projectedValue =
				DirectX::XMVector3TransformCoord(object.transform->position, viewProjection);
			Vector3 projected;
			DirectX::XMStoreFloat3(&projected, projectedValue);
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
	if (!OnRequestExit()) return;

	auto& graphics = Game::Graphics::Instance();
	if (!graphics.LoadSkyMap("Default"))
	{
		ErrorMessage((const char*)u8"デフォルトのライティング環境を読み込めませんでした。");
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
		(GetVstgEditorResourceRoot() / "Stage").string();
	std::string filename;
	if (Dialog::OpenFileName(filename, "VSTG (*.vstg)\0*.vstg\0",
			(const char*)u8"VSTGを開く", initialDirectory.c_str()) != DialogResult::OK)
		return;
	LoadStage(filename);
}

bool VstgEditorScene::LoadStage(const std::filesystem::path& stagePath)
{
	VSTG loaded;
	if (!loaded.Load(stagePath))
	{
		ErrorMessage(loaded.GetError());
		return false;
	}
	CreateStage();
	if (!loaded.Apply(*terrain, *navMesh, *stageLoader, currentStage->GetLightManager()))
	{
		ErrorMessage(loaded.GetError());
		return false;
	}
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

		const std::filesystem::path resourceRoot = GetVstgEditorResourceRoot();
		const std::filesystem::path modelRoot = resourceRoot / "Model";
		std::string replacementFile;
		if (Dialog::OpenFileName(replacementFile, "VMDL (*.vmdl)\0*.vmdl\0\0",
				(const char*)u8"代わりとなるVMDLを選択", modelRoot.string().c_str()) !=
			DialogResult::OK)
			continue;

		std::error_code pathError;
		const std::filesystem::path selectedPath =
			std::filesystem::weakly_canonical(replacementFile, pathError);
		std::filesystem::path relativePath;
		if (!pathError)
		{
			for (const auto& root : {resourceRoot, std::filesystem::absolute("Resources")})
			{
				std::error_code rootError;
				const auto candidate = std::filesystem::relative(selectedPath, root, rootError);
				if (rootError || candidate.empty() || candidate.is_absolute() ||
					*candidate.begin() == "..") continue;
				relativePath = candidate;
				break;
			}
		}
		if (relativePath.empty())
		{
			ErrorMessage((const char*)u8"Resourcesフォルダ内のVMDLを選択してください。");
			continue;
		}

		const std::string replacementPath =
			(std::filesystem::path("Resources") / relativePath).generic_string();
		RefreshPropModels();
		if (!stageLoader->ReplaceMissingModelPath(missingPath, replacementPath))
		{
			ErrorMessage(stageLoader->GetError().empty()
				? (const char*)u8"選択したVMDLを読み込めませんでした。" : stageLoader->GetError());
			continue;
		}
		changed = true;
	}
	return changed;
}

void VstgEditorScene::Save()
{
	if (path.empty())
	{
		SaveAs();
		return;
	}
	path = ResourceManager::ResolveSourcePath(path);
	terrain->BakeCollider();
	if (!data.Capture(*terrain, *navMesh, *stageLoader, currentStage->GetLightManager()) ||
		!data.Save(path))
	{
		ErrorMessage(data.GetError());
		return;
	}
	if (!ResourceManager::Instance().RefreshResources(path))
	{
		ErrorMessage("Stage saved, but runtime cache refresh failed. Save again to retry.");
		return;
	}
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
			(const char*)u8"VSTGを保存", "vstg") != DialogResult::OK)
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

void VstgEditorScene::ErrorMessage(const std::string& message)
{
	MessageBoxW(Game::Graphics::Instance().GetWindowHandle(),
		Utf8ToWide(message).c_str(), L"VEER", MB_ICONERROR);
}
