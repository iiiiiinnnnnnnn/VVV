// VstgEditorScene.cpp
#include "Gameplay/Scene/VstgEditorScene.h"

#include <algorithm>
#include <cfloat>
#include <cctype>
#include <fstream>
#include <format>
#include <limits>

#include <imguizmo/ImGuizmo.h>

#include "Application/Tools/Dialog.h"
#include "Application/Input/Input.h"
#include "Core/Foundation/Json.h"
#include "Core/Object/Object.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Scene/GameStartScene.h"
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
#include "Resource/VMDLModel.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Core/RenderTarget.h"
#include "Rendering/Renderer/ImGuiTheme.h"
#include "IconsFontAwesome5.h"

namespace
{
std::wstring Utf8ToWide(const std::string& text)
{
	if (text.empty()) return {};
	const int length = MultiByteToWideChar(
		CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
	if (length <= 0) return std::wstring(text.begin(), text.end());
	std::wstring result(static_cast<size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
		result.data(), length);
	return result;
}
}

VstgEditorScene::VstgEditorScene()
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
	DirectionalLight& previewLight = propPreviewLights.GetDirectionalLight();
	previewLight.transform.rotation =
		Quaternion::CreateFromYawPitchRoll(RAD(-30.0f), RAD(35.0f), 0.0f);
	previewLight.transform.Update();
	CreateStage();
	RefreshPropModels();
	LoadEditorSettings();
	if (!recentStagePath.empty() && std::filesystem::exists(recentStagePath))
		LoadStage(recentStagePath);
}

VstgEditorScene::~VstgEditorScene()
{
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
			camera->SetPerspectiveFov(RAD(45.0f), width / height, 0.1f, 2000.0f);
		}
	}
}

void VstgEditorScene::CreateStage()
{
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
		RAD(45.0f), Game::Graphics::ScreenWidth / Game::Graphics::ScreenHeight, 0.1f, 2000.0f);
	camera->SetLookAt({0.0f, 20.0f, -30.0f}, Vector3::Zero, Vector3::Up);
	freeCameraController =
		currentStage->GetDefaultCameraActor()->AddComponent<FreeCameraController>();
	freeCameraController->SetMoveSpeed(10.0f);
	cleanStateHash = data.BuildEditorStateHash(
		*terrain, *navMesh, *stageLoader, currentStage->GetLightManager());
}

void VstgEditorScene::ConfigureRenderSettings(RenderSettings& settings)
{
	settings.distanceFogEnabled = settings.distanceFogEnabled && showFog;
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
			found == propModels.end() || !found->second ? nullptr : found->second->Clone();
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
		RAD(45.0f), 1.0f, std::max(0.01f, radius * 0.01f), radius * 10.0f);

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
	propPreviewRequestPath.clear();
	showDragPreview = false;
	showPlayerStartDragPreview = false;
	ImGuizmo::BeginFrame();

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
		if (ImGui::BeginMenu((const char*)u8"ファイル"))
		{
			if (ImGui::MenuItem((const char*)u8"VSTGを開く", "Ctrl+O")) Open();
			if (ImGui::MenuItem((const char*)u8"VSTGを保存", "Ctrl+S")) Save();
			if (ImGui::MenuItem((const char*)u8"名前を付けて保存", "Ctrl+Shift+S")) SaveAs();
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
			ImGui::Checkbox((const char*)u8"コンポーネント", &renderSettings.showComponentDebug);
			ImGui::Checkbox((const char*)u8"ライト", &renderSettings.showLightDebug);
			ImGui::Checkbox((const char*)u8"ナビメッシュ", &renderSettings.showNavMeshDebug);
			ImGui::Checkbox((const char*)u8"ワイヤーフレーム", &renderSettings.wireframe);
			ImGui::Checkbox((const char*)u8"フォグ", &showFog);
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
	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false))
	{
		Open();
	}
	else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
	{
		if (io.KeyShift) SaveAs();
		else Save();
	}

	// 左右パネルと3Dビューの表示範囲
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	constexpr float windowMargin = 10.0f;
	const ImVec2 workPosition = {
		viewport->Pos.x + windowMargin, viewport->Pos.y + mainMenuBarHeight + windowMargin};
	const ImVec2 workSize = {std::max(1.0f, viewport->Size.x - windowMargin * 2.0f),
		std::max(1.0f, viewport->Pos.y + viewport->Size.y - workPosition.y - windowMargin)};
	const float leftWidth = workSize.x * 0.24f;
	const float rightWidth = workSize.x * 0.24f;
	constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;

	// 左側のステージ設定
	ImGui::SetNextWindowPos(workPosition, ImGuiCond_Always);
	ImGui::SetNextWindowSize({leftWidth, workSize.y}, ImGuiCond_Always);
	if (ImGui::Begin((const char*)u8"ステージ設定###VSTG Stage Settings", nullptr, windowFlags))
	{
		if (ImGui::BeginTabBar("VSTG Settings Tabs"))
		{
			if (ImGui::BeginTabItem((const char*)u8"ライト"))
			{
				if (ImGui::Button((const char*)u8"ポイントライト追加"))
				{
					currentStage->GetLightManager().AddPointLight();
					dirty = true;
				}
				ImGui::SameLine();
				if (ImGui::Button((const char*)u8"スポットライト追加"))
				{
					currentStage->GetLightManager().AddSpotLight();
					dirty = true;
				}
				ImGui::SameLine();
				if (ImGui::Button((const char*)u8"エリアライト追加"))
				{
					currentStage->GetLightManager().AddAreaLight();
					dirty = true;
				}
				currentStage->GetLightManager().DrawGUI();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem((const char*)u8"地形"))
			{
				if (terrain) terrain->DrawGUI();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem((const char*)u8"ナビメッシュ"))
			{
				if (navMesh) navMesh->DrawGUI();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();

	// 右側の配置候補と配置済みオブジェクト
	ImGui::SetNextWindowPos(
		{workPosition.x + workSize.x - rightWidth, workPosition.y}, ImGuiCond_Always);
	ImGui::SetNextWindowSize({rightWidth, workSize.y}, ImGuiCond_Always);
	if (ImGui::Begin(
			(const char*)u8"ステージオブジェクト###VSTG Stage Objects", nullptr, windowFlags))
	{
		ImGui::TextUnformatted((const char*)u8"配置物一覧");
		DrawPropBrowser(ImGui::GetContentRegionAvail().y * 0.35f);
		ImGui::Separator();
		ImGui::TextUnformatted((const char*)u8"配置済み");
		if (ImGui::BeginChild("##VSTG Placed Objects", ImVec2(0.0f, 0.0f), true))
			if (stageLoader) stageLoader->DrawGUI();
		ImGui::EndChild();
	}
	ImGui::End();

	// ギズモ操作のショートカット
	if (!io.WantTextInput &&
		(Game::Input::Instance().GetMouse().GetButton() & Mouse::BTN_RIGHT) == 0)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_W, false)) gizmoOperation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_E, false)) gizmoOperation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_R, false)) gizmoOperation = ImGuizmo::SCALE;
	}

	// 3Dビュー上部のギズモ切り替え
	const ImVec2 viewportMin = {workPosition.x + leftWidth, workPosition.y};
	const ImVec2 viewportMax = {
		workPosition.x + workSize.x - rightWidth, workPosition.y + workSize.y};
	constexpr ImGuiWindowFlags toolbarFlags =
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoCollapse;
	const float toolbarHeight = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;
	ImGui::SetNextWindowPos(viewportMin, ImGuiCond_Always);
	ImGui::SetNextWindowSize({viewportMax.x - viewportMin.x, toolbarHeight}, ImGuiCond_Always);
	if (ImGui::Begin("###VSTG Viewport Toolbar", nullptr, toolbarFlags))
	{
		ImGui::TextUnformatted((const char*)u8"3Dビュー");
		ImGui::SameLine();
		if (ImGui::SmallButton((const char*)u8"移動")) gizmoOperation = ImGuizmo::TRANSLATE;
		ImGui::SameLine();
		if (ImGui::SmallButton((const char*)u8"回転")) gizmoOperation = ImGuizmo::ROTATE;
		ImGui::SameLine();
		if (ImGui::SmallButton((const char*)u8"拡大縮小")) gizmoOperation = ImGuizmo::SCALE;
	}
	ImGui::End();

	// 3Dビュー上の配置と選択操作
	DrawPlacementDropTarget(viewportMin, viewportMax);
	DrawObjectGizmo(viewportMin, viewportMax);

	// UI編集後の変更検出
	if (!dirty && (ImGui::IsAnyItemActive() || ImGui::IsMouseReleased(ImGuiMouseButton_Left)) &&
		cleanStateHash != data.BuildEditorStateHash(
							  *terrain, *navMesh, *stageLoader, currentStage->GetLightManager()))
		dirty = true;
	ImGui::PopStyleColor(28);
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
	const std::filesystem::path resourceRoot = ResourceManager::FindSourceResourceRoot();
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
			model = ResourceManager::Instance().LoadModel(modelPath);
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

	// VMDLではない特別な配置物を常に先頭に表示する。
	ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(125, 82, 8, 255));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(170, 112, 12, 255));
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 211, 96, 255));
	ImGui::Selectable(ICON_FA_MAP_MARKER_ALT "  プレイヤー初期位置",
		false, ImGuiSelectableFlags_None, ImVec2(0.0f, ImGui::GetFrameHeight() * 1.2f));
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
	{
		constexpr char payload[] = "PLAYER_START";
		ImGui::SetDragDropPayload("VSTG_PLAYER_START", payload, sizeof(payload));
		ImGui::TextUnformatted((const char*)u8"プレイヤー初期位置");
		ImGui::EndDragDropSource();
	}
	ImGui::PopStyleColor(3);
	ImGui::Separator();

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
		const std::string label = std::string(ICON_FA_CUBE "  ") + name;
		ImGui::Selectable(
			label.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.0f, ImGui::GetFrameHeight()));
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
		++visibleIndex;
	}
	if (visibleIndex == 0) ImGui::TextDisabled((const char*)u8"該当するVMDLはありません");
	ImGui::EndChild();
}

bool VstgEditorScene::OpenVmdlEditor(const std::string& modelPath)
{
	if (!OnRequestExit()) return false;
	std::filesystem::path editorPath = modelPath;
	const std::filesystem::path sourceResourceRoot = ResourceManager::FindSourceResourceRoot();
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
	// ドラッグ中の地形位置と配置プレビュー
	const ImGuiPayload* draggingPayload = ImGui::GetDragDropPayload();
	const bool draggingProp = draggingPayload && draggingPayload->IsDataType("VSTG_PROP");
	const bool draggingPlayerStart =
		draggingPayload && draggingPayload->IsDataType("VSTG_PLAYER_START");
	if (!draggingProp && !draggingPlayerStart) return;
	const std::string modelPath = draggingProp
		? static_cast<const char*>(draggingPayload->Data) : std::string{};
	Vector3 terrainPoint;
	const bool hasTerrainPoint =
		stageLoader && ScreenToTerrainPoint(ImGui::GetMousePos(), terrainPoint);
	if (hasTerrainPoint)
	{
		if (draggingPlayerStart)
		{
			playerStartDragPreviewPosition = terrainPoint;
			showPlayerStartDragPreview = true;
		}
		else if (dragPreviewModelPath != modelPath)
		{
			const auto found = propModels.find(modelPath);
			dragPreviewModel =
				found == propModels.end() || !found->second ? nullptr : found->second->Clone();
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
			showDragPreview =
				dragPreviewModel && stageLoader->BuildEditorPropTransform(
					*dragPreviewModel, terrainPoint, dragPreviewTransform);
	}

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
		ImGui::InvisibleButton("##VSTG Terrain Drop", ImGui::GetContentRegionAvail());
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("VSTG_PROP"))
			{
				if (payload->IsDelivery() && showDragPreview &&
					stageLoader->AddEditorProp(
						static_cast<const char*>(payload->Data), terrainPoint))
				{
					dirty = true;
				}
			}
			if (const ImGuiPayload* payload =
					ImGui::AcceptDragDropPayload("VSTG_PLAYER_START"))
			{
				if (payload->IsDelivery() && hasTerrainPoint && stageLoader)
				{
					stageLoader->SetEditorPlayerStart(terrainPoint);
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

bool VstgEditorScene::ScreenToTerrainPoint(const ImVec2& mousePosition, Vector3& point) const
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
			return true;
		}
		previousDistance = distance;
		previousDifference = difference;
	}
	return false;
}

void VstgEditorScene::DrawObjectGizmo(const ImVec2& viewportMin, const ImVec2& viewportMax)
{
	if (!stageLoader || !currentStage) return;
	Camera* camera = currentStage->GetActiveCamera();
	Transform* transform = stageLoader->GetSelectedEditorTransform();
	if (camera && transform)
	{
		Matrix view = camera->GetView();
		Matrix projection = camera->GetProjection();
		Matrix world = transform->matrix;
		ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
		const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
		ImGuizmo::SetRect(mainViewport->Pos.x, mainViewport->Pos.y,
			Game::Graphics::ScreenWidth, Game::Graphics::ScreenHeight);
		const bool manipulated = ImGuizmo::Manipulate(&view._11, &projection._11,
			static_cast<ImGuizmo::OPERATION>(gizmoOperation), ImGuizmo::LOCAL, &world._11);
		if (gizmoOperation == ImGuizmo::TRANSLATE &&
			ImGuizmo::GetActiveHandleType() == ImGuizmo::MT_MOVE_SCREEN)
		{
			terrainSnapActive = true;
		}
		if (manipulated)
		{
			world.Decompose(transform->scale, transform->rotation, transform->position);
			if (terrainSnapActive)
			{
				Vector3 terrainPoint;
				if (ScreenToTerrainPoint(ImGui::GetMousePos(), terrainPoint))
					transform->position = terrainPoint;
			}
			stageLoader->RefreshSelectedEditorObject();
			dirty = true;
		}
	}
	if (!ImGuizmo::IsUsing()) terrainSnapActive = false;

	if (ImGuizmo::IsUsing() || ImGuizmo::IsOver() ||
		ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
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
			L"変更を保存しますか？", L"VSTG Editor", MB_YESNOCANCEL | MB_ICONQUESTION);
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

void VstgEditorScene::Open()
{
	const std::string initialDirectory =
		(ResourceManager::FindSourceResourceRoot() / "Stage").string();
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
	path = stagePath;
	recentStagePath = stagePath;
	SaveEditorSettings();
	cleanStateHash = data.BuildEditorStateHash(
		*terrain, *navMesh, *stageLoader, currentStage->GetLightManager());
	dirty = recoveredMissingModels;
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

		const std::filesystem::path resourceRoot = ResourceManager::FindSourceResourceRoot();
		const std::filesystem::path modelRoot = resourceRoot / "Model";
		std::string replacementFile;
		if (Dialog::OpenFileName(replacementFile, "VMDL (*.vmdl)\0*.vmdl\0\0",
				(const char*)u8"代わりとなるVMDLを選択", modelRoot.string().c_str()) !=
			DialogResult::OK)
			continue;

		std::error_code pathError;
		const std::filesystem::path selectedPath =
			std::filesystem::weakly_canonical(replacementFile, pathError);
		const std::filesystem::path canonicalRoot =
			std::filesystem::weakly_canonical(resourceRoot, pathError);
		const std::filesystem::path relativePath = selectedPath.lexically_relative(canonicalRoot);
		if (pathError || relativePath.empty() ||
			(!relativePath.empty() && *relativePath.begin() == ".."))
		{
			ErrorMessage((const char*)u8"Resourcesフォルダ内のVMDLを選択してください。");
			continue;
		}

		const std::string replacementPath =
			(std::filesystem::path("Resources") / relativePath).generic_string();
		RefreshPropModels();
		if (!stageLoader->ReplaceMissingModelPath(missingPath, replacementPath))
		{
			ErrorMessage((const char*)u8"選択したVMDLを読み込めませんでした。");
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
	std::ifstream stream("Resources/VstgEditorSettings.json");
	if (!stream) return;
	try
	{
		json root;
		stream >> root;
		const std::string recentPathUtf8 = root.value("recentStagePath", std::string{});
		recentStagePath = std::filesystem::path(std::u8string(
			reinterpret_cast<const char8_t*>(recentPathUtf8.data()), recentPathUtf8.size()));
		recentStagePath = ResourceManager::ResolveSourcePath(recentStagePath);
	}
	catch (const json::exception&)
	{}
}

void VstgEditorScene::SaveEditorSettings() const
{
	if (recentStagePath.empty()) return;
	std::ofstream stream("Resources/VstgEditorSettings.json");
	if (!stream) return;
	const std::u8string recentPathUtf8 = recentStagePath.u8string();
	const std::string recentPath(
		reinterpret_cast<const char*>(recentPathUtf8.data()), recentPathUtf8.size());
	const json root = {{"recentStagePath", recentPath}};
	stream << root.dump(1);
}

void VstgEditorScene::UpdateTitle()
{
	SetWindowTextW(Game::Graphics::Instance().GetWindowHandle(), L"VSTG Editor");
}

void VstgEditorScene::ErrorMessage(const std::string& message)
{
	MessageBoxW(Game::Graphics::Instance().GetWindowHandle(),
		std::wstring(message.begin(), message.end()).c_str(), L"VSTG Editor", MB_ICONERROR);
}
