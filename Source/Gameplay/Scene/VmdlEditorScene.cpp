#include "Gameplay/Scene/VmdlEditorScene.h"

#include "Application/SettingsAndDebug/PhysicsLayerManager.h"
#include "Application/Tools/Dialog.h"
#include "Animation/Animator.h"
#include "Animation/FootIK.h"
#include "Animation/HumanoidFootIK.h"
#include "Animation/MultiLegFootIK.h"
#include "Animation/SpringBone.h"
#include "Audio/SoundSystem.h"
#include "Resource/ResourcePath.h"
#include "Rendering/Renderer/ImGuiPath.h"
#include "Core/Object/Object.h"
#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Camera/CameraMoveSpeedOverlay.h"
#include "Physics/Collider/MeshCollider.h"
#include "Physics/RigidBody/Rigidbody.h"
#include "Rendering/Component/TrailRenderComponent.h"
#include "Rendering/Component/VMDLParticleEmitterComponent.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Effect/EffectManager.h"
#include "Rendering/Renderer/ImGuiTheme.h"
#include "Resource/VMDLModel.h"
#include "Resource/VSTG.h"
#include "Resource/EmbeddedResources.h"

#include "SceneManager.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imguizmo/ImGuizmo.h>

#include <algorithm>
#include <format>
#include <cfloat>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string_view>
#include <type_traits>
#include "TitleScene.h"
#include "Application/Time/GameTime.h"
#include "Resource/ResourceManager.h"
#include "Resource/MeshCache.h"
#include "Rendering/Renderer/ImGuiRenderer.h"

constexpr float PreviewMinCameraDistance = 0.1f;
constexpr float PreviewMaxCameraDistance = 100000.0f;

template<typename T>
int Count(const T& values)
{
	return static_cast<int>(values.size());
}

template<typename T, size_t Size>
constexpr int Count(const T (&)[Size])
{
	return static_cast<int>(Size);
}

std::string MakeVmshFileLabel(const std::string& source)
{
	std::string result;
	result.reserve(std::min<size_t>(source.size(), 48));
	for (unsigned char c : source)
	{
		if (result.size() >= 48) break;
		if (std::isalnum(c) || c == '_' || c == '-') result.push_back(c);
		else if (std::isspace(c) && !result.empty() && result.back() != '_') result.push_back('_');
	}
	return result.empty() ? "mesh" : result;
}

std::string MakeMeshLabel(const VMDLModel& model, int meshIndex)
{
	const auto& meshes = model.GetMeshes();
	if (meshIndex < 0 || meshIndex >= Count(meshes)) return (const char*)u8"メッシュなし";

	const auto& mesh = meshes[meshIndex];
	const std::string nodeName = mesh.node ? mesh.node->name : (const char*)u8"ノードなし";
	const std::string materialName = mesh.material ? mesh.material->name : (const char*)u8"マテリアルなし";
	return std::format("{}: {} / {}", meshIndex, nodeName, materialName);
}

bool IsSameFilePath(const std::filesystem::path& left, const std::filesystem::path& right)
{
	std::error_code error;
	if (std::filesystem::exists(left, error) && std::filesystem::exists(right, error) &&
		std::filesystem::equivalent(left, right, error))
		return true;
	error.clear();
	const std::wstring normalizedLeft =
		std::filesystem::weakly_canonical(std::filesystem::absolute(left), error).wstring();
	error.clear();
	const std::wstring normalizedRight =
		std::filesystem::weakly_canonical(std::filesystem::absolute(right), error).wstring();
	return CompareStringOrdinal(normalizedLeft.c_str(), -1, normalizedRight.c_str(), -1, TRUE) ==
		CSTR_EQUAL;
}

std::string PortableResourcePath(const std::filesystem::path& path)
{
	if (path.empty()) return {};
	if (path.is_relative()) return path.lexically_normal().generic_string();
	const std::filesystem::path sourceRoot = ResourceManager::FindSourceResourceRoot();
	if (!sourceRoot.empty())
	{
		const std::filesystem::path relative = path.lexically_relative(sourceRoot);
		if (!relative.empty() && *relative.begin() != "..")
			return (std::filesystem::path("Resources") / relative).lexically_normal().generic_string();
	}
	return path.lexically_normal().generic_string();
}

bool DrawComponentTransform(VMDLModel& model, VMDLModel::VmdlComponentTransform& transform)
{
	ImGui::SeparatorText("Transform Offset");
	bool changed = false;
	Vector3 position = model.GetScaledAttachmentVector(transform.position);
	if (ImGui::DragFloat3((const char*)u8"位置", &position.x, 0.01f))
	{
		transform.position = model.GetUnscaledAttachmentVector(position);
		changed = true;
	}
	changed |= ImGui::DragFloat3((const char*)u8"回転", &transform.rotation.x, 0.1f);
	changed |= ImGui::DragFloat3((const char*)u8"スケール", &transform.scale.x, 0.01f);
	return changed;
}

VmdlEditorScene::VmdlEditorScene() : VmdlEditorScene(std::filesystem::path{}) {}

VmdlEditorScene::VmdlEditorScene(std::filesystem::path filepath)
	: initialModelPath(std::move(filepath))
{}

void VmdlEditorScene::OnInit()
{
	auto filepath = initialModelPath;
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(false);
	graphics.SetWindowMovementLocked(false);

	ResizePreviewTargets();
	if (previewPostProcess) postProcess.CopySettingsFrom(*previewPostProcess);
	if (previewLighting)
	{
		usesCustomLighting = true;
		editorLights.SetDirectionalLight(previewLighting->GetDirectionalLight());
		editorLights.SetAmbientColor(previewLighting->GetAmbientColor());
		renderSettings = previewRenderSettings;
	}

	cameraOwner = std::make_unique<Object>("VMDL Editor Camera");

	editorCamera = cameraOwner->AddComponent<Camera>();

	editorLightDirection = {0.6f, -0.7f, 0.0f};
	LoadLayoutSettings();
	const bool restoreRecentModel = filepath.empty();
	if (restoreRecentModel && !recentModelPath.empty()) filepath = recentModelPath;
	if (!filepath.empty() && filepath.is_relative() && !ResourceManager::FindSourceResourceRoot().empty())
	{
		const std::filesystem::path relativePath =
			filepath.lexically_relative(std::filesystem::path("Resources"));
		const std::filesystem::path sourcePath =
			ResourceManager::FindSourceResourceRoot() / relativePath;
		if (std::filesystem::exists(sourcePath)) filepath = sourcePath;
	}
	if (!filepath.empty())
	{
		std::error_code error;
		if (!restoreRecentModel || std::filesystem::is_regular_file(
			ResourceManager::ResolveSourcePath(filepath), error)) LoadModel(filepath);
	}

	if (!footIkTestStageModel) LoadFootIkTestStage();
}

VmdlEditorScene::~VmdlEditorScene()
{
	Uninit();
}

void VmdlEditorScene::OnUninit()
{
	SoundSystem::Instance().SetListenerOverride(nullptr);
	SaveLayoutSettings();
	springPreviewOwner.reset();
	trailPreviewOwner.reset();
	particlePreviewOwner.reset();
	footIkPreviewOwner.reset();
	footIkTestStageActor.reset();
	footIkTestStageModel.reset();
	cameraOwner.reset();
	previewLuminance.reset();
	previewBloomWork.reset();
	previewSsao.reset();
	previewPostA.reset();
	previewPostB.reset();
	previewSceneTarget.reset();
	previewTarget.reset();
	model.reset();
	externalMeshPreviewCaches.clear();
	editorPointLights.clear();
	springPreviewComponents.clear();
	trailPreviewComponents.clear();
	particlePreviewComponents.clear();
	editorCamera = nullptr;
	footIkPreviewAnimator = nullptr;
	footIkTestStageRigidbody = nullptr;
	footIkTestStageCollider = nullptr;
	Game::Graphics& graphics = Game::Graphics::Instance();
	if (GetCapture() == graphics.GetWindowHandle()) ReleaseCapture();
	graphics.SetBorderlessFullscreen(false);
	graphics.SetWindowMovementLocked(false);
}

std::string VmdlEditorScene::SoundPathLabel(const std::string& soundPath) const
{
	return soundPath.empty() ? (const char*)u8"なし" : ResourceManager::ShortResourcePath(soundPath, "Sound");
}

bool VmdlEditorScene::DrawSoundPathSelector(const char* label, std::string& soundPath)
{
	const std::string display = SoundPathLabel(soundPath);
	const float previewWidth = ImGui::CalcItemWidth() - ImGui::GetFrameHeight() - ImGui::GetStyle().FramePadding.x * 2;
	const std::string preview = ImGuiPath::Tail(display, previewWidth);
	bool changed = false;
	const bool open = ImGui::BeginCombo(label, preview.c_str());
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", display.c_str());
	if (open)
	{
		if (ImGui::Selectable((const char*)u8"なし", soundPath.empty()))
		{
			soundPath.clear();
			changed = true;
		}
		for (const auto& candidate : SoundSystem::GetSoundPaths())
		{
			ImGui::PushID(candidate.c_str());
			const std::string item = ImGuiPath::Tail(candidate, ImGui::GetContentRegionAvail().x);
			if (ImGui::Selectable(item.c_str(), ResourceManager::ShortResourcePath(soundPath, "Sound") == candidate))
			{
				soundPath = candidate;
				changed = true;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", candidate.c_str());
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}
	return changed;
}

std::string VmdlEditorScene::EffectLabel(const std::string& effectName) const
{
	return effectName.empty() ? (const char*)u8"なし" : ResourceManager::ShortResourcePath(effectName, "Effect");
}

bool VmdlEditorScene::DrawEffectSelector(const char* label, std::string& effectName)
{
	const std::string preview = EffectLabel(effectName);
	bool changed = false;
	if (ImGui::BeginCombo(label, preview.c_str()))
	{
		if (ImGui::Selectable((const char*)u8"なし", effectName.empty()))
		{
			effectName.clear();
			changed = true;
		}
		for (const auto& candidate : EffectManager::GetEffectPaths())
		{
			const std::string item = EffectLabel(candidate);
			if (ImGui::Selectable(item.c_str(), ResourceManager::ShortResourcePath(effectName, "Effect") == candidate))
			{
				effectName = candidate;
				changed = true;
			}
		}
		ImGui::EndCombo();
	}
	return changed;
}

void VmdlEditorScene::LoadLayoutSettings()
{
	const VmdlEditorLayoutSettings& settings =
		ImGuiRenderer::GetVmdlEditorLayoutSettings();
	if (settings.loaded)
	{
		savedWindowX = settings.windowX;
		savedWindowY = settings.windowY;
		savedWindowWidth = settings.windowWidth;
		savedWindowHeight = settings.windowHeight;
		savedWindowMaximized = settings.windowMaximized;
		savedWindowPlacementValid = savedWindowWidth >= 640 && savedWindowHeight >= 480;
		loadedPropertyPanelRatio = settings.propertyPanelRatio;
		loadedViewportPanelRatio = settings.viewportPanelRatio;
		loadedBottomPanelRatio = settings.bottomPanelRatio;
		const std::string& path = settings.recentModelPath;
		recentModelPath = std::filesystem::path(std::u8string(
			reinterpret_cast<const char8_t*>(path.data()), path.size()));
		return;
	}
}

void VmdlEditorScene::SaveLayoutSettings()
{
	auto& settings = ImGuiRenderer::GetVmdlEditorLayoutSettings();
	HWND window = Game::Graphics::Instance().GetWindowHandle();
	WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
	RECT rect{};
	bool maximized = false;
	if (window && GetWindowPlacement(window, &placement))
	{
		maximized = placement.showCmd == SW_SHOWMAXIMIZED || IsZoomed(window);
		if (maximized) rect = placement.rcNormalPosition;
		else GetWindowRect(window, &rect);
	}
	const int windowWidth = std::max(1L, rect.right - rect.left);
	const int windowHeight = std::max(1L, rect.bottom - rect.top);
	const float propertyRatio = layoutInitialized && layoutColumnWidth > 0.0f
		? propertyPanelWidth / layoutColumnWidth : loadedPropertyPanelRatio;
	const float viewportRatio = layoutInitialized && layoutColumnWidth > 0.0f
		? viewportPanelWidth / layoutColumnWidth : loadedViewportPanelRatio;
	const float bottomRatio = layoutInitialized && layoutTotalHeight > 0.0f
		? bottomPanelHeight / layoutTotalHeight : loadedBottomPanelRatio;
	const std::u8string recentUtf8 = recentModelPath.u8string();
	const std::string recentPath(
		reinterpret_cast<const char*>(recentUtf8.data()), recentUtf8.size());

	settings.windowX = rect.left;
	settings.windowY = rect.top;
	settings.windowWidth = windowWidth;
	settings.windowHeight = windowHeight;
	settings.windowMaximized = maximized;
	settings.propertyPanelRatio = propertyRatio;
	settings.viewportPanelRatio = viewportRatio;
	settings.bottomPanelRatio = bottomRatio;
	settings.recentModelPath = recentPath;
	settings.loaded = true;
	ImGuiRenderer::SaveSettings();

	savedWindowX = rect.left;
	savedWindowY = rect.top;
	savedWindowWidth = windowWidth;
	savedWindowHeight = windowHeight;
	savedWindowMaximized = maximized;
	savedWindowPlacementValid = windowWidth >= 640 && windowHeight >= 480;
	loadedPropertyPanelRatio = propertyRatio;
	loadedViewportPanelRatio = viewportRatio;
	loadedBottomPanelRatio = bottomRatio;
}

void VmdlEditorScene::OnUpdate()
{
	Game::Graphics& graphics = Game::Graphics::Instance();
	HWND window = graphics.GetWindowHandle();
	SetWindowTextW(window, L"VEER");

	// ボーダーレス解除後に、前回の通常ウィンドウ位置・サイズ・最大化状態を復元する。
	if (restoreWindowPending && !graphics.IsBorderlessFullscreen())
	{
		SetWindowLongPtr(window, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
		SetWindowPos(window, HWND_TOP, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		if (savedWindowPlacementValid && !savedWindowMaximized)
		{
			RECT desired{savedWindowX, savedWindowY,
				savedWindowX + savedWindowWidth, savedWindowY + savedWindowHeight};
			MONITORINFO monitorInfo{sizeof(MONITORINFO)};
			GetMonitorInfo(MonitorFromRect(&desired, MONITOR_DEFAULTTONEAREST), &monitorInfo);
			const RECT work = monitorInfo.rcWork;
			const int workLeft = work.left;
			const int workTop = work.top;
			const int workRight = work.right;
			const int workBottom = work.bottom;
			const int width = std::clamp(savedWindowWidth, 640, workRight - workLeft);
			const int height = std::clamp(savedWindowHeight, 480, workBottom - workTop);
			const int x = std::clamp(savedWindowX, workLeft, workRight - width);
			const int y = std::clamp(savedWindowY, workTop, workBottom - height);
			ShowWindow(window, SW_RESTORE);
			SetWindowPos(window, HWND_TOP, x, y, width, height,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		}
		else
		{
			ShowWindow(window, SW_MAXIMIZE);
		}
		restoreWindowPending = false;
	}
}

// F3でプレビュー内のデバッグ補助表示をまとめて切り替える
void VmdlEditorScene::ToggleDebugDisplay()
{
	showDebugOverlays = !showDebugOverlays;
}

void VmdlEditorScene::OnDrawGUI()
{
	auto& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();
	const ImGuiStyle gameStyle = style;
	bool selectionFocused = false;
	ImGuiTheme::ApplyRedTheme(style);

	// エディタのショートカット
	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false))
	{
		OpenVmdl();
	}
	else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
	{
		if (io.KeyShift) SaveVmdlAs();
		else SaveVmdl();
	}
	else if (!io.WantTextInput && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt && model &&
			 !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
			 ImGui::IsKeyPressed(ImGuiKey_Space, false))
	{
		if (animationPlaying) StopUnifiedPreview(false);
		else StartUnifiedPreview();
	}

	// アニメーションプレビューの再生
	if (animationPlaying && model && selectedAnimation >= 0 &&
		selectedAnimation < Count(model->GetAnimations()))
	{
		const float length = model->GetAnimations()[selectedAnimation].secondsLength;
		const float previousTime = animationTime;
		animationTime += io.DeltaTime * playbackSpeed;
		bool looped = false;
		if (animationTime > length)
		{
			if (animationLoop && length > 0.0f)
			{
				animationTime = std::fmod(animationTime, length);
				looped = true;
			}
			else
			{
				animationTime = length;
				animationPlaying = false;
			}
		}
		const float soundBegin = animationSoundPreviewStarting
			? previousTime - 0.0001f : previousTime;
		if (looped)
		{
			particlePreviewPlayPending = true;
			PlayAnimationSoundPreview(selectedAnimation, soundBegin, length);
			PlayAnimationSoundPreview(selectedAnimation, -0.0001f, animationTime);
			PlayPresentationPreviewEvents(selectedAnimation, soundBegin, length);
			PlayPresentationPreviewEvents(selectedAnimation, -0.0001f, animationTime);
		}
		else
		{
			PlayAnimationSoundPreview(selectedAnimation, soundBegin, animationTime);
			PlayPresentationPreviewEvents(selectedAnimation, soundBegin, animationTime);
		}
		animationSoundPreviewStarting = false;
		ApplyAnimationPreview();
	}

	RenderPreview();
	ImGuizmo::BeginFrame();

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (previewTarget)
		ImGui::GetBackgroundDrawList()->AddImage(previewTarget->GetSRV(), viewport->Pos,
			ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y));

	// 全画面の3D描画に、不透明な独立パネルを重ねる。
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.055f, 0.06f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.055f, 0.06f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.055f, 0.055f, 0.06f, 1.0f));
	DrawMenuBar();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));

	constexpr float gap = 3.0f;
	const ImVec2 workPosition(viewport->Pos.x + gap, viewport->Pos.y + ImGui::GetFrameHeight() + gap);
	const float totalWidth = std::max(1.0f, viewport->Size.x - gap * 2.0f);
	const float totalHeight = std::max(1.0f, viewport->Pos.y + viewport->Size.y - workPosition.y - gap);
	const bool canInitializeLayout = !restoreWindowPending && !layoutWindowMetricsPending &&
		totalWidth >= 660.0f && totalHeight >= 360.0f;
	propertyPanelWidth = std::min(340.0f, totalWidth * 0.25f);
	const float hierarchyWidth = std::min(300.0f, totalWidth * 0.23f);
	viewportPanelWidth = std::max(1.0f, totalWidth - propertyPanelWidth - hierarchyWidth - gap * 2.0f);
	bottomPanelHeight = totalHeight * 0.36f;
	const float upperHeight = std::max(1.0f, totalHeight - bottomPanelHeight - gap);
	if (canInitializeLayout) layoutInitialized = true;
	if (layoutInitialized)
	{
		layoutColumnWidth = totalWidth;
		layoutTotalHeight = totalHeight;
	}
	if (!restoreWindowPending && layoutWindowMetricsPending) layoutWindowMetricsPending = false;

	constexpr ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
	ImGui::SetNextWindowPos(workPosition, ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(propertyPanelWidth, upperHeight), ImGuiCond_Always);
	if (ImGui::Begin((const char*)(ICON_FA_COG " " u8"インスペクタ###VMDL Inspector"), nullptr, panelFlags))
	{
		DrawProperty();
		selectionFocused |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	}
	ImGui::End();

	ImGui::SetNextWindowPos(ImVec2(workPosition.x + totalWidth - hierarchyWidth, workPosition.y), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(hierarchyWidth, upperHeight), ImGuiCond_Always);
	if (ImGui::Begin((const char*)(ICON_FA_LIST " " u8"ヒエラルキー###VMDL Hierarchy"), nullptr, panelFlags))
	{
		DrawHierarchy();
		selectionFocused |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	}
	ImGui::End();

	const ImVec2 toolbarPosition(workPosition.x + propertyPanelWidth + gap, workPosition.y);
	const float toolbarHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing() +
		ImGui::GetStyle().WindowPadding.y * 2.0f;
	ImGui::SetNextWindowPos(toolbarPosition, ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(viewportPanelWidth, toolbarHeight), ImGuiCond_Always);
	if (ImGui::Begin("###VMDL Preview Toolbar", nullptr, panelFlags | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
		DrawPreviewToolbar();
	ImGui::End();

	ImGui::SetNextWindowPos(ImVec2(workPosition.x, workPosition.y + upperHeight + gap), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(totalWidth, bottomPanelHeight), ImGuiCond_Always);
	const bool bottomVisible = ImGui::Begin((const char*)u8"アニメーション・編集###VMDL Bottom", nullptr, panelFlags);
	const bool bottomFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (bottomVisible)
	{
		const ImVec4 menuColor = ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg);
		ImGui::PushStyleColor(ImGuiCol_Tab, menuColor);
		ImGui::PushStyleColor(ImGuiCol_TabSelected, ImGuiTheme::Selected);
		ImGui::PushStyleColor(ImGuiCol_TabHovered, ImGuiTheme::Selected);
		ImGui::PushStyleColor(ImGuiCol_TabDimmed, menuColor);
		ImGui::PushStyleColor(ImGuiCol_TabDimmedSelected, ImGuiTheme::Selected);
		ImGui::PushStyleColor(ImGuiCol_TabSelectedOverline, ImGuiTheme::SelectedBorder);
		ImGui::PushStyleColor(ImGuiCol_TabDimmedSelectedOverline, ImGuiTheme::SelectedBorder);
		ImGui::PushStyleColor(ImGuiCol_Header, ImGuiTheme::Selected);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGuiTheme::Selected);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGuiTheme::Selected);
		if (ImGui::BeginTabBar("VMDL Editor Tabs"))
		{
			if (ImGui::BeginTabItem((const char*)u8"アニメーション"))
			{
				const ImVec2 tabMin = ImGui::GetItemRectMin();
				const ImVec2 tabMax = ImGui::GetItemRectMax();
				ImGui::GetWindowDrawList()->AddRect(
					tabMin, tabMax, ImGuiTheme::SelectedOutline, 2.0f, 0, 2.0f);
				ImGui::GetWindowDrawList()->AddRectFilled(
					tabMin, ImVec2(tabMin.x + 4.0f, tabMax.y), ImGuiTheme::SelectedAccent);
				DrawTimeline();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem((const char*)u8"モーフ"))
			{
				const ImVec2 tabMin = ImGui::GetItemRectMin();
				const ImVec2 tabMax = ImGui::GetItemRectMax();
				ImGui::GetWindowDrawList()->AddRect(
					tabMin, tabMax, ImGuiTheme::SelectedOutline, 2.0f, 0, 2.0f);
				ImGui::GetWindowDrawList()->AddRectFilled(
					tabMin, ImVec2(tabMin.x + 4.0f, tabMax.y), ImGuiTheme::SelectedAccent);
				DrawMorphEditor();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem((const char*)u8"IK設定"))
			{
				const ImVec2 tabMin = ImGui::GetItemRectMin();
				const ImVec2 tabMax = ImGui::GetItemRectMax();
				ImGui::GetWindowDrawList()->AddRect(
					tabMin, tabMax, ImGuiTheme::SelectedOutline, 2.0f, 0, 2.0f);
				ImGui::GetWindowDrawList()->AddRectFilled(
					tabMin, ImVec2(tabMin.x + 4.0f, tabMax.y), ImGuiTheme::SelectedAccent);
				DrawIkSettings();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
		ImGui::PopStyleColor(10);
	}
	ImGui::End();

	const ImVec2 inputMin(toolbarPosition.x, toolbarPosition.y + toolbarHeight);
	const ImVec2 inputMax(workPosition.x + totalWidth - hierarchyWidth - gap, workPosition.y + upperHeight);
	DrawViewport(inputMin, inputMax);
	if (!bottomFocused && (selectionFocused || !ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) &&
		Game::Input::IsFocusedWindow(true) && !io.WantTextInput && !ImGui::IsAnyItemActive() &&
		!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && !ImGuizmo::IsUsing() &&
		ImGui::IsKeyPressed(ImGuiKey_Delete, false)) DeleteSelection();

	if (animationRecording)
	{
		const ImVec2 position = viewport->Pos;
		const ImVec2 size = viewport->Size;

		constexpr float thickness = 4.0f;
		constexpr float inset = thickness * 0.5f;

		ImGui::GetForegroundDrawList()->AddRect(ImVec2(position.x + inset, position.y + inset),
			ImVec2(position.x + size.x - inset, position.y + size.y - inset),
			IM_COL32(255, 0, 0, 255), 0.0f, ImDrawFlags_None, thickness);
	}

	if (showPhysicsLayerWindow) PhysicsLayerManager::Instance().DrawGUI(&showPhysicsLayerWindow);

	if (showSetScaleWindow) ImGui::OpenPopup((const char*)u8"スケール設定");
	showSetScaleWindow = false;
	if (ImGui::BeginPopupModal(
			(const char*)u8"スケール設定", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::SetNextItemWidth(220.0f);
		ImGui::InputFloat((const char*)u8"スケール", &setScaleValue, 0.01f, 0.1f, "%.4f");
		const bool validScale = model && std::isfinite(setScaleValue) && setScaleValue > 0.0f;
		ImGui::BeginDisabled(!validScale);
		if (ImGui::Button((const char*)u8"適用", ImVec2(100.0f, 0.0f)))
		{
			model->SetModelScale(setScaleValue);
			setScaleValue = model->GetModelScale();
			UpdateModelFraming();
			MarkDirty();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button((const char*)u8"キャンセル", ImVec2(100.0f, 0.0f)))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(2);
	style = gameStyle;
}

void VmdlEditorScene::StartUnifiedPreview()
{
	if (!model) return;
	if (unifiedPreviewActive)
	{
		animationPlaying = true;
		animationSoundPreviewStarting = true;
		return;
	}
	if (selectedAnimation >= 0 && selectedAnimation < Count(model->GetAnimations()) &&
		animationTime >= model->GetAnimations()[selectedAnimation].secondsLength - 0.0001f)
	{
		animationTime = 0.0f;
		ApplyAnimationPreview();
	}
	unifiedPreviewActive = true;
	animationPlaying = true;
	animationSoundPreviewStarting = true;
	particlePreviewPlayPending = true;
	springPreviewSignature.clear();
	StartPresentationPreview();
	showFootIkTestStage = true;
}

void VmdlEditorScene::StopUnifiedPreview(bool rewind)
{
	animationPlaying = false;
	particlePreviewPlayPending = false;
	manualParticlePreviewIndex = -1;
	if (!rewind) return;

	unifiedPreviewActive = false;
	animationTime = 0.0f;
	ApplyAnimationPreview();
	ResetAnimationControlPreview();
	animationSoundPreviewStarting = true;
	cameraShakePreviewTimer.Stop();
	cameraShakePreviewDuration = cameraShakePreviewIntensity = 0.0f;
	radialBlurPreviewTimer.Stop();
	radialBlurPreviewDuration = radialBlurPreviewPower = 0.0f;
	springPreviewOwner.reset();
	springPreviewComponents.clear();
	springPreviewSignature.clear();
	trailPreviewOwner.reset();
	trailPreviewComponents.clear();
	trailPreviewSignature.clear();
	particlePreviewOwner.reset();
	particlePreviewComponents.clear();
	SoundSystem::Instance().StopAll();
}

void VmdlEditorScene::StartPresentationPreview()
{
	cameraShakePreviewTimer.Stop();
	cameraShakePreviewDuration = cameraShakePreviewIntensity = 0.0f;
	radialBlurPreviewTimer.Stop();
	radialBlurPreviewDuration = radialBlurPreviewPower = 0.0f;
	if (!model || !editorCamera) return;
	if (selectedAnimation >= 0 && selectedAnimation < Count(model->GetAnimations()))
	{
		const std::string& animationName = model->GetAnimations()[selectedAnimation].name;
		const auto& presentation = model->GetVmdlPresentationData();
		const auto hasTrack = [&animationName](const auto& tracks) {
			return std::any_of(tracks.begin(), tracks.end(), [&animationName](const auto& track) {
				return track.animationName == animationName && !track.keys.empty();
			});
		};
		if (hasTrack(presentation.cameraShakeTracks) || hasTrack(presentation.radialBlurTracks))
			return;
	}
	model->UpdateTransform(Matrix::Identity);
	const Vector3 listener = editorCamera->GetEye();
	const auto strengthAt = [this, &listener](int node, float range, bool attenuate) {
		const Vector3 origin = node >= 0 && node < Count(model->GetNodes())
			? (model->GetNodes()[node].worldTransform * model->GetRenderScaleTransform()).Translation()
			: Vector3::Zero;
		const float distance = Vector3::Distance(origin, listener);
		if (distance > range) return 0.0f;
		if (!attenuate) return 1.0f;
		const float t = std::clamp(1.0f - distance / std::max(range, 0.01f), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	};
	for (const auto& value : model->GetVmdlPresentationData().cameraShakes)
	{
		const float strength = strengthAt(value.nodeIndex, value.range, value.distanceAttenuation);
		cameraShakePreviewDuration = std::max(cameraShakePreviewDuration, value.duration);
		cameraShakePreviewTimer.Extend(value.duration);
		cameraShakePreviewIntensity = std::max(cameraShakePreviewIntensity, value.intensity * strength);
	}
	for (const auto& value : model->GetVmdlPresentationData().radialBlurs)
	{
		const float strength = strengthAt(value.nodeIndex, value.range, value.distanceAttenuation);
		if (value.power * strength <= radialBlurPreviewPower) continue;
		radialBlurPreviewDuration = value.duration;
		radialBlurPreviewTimer.Reset(value.duration);
		radialBlurPreviewPower = value.power * strength;
		radialBlurPreviewAttackRate = value.attackRate;
	}
}

void VmdlEditorScene::PlayPresentationPreviewEvents(
	int animationIndex, float beginTime, float endTime)
{
	if (!model || !editorCamera || animationIndex < 0 ||
		animationIndex >= Count(model->GetAnimations()) || endTime < beginTime)
		return;
	const std::string& animationName = model->GetAnimations()[animationIndex].name;
	const auto& presentation = model->GetVmdlPresentationData();
	const Vector3 listener = editorCamera->GetEye();
	const auto strengthAt = [this, &listener](int node, float range, bool attenuate) {
		const Vector3 origin = node >= 0 && node < Count(model->GetNodes())
			? (model->GetNodes()[node].worldTransform * model->GetRenderScaleTransform()).Translation()
			: Vector3::Zero;
		const float distance = Vector3::Distance(origin, listener);
		if (distance > range) return 0.0f;
		if (!attenuate) return 1.0f;
		const float t = std::clamp(1.0f - distance / std::max(range, 0.01f), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	};
	for (const auto& track : presentation.cameraShakeTracks)
	{
		if (track.animationName != animationName) continue;
		for (const auto& key : track.keys)
		{
			if (key.seconds <= beginTime || key.seconds > endTime + 0.0001f ||
				key.componentIndex < 0 ||
				key.componentIndex >= Count(presentation.cameraShakes)) continue;
			const auto& value = presentation.cameraShakes[key.componentIndex];
			const float strength = strengthAt(value.nodeIndex, value.range, value.distanceAttenuation);
			cameraShakePreviewDuration = value.duration;
			cameraShakePreviewTimer.Reset(value.duration);
			cameraShakePreviewIntensity = value.intensity * strength;
		}
	}
	for (const auto& track : presentation.radialBlurTracks)
	{
		if (track.animationName != animationName) continue;
		for (const auto& key : track.keys)
		{
			if (key.seconds <= beginTime || key.seconds > endTime + 0.0001f ||
				key.componentIndex < 0 ||
				key.componentIndex >= Count(presentation.radialBlurs)) continue;
			const auto& value = presentation.radialBlurs[key.componentIndex];
			const float strength = strengthAt(value.nodeIndex, value.range, value.distanceAttenuation);
			radialBlurPreviewDuration = value.duration;
			radialBlurPreviewTimer.Reset(value.duration);
			radialBlurPreviewPower = value.power * strength;
			radialBlurPreviewAttackRate = value.attackRate;
		}
	}
}

void VmdlEditorScene::UpdatePointLightPreview()
{
	if (!model)
	{
		editorPointLights.clear();
		editorLights.ClearAttachedPointLights();
		return;
	}

	const auto& values = model->GetVmdlLightData().pointLights;
	if (editorPointLights.size() != values.size())
	{
		editorPointLights.clear();
		editorLights.ClearAttachedPointLights();
		for (const auto& value : values)
		{
			auto light = std::make_shared<PointLight>(value.name);
			editorLights.RegisterAttachedPointLight(light);
			editorPointLights.push_back(std::move(light));
		}
	}

	for (const auto& light : editorPointLights) editorLights.RegisterAttachedPointLight(light);
	const auto& nodes = model->GetNodes();
	for (size_t i = 0; i < values.size(); ++i)
	{
		const auto& value = values[i];
		PointLight& light = *editorPointLights[i];
		light.SetName(value.name);
		light.SetActive(value.active);
		light.SetColor(value.color);
		light.SetIntensity(value.intensity);
		light.SetRange(value.range);
		if (value.nodeIndex < 0 || value.nodeIndex >= Count(nodes))
		{
			light.transform.position = Vector3::Zero;
			continue;
		}
		light.transform.position = model->GetScaledAttachmentTransform(
			value.transform.ToMatrix() * nodes[value.nodeIndex].worldTransform).Translation();
	}
}

Vector3 VmdlEditorScene::GetPreviewCameraOffset() const
{
	const float horizontalDistance = cameraDistance * std::cos(cameraPitch);
	return Vector3(std::sin(cameraYaw) * horizontalDistance,
		std::sin(cameraPitch) * cameraDistance, -std::cos(cameraYaw) * horizontalDistance);
}

void VmdlEditorScene::RotatePreviewCamera(float yawDelta, float pitchDelta, bool orbitModel)
{
	cameraReturningToFront = false;
	const Vector3 oldOffset = GetPreviewCameraOffset();
	const Matrix oldRotation = Matrix::CreateFromYawPitchRoll(-cameraYaw, cameraPitch, 0.0f);
	cameraYaw = std::remainder(cameraYaw + yawDelta, DirectX::XM_2PI);
	const float pitchLimit = DirectX::XM_PIDIV2 - 0.01f;
	cameraPitch = std::clamp(cameraPitch + pitchDelta, -pitchLimit, pitchLimit);
	if (orbitModel)
	{
		// 視点と注視点を同じだけモデル中心の周囲へ回す。見渡し後も位置が飛ばない。
		const Matrix rotation = oldRotation.Invert() *
			Matrix::CreateFromYawPitchRoll(-cameraYaw, cameraPitch, 0.0f);
		const Vector3 focus = Vector3(0.0f, 1.0f, 0.0f) + cameraFocusOffset;
		cameraFocusOffset = cameraOrbitPivot +
			Vector3::TransformNormal(focus - cameraOrbitPivot, rotation) - Vector3(0.0f, 1.0f, 0.0f);
	}
	else
	{
		// 注視点を補正してカメラ位置を固定し、視線だけを回す。
		cameraFocusOffset += oldOffset - GetPreviewCameraOffset();
	}
}

void VmdlEditorScene::ResizePreviewTargets()
{
	const UINT width = static_cast<UINT>(std::max(1.0f, Game::Graphics::ScreenWidth));
	const UINT height = static_cast<UINT>(std::max(1.0f, Game::Graphics::ScreenHeight));
	if (previewTarget && previewTarget->GetWidth() == width && previewTarget->GetHeight() == height) return;
	ID3D11Device* device = Game::Graphics::Instance().GetDevice();
	previewSceneTarget = std::make_unique<RenderTarget>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
	previewTarget = std::make_unique<RenderTarget>(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
	previewLuminance = std::make_unique<RenderTarget>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
	previewBloomWork = std::make_unique<RenderTarget>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
	previewSsao = std::make_unique<RenderTarget>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
	previewPostA = std::make_unique<RenderTarget>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
	previewPostB = std::make_unique<RenderTarget>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
}

void VmdlEditorScene::RenderPreview()
{
	ResizePreviewTargets();
	if (!previewSceneTarget || !previewTarget || !editorCamera) return;

	if (Game::Input::Instance().GetGamePad().GetButtonDown() & GamePad::BTN_LEFT_TRIGGER)
		cameraReturningToFront = true;
	if (cameraReturningToFront)
	{
		const float returnT = 1.0f - std::exp(-7.0f * Game::Time::unscaledDeltaTime);
		cameraYaw = std::remainder(cameraYaw, DirectX::XM_2PI);
		cameraYaw = std::lerp(cameraYaw, 0.0f, returnT);
		cameraPitch = std::lerp(cameraPitch, 0.0f, returnT);
		if (std::abs(cameraYaw) < 0.001f && std::abs(cameraPitch) < 0.001f)
		{
			cameraYaw = 0.0f;
			cameraPitch = 0.0f;
			cameraReturningToFront = false;
		}
	}

	cameraDistance = targetCameraDistance;
	const Vector3 focus = Vector3(0.0f, 1.0f, 0.0f) + cameraFocusOffset;
	const Vector3 eye = focus + GetPreviewCameraOffset();
	if (!previewCameraViewInitialized)
	{
		previewCameraEye = eye;
		previewCameraFocus = focus;
		previewCameraViewInitialized = true;
	}
	else
	{
		const float follow = std::clamp(Game::Time::unscaledDeltaTime *
			FreeCameraController::CameraFollowSpeed, 0.0f, 1.0f);
		previewCameraEye = Vector3::Lerp(previewCameraEye, eye, follow);
		previewCameraFocus = Vector3::Lerp(previewCameraFocus, focus, follow);
	}
	editorCamera->SetLookAt(previewCameraEye, previewCameraFocus, Vector3::Up);
	if (cameraShakePreviewTimer.GetRemaining() > 0.0f)
	{
		cameraShakePreviewTimer.Update(Game::Time::unscaledDeltaTime);
		const float fade = cameraShakePreviewDuration > 0.0f
			? cameraShakePreviewTimer.GetRemaining() / cameraShakePreviewDuration : 0.0f;
		// VMDLプレビューはモデル全体を収めるためゲームカメラより遠くなることがある。
		// ワールド単位の強度をそのまま使うと画面上でサブピクセルになるので、
		// 基準距離5mに対する表示距離で補正し、見た目の揺れ幅を維持する。
		const float previewDistanceScale = std::max(cameraDistance / 5.0f, 1.0f);
		const float elapsed = std::max(cameraShakePreviewDuration - cameraShakePreviewTimer.GetRemaining(), 0.0f);
		const float horizontal = std::sin(elapsed * 71.0f) * cameraShakePreviewIntensity;
		const float vertical = std::sin(elapsed * 97.0f + 1.3f) * cameraShakePreviewIntensity;
		const Vector3 offset = (editorCamera->GetRight() * horizontal +
			editorCamera->GetUp() * vertical) * previewDistanceScale * fade;
		editorCamera->SetLookAt(previewCameraEye + offset, previewCameraFocus + offset, Vector3::Up);
	}
	SoundSystem::Instance().SetListenerOverride(editorCamera);
	const float nearClip = std::max(0.01f, cameraDistance * 0.0001f);
	const float farClip = std::max(1000.0f, cameraDistance * 2.0f);
	editorCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(previewVerticalFovDegrees),
		static_cast<float>(previewTarget->GetWidth()) / previewTarget->GetHeight(), nearClip, farClip);

	Game::Graphics& graphics = Game::Graphics::Instance();
	ID3D11DeviceContext* dc = graphics.GetDeviceContext();

	RenderContext rc{};
	rc.deviceContext = dc;
	rc.renderState = graphics.GetRenderState();
	rc.camera = editorCamera;
	if (!usesCustomLighting)
		editorLights.GetDirectionalLight().transform.SetAngle(editorLightDirection);
	rc.renderSettings = renderSettings;
	rc.renderSettings.distanceFogEnabled = false;
	rc.lightManager = &editorLights;
	rc.iblData.diffuseIrradianceEnvironmentMap = graphics.GetIBLDiffuseIEM();
	rc.iblData.specularPremappingRadianceEnvironmentMap = graphics.GetIBLSpecularPMREM();
	rc.iblData.ggxLookUpTableMap = graphics.GetIBLGGXLUT();
	if (model)
	{
		model->UpdateTransform(Matrix::Identity);
		graphics.GetShadowMapRenderer()->Draw(model.get());
	}
	graphics.GetShadowMapRenderer()->Render(rc,
		editorLights.GetDirectionalLight().GetDirection(), 500.0f);
	for (int i = 0; i < ShadowMapData::CascadeCount; ++i)
	{
		rc.shadowMapData.shadowMaps[i] = graphics.GetShadowMapRenderer()->GetDepthSRV(i);
		rc.shadowMapData.lightViewProjections[i] =
			graphics.GetShadowMapRenderer()->GetLightViewProjection(i);
	}
	rc.shadowMapData.cascadeSplits = graphics.GetShadowMapRenderer()->GetCascadeSplits();
	previewSceneTarget->Clear(dc, 0, 0, 0, 1.0f);
	previewSceneTarget->Activate(dc);
	graphics.GetSkyBoxRenderer()->Render(dc, rc.renderState, *editorCamera,
		graphics.GetIBLSpecularPMREM(), rc.renderSettings);
	if (showFootIkTestStage && footIkTestStageModel)
	{
		const Matrix stageTransform =
			Matrix::CreateTranslation(footIkTestStageModel->GetVmdlExtensionData().rootOffset) *
			Matrix::CreateScale(footIkTestStageSize) *
			Matrix::CreateFromYawPitchRoll(RAD(footIkTestStageRotation.y),
				RAD(footIkTestStageRotation.x), RAD(footIkTestStageRotation.z)) *
			Matrix::CreateTranslation(footIkTestStagePosition);
		footIkTestStageModel->UpdateTransform(stageTransform);
		graphics.GetModelRenderer()->Draw(ModelShaderId::VMat, footIkTestStageModel);
		RenderContext stageContext = rc;
		stageContext.renderSettings.pointMaterialTextures = true;
		graphics.GetModelRenderer()->Render(stageContext);
	}

	// デバッグ表示を含めずにポストエフェクトを完了し、最終画像へ重ね描きする。
	auto finishSceneColor = [&]() {
		previewSceneTarget->Deactivate(dc);

		postProcess.PrepareSceneColor(rc, previewSceneTarget.get(), previewLuminance.get(),
			previewBloomWork.get(), previewSsao.get(), previewPostA.get(), previewPostB.get());
		postProcess.ClearRuntimeEffects();
		if (radialBlurPreviewTimer.GetRemaining() > 0.0f)
		{
			radialBlurPreviewTimer.Update(Game::Time::unscaledDeltaTime);
			const float elapsed = radialBlurPreviewDuration - radialBlurPreviewTimer.GetRemaining();
			const float attackDuration = radialBlurPreviewDuration * radialBlurPreviewAttackRate;
			const float intensity = elapsed < attackDuration
				? elapsed / std::max(attackDuration, 0.001f)
				: radialBlurPreviewTimer.GetRemaining() / std::max(radialBlurPreviewDuration - attackDuration, 0.001f);
			postProcess.AddRuntimeRadialBlur(radialBlurPreviewPower * std::clamp(intensity, 0.0f, 1.0f));
		}
		postProcess.RenderFinal(rc, previewPostA->GetSRV(),
			previewPostB.get(), previewPostA.get(), previewTarget.get());
	};

	if (model)
	{
		model->UpdateTransform(Matrix::Identity);
		std::vector<VMDLModel::NodePose> animationPose;
		const bool useFootIk = animationPlaying && showFootIkTestStage && footIkTestStageModel;
		if (useFootIk || animationPlaying) model->GetNodePoses(animationPose);
		const bool footIkApplied = useFootIk && ApplyFootIkPreview();
		const bool springApplied = UpdateSpringPreview();
		UpdatePointLightPreview();

		if (showMesh)
		{
			UpdateExternalMeshPreview();
			rc.renderSettings.wireframe = false;
			VMatRenderParams params;
			model->ApplyMaterialAnimation(selectedAnimation, animationTime, params);
			params.unlit = previewShadingMode == PreviewShadingMode::Unlit;
			if (previewShadingMode == PreviewShadingMode::Solid)
			{
				for (const VMDLModel::Material& material : model->GetMaterials())
				{
					params.materials[material.name] = {
						.baseColor = solidColor,
						.useBaseColorTexture = false,
					};
				}
			}
			if (previewShadingMode != PreviewShadingMode::Pbr)
				for (const auto& material : model->GetMaterials()) params.materials[material.name].transmission = 0.0f;
			graphics.GetModelRenderer()->Draw(ModelShaderId::VMat, model, &params);
			for (const auto& [groupIndex, cache] : externalMeshPreviewCaches)
				graphics.GetModelRenderer()->DrawMeshCache(
					ModelShaderId::VMat, cache, model, &params);
			graphics.GetModelRenderer()->Render(rc);
		}
		UpdateTrailPreview(rc);
		UpdateParticlePreview(rc);
		if (unifiedPreviewActive && showParticle)
			EffectManager::Instance().Render(rc);
		finishSceneColor();
		dc->OMSetDepthStencilState(
			rc.renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

		bool hasShapes = false;
		if (showDebugOverlays && showBones)
		{
			const auto& nodes = model->GetNodes();
			const Matrix renderScaleTransform = model->GetRenderScaleTransform();
			for (int nodeIndex = 0; nodeIndex < Count(nodes); ++nodeIndex)
			{
				const VMDLModel::Node& node = nodes[nodeIndex];
				if (!node.parent) continue;
				const Vector3 start =
					(node.parent->worldTransform * renderScaleTransform).Translation();
				const Vector3 end = (node.worldTransform * renderScaleTransform).Translation();
				const float baseWidth = std::max(0.006f, (end - start).Length() * 0.12f);
				const bool selected = selectedMesh < 0 && IsNodeSelected(nodeIndex);
				const bool primary = selected && selectedNode == nodeIndex;
				const float width = selected ? baseWidth * (primary ? 1.25f : 1.15f) : baseWidth;
				const Color color = primary
					? Color(0.95f, 0.05f, 0.07f, 1.0f)
					: selected ? Color(0.68f, 0.03f, 0.05f, 0.92f)
							   : Color(0.42f, 0.45f, 0.50f, 0.58f);
				graphics.GetDebugSolidRenderer()->DrawBone(start, end, width, color);
			}
			graphics.GetDebugSolidRenderer()->Render(
				dc, editorCamera->GetView(), editorCamera->GetProjection());
		}

		const auto nodeOffsetTransform = [&](int nodeIndex, const Vector3& offset,
											 const Vector3& rotation = Vector3::Zero) {
			const Matrix local =
				Matrix::CreateFromYawPitchRoll(RAD(rotation.y), RAD(rotation.x), RAD(rotation.z)) *
				Matrix::CreateTranslation(offset);
			if (nodeIndex < 0 || nodeIndex >= Count(model->GetNodes()))
				return local;
			return local * model->GetNodes()[nodeIndex].worldTransform;
		};
		const auto matrixPosition = [](const Matrix& transform) {
			return Vector3(transform._41, transform._42, transform._43);
		};
		auto& data = model->GetVmdlExtensionData();
		if (showDebugOverlays && showIkPole && !(footIkApplied && showFootIkDebug))
		{
			const auto& nodes = model->GetNodes();
			const auto& ikSettings = model->GetVmdlIKSettings();
			const auto& poles = model->GetVmdlIKPoles();
			const Matrix renderScaleTransform = model->GetRenderScaleTransform();
			const auto previewFeet = footIkApplied
				? footIkPreviewOwner->GetComponents<FootIK>() : std::vector<FootIK*>{};
			for (int i = 0; i < Count(ikSettings.legs); ++i)
			{
				const auto& leg = ikSettings.legs[i];
				const int rootIndex = model->GetNodeIndex(leg.root.c_str());
				const int midIndex = model->GetNodeIndex(leg.mid.c_str());
				const int tipIndex = model->GetNodeIndex(leg.tip.c_str());
				if (rootIndex < 0 || midIndex < 0 || tipIndex < 0) continue;
				if (i < Count(previewFeet) && previewFeet[i]->IsPoleInitialized())
				{
					graphics.GetShapeRenderer()->DrawSphere(previewFeet[i]->GetPoleWorldPosition(),
						0.05f, Color(0.0f, 1.0f, 1.0f, 1.0f));
					hasShapes = true;
					continue;
				}

				Vector3 polePosition;
				if (i < Count(poles) && poles[i].custom)
				{
					polePosition = Vector3::Transform(poles[i].position, renderScaleTransform);
				}
				else
				{
					const Vector3 rootPosition =
						(nodes[rootIndex].worldTransform * renderScaleTransform).Translation();
					const Vector3 midPosition =
						(nodes[midIndex].worldTransform * renderScaleTransform).Translation();
					const Vector3 tipPosition =
						(nodes[tipIndex].worldTransform * renderScaleTransform).Translation();
					Vector3 rootToTip = tipPosition - rootPosition;
					Vector3 poleDirection = Vector3::UnitZ;
					if (rootToTip.LengthSquared() > eps)
					{
						rootToTip.Normalize();
						const Vector3 projectedMid =
							rootPosition + rootToTip * (midPosition - rootPosition).Dot(rootToTip);
						poleDirection = midPosition - projectedMid;
						if (poleDirection.LengthSquared() <= eps) poleDirection = Vector3::UnitZ;
						else poleDirection.Normalize();
					}
					const float poleLift = ikSettings.type == 1 ? 0.35f : 0.0f;
					const Vector3 scaledPoleOffset =
						model->GetScaledAttachmentVector(Vector3(0.5f, poleLift, 0.0f));
					polePosition = midPosition + poleDirection * scaledPoleOffset.x +
								   Vector3::Up * scaledPoleOffset.y;
				}

				graphics.GetShapeRenderer()->DrawSphere(
					polePosition, 0.05f, Color(0.0f, 1.0f, 1.0f, 1.0f));
				hasShapes = true;
			}
		}
		if (showDebugOverlays && showCollider)
		{
			for (int i = 0; i < Count(data.colliders); ++i)
			{
				if (i < Count(previewColliderActive) &&
					previewColliderActive[i] == 0)
					continue;
				const auto& value = data.colliders[i];
				if (value.nodeIndex < 0 || value.nodeIndex >= Count(model->GetNodes()))
					continue;
				Matrix transform = model->GetScaledAttachmentTransform(
					value.transform.ToMatrix() * model->GetNodes()[value.nodeIndex].worldTransform);
				const Vector3 scaledSize = model->GetScaledAttachmentVector(Vector3(
					value.size.x * std::abs(value.transform.scale.x),
					value.size.y * std::abs(value.transform.scale.y),
					value.size.z * std::abs(value.transform.scale.z)));
				Vector3 transformScale;
				Vector3 position;
				Quaternion rotation;
				transform.Decompose(transformScale, rotation, position);
				const Matrix pose =
					Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(position);
				if (value.shape >= 3)
				{
					const int meshIndex = value.shape - 3;
					if (meshIndex >= 0 && meshIndex < Count(model->GetMeshes()))
					{
						const auto& mesh = model->GetMeshes()[meshIndex];
						const Matrix meshNodeTransform = mesh.node
							? mesh.node->globalTransform : Matrix::Identity;
						const Matrix meshWorld = meshNodeTransform *
							model->GetNodes()[value.nodeIndex].globalTransform.Invert() * transform;
						Vector3 minimum(FLT_MAX, FLT_MAX, FLT_MAX);
						Vector3 maximum(-FLT_MAX, -FLT_MAX, -FLT_MAX);
						for (const auto& vertex : mesh.vertices)
						{
							const Vector3 position = Vector3::Transform(vertex.position, meshWorld);
							minimum = Vector3::Min(minimum, position);
							maximum = Vector3::Max(maximum, position);
						}
						if (!mesh.vertices.empty())
							graphics.GetShapeRenderer()->DrawBox((minimum + maximum) * 0.5f,
								Vector3::Zero, (maximum - minimum) * 0.5f,
								Color(0.1f, 0.9f, 1.0f, 0.7f));
					}
				}
				else if (value.shape == 1)
					graphics.GetShapeRenderer()->DrawSphere(
						position, std::max(0.001f, scaledSize.x), Color(0.1f, 0.9f, 1.0f, 0.7f));
				else if (value.shape == 2)
					graphics.GetShapeRenderer()->DrawCapsule(pose, std::max(0.001f, scaledSize.x),
						std::max(0.001f, scaledSize.y), Color(0.1f, 0.9f, 1.0f, 0.7f));
				else
				{
					graphics.GetShapeRenderer()->DrawBox(
						position, rotation.ToEuler(), scaledSize, Color(0.1f, 0.9f, 1.0f, 0.7f));
				}
				hasShapes = true;
			}
		}
		// Inspectorで選択している3Dサウンドだけ範囲を表示する。
		if (showDebugOverlays && showSoundRange &&
			selectedComponentType == AttachedComponentType::SoundSource)
		{
			const auto& soundSources = model->GetVmdlSoundData().sources;
			const auto& nodes = model->GetNodes();
			for (int i = 0; i < Count(soundSources); ++i)
			{
				const auto& source = soundSources[i];
				if (i != selectedComponentIndex || source.nodeIndex != selectedNode ||
					!source.spatial || source.nodeIndex < 0 ||
					source.nodeIndex >= Count(nodes))
					continue;

				const Vector3 position = matrixPosition(model->GetScaledAttachmentTransform(
					source.transform.ToMatrix() * nodes[source.nodeIndex].worldTransform));
				graphics.GetShapeRenderer()->DrawSphere(position,
					std::max(source.maxDistance, source.minDistance + 0.01f),
					Color(0.95f, 0.20f, 0.75f, 0.7f));
				graphics.GetShapeRenderer()->DrawSphere(position,
					std::max(0.01f, source.minDistance),
					Color(0.25f, 0.85f, 1.0f, 0.9f));
				hasShapes = true;
			}
		}
		if (showDebugOverlays && showPointLight)
		{
			for (const auto& light : editorPointLights)
			{
				if (!light || !light->IsActive()) continue;
				Color color = light->GetColor();
				color.w = 0.75f;
				graphics.GetShapeRenderer()->DrawSphere(
					light->transform.position, light->GetRange(), color);
				graphics.GetShapeRenderer()->DrawSphere(
					light->transform.position, 0.08f, Color(color.x, color.y, color.z, 1.0f));
				hasShapes = true;
			}
		}
		if (showDebugOverlays && showSoundRange)
		{
			const auto& nodes = model->GetNodes();
			const auto& presentation = model->GetVmdlPresentationData();
			for (const auto& value : presentation.cameraShakes)
			{
				if (value.nodeIndex < 0 || value.nodeIndex >= Count(nodes)) continue;
				graphics.GetShapeRenderer()->DrawSphere(matrixPosition(model->GetScaledAttachmentTransform(
					value.transform.ToMatrix() * nodes[value.nodeIndex].worldTransform)),
					value.range, Color(1.0f, 0.65f, 0.1f, 0.7f));
				hasShapes = true;
			}
			for (const auto& value : presentation.radialBlurs)
			{
				if (value.nodeIndex < 0 || value.nodeIndex >= Count(nodes)) continue;
				graphics.GetShapeRenderer()->DrawSphere(matrixPosition(model->GetScaledAttachmentTransform(
					value.transform.ToMatrix() * nodes[value.nodeIndex].worldTransform)),
					value.range, Color(0.7f, 0.25f, 1.0f, 0.7f));
				hasShapes = true;
			}
		}
		if (showDebugOverlays && showSpringCollider)
		{
			for (const auto& value : data.springColliders)
			{
				if (value.nodeIndex < 0 || value.nodeIndex >= Count(model->GetNodes()))
					continue;
				graphics.GetShapeRenderer()->DrawSphere(
					matrixPosition(value.transform.ToMatrix() * model->GetNodes()[value.nodeIndex].worldTransform),
					value.radius * std::max({std::abs(value.transform.scale.x),
						std::abs(value.transform.scale.y), std::abs(value.transform.scale.z)}),
					Color(1.0f, 0.2f, 0.9f, 0.7f));
				hasShapes = true;
			}
		}
		if (hasShapes)
			graphics.GetShapeRenderer()->Render(
				dc, editorCamera->GetView(), editorCamera->GetProjection());
		if (showDebugOverlays && showSpring)
		{
			for (const auto& value : data.springs)
			{
				if (value.nodeIndex < 0 || value.nodeIndex >= Count(model->GetNodes()))
					continue;
				const Vector3 start =
					matrixPosition(nodeOffsetTransform(value.nodeIndex, Vector3::Zero));
				const Vector3 end =
					matrixPosition(value.transform.ToMatrix() * model->GetNodes()[value.nodeIndex].worldTransform);
				graphics.GetPrimitiveRenderer()->DrawLine(
					start, end, Color(0.3f, 1.0f, 0.3f, 1.0f), Color(0.1f, 0.5f, 0.1f, 1.0f));
			}
			graphics.GetPrimitiveRenderer()->Render(dc, editorCamera->GetView(),
				editorCamera->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		}
		if (showDebugOverlays && showTrail)
		{
			const auto& trails = model->GetVmdlTrailData().trails;
			for (int i = 0; i < Count(trails); ++i)
			{
				if (i < Count(previewTrailActive) && previewTrailActive[i] == 0)
					continue;
				const auto& value = trails[i];
				if (value.nodeIndex < 0 || value.nodeIndex >= Count(model->GetNodes()))
					continue;
				const Vector3 root = matrixPosition(model->GetScaledAttachmentTransform(
					Matrix::CreateFromYawPitchRoll(RAD(value.offsetAngle.y), RAD(value.offsetAngle.x),
						RAD(value.offsetAngle.z)) * Matrix::CreateTranslation(value.rootOffset) *
					value.transform.ToMatrix() * model->GetNodes()[value.nodeIndex].worldTransform));
				const Vector3 tip = matrixPosition(model->GetScaledAttachmentTransform(
					Matrix::CreateFromYawPitchRoll(RAD(value.offsetAngle.y), RAD(value.offsetAngle.x),
						RAD(value.offsetAngle.z)) * Matrix::CreateTranslation(value.tipOffset) *
					value.transform.ToMatrix() * model->GetNodes()[value.nodeIndex].worldTransform));
				graphics.GetPrimitiveRenderer()->DrawLine(root, tip, value.color, value.color);
			}
			graphics.GetPrimitiveRenderer()->Render(dc, editorCamera->GetView(),
				editorCamera->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		}
		// 停止中も接地レイの編集結果を表示する。
		if (!footIkApplied && showDebugOverlays && showFootIkDebug)
		{
			const auto& settings = model->GetVmdlIKSettings();
			const auto& rays = model->GetVmdlIKRaySettings();
			const Matrix scaleTransform = model->GetRenderScaleTransform();
			for (int i = 0; i < Count(settings.legs); ++i)
			{
				const auto& leg = settings.legs[i];
				const int contactIndex = model->GetNodeIndex(
					(leg.contact.empty() ? leg.tip : leg.contact).c_str());
				if (contactIndex < 0) continue;
				Vector3 startOffset(0.0f, settings.type == 1 ? 0.2f : 1.0f, 0.0f);
				float length = settings.type == 1 ? 0.7f : 6.0f;
				Vector3 syncOffset = Vector3::Zero;
				if (i < Count(rays) && rays[i].custom)
				{
					startOffset = rays[i].startOffset;
					length = rays[i].length;
					syncOffset = rays[i].syncOffset;
				}
				const Vector3 start =
					(model->GetNodes()[contactIndex].worldTransform * scaleTransform).Translation() +
					startOffset + Vector3::TransformNormal(syncOffset, scaleTransform);
				graphics.GetPrimitiveRenderer()->DrawLine(start,
					start - Vector3::Up * std::max(0.01f, length), Color(1, 0, 0, 1), Color(1, 0, 0, 1));
			}
			graphics.GetPrimitiveRenderer()->Render(dc, editorCamera->GetView(),
				editorCamera->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		}
		if (footIkApplied && showFootIkDebug && footIkPreviewOwner)
		{
			rc.renderSettings.showDebug = true;
			rc.renderSettings.showColliderDebug = true;
			footIkPreviewOwner->Render(rc);
			graphics.GetShapeRenderer()->Render(
				dc, editorCamera->GetView(), editorCamera->GetProjection());
			graphics.GetPrimitiveRenderer()->Render(dc, editorCamera->GetView(),
				editorCamera->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		}

		dc->OMSetDepthStencilState(
			rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
		if (footIkApplied || springApplied)
		{
			model->SetNodePoses(animationPose);
			model->UpdateTransform(Matrix::Identity);
		}
	}
	else finishSceneColor();

	previewTarget->Deactivate(dc);
}

void VmdlEditorScene::DrawMenuBar()
{
	auto& io = ImGui::GetIO();
	if (!ImGui::BeginMainMenuBar()) return;

	// ファイル操作
	if (ImGui::BeginMenu((const char*)(ICON_FA_FILE " " u8"ファイル")))
	{
		if (ImGui::MenuItem((const char*)(ICON_FA_FOLDER_OPEN " " u8"VMDLを開く"), "Ctrl+O")) OpenVmdl();
		if (ImGui::MenuItem((const char*)(ICON_FA_SAVE " " u8"VMDLを保存"), "Ctrl+S", false, model != nullptr)) SaveVmdl();
		if (ImGui::MenuItem((const char*)(ICON_FA_COPY " " u8"名前を付けて保存"), "Ctrl+Shift+S", false, model != nullptr)) SaveVmdlAs();
		ImGui::Separator();
		if (ImGui::MenuItem((const char*)(ICON_FA_FILE_IMPORT " " u8"GLBをインポート"))) ImportGlb();
		//if (ImGui::MenuItem((const char*)(ICON_FA_FILE_EXPORT " " u8"GLBをエクスポート"), nullptr, false, model != nullptr)) ExportGlb();
		if (ImGui::MenuItem((const char*)(ICON_FA_EXCHANGE_ALT " " u8"GLBキャッシュを置換"), nullptr, false, model != nullptr)) ReplaceGlbCache();
		if (ImGui::MenuItem((const char*)(ICON_FA_PLUS " " u8"アニメーションGLBを追加"), nullptr, false, model != nullptr)) AppendAnimationGlb();
		ImGui::Separator();
		if (ImGui::MenuItem((const char*)(ICON_FA_SIGN_OUT_ALT " " u8"終了")) && OnRequestExit()) exiting = true;
		ImGui::EndMenu();
	}

	// プレビュー表示
	if (ImGui::BeginMenu((const char*)(ICON_FA_EYE " " u8"表示")))
	{
		if (ImGui::MenuItem(ICON_FA_ADJUST " PBR", nullptr, previewShadingMode == PreviewShadingMode::Pbr))
			previewShadingMode = PreviewShadingMode::Pbr;
		if (ImGui::MenuItem(ICON_FA_SUN " Unlit", nullptr, previewShadingMode == PreviewShadingMode::Unlit))
			previewShadingMode = PreviewShadingMode::Unlit;
		if (ImGui::MenuItem((const char*)(ICON_FA_ADJUST " " u8"ソリッド"), nullptr, previewShadingMode == PreviewShadingMode::Solid))
			previewShadingMode = PreviewShadingMode::Solid;
		ImGui::MenuItem((const char*)(ICON_FA_CUBE " " u8"メッシュ"), "M", &showMesh);
		ImGui::Separator();
		ImGui::MenuItem((const char*)(ICON_FA_BUG " " u8"デバッグ表示"), "F3", &showDebugOverlays);
		ImGui::Separator();
		ImGui::MenuItem((const char*)(ICON_FA_BONE " " u8"ボーン"), "B", &showBones);
		ImGui::MenuItem((const char*)(ICON_FA_BULLSEYE " " u8"IKポール"), "I", &showIkPole);
		ImGui::MenuItem((const char*)(ICON_FA_SHAPES " " u8"コライダー"), "C", &showCollider);
		ImGui::MenuItem((const char*)(ICON_FA_VOLUME_UP " " u8"サウンド・演出範囲"), nullptr, &showSoundRange);
		ImGui::MenuItem((const char*)(ICON_FA_LIGHTBULB " " u8"ポイントライト"), nullptr, &showPointLight);
		ImGui::MenuItem((const char*)(ICON_FA_LINK " " u8"スプリング"), "S", &showSpring);
		ImGui::MenuItem((const char*)(ICON_FA_BULLSEYE " " u8"スプリングコライダー"), "Shift+C", &showSpringCollider);
		ImGui::MenuItem((const char*)(ICON_FA_WIND " " u8"トレイル"), "T", &showTrail);
		ImGui::MenuItem((const char*)(ICON_FA_MAGIC " " u8"パーティクル"), "P", &showParticle);
		ImGui::EndMenu();
	}

	// 補助ウィンドウ
	if (ImGui::BeginMenu((const char*)(ICON_FA_WINDOW_MAXIMIZE " " u8"ウィンドウ")))
	{
		if (ImGui::MenuItem((const char*)(ICON_FA_PROJECT_DIAGRAM " " u8"アニメーター")))
			OpenAnimatorEditor();
		if (ImGui::MenuItem((const char*)(ICON_FA_LAYER_GROUP " " u8"物理レイヤー"))) showPhysicsLayerWindow = true;
		ImGui::EndMenu();
	}

	// プレビュー用ツール
	if (ImGui::BeginMenu((const char*)(ICON_FA_TOOLS " " u8"ツール")))
	{
		if (ImGui::MenuItem((const char*)(ICON_FA_UNDO " " u8"ライティング環境をリセット"))) ResetLightingEnvironment();
		if (ImGui::MenuItem((const char*)(ICON_FA_LIGHTBULB " " u8"VSTGからライティング環境を適用"))) ApplyVstgLighting();
		if (ImGui::MenuItem((const char*)(ICON_FA_EXPAND_ARROWS_ALT " " u8"スケール設定"), nullptr, false, model != nullptr))
		{
			setScaleValue = model->GetModelScale();
			showSetScaleWindow = true;
		}
		ImGui::EndMenu();
	}

	// 右側のFPSと編集中ファイル
	std::string title = (const char*)u8"名称未設定";
	if (!documentPath.empty())
	{
		const std::u8string utf8Path = documentPath.u8string();
		title.assign(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());
	}

	if (dirty)
	{
		title += " *";
	}

	const std::string fpsText = std::format("FPS: {:.0f}", io.Framerate);
	const float titleWidth = ImGui::CalcTextSize(title.c_str()).x;
	const float fpsWidth = ImGui::CalcTextSize(fpsText.c_str()).x;
	const float rightPadding = 12.0f;

	ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX() + 20.0f,
		ImGui::GetWindowWidth() - fpsWidth - titleWidth - rightPadding - 20.0f));

	ImGui::TextColored(
		ImVec4(0.45f, 1.0f, 0.55f, 1.0f), "%s", fpsText.c_str());
	ImGui::SameLine(0.0f, 20.0f);
	ImGui::TextUnformatted(title.c_str());
	ImGui::EndMainMenuBar();

	if (exiting && SceneManager::Instance().LoadScene<TitleScene>()) exiting = false;
}

void VmdlEditorScene::UpdateExternalMeshPreview()
{
	if (!model)
	{
		externalMeshPreviewCaches.clear();
		return;
	}
	const auto& groups = model->GetExternalMeshGroups();
	const auto& meshes = model->GetMeshes();
	for (int groupIndex = 0; groupIndex < Count(groups); ++groupIndex)
	{
		const auto& group = groups[groupIndex];
		bool active = false;
		for (int meshIndex : group.meshIndices)
			if (meshIndex >= 0 && meshIndex < Count(meshes) &&
				meshes[meshIndex].isDraw) { active = true; break; }
		auto loaded = externalMeshPreviewCaches.find(groupIndex);
		if (!active)
		{
			if (loaded != externalMeshPreviewCaches.end()) externalMeshPreviewCaches.erase(loaded);
			continue;
		}
		if (loaded == externalMeshPreviewCaches.end())
		{
			try
			{
				const auto resolved = ResourceManager::Instance().ResolvePath(group.path);
				auto cache = std::make_shared<MeshCache>(resolved, *model);
				cache->SyncMaterialsFrom(*model);
				loaded = externalMeshPreviewCaches.emplace(groupIndex, std::move(cache)).first;
			}
			catch (const std::exception&)
			{
				continue;
			}
		}
		auto& cacheMeshes = loaded->second->GetMeshes();
		for (auto& cacheMesh : cacheMeshes) cacheMesh.isDraw = false;
		for (int bindingSlot = 0; bindingSlot < Count(group.meshIndices); ++bindingSlot)
		{
			const int cacheSlot = bindingSlot < group.cacheMeshIndices.size()
				? group.cacheMeshIndices[bindingSlot]
				: bindingSlot;
			const int meshIndex = group.meshIndices[bindingSlot];
			if (cacheSlot < 0 || cacheSlot >= Count(cacheMeshes)) continue;
			cacheMeshes[cacheSlot].isDraw = meshIndex >= 0 &&
				meshIndex < Count(meshes) && meshes[meshIndex].isDraw;
		}
	}
}

void VmdlEditorScene::LoadFootIkTestStage()
{
	if (footIkTestStageModel) return;

	try
	{
		const HMODULE module = GetModuleHandleW(nullptr);
		const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_PREVIEW_STAGE),
			MAKEINTRESOURCEW(10)); // RT_RCDATA
		if (!resource) throw std::runtime_error("Embedded PreviewStage.vmdl was not found.");
		const HGLOBAL loaded = LoadResource(module, resource);
		const auto* data = static_cast<const uint8_t*>(loaded ? LockResource(loaded) : nullptr);
		const DWORD size = SizeofResource(module, resource);
		if (!data || !size) throw std::runtime_error("Embedded PreviewStage.vmdl could not be read.");
		footIkTestStageModel = std::make_shared<VMDLModel>("PreviewStage.vmdl", 60.0f, nullptr, data, size);
	}
	catch (const std::exception& exception)
	{
		ErrorMessage(exception.what());
		return;
	}

	footIkTestStageActor = std::make_unique<Actor>("Foot IK Test Stage");
	footIkTestStageActor->transform.SetPosition(footIkTestStagePosition);
	footIkTestStageActor->transform.SetAngle(footIkTestStageRotation);
	footIkTestStageRigidbody = footIkTestStageActor->AddComponent<RigidbodyStatic>();
	footIkTestStageCollider = footIkTestStageActor->AddComponent<MeshCollider>(Layers::Get("Stage"),
		footIkTestStageRigidbody, footIkTestStageModel, Vector3(footIkTestStageSize));
	footIkTestStageActor->Awake();
	UpdateFootIkTestStage();
}

void VmdlEditorScene::UpdateFootIkTestStage()
{
	footIkTestStageSize = std::clamp(footIkTestStageSize, 0.01f, 100000.0f);
	if (footIkTestStageActor)
	{
		footIkTestStageActor->transform.SetPosition(footIkTestStagePosition);
		footIkTestStageActor->transform.SetAngle(footIkTestStageRotation);
	}
	if (footIkTestStageRigidbody)
	{
		footIkTestStageRigidbody->SetPosition(footIkTestStagePosition);
		footIkTestStageRigidbody->SetRotation(
			Quaternion::CreateFromYawPitchRoll(RAD(footIkTestStageRotation.y),
				RAD(footIkTestStageRotation.x), RAD(footIkTestStageRotation.z)));
	}
	if (footIkTestStageCollider)
	{
		footIkTestStageCollider->SetLocalScale(Vector3(footIkTestStageSize));
		footIkTestStageCollider->SetCollisionEnabled(showFootIkTestStage);
	}
}

void VmdlEditorScene::RebuildFootIkPreview()
{
	footIkPreviewOwner.reset();
	footIkPreviewAnimator = nullptr;
	if (!model || model->GetAnimations().empty()) return;

	const auto& settings = model->GetVmdlIKSettings();
	if (settings.type == 0) return;

	auto owner = std::make_unique<Actor>("VMDL Foot IK Preview");
	Animator* animator = owner->AddComponent<Animator>(model, true);
	animator->AddLayer("Preview");
	for (int animationIndex = 0; animationIndex < Count(model->GetAnimations());
		++animationIndex)
	{
		animator->AddState(0, model->GetAnimations()[animationIndex].name, animationIndex, true);
	}

	if (settings.type == 1)
	{
		if (settings.legs.size() < 2) return;
		for (int legIndex = 0; legIndex < 2; ++legIndex)
		{
			const auto& leg = settings.legs[legIndex];
			if (model->GetNodeIndex(leg.root.c_str()) < 0 ||
				model->GetNodeIndex(leg.mid.c_str()) < 0 ||
				model->GetNodeIndex(leg.tip.c_str()) < 0 ||
				(!leg.contact.empty() && model->GetNodeIndex(leg.contact.c_str()) < 0))
				return;
		}

		const auto& left = settings.legs[0];
		const auto& right = settings.legs[1];
		owner->AddComponent<HumanoidFootIK>(Layers::Get("Foot"), model.get(), animator, nullptr,
			settings.centerNode.c_str(), left.root.c_str(), left.mid.c_str(), left.tip.c_str(),
			left.contact.empty() ? nullptr : left.contact.c_str(), right.root.c_str(),
			right.mid.c_str(), right.tip.c_str(),
			right.contact.empty() ? nullptr : right.contact.c_str());
	}
	else
	{
		auto* multiLeg =
			owner->AddComponent<MultiLegFootIK>(Layers::Get("Foot"), model.get(), animator);
		if (multiLeg->AddLegsFromVmdlSettings() == 0) return;
	}

	owner->Awake();
	footIkPreviewAnimator = animator;
	footIkPreviewOwner = std::move(owner);
}

bool VmdlEditorScene::ApplyFootIkPreview()
{
	if (!model || selectedAnimation < 0 ||
		selectedAnimation >= Count(model->GetAnimations()))
		return false;

	const auto& settings = model->GetVmdlIKSettings();
	std::string signature = std::to_string(settings.type) + "|" + settings.centerNode + "|";
	const auto& solverSettings = model->GetVmdlMultiLegIKSettings();
	signature += std::to_string(solverSettings.bodyHeightOffset) + "|" +
		std::to_string(solverSettings.interpolationEasing) + "|" +
		std::to_string(solverSettings.contactOffset) + "|" +
		std::to_string(solverSettings.maxUpCorrection) + "|" +
		std::to_string(solverSettings.maxDownCorrection) + "|";
	for (const auto& animation : model->GetAnimations()) signature += animation.name + "|";
	for (const auto& leg : settings.legs)
	{
		signature += leg.root + "|" + leg.mid + "|" + leg.tip + "|" + leg.contact + "|";
	}
	for (const auto& pole : model->GetVmdlIKPoles())
	{
		signature += std::to_string(pole.custom) + "|" + std::to_string(pole.position.x) + "|" +
					 std::to_string(pole.position.y) + "|" + std::to_string(pole.position.z) + "|";
	}
	if (signature != footIkPreviewSignature)
	{
		footIkPreviewSignature = std::move(signature);
		RebuildFootIkPreview();
	}
	if (!footIkPreviewOwner || !footIkPreviewAnimator) return false;

	auto& layer = footIkPreviewAnimator->GetLayer(0);
	if (selectedAnimation >= Count(layer.states)) return false;
	layer.currentStateIndex = selectedAnimation;
	layer.currentTime = animationTime;
	model->UpdateTransform(Matrix::Identity);
	footIkPreviewOwner->LateUpdate();
	return true;
}

void VmdlEditorScene::DrawHierarchy()
{
	ImGui::TextUnformatted((const char*)u8"ヒエラルキー");
	ImGui::Separator();

	// ノードとコンポーネントの検索
	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::InputTextWithHint("##HierarchySearch",
			(const char*)(ICON_FA_SEARCH u8" ノード・メッシュ・コンポーネントを検索"),
			&hierarchySearch))
		hierarchySearchUpper = ToUpperString(hierarchySearch);
	ImGui::Separator();
	ImGui::BeginChild("HierarchyTree", ImVec2(0.0f, 0.0f), false,
		ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar);
	if (MatchesHierarchySearch((const char*)u8"プレビューステージ Preview Stage"))
	{
		if (ImGui::Selectable((const char*)u8"プレビューステージ", previewStageSelected))
		{
			previewStageSelected = true;
			selectedNode = -1;
			selectedNodes.clear();
			selectedMesh = -1;
			selectedMeshes.clear();
			selectedComponentType = AttachedComponentType::None;
			selectedComponentIndex = -1;
			selectedComponentIndices.clear();
			focusSelectedComponent = false;
			showSelectionGizmo = false;
			ikGizmoTarget = IkGizmoTarget::None;
			selectedKeyTrack = -1;
			selectedKeyIndex = -1;
		}
	}
	if (!model)
	{
		ImGui::TextDisabled((const char*)u8"モデルが読み込まれていません");
		ImGui::EndChild();
		return;
	}

	// 検索条件に一致したノードツリー
	const auto& nodes = model->GetNodes();
	bool found = MatchesHierarchySearch((const char*)u8"プレビューステージ Preview Stage");
	for (int index = 0; index < Count(nodes); ++index)
	{
		if (nodes[index].parentIndex >= 0 || !NodeMatchesHierarchySearch(index)) continue;
		DrawNodeTree(index);
		found = true;
	}
	if (!found) ImGui::TextDisabled((const char*)u8"一致する項目がありません");
	ImGui::EndChild();
	// ツリーを描画し終えてから移動し、同じフレームでの二重表示を防ぐ。
	ReparentAttachedComponents();
}

bool VmdlEditorScene::MatchesHierarchySearch(const std::string& text) const
{
	if (hierarchySearchUpper.empty()) return true;
	return ToUpperString(text).find(hierarchySearchUpper) != std::string::npos;
}

bool VmdlEditorScene::ComponentMatchesHierarchySearch(
	const char* typeNames, const std::string& name) const
{
	return MatchesHierarchySearch(typeNames) || MatchesHierarchySearch(name);
}

bool VmdlEditorScene::NodeMatchesHierarchySearch(int nodeIndex) const
{
	if (hierarchySearchUpper.empty()) return true;

	const auto& nodes = model->GetNodes();
	const VMDLModel::Node& node = nodes[nodeIndex];
	if (MatchesHierarchySearch(MakeNodeLabel(nodeIndex, node.name))) return true;

	for (const VMDLModel::Mesh& mesh : model->GetMeshes())
	{
		if (mesh.nodeIndex != nodeIndex) continue;
		if (MatchesHierarchySearch((const char*)u8"メッシュ MESH") ||
			MatchesHierarchySearch(mesh.material->name))
			return true;
	}

	const auto& data = model->GetVmdlExtensionData();
	for (const auto& value : data.rigidBodies)
	{
		if (value.nodeIndex == nodeIndex && ComponentMatchesHierarchySearch(
				(const char*)u8"リジッドボディ RIGID BODY", value.name))
			return true;
	}
	for (const auto& value : data.colliders)
	{
		if (value.nodeIndex == nodeIndex && ComponentMatchesHierarchySearch(
				(const char*)u8"コライダー COLLIDER", value.name))
			return true;
	}
	for (const auto& value : data.springs)
	{
		if (value.nodeIndex == nodeIndex && ComponentMatchesHierarchySearch(
				(const char*)u8"スプリング SPRING", value.name))
			return true;
	}
	for (const auto& value : data.springColliders)
	{
		if (value.nodeIndex == nodeIndex && ComponentMatchesHierarchySearch(
				(const char*)u8"スプリングコライダー SPRING COLLIDER", value.name))
			return true;
	}
	for (const auto& value : model->GetVmdlTrailData().trails)
	{
		if (value.nodeIndex == nodeIndex && ComponentMatchesHierarchySearch(
				(const char*)u8"トレイル TRAIL", value.name))
			return true;
	}
	for (const auto& value : model->GetVmdlParticleData().emitters)
	{
		if (value.nodeIndex == nodeIndex && ComponentMatchesHierarchySearch(
				(const char*)u8"パーティクル PARTICLE", value.name))
			return true;
	}
	for (const auto& value : model->GetVmdlSoundData().sources)
	{
		if (value.nodeIndex == nodeIndex && ComponentMatchesHierarchySearch(
				(const char*)u8"サウンドソース SOUND SOURCE", value.name))
			return true;
	}
	for (const auto& value : model->GetVmdlLightData().pointLights)
	{
		if (value.nodeIndex == nodeIndex && ComponentMatchesHierarchySearch(
				(const char*)u8"ポイントライト POINT LIGHT", value.name))
			return true;
	}
	for (const auto& value : model->GetVmdlPresentationData().cameraShakes)
		if (value.nodeIndex == nodeIndex && ComponentMatchesHierarchySearch(
				(const char*)u8"カメラシェイク CAMERA SHAKE", value.name)) return true;
	for (const auto& value : model->GetVmdlPresentationData().radialBlurs)
		if (value.nodeIndex == nodeIndex && ComponentMatchesHierarchySearch(
				(const char*)u8"ラジアルブラー RADIAL BLUR", value.name)) return true;

	for (const VMDLModel::Node* child : node.children)
	{
		if (NodeMatchesHierarchySearch(static_cast<int>(child - nodes.data()))) return true;
	}
	return false;
}

void VmdlEditorScene::SelectNode(int nodeIndex, bool toggleSelection)
{
	previewStageSelected = false;
	if (!model || nodeIndex < 0 || nodeIndex >= Count(model->GetNodes())) return;
	showSelectionGizmo = true;
	ikGizmoTarget = IkGizmoTarget::None;

	const auto selected = std::find(selectedNodes.begin(), selectedNodes.end(), nodeIndex);
	if (!toggleSelection)
	{
		selectedNodes.assign(1, nodeIndex);
		selectedNode = nodeIndex;
	}
	else if (selected != selectedNodes.end())
	{
		selectedNodes.erase(selected);
		if (selectedNode == nodeIndex)
			selectedNode = selectedNodes.empty() ? -1 : selectedNodes.back();
	}
	else
	{
		selectedNodes.push_back(nodeIndex);
		selectedNode = nodeIndex;
	}

	selectedMesh = -1;
	selectedMeshes.clear();
	selectedComponentType = AttachedComponentType::None;
	selectedComponentIndex = -1;
	selectedComponentIndices.clear();
	focusSelectedComponent = false;
	selectedKeyTrack = -1;
	selectedKeyIndex = -1;
}

bool VmdlEditorScene::IsNodeSelected(int nodeIndex) const
{
	return std::find(selectedNodes.begin(), selectedNodes.end(), nodeIndex) != selectedNodes.end();
}

// メッシュを単独選択またはCtrlによる複数選択へ反映する
void VmdlEditorScene::SelectMesh(int meshIndex, bool toggleSelection)
{
	previewStageSelected = false;
	if (!model || meshIndex < 0 || meshIndex >= Count(model->GetMeshes())) return;
	showSelectionGizmo = true;
	ikGizmoTarget = IkGizmoTarget::None;

	const auto selected = std::find(selectedMeshes.begin(), selectedMeshes.end(), meshIndex);
	if (!toggleSelection)
	{
		selectedMeshes.assign(1, meshIndex);
		selectedMesh = meshIndex;
	}
	else if (selected != selectedMeshes.end())
	{
		selectedMeshes.erase(selected);
		selectedMesh = selectedMeshes.empty() ? -1 : selectedMeshes.back();
	}
	else
	{
		selectedMeshes.push_back(meshIndex);
		selectedMesh = meshIndex;
	}

	if (selectedMesh >= 0)
	{
		selectedNode = model->GetMeshes()[selectedMesh].nodeIndex;
		selectedNodes.assign(1, selectedNode);
		selectedMaterial = model->GetMeshes()[selectedMesh].materialIndex;
	}
	selectedComponentType = AttachedComponentType::None;
	selectedComponentIndex = -1;
	selectedComponentIndices.clear();
	focusSelectedComponent = false;
	selectedKeyTrack = -1;
	selectedKeyIndex = -1;
}

// 指定メッシュが複数選択に含まれているかを返す
bool VmdlEditorScene::IsMeshSelected(int meshIndex) const
{
	return std::find(selectedMeshes.begin(), selectedMeshes.end(), meshIndex) !=
		selectedMeshes.end();
}

std::string VmdlEditorScene::MakeUniqueAttachedComponentName(
	AttachedComponentType type, const std::string& baseName) const
{
	std::vector<std::string> names;
	const auto& data = model->GetVmdlExtensionData();
	switch (type)
	{
	case AttachedComponentType::RigidBody:
		for (const auto& value : data.rigidBodies) names.push_back(value.name);
		break;
	case AttachedComponentType::Collider:
		for (const auto& value : data.colliders) names.push_back(value.name);
		break;
	case AttachedComponentType::Spring:
		for (const auto& value : data.springs) names.push_back(value.name);
		break;
	case AttachedComponentType::SpringCollider:
		for (const auto& value : data.springColliders) names.push_back(value.name);
		break;
	case AttachedComponentType::Trail:
		for (const auto& value : model->GetVmdlTrailData().trails) names.push_back(value.name);
		break;
	case AttachedComponentType::Particle:
		for (const auto& value : model->GetVmdlParticleData().emitters) names.push_back(value.name);
		break;
	case AttachedComponentType::SoundSource:
		for (const auto& value : model->GetVmdlSoundData().sources) names.push_back(value.name);
		break;
	case AttachedComponentType::PointLight:
		for (const auto& value : model->GetVmdlLightData().pointLights) names.push_back(value.name);
		break;
	case AttachedComponentType::CameraShake:
		for (const auto& value : model->GetVmdlPresentationData().cameraShakes) names.push_back(value.name);
		break;
	case AttachedComponentType::RadialBlur:
		for (const auto& value : model->GetVmdlPresentationData().radialBlurs) names.push_back(value.name);
		break;
	default:
		break;
	}

	const std::string base = ToUpperString(baseName);
	if (std::find(names.begin(), names.end(), base) == names.end()) return base;
	for (int suffix = 2;; ++suffix)
	{
		const std::string candidate = base + " " + std::to_string(suffix);
		if (std::find(names.begin(), names.end(), candidate) == names.end()) return candidate;
	}
}

void VmdlEditorScene::AddAttachedComponentToSelectedNodes(AttachedComponentType type)
{
	if (!model || selectedNodes.empty()) return;

	auto& data = model->GetVmdlExtensionData();
	int primaryComponentIndex = -1;
	selectedComponentIndices.clear();
	for (int nodeIndex : selectedNodes)
	{
		if (nodeIndex < 0 || nodeIndex >= Count(model->GetNodes())) continue;

		int componentIndex = -1;
		switch (type)
		{
		case AttachedComponentType::RigidBody:
		{
			const std::string name =
				MakeUniqueAttachedComponentName(type, "RIGIDBODY");
			componentIndex = Count(data.rigidBodies);
			auto& value = data.rigidBodies.emplace_back();
			value.name = name;
			value.nodeIndex = nodeIndex;
			break;
		}
		case AttachedComponentType::Collider:
		{
			const std::string name = MakeUniqueAttachedComponentName(type, "COLLIDER");
			componentIndex = Count(data.colliders);
			auto& value = data.colliders.emplace_back();
			value.name = name;
			value.nodeIndex = nodeIndex;
			break;
		}
		case AttachedComponentType::Spring:
		{
			const std::string name = MakeUniqueAttachedComponentName(type, "SPRING");
			componentIndex = Count(data.springs);
			auto& value = data.springs.emplace_back();
			value.name = name;
			value.nodeIndex = nodeIndex;
			break;
		}
		case AttachedComponentType::SpringCollider:
		{
			const std::string name =
				MakeUniqueAttachedComponentName(type, "SPRING COLLIDER");
			componentIndex = Count(data.springColliders);
			auto& value = data.springColliders.emplace_back();
			value.name = name;
			value.nodeIndex = nodeIndex;
			break;
		}
		case AttachedComponentType::Trail:
		{
			const std::string name = MakeUniqueAttachedComponentName(type, "TRAIL");
			auto& trails = model->GetVmdlTrailData().trails;
			componentIndex = Count(trails);
			auto& value = trails.emplace_back();
			value.name = name;
			value.nodeIndex = nodeIndex;
			break;
		}
		case AttachedComponentType::Particle:
		{
			const std::string name = MakeUniqueAttachedComponentName(type, "PARTICLE");
			auto& emitters = model->GetVmdlParticleData().emitters;
			componentIndex = Count(emitters);
			auto& value = emitters.emplace_back();
			value.name = name;
			value.nodeIndex = nodeIndex;
			model->SetParticleInitialActive(componentIndex, false);
			break;
		}
		case AttachedComponentType::SoundSource:
		{
			const std::string name = MakeUniqueAttachedComponentName(type, "SOUND SOURCE");
			auto& sources = model->GetVmdlSoundData().sources;
			componentIndex = Count(sources);
			auto& value = sources.emplace_back();
			value.name = name;
			value.nodeIndex = nodeIndex;
			break;
		}
		case AttachedComponentType::PointLight:
		{
			auto& values = model->GetVmdlLightData().pointLights;
			componentIndex = Count(values);
			auto& value = values.emplace_back();
			value.name = MakeUniqueAttachedComponentName(type, "POINT LIGHT");
			value.nodeIndex = nodeIndex;
			break;
		}
		case AttachedComponentType::CameraShake:
		{
			auto& values = model->GetVmdlPresentationData().cameraShakes;
			componentIndex = Count(values);
			auto& value = values.emplace_back();
			value.name = MakeUniqueAttachedComponentName(type, "CAMERA SHAKE");
			value.nodeIndex = nodeIndex;
			break;
		}
		case AttachedComponentType::RadialBlur:
		{
			auto& values = model->GetVmdlPresentationData().radialBlurs;
			componentIndex = Count(values);
			auto& value = values.emplace_back();
			value.name = MakeUniqueAttachedComponentName(type, "RADIAL BLUR");
			value.nodeIndex = nodeIndex;
			break;
		}
		default:
			break;
		}
		if (componentIndex >= 0) selectedComponentIndices.push_back(componentIndex);
		if (nodeIndex == selectedNode) primaryComponentIndex = componentIndex;
	}

	selectedMesh = -1;
	selectedMeshes.clear();
	selectedComponentType = type;
	selectedComponentIndex = primaryComponentIndex;
	focusSelectedComponent = primaryComponentIndex >= 0;
	MarkDirty();
}

void VmdlEditorScene::DeleteSelection()
{
	if (!model || previewStageSelected) return;
	if (selectedComponentType != AttachedComponentType::None)
	{
		const auto type = selectedComponentType;
		auto indices = selectedComponentIndices;
		if (indices.empty() && selectedComponentIndex >= 0) indices.push_back(selectedComponentIndex);
		std::sort(indices.begin(), indices.end());
		indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
		StopUnifiedPreview(true);
		for (auto it = indices.rbegin(); it != indices.rend(); ++it) DeleteAttachedComponent(type, *it);
		selectedComponentType = AttachedComponentType::None;
		selectedComponentIndex = -1;
		selectedComponentIndices.clear();
		timelineSelectedKeys.clear();
		selectedKeyTrack = selectedKeyIndex = -1;
		StopUnifiedPreview(true);
		return;
	}

	auto meshes = selectedMeshes;
	std::vector<int> nodes;
	if (meshes.empty())
	{
		const auto& values = model->GetNodes();
		for (int index = 0; index < Count(values); ++index)
		{
			for (int parent = index; parent >= 0; parent = values[parent].parentIndex)
			{
				if (!IsNodeSelected(parent)) continue;
				nodes.push_back(index);
				break;
			}
		}
	}
	if (meshes.empty() && nodes.empty()) return;
	const auto deletingNode = [&](int index) {
		return std::find(nodes.begin(), nodes.end(), index) != nodes.end();
	};
	// ほかのメッシュが使用中のボーンは、スキニングを壊すため削除しない。
	for (const auto& mesh : model->GetMeshes())
	{
		if (deletingNode(mesh.nodeIndex)) continue;
		for (const auto& bone : mesh.bones)
		{
			if (!deletingNode(bone.nodeIndex)) continue;
			ErrorMessage("This node is used by another mesh as a bone.");
			return;
		}
	}
	StopUnifiedPreview(true);
	footIkPreviewOwner.reset();
	footIkPreviewAnimator = nullptr;
	footIkPreviewSignature.clear();
	const auto deleteOnNodes = [&](AttachedComponentType type, const auto& values) {
		for (int index = Count(values) - 1; index >= 0; --index)
			if (deletingNode(values[index].nodeIndex)) DeleteAttachedComponent(type, index);
	};
	auto& data = model->GetVmdlExtensionData();
	deleteOnNodes(AttachedComponentType::RigidBody, data.rigidBodies);
	deleteOnNodes(AttachedComponentType::Collider, data.colliders);
	deleteOnNodes(AttachedComponentType::Spring, data.springs);
	deleteOnNodes(AttachedComponentType::SpringCollider, data.springColliders);
	deleteOnNodes(AttachedComponentType::Trail, model->GetVmdlTrailData().trails);
	deleteOnNodes(AttachedComponentType::Particle, model->GetVmdlParticleData().emitters);
	deleteOnNodes(AttachedComponentType::SoundSource, model->GetVmdlSoundData().sources);
	deleteOnNodes(AttachedComponentType::PointLight, model->GetVmdlLightData().pointLights);
	deleteOnNodes(AttachedComponentType::CameraShake, model->GetVmdlPresentationData().cameraShakes);
	deleteOnNodes(AttachedComponentType::RadialBlur, model->GetVmdlPresentationData().radialBlurs);
	const bool changed = nodes.empty() ? model->RemoveMeshes(meshes) : model->RemoveNodes(nodes);
	if (!changed) return;
	selectedNode = selectedMesh = selectedMaterial = -1;
	selectedNodes.clear();
	selectedMeshes.clear();
	selectedComponentType = AttachedComponentType::None;
	selectedComponentIndex = -1;
	selectedComponentIndices.clear();
	showSelectionGizmo = false;
	timelineSelectedKeys.clear();
	selectedKeyTrack = selectedKeyIndex = -1;
	externalMeshPreviewCaches.clear();
	StopUnifiedPreview(true);
	UpdateModelFraming();
	MarkDirty();
}

void VmdlEditorScene::DuplicateSelectedAttachedComponent()
{
	if (!model || selectedComponentIndex < 0) return;

	auto& extension = model->GetVmdlExtensionData();
	const int sourceIndex = selectedComponentIndex;
	int duplicateIndex = -1;

	switch (selectedComponentType)
	{
	case AttachedComponentType::RigidBody:
		if (sourceIndex >= Count(extension.rigidBodies)) return;
		duplicateIndex = Count(extension.rigidBodies);
		extension.rigidBodies.push_back(extension.rigidBodies[sourceIndex]);
		extension.rigidBodies.back().name = MakeUniqueAttachedComponentName(
			selectedComponentType, extension.rigidBodies[sourceIndex].name);
		break;
	case AttachedComponentType::Collider:
	{
		if (sourceIndex >= Count(extension.colliders)) return;
		const bool initialActive = model->GetColliderInitialActive(sourceIndex);
		duplicateIndex = Count(extension.colliders);
		extension.colliders.push_back(extension.colliders[sourceIndex]);
		extension.colliders.back().name = MakeUniqueAttachedComponentName(
			selectedComponentType, extension.colliders[sourceIndex].name);
		model->SetColliderInitialActive(duplicateIndex, initialActive);
		auto& tracks = model->GetVmdlAnimationControlData().colliderTracks;
		std::vector<VMDLModel::VmdlColliderAnimationTrack> duplicates;
		for (const auto& track : tracks)
		{
			if (track.colliderIndex != sourceIndex) continue;
			duplicates.push_back(track);
			duplicates.back().colliderIndex = duplicateIndex;
		}
		tracks.insert(tracks.end(), duplicates.begin(), duplicates.end());
		previewColliderActive.push_back(initialActive ? 1 : 0);
		break;
	}
	case AttachedComponentType::Spring:
		if (sourceIndex >= Count(extension.springs)) return;
		duplicateIndex = Count(extension.springs);
		extension.springs.push_back(extension.springs[sourceIndex]);
		extension.springs.back().name = MakeUniqueAttachedComponentName(
			selectedComponentType, extension.springs[sourceIndex].name);
		RebuildSpringPreview();
		break;
	case AttachedComponentType::SpringCollider:
		if (sourceIndex >= Count(extension.springColliders)) return;
		duplicateIndex = Count(extension.springColliders);
		extension.springColliders.push_back(extension.springColliders[sourceIndex]);
		extension.springColliders.back().name = MakeUniqueAttachedComponentName(
			selectedComponentType, extension.springColliders[sourceIndex].name);
		RebuildSpringPreview();
		break;
	case AttachedComponentType::Trail:
	{
		auto& data = model->GetVmdlTrailData();
		if (sourceIndex >= Count(data.trails)) return;
		const bool initialActive = model->GetTrailInitialActive(sourceIndex);
		duplicateIndex = Count(data.trails);
		data.trails.push_back(data.trails[sourceIndex]);
		data.trails.back().name = MakeUniqueAttachedComponentName(
			selectedComponentType, data.trails[sourceIndex].name);
		model->SetTrailInitialActive(duplicateIndex, initialActive);
		std::vector<VMDLModel::VmdlTrailAnimationTrack> duplicates;
		for (const auto& track : data.tracks)
		{
			if (track.trailIndex != sourceIndex) continue;
			duplicates.push_back(track);
			duplicates.back().trailIndex = duplicateIndex;
		}
		data.tracks.insert(data.tracks.end(), duplicates.begin(), duplicates.end());
		previewTrailActive.push_back(initialActive ? 1 : 0);
		RebuildTrailPreview();
		break;
	}
	case AttachedComponentType::Particle:
	{
		auto& data = model->GetVmdlParticleData();
		if (sourceIndex >= Count(data.emitters)) return;
		const bool initialActive = model->GetParticleInitialActive(sourceIndex);
		duplicateIndex = Count(data.emitters);
		data.emitters.push_back(data.emitters[sourceIndex]);
		data.emitters.back().name = MakeUniqueAttachedComponentName(
			selectedComponentType, data.emitters[sourceIndex].name);
		model->SetParticleInitialActive(duplicateIndex, initialActive);
		std::vector<VMDLModel::VmdlParticleAnimationTrack> duplicates;
		for (const auto& track : data.tracks)
		{
			if (track.emitterIndex != sourceIndex) continue;
			duplicates.push_back(track);
			duplicates.back().emitterIndex = duplicateIndex;
		}
		data.tracks.insert(data.tracks.end(), duplicates.begin(), duplicates.end());
		previewParticleActive.push_back(initialActive ? 1 : 0);
		RebuildParticlePreview();
		break;
	}
	case AttachedComponentType::SoundSource:
	{
		auto& data = model->GetVmdlSoundData();
		if (sourceIndex >= Count(data.sources)) return;
		duplicateIndex = Count(data.sources);
		data.sources.push_back(data.sources[sourceIndex]);
		data.sources.back().name = MakeUniqueAttachedComponentName(
			selectedComponentType, data.sources[sourceIndex].name);
		for (auto& track : data.tracks)
		{
			std::vector<VMDLModel::VmdlSoundKeyframe> duplicates;
			for (const auto& key : track.keys)
			{
				if (key.sourceIndex != sourceIndex) continue;
				duplicates.push_back(key);
				duplicates.back().sourceIndex = duplicateIndex;
			}
			track.keys.insert(track.keys.end(), duplicates.begin(), duplicates.end());
		}
		break;
	}
	case AttachedComponentType::PointLight:
	{
		auto& values = model->GetVmdlLightData().pointLights;
		if (sourceIndex >= Count(values)) return;
		duplicateIndex = Count(values);
		values.push_back(values[sourceIndex]);
		values.back().name = MakeUniqueAttachedComponentName(
			selectedComponentType, values[sourceIndex].name);
		break;
	}
	case AttachedComponentType::CameraShake:
	case AttachedComponentType::RadialBlur:
	{
		auto& data = model->GetVmdlPresentationData();
		auto duplicatePresentation = [this, sourceIndex, &duplicateIndex](auto& values, auto& tracks)
		{
			if (sourceIndex >= Count(values)) return false;
			duplicateIndex = Count(values);
			values.push_back(values[sourceIndex]);
			values.back().name = MakeUniqueAttachedComponentName(
				selectedComponentType, values[sourceIndex].name);
			for (auto& track : tracks)
			{
				std::vector<VMDLModel::VmdlPresentationKeyframe> duplicates;
				for (const auto& key : track.keys)
				{
					if (key.componentIndex != sourceIndex) continue;
					duplicates.push_back(key);
					duplicates.back().componentIndex = duplicateIndex;
				}
				track.keys.insert(track.keys.end(), duplicates.begin(), duplicates.end());
			}
			return true;
		};
		if (selectedComponentType == AttachedComponentType::CameraShake)
		{
			if (!duplicatePresentation(data.cameraShakes, data.cameraShakeTracks)) return;
		}
		else if (!duplicatePresentation(data.radialBlurs, data.radialBlurTracks)) return;
		break;
	}
	default:
		return;
	}

	selectedComponentIndex = duplicateIndex;
	selectedComponentIndices.assign(1, duplicateIndex);
	focusSelectedComponent = true;
	MarkDirty();
}

void VmdlEditorScene::DrawHierarchyComponent(int nodeIndex, AttachedComponentType type,
	int componentIndex, const char* label, const std::string& name)
{
	ImGui::PushID(static_cast<int>(type));
	ImGui::PushID(componentIndex);
	const std::string componentName = name.empty() ? label : std::string(label) + " : " + name;
	const std::string displayName = std::string((const char*)u8"│  ") + componentName;
	const bool selected = selectedComponentType == type &&
		(selectedComponentIndex == componentIndex ||
			std::find(selectedComponentIndices.begin(), selectedComponentIndices.end(), componentIndex) != selectedComponentIndices.end());
	if (ImGui::Selectable(displayName.c_str(), selected))
	{
		const bool extendSelection = ImGui::GetIO().KeyCtrl && selectedComponentType == type;
		if (!extendSelection)
		{
			SelectNode(nodeIndex, false);
			selectedComponentIndices.assign(1, componentIndex);
		}
		else
		{
			if (selectedComponentIndices.empty() && selectedComponentIndex >= 0)
				selectedComponentIndices.push_back(selectedComponentIndex);
			const auto found = std::find(selectedComponentIndices.begin(), selectedComponentIndices.end(), componentIndex);
			if (found == selectedComponentIndices.end()) selectedComponentIndices.push_back(componentIndex);
			else selectedComponentIndices.erase(found);
		}
		selectedComponentType = selectedComponentIndices.empty() ? AttachedComponentType::None : type;
		selectedComponentIndex = selectedComponentIndices.empty() ? -1 : selectedComponentIndices.back();
		selectedNode = GetComponentNodeIndex(selectedComponentIndex);
		selectedNodes.clear();
		if (selectedNode >= 0) selectedNodes.push_back(selectedNode);
		selectedMesh = -1;
		selectedMeshes.clear();
		previewStageSelected = false;
		showSelectionGizmo = true;
		ikGizmoTarget = IkGizmoTarget::None;
		focusSelectedComponent = true;
		selectedKeyTrack = -1;
		selectedKeyIndex = -1;
	}
	if (ImGui::BeginDragDropSource())
	{
		std::vector<int> payload{static_cast<int>(type)};
		if (selectedComponentType == type &&
			std::find(selectedComponentIndices.begin(), selectedComponentIndices.end(), componentIndex) !=
				selectedComponentIndices.end())
			payload.insert(payload.end(), selectedComponentIndices.begin(), selectedComponentIndices.end());
		else payload.push_back(componentIndex);
		ImGui::SetDragDropPayload("VMDL_COMPONENT_PARENT", payload.data(), payload.size() * sizeof(int));
		ImGui::TextUnformatted(componentName.c_str());
		if (payload.size() > 2)
			ImGui::Text((const char*)u8"%dコンポーネント", static_cast<int>(payload.size()) - 1);
		ImGui::EndDragDropSource();
	}
	ImGui::PopID();
	ImGui::PopID();
}

void VmdlEditorScene::ReparentAttachedComponents()
{
	if (pendingComponentParentNode < 0) return;
	const int nodeIndex = pendingComponentParentNode;
	const auto type = pendingComponentParentType;
	auto indices = std::move(pendingComponentParentIndices);
	pendingComponentParentNode = -1;
	pendingComponentParentType = AttachedComponentType::None;
	pendingComponentParentIndices.clear();
	if (!model || nodeIndex >= Count(model->GetNodes())) return;

	bool changed = false;
	const auto reparent = [&](auto& values) {
		std::erase_if(indices, [&](int index) { return index < 0 || index >= Count(values); });
		for (int index : indices)
		{
			if (values[index].nodeIndex == nodeIndex) continue;
			values[index].nodeIndex = nodeIndex;
			changed = true;
		}
	};
	auto& extension = model->GetVmdlExtensionData();
	switch (type)
	{
	case AttachedComponentType::RigidBody: reparent(extension.rigidBodies); break;
	case AttachedComponentType::Collider: reparent(extension.colliders); break;
	case AttachedComponentType::Spring: reparent(extension.springs); break;
	case AttachedComponentType::SpringCollider: reparent(extension.springColliders); break;
	case AttachedComponentType::Trail: reparent(model->GetVmdlTrailData().trails); break;
	case AttachedComponentType::Particle: reparent(model->GetVmdlParticleData().emitters); break;
	case AttachedComponentType::SoundSource: reparent(model->GetVmdlSoundData().sources); break;
	case AttachedComponentType::PointLight: reparent(model->GetVmdlLightData().pointLights); break;
	case AttachedComponentType::CameraShake: reparent(model->GetVmdlPresentationData().cameraShakes); break;
	case AttachedComponentType::RadialBlur: reparent(model->GetVmdlPresentationData().radialBlurs); break;
	default: return;
	}
	if (!changed) return;

	selectedComponentType = type;
	selectedComponentIndices = std::move(indices);
	selectedComponentIndex = selectedComponentIndices.back();
	selectedNode = nodeIndex;
	selectedNodes.assign(1, nodeIndex);
	selectedMesh = -1;
	selectedMeshes.clear();
	previewStageSelected = false;
	showSelectionGizmo = true;
	ikGizmoTarget = IkGizmoTarget::None;
	focusSelectedComponent = true;
	selectedKeyTrack = selectedKeyIndex = -1;
	StopUnifiedPreview(true);
	MarkDirty();
}

void VmdlEditorScene::DrawNodeTree(int nodeIndex)
{
	auto& io = ImGui::GetIO();
	const auto& nodes = model->GetNodes();
	const VMDLModel::Node& node = nodes[nodeIndex];
	const auto& extension = model->GetVmdlExtensionData();
	bool hasMesh = false;
	for (const VMDLModel::Mesh& mesh : model->GetMeshes())
	{
		if (mesh.nodeIndex == nodeIndex) hasMesh = true;
	}
	const bool hasComponent =
		std::any_of(extension.rigidBodies.begin(), extension.rigidBodies.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }) ||
		std::any_of(extension.colliders.begin(), extension.colliders.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }) ||
		std::any_of(extension.springs.begin(), extension.springs.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }) ||
		std::any_of(extension.springColliders.begin(), extension.springColliders.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }) ||
	std::any_of(model->GetVmdlTrailData().trails.begin(),
			model->GetVmdlTrailData().trails.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }) ||
		std::any_of(model->GetVmdlParticleData().emitters.begin(),
			model->GetVmdlParticleData().emitters.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }) ||
		std::any_of(model->GetVmdlSoundData().sources.begin(),
			model->GetVmdlSoundData().sources.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }) ||
		std::any_of(model->GetVmdlLightData().pointLights.begin(),
			model->GetVmdlLightData().pointLights.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }) ||
		std::any_of(model->GetVmdlPresentationData().cameraShakes.begin(),
			model->GetVmdlPresentationData().cameraShakes.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }) ||
		std::any_of(model->GetVmdlPresentationData().radialBlurs.begin(),
			model->GetVmdlPresentationData().radialBlurs.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; });

	const bool selected = IsNodeSelected(nodeIndex) && selectedMesh < 0 &&
		selectedComponentType == AttachedComponentType::None;
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen |
							   ImGuiTreeNodeFlags_SpanAvailWidth;
	if (node.children.empty() && !hasMesh && !hasComponent) flags |= ImGuiTreeNodeFlags_Leaf;
	if (selected) flags |= ImGuiTreeNodeFlags_Selected;
	const std::string nodeLabel = MakeNodeLabel(nodeIndex, node.name);
	const bool showAllContents =
		hierarchySearchUpper.empty() || MatchesHierarchySearch(nodeLabel);
	if (!hierarchySearchUpper.empty()) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
	if (selected)
	{
		ImGui::PushStyleColor(ImGuiCol_Header, ImGuiTheme::Selected);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGuiTheme::Selected);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGuiTheme::Selected);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
	}
	const bool open =
		ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(nodeIndex + 1)), flags,
			"%s", nodeLabel.c_str());
	const ImVec2 itemMin = ImGui::GetItemRectMin();
	const ImVec2 itemMax = ImGui::GetItemRectMax();
	if (selected)
	{
		ImGui::PopStyleColor(4);
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRect(itemMin, itemMax, ImGuiTheme::SelectedOutline, 2.0f, 0, 2.0f);
		drawList->AddRectFilled(
			itemMin, ImVec2(itemMin.x + 4.0f, itemMax.y), ImGuiTheme::SelectedAccent);
	}
	const bool nodeClicked = ImGui::IsItemClicked();
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("VMDL_COMPONENT_PARENT"))
		{
			if (payload->DataSize >= static_cast<int>(2 * sizeof(int)) &&
				payload->DataSize % sizeof(int) == 0)
			{
				const int* values = static_cast<const int*>(payload->Data);
				pendingComponentParentType = static_cast<AttachedComponentType>(values[0]);
				pendingComponentParentIndices.assign(values + 1, values + payload->DataSize / sizeof(int));
				pendingComponentParentNode = nodeIndex;
			}
		}
		ImGui::EndDragDropTarget();
	}
	DrawNodeContextMenu(nodeIndex);
	const auto drawBadge = [](const char* text) {
		ImGui::SameLine(0.0f, 4.0f);
		ImGui::TextUnformatted(text);
	};
	if (std::any_of(extension.rigidBodies.begin(), extension.rigidBodies.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }))
		drawBadge("[RB]");
	if (std::any_of(extension.colliders.begin(), extension.colliders.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }))
		drawBadge("[C]");
	if (std::any_of(extension.springs.begin(), extension.springs.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }))
		drawBadge("[S]");
	if (std::any_of(extension.springColliders.begin(), extension.springColliders.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }))
		drawBadge("[SC]");
	const auto& trails = model->GetVmdlTrailData().trails;
	if (std::any_of(trails.begin(), trails.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }))
		drawBadge("[T]");
	const auto& particleEmitters = model->GetVmdlParticleData().emitters;
	if (std::any_of(particleEmitters.begin(), particleEmitters.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }))
		drawBadge("[P]");
	const auto& soundSources = model->GetVmdlSoundData().sources;
	if (std::any_of(soundSources.begin(), soundSources.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }))
		drawBadge("[SS]");
	const auto& pointLights = model->GetVmdlLightData().pointLights;
	if (std::any_of(pointLights.begin(), pointLights.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }))
		drawBadge("[PL]");
	const auto& cameraShakes = model->GetVmdlPresentationData().cameraShakes;
	if (std::any_of(cameraShakes.begin(), cameraShakes.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; })) drawBadge("[CS]");
	const auto& radialBlurs = model->GetVmdlPresentationData().radialBlurs;
	if (std::any_of(radialBlurs.begin(), radialBlurs.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; })) drawBadge("[RB]");
	if (nodeClicked)
	{
		SelectNode(nodeIndex, io.KeyCtrl);
	}
	if (!open) return;

	auto& meshes = model->GetMeshes();
	for (int meshIndex = 0; meshIndex < Count(meshes); ++meshIndex)
	{
		VMDLModel::Mesh& mesh = meshes[meshIndex];
		if (mesh.nodeIndex != nodeIndex) continue;
		const bool external = model->IsExternalMesh(meshIndex);
		const std::string label = (external ? "[VMSH] " : "") +
			std::string((const char*)u8"メッシュ ") + std::to_string(meshIndex) + " : " +
			mesh.material->name;
		if (!showAllContents && !MatchesHierarchySearch(label) &&
			!MatchesHierarchySearch((const char*)u8"メッシュ MESH"))
			continue;
		if (external)
			ImGui::PushStyleColor(ImGuiCol_Text, mesh.isDraw
				? ImVec4(0.20f, 0.85f, 1.0f, 1.0f) : ImVec4(0.20f, 0.60f, 0.70f, 0.65f));
		else if (!mesh.isDraw)
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		if (ImGui::Selectable(label.c_str(), IsMeshSelected(meshIndex)))
		{
			SelectMesh(meshIndex, io.KeyCtrl);
		}
		if (external || !mesh.isDraw) ImGui::PopStyleColor();
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			mesh.isDraw = !mesh.isDraw;
			MarkDirty();
		}
		if (ImGui::BeginPopupContextItem("Mesh Actions"))
		{
			// コンテキストメニューの操作対象は右クリックした1メッシュだけに限定する
			if (selectedMeshes.size() != 1 || !IsMeshSelected(meshIndex))
				SelectMesh(meshIndex, false);
			if (external)
			{
				if (ImGui::MenuItem((const char*)u8"VMDLへ結合"))
					RestoreExternalMesh(meshIndex);
			}
			else
			{
				if (ImGui::MenuItem((const char*)u8"VMSHへ分離", nullptr, false,
						selectedMorph >= 0))
					SeparateMeshToCache(meshIndex);
				if (selectedMorph < 0)
					ImGui::TextDisabled((const char*)u8"分離には適用先モーフの選択が必要です");
			}
			ImGui::EndPopup();
		}
	}
	for (int i = 0; i < Count(extension.rigidBodies); ++i)
	{
		const auto& value = extension.rigidBodies[i];
		if (value.nodeIndex != nodeIndex ||
			(!showAllContents && !ComponentMatchesHierarchySearch(
				(const char*)u8"リジッドボディ RIGID BODY", value.name)))
			continue;
		DrawHierarchyComponent(
			nodeIndex, AttachedComponentType::RigidBody, i, (const char*)(ICON_FA_CUBE " " u8"リジッドボディ"), value.name);
	}
	for (int i = 0; i < Count(extension.colliders); ++i)
	{
		const auto& value = extension.colliders[i];
		if (value.nodeIndex != nodeIndex ||
			(!showAllContents && !ComponentMatchesHierarchySearch(
				(const char*)u8"コライダー COLLIDER", value.name)))
			continue;
		DrawHierarchyComponent(
			nodeIndex, AttachedComponentType::Collider, i, (const char*)(ICON_FA_SHAPES " " u8"コライダー"), value.name);
	}
	for (int i = 0; i < Count(extension.springs); ++i)
	{
		const auto& value = extension.springs[i];
		if (value.nodeIndex != nodeIndex ||
			(!showAllContents && !ComponentMatchesHierarchySearch(
				(const char*)u8"スプリング SPRING", value.name)))
			continue;
		DrawHierarchyComponent(
			nodeIndex, AttachedComponentType::Spring, i, (const char*)(ICON_FA_LINK " " u8"スプリング"), value.name);
	}
	for (int i = 0; i < Count(extension.springColliders); ++i)
	{
		const auto& value = extension.springColliders[i];
		if (value.nodeIndex != nodeIndex ||
			(!showAllContents && !ComponentMatchesHierarchySearch(
				(const char*)u8"スプリングコライダー SPRING COLLIDER", value.name)))
			continue;
		DrawHierarchyComponent(nodeIndex, AttachedComponentType::SpringCollider, i,
			(const char*)(ICON_FA_BULLSEYE " " u8"スプリングコライダー"), value.name);
	}
	const auto& trailComponents = model->GetVmdlTrailData().trails;
	for (int i = 0; i < Count(trailComponents); ++i)
	{
		const auto& value = trailComponents[i];
		if (value.nodeIndex != nodeIndex ||
			(!showAllContents && !ComponentMatchesHierarchySearch(
				(const char*)u8"トレイル TRAIL", value.name)))
			continue;
		DrawHierarchyComponent(
			nodeIndex, AttachedComponentType::Trail, i, (const char*)(ICON_FA_WIND " " u8"トレイル"), value.name);
	}
	for (int i = 0; i < Count(particleEmitters); ++i)
	{
		const auto& value = particleEmitters[i];
		if (value.nodeIndex != nodeIndex ||
			(!showAllContents && !ComponentMatchesHierarchySearch(
				(const char*)u8"パーティクル PARTICLE", value.name)))
			continue;
		DrawHierarchyComponent(nodeIndex, AttachedComponentType::Particle, i,
			(const char*)(ICON_FA_MAGIC " " u8"パーティクル"), value.name);
	}
	for (int i = 0; i < Count(soundSources); ++i)
	{
		const auto& value = soundSources[i];
		if (value.nodeIndex != nodeIndex ||
			(!showAllContents && !ComponentMatchesHierarchySearch(
				(const char*)u8"サウンドソース SOUND SOURCE", value.name)))
			continue;
		DrawHierarchyComponent(nodeIndex, AttachedComponentType::SoundSource, i,
			(const char*)(ICON_FA_VOLUME_UP " " u8"サウンドソース"), value.name);
	}
	for (int i = 0; i < Count(pointLights); ++i)
	{
		const auto& value = pointLights[i];
		if (value.nodeIndex != nodeIndex ||
			(!showAllContents && !ComponentMatchesHierarchySearch(
				(const char*)u8"ポイントライト POINT LIGHT", value.name)))
			continue;
		DrawHierarchyComponent(nodeIndex, AttachedComponentType::PointLight, i,
			(const char*)(ICON_FA_LIGHTBULB " " u8"ポイントライト"), value.name);
	}
	for (int i = 0; i < Count(cameraShakes); ++i)
	{
		const auto& value = cameraShakes[i];
		if (value.nodeIndex != nodeIndex) continue;
		DrawHierarchyComponent(nodeIndex, AttachedComponentType::CameraShake, i,
			(const char*)(ICON_FA_CAMERA " " u8"カメラシェイク"), value.name);
	}
	for (int i = 0; i < Count(radialBlurs); ++i)
	{
		const auto& value = radialBlurs[i];
		if (value.nodeIndex != nodeIndex) continue;
		DrawHierarchyComponent(nodeIndex, AttachedComponentType::RadialBlur, i,
			(const char*)(ICON_FA_SUN " " u8"ラジアルブラー"), value.name);
	}
	for (const VMDLModel::Node* child : node.children)
	{
		const int childIndex = static_cast<int>(child - nodes.data());
		if (showAllContents || NodeMatchesHierarchySearch(childIndex)) DrawNodeTree(childIndex);
	}
	ImGui::TreePop();
}

void VmdlEditorScene::DrawNodeContextMenu(int nodeIndex)
{
	ImGui::PushID(nodeIndex);
	if (ImGui::BeginPopupContextItem("Node Actions"))
	{
		if (!IsNodeSelected(nodeIndex)) SelectNode(nodeIndex, false);
		const auto& node = model->GetNodes()[nodeIndex];
		if (selectedComponentIndices.size() > 1)
			ImGui::TextDisabled((const char*)u8"選択中: %zuコンポーネント", selectedComponentIndices.size());
		else if (selectedNodes.size() > 1)
			ImGui::TextDisabled((const char*)u8"選択中: %zuノード", selectedNodes.size());
		else
			ImGui::TextDisabled(
				(const char*)u8"ノード: %s", MakeNodeLabel(nodeIndex, node.name).c_str());
		ImGui::Separator();
		if (ImGui::BeginMenu((const char*)(ICON_FA_PLUS " " u8"追加")))
		{
			if (ImGui::BeginMenu((const char*)(ICON_FA_CAMERA " " u8"カメラ")))
			{
				if (ImGui::MenuItem((const char*)(ICON_FA_CAMERA " " u8"カメラシェイク")))
					AddAttachedComponentToSelectedNodes(AttachedComponentType::CameraShake);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu((const char*)(ICON_FA_MAGIC " " u8"ポストエフェクト")))
			{
				if (ImGui::MenuItem((const char*)(ICON_FA_SUN " " u8"ラジアルブラー")))
					AddAttachedComponentToSelectedNodes(AttachedComponentType::RadialBlur);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu((const char*)(ICON_FA_LIGHTBULB " " u8"ライト")))
			{
				if (ImGui::MenuItem((const char*)(ICON_FA_LIGHTBULB " " u8"ポイントライト")))
					AddAttachedComponentToSelectedNodes(AttachedComponentType::PointLight);
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem((const char*)(ICON_FA_CUBE " " u8"リジッドボディ")))
				AddAttachedComponentToSelectedNodes(AttachedComponentType::RigidBody);
			if (ImGui::MenuItem((const char*)(ICON_FA_SHAPES " " u8"コライダー")))
				AddAttachedComponentToSelectedNodes(AttachedComponentType::Collider);
			if (ImGui::MenuItem((const char*)(ICON_FA_LINK " " u8"スプリング")))
				AddAttachedComponentToSelectedNodes(AttachedComponentType::Spring);
			if (ImGui::MenuItem((const char*)(ICON_FA_BULLSEYE " " u8"スプリングコライダー")))
				AddAttachedComponentToSelectedNodes(AttachedComponentType::SpringCollider);
			if (ImGui::MenuItem((const char*)(ICON_FA_WIND " " u8"トレイル")))
				AddAttachedComponentToSelectedNodes(AttachedComponentType::Trail);
			if (ImGui::MenuItem((const char*)(ICON_FA_MAGIC " " u8"パーティクル")))
				AddAttachedComponentToSelectedNodes(AttachedComponentType::Particle);
			if (ImGui::MenuItem((const char*)(ICON_FA_VOLUME_UP " " u8"サウンドソース")))
				AddAttachedComponentToSelectedNodes(AttachedComponentType::SoundSource);
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();
}

void VmdlEditorScene::DrawPreviewToolbar()
{
	ImGui::TextUnformatted((const char*)u8"プレビュー");
	const float transportButtonWidth = 42.0f;
	const float loopWidth = 58.0f;
	const float speedWidth = 74.0f;
	ImGui::SameLine();
	if (ImGui::Button(animationPlaying ? ICON_FA_PAUSE "##UnifiedPlay" : ICON_FA_PLAY "##UnifiedPlay",
			ImVec2(transportButtonWidth, 0.0f)))
	{
		if (animationPlaying) StopUnifiedPreview(false); else StartUnifiedPreview();
	}
	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_STOP "##UnifiedStop", ImVec2(transportButtonWidth, 0.0f)))
		StopUnifiedPreview(true);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(loopWidth);
	ImGui::Checkbox((const char*)u8"ループ", &animationLoop);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(speedWidth);
	ImGui::DragFloat("##PlaybackSpeed", &playbackSpeed, 0.05f, 0.05f, 4.0f, "x%.2f");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"再生速度");
	ImGui::SameLine();
	if (ImGui::SmallButton((const char*)(ICON_FA_ARROWS_ALT " " u8"移動"))) gizmoOperation = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (ImGui::SmallButton((const char*)(ICON_FA_SYNC_ALT " " u8"回転"))) gizmoOperation = ImGuizmo::ROTATE;
	ImGui::SameLine();
	if (ImGui::SmallButton((const char*)(ICON_FA_EXPAND_ARROWS_ALT " " u8"サイズ"))) gizmoOperation = ImGuizmo::SCALE;
	ImGui::TextDisabled((const char*)u8"右ドラッグ: 見渡し / Alt+左ドラッグ: 回り込み / 中ドラッグ: 平行移動");
}

void VmdlEditorScene::DrawViewport(const ImVec2& inputMin, const ImVec2& inputMax)
{
	auto& io = ImGui::GetIO();
	const Mouse& mouseInput = Game::Input::Instance().GetMouse();
	const float yawDelta = -mouseInput.GetAxisX() * FreeCameraController::MouseRotationSensitivity;
	const float pitchDelta = mouseInput.GetAxisY() * FreeCameraController::MouseRotationSensitivity;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const ImVec2 imageMin = viewport->Pos;
	const ImVec2 imageMax(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
	if (viewportRotationDragging)
	{
		const std::string speedText = std::format("移動速度 {:.1f}  ホイールで調整", previewCameraMoveSpeed);
		ImGui::GetForegroundDrawList()->AddText(
			ImVec2(inputMin.x + 10.0f, inputMin.y + 10.0f), IM_COL32(255, 255, 255, 220), speedText.c_str());
	}
	// ImGuizmo自身のマウスキャプチャでギズモを無効にしない。
	const bool imageHovered = io.MousePos.x >= inputMin.x && io.MousePos.x <= inputMax.x &&
		io.MousePos.y >= inputMin.y && io.MousePos.y <= inputMax.y &&
		!ImGui::IsAnyItemActive() &&
		!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) &&
		!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
	if ((imageHovered || viewportRotationDragging) && !io.WantTextInput && io.KeyCtrl && io.MouseWheel != 0.0f)
		previewVerticalFovDegrees = std::clamp(previewVerticalFovDegrees - io.MouseWheel * 2.0f, 10.0f, 120.0f);
	if (imageHovered && !io.WantTextInput && !io.KeyCtrl)
	{
		if (io.KeyShift && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Z, false))
			showDebugOverlays = !showDebugOverlays;
		else if (!io.KeyAlt && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_C, false))
			showSpringCollider = !showSpringCollider;
		else if (!io.KeyAlt && !io.KeyShift)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_M, false)) showMesh = !showMesh;
			else if (ImGui::IsKeyPressed(ImGuiKey_B, false)) showBones = !showBones;
			else if (ImGui::IsKeyPressed(ImGuiKey_I, false)) showIkPole = !showIkPole;
			else if (ImGui::IsKeyPressed(ImGuiKey_C, false)) showCollider = !showCollider;
			else if (ImGui::IsKeyPressed(ImGuiKey_S, false)) showSpring = !showSpring;
			else if (ImGui::IsKeyPressed(ImGuiKey_T, false)) showTrail = !showTrail;
		}
	}

	if (imageHovered && io.KeyAlt && !viewportRotationDragging && !ImGuizmo::IsUsing() &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		viewportOrbitDragging = true;
		SetCapture(Game::Graphics::Instance().GetWindowHandle());
	}
	if (viewportOrbitDragging)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && io.KeyAlt)
			RotatePreviewCamera(yawDelta, pitchDelta, true);
		else
		{
			viewportOrbitDragging = false;
			if (GetCapture() == Game::Graphics::Instance().GetWindowHandle()) ReleaseCapture();
		}
	}
	const bool navigatingCamera = viewportRotationDragging || viewportOrbitDragging ||
		(imageHovered && (io.KeyAlt || ImGui::IsMouseDown(ImGuiMouseButton_Right)));
	bool gizmoHovered = false;
	ImGuizmo::Enable(imageHovered || ImGuizmo::IsUsingAny());
	ImGui::GetForegroundDrawList()->PushClipRect(inputMin, inputMax, true);
	if (!navigatingCamera && model && ikGizmoTarget != IkGizmoTarget::None)
	{
		const auto& legs = model->GetVmdlIKSettings().legs;
		auto& poles = model->GetVmdlIKPoles();
		auto& rays = model->GetVmdlIKRaySettings();
		if (ikGizmoLeg < 0 || ikGizmoLeg >= Count(legs) ||
			ikGizmoLeg >= Count(poles) || ikGizmoLeg >= Count(rays))
			ikGizmoTarget = IkGizmoTarget::None;
		else
		{
			auto& pole = poles[ikGizmoLeg];
			auto& ray = rays[ikGizmoLeg];
			const auto& leg = legs[ikGizmoLeg];
			const int contactIndex = model->GetNodeIndex(
				(leg.contact.empty() ? leg.tip : leg.contact).c_str());
			if ((ikGizmoTarget == IkGizmoTarget::Pole && !pole.custom) ||
				(ikGizmoTarget != IkGizmoTarget::Pole && (!ray.custom || contactIndex < 0)))
				ikGizmoTarget = IkGizmoTarget::None;
			else
			{
				const Matrix scaleTransform = model->GetRenderScaleTransform();
				const Vector3 contactPosition = contactIndex >= 0
					? (model->GetNodes()[contactIndex].worldTransform * scaleTransform).Translation()
					: Vector3::Zero;
				const Vector3 position = ikGizmoTarget == IkGizmoTarget::Pole
					? Vector3::Transform(pole.position, scaleTransform)
					: contactPosition + ray.startOffset +
						Vector3::TransformNormal(ray.syncOffset, scaleTransform);
				Matrix world = Matrix::CreateTranslation(position);
				Matrix view = editorCamera->GetView();
				Matrix projection = editorCamera->GetProjection();
				ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
				ImGuizmo::SetRect(imageMin.x, imageMin.y,
					imageMax.x - imageMin.x, imageMax.y - imageMin.y);
				if (ImGuizmo::Manipulate(&view._11, &projection._11,
						ImGuizmo::TRANSLATE, ImGuizmo::WORLD, &world._11))
				{
					if (ikGizmoTarget == IkGizmoTarget::Pole)
						pole.position = Vector3::Transform(world.Translation(), scaleTransform.Invert());
					else if (ikGizmoTarget == IkGizmoTarget::RayStart)
						ray.startOffset = world.Translation() - contactPosition -
							Vector3::TransformNormal(ray.syncOffset, scaleTransform);
					else
						ray.syncOffset = Vector3::TransformNormal(
							world.Translation() - contactPosition - ray.startOffset, scaleTransform.Invert());
					MarkDirty();
				}
				gizmoHovered = ImGuizmo::IsOver(ImGuizmo::UNIVERSAL) ||
					ImGuizmo::IsOver(&world._41, 24.0f);
			}
		}
	}
	else if (!navigatingCamera && model && showSelectionGizmo && selectedNode >= 0 && selectedNode < Count(model->GetNodes()))
	{
		Matrix view = editorCamera->GetView();
		Matrix projection = editorCamera->GetProjection();
		const Matrix renderScaleTransform = model->GetRenderScaleTransform();
		Matrix world = model->GetNodes()[selectedNode].worldTransform * renderScaleTransform;
		ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
		ImGuizmo::SetRect(imageMin.x, imageMin.y, imageMax.x - imageMin.x, imageMax.y - imageMin.y);
		if (ImGuizmo::Manipulate(&view._11, &projection._11,
				static_cast<ImGuizmo::OPERATION>(gizmoOperation), ImGuizmo::LOCAL, &world._11))
		{
			VMDLModel::Node& node = model->GetNodes()[selectedNode];
			Matrix global = world * renderScaleTransform.Invert();
			Matrix local = node.parent ? global * node.parent->globalTransform.Invert() : global;
			local.Decompose(node.scale, node.rotation, node.position);
			MarkDirty();
			if (animationRecording) RecordSelectedNodeKey();
		}
		gizmoHovered = ImGuizmo::IsOver(ImGuizmo::UNIVERSAL) ||
			ImGuizmo::IsOver(&world._41, 24.0f);
	}
	ImGui::GetForegroundDrawList()->PopClipRect();
	ImGuizmo::Enable(true);
	if (imageHovered && !viewportOrbitDragging && !ImGuizmo::IsUsing() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		viewportRotationDragging = true;
		SetCapture(Game::Graphics::Instance().GetWindowHandle());
	}
	if (viewportRotationDragging)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			RotatePreviewCamera(yawDelta, pitchDelta, false);
			if (io.MouseWheel != 0.0f && !io.KeyCtrl)
			{
				previewCameraMoveSpeed = std::clamp(previewCameraMoveSpeed *
					std::pow(1.1f, io.MouseWheel), 0.1f, 500.0f);
				CameraMoveSpeedOverlay::Show(previewCameraMoveSpeedOverlayTimer);
			}
			Vector3 move = Vector3::Zero;
			if (ImGui::IsKeyDown(ImGuiKey_W)) move += editorCamera->GetFront();
			if (ImGui::IsKeyDown(ImGuiKey_S)) move -= editorCamera->GetFront();
			if (ImGui::IsKeyDown(ImGuiKey_A)) move -= editorCamera->GetRight();
			if (ImGui::IsKeyDown(ImGuiKey_D)) move += editorCamera->GetRight();
			if (ImGui::IsKeyDown(ImGuiKey_E)) move += editorCamera->GetUp();
			if (ImGui::IsKeyDown(ImGuiKey_Q)) move -= editorCamera->GetUp();
			if (move.LengthSquared() > 0.0f)
			{
				move.Normalize();
				cameraFocusOffset += move * previewCameraMoveSpeed *
					(io.KeyShift ? 3.0f : 1.0f) * Game::Time::unscaledDeltaTime;
			}
		}
		else
		{
			viewportRotationDragging = false;
			if (GetCapture() == Game::Graphics::Instance().GetWindowHandle()) ReleaseCapture();
		}
	}
	CameraMoveSpeedOverlay::Draw(
		previewCameraMoveSpeed, previewCameraMoveSpeedOverlayTimer);
	if (!imageHovered || viewportOrbitDragging || viewportRotationDragging ||
		io.KeyAlt || gizmoHovered || ImGuizmo::IsUsingAny() || ImGuizmo::IsOver()) return;

	if (model && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		ImGui::SetWindowFocus(nullptr);
		const auto& nodes = model->GetNodes();
		const Matrix renderScaleTransform = model->GetRenderScaleTransform();
		const Matrix viewProjection = editorCamera->GetView() * editorCamera->GetProjection();
		const ImVec2 mouse = io.MousePos;
		const float imageWidth = imageMax.x - imageMin.x;
		const float imageHeight = imageMax.y - imageMin.y;
		float shortestBoneLengthSquared = FLT_MAX;
		float nearestDistanceSquared = 14.0f * 14.0f;
		float nearestDepth = FLT_MAX;
		int nearestNode = -1;
		for (int nodeIndex = 0; nodeIndex < Count(nodes); ++nodeIndex)
		{
			if (!showDebugOverlays || !showBones) break;
			const VMDLModel::Node& node = nodes[nodeIndex];
			if (!node.parent) continue;
			const Vector3 start =
				(node.parent->worldTransform * renderScaleTransform).Translation();
			const Vector3 end = (node.worldTransform * renderScaleTransform).Translation();
			const float boneLengthSquared = (end - start).LengthSquared();

			Vector3 projectedStart;
			Vector3 projectedEnd;
			DirectX::XMStoreFloat3(
				&projectedStart, DirectX::XMVector3TransformCoord(start, viewProjection));
			DirectX::XMStoreFloat3(
				&projectedEnd, DirectX::XMVector3TransformCoord(end, viewProjection));
			if ((projectedStart.z < 0.0f && projectedEnd.z < 0.0f) ||
				(projectedStart.z > 1.0f && projectedEnd.z > 1.0f))
				continue;

			const ImVec2 screenStart(imageMin.x + (projectedStart.x + 1.0f) * 0.5f * imageWidth,
				imageMin.y + (1.0f - projectedStart.y) * 0.5f * imageHeight);
			const ImVec2 screenEnd(imageMin.x + (projectedEnd.x + 1.0f) * 0.5f * imageWidth,
				imageMin.y + (1.0f - projectedEnd.y) * 0.5f * imageHeight);
			const float segmentX = screenEnd.x - screenStart.x;
			const float segmentY = screenEnd.y - screenStart.y;
			const float segmentLengthSquared = segmentX * segmentX + segmentY * segmentY;
			if (segmentLengthSquared < 0.0001f) continue;
			const float rate = std::clamp(
				((mouse.x - screenStart.x) * segmentX + (mouse.y - screenStart.y) * segmentY) /
					segmentLengthSquared,
				0.0f, 1.0f);
			const float closestX = screenStart.x + segmentX * rate;
			const float closestY = screenStart.y + segmentY * rate;
			const float distanceX = mouse.x - closestX;
			const float distanceY = mouse.y - closestY;
			const float distanceSquared = distanceX * distanceX + distanceY * distanceY;
			const float depth = std::lerp(projectedStart.z, projectedEnd.z, rate);
			if (distanceSquared > 14.0f * 14.0f || boneLengthSquared > shortestBoneLengthSquared ||
				(boneLengthSquared == shortestBoneLengthSquared &&
					(distanceSquared > nearestDistanceSquared ||
						(distanceSquared == nearestDistanceSquared && depth >= nearestDepth))))
				continue;
			shortestBoneLengthSquared = boneLengthSquared;
			nearestDistanceSquared = distanceSquared;
			nearestDepth = depth;
			nearestNode = nodeIndex;
		}
		if (nearestNode >= 0)
		{
			SelectNode(nearestNode, io.KeyCtrl);
		}
		else
		{
			showSelectionGizmo = false;
			ikGizmoTarget = IkGizmoTarget::None;
		}
	}
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
	{
		const float panScale = cameraDistance * 0.002f;
		cameraFocusOffset -= editorCamera->GetRight() * static_cast<float>(mouseInput.GetAxisX()) * panScale;
		cameraFocusOffset += editorCamera->GetUp() * static_cast<float>(mouseInput.GetAxisY()) * panScale;
	}
	if (io.MouseWheel != 0.0f && !io.KeyCtrl && !viewportRotationDragging)
	{
		targetCameraDistance -= static_cast<float>(mouseInput.GetWheel()) * targetCameraDistance * 0.1f;
		targetCameraDistance =
			std::clamp(targetCameraDistance, PreviewMinCameraDistance, PreviewMaxCameraDistance);
	}
}

void VmdlEditorScene::DrawProperty()
{
	ImGui::TextUnformatted((const char*)u8"プロパティ");
	ImGui::Separator();
	if (previewStageSelected)
	{
		ImGui::TextUnformatted((const char*)u8"プレビューステージ");
		if (ImGui::Checkbox((const char*)u8"表示", &showFootIkTestStage))
		{
			if (showFootIkTestStage) LoadFootIkTestStage();
			UpdateFootIkTestStage();
		}
		ImGui::SeparatorText("Transform");
		bool changed = ImGui::DragFloat3((const char*)u8"位置", &footIkTestStagePosition.x, 0.1f);
		changed |= ImGui::DragFloat3((const char*)u8"回転", &footIkTestStageRotation.x, 0.1f);
		changed |= ImGui::DragFloat((const char*)u8"スケール", &footIkTestStageSize, 0.1f, 0.01f, 100000.0f);
		if (changed) UpdateFootIkTestStage();
		return;
	}
	if (!model || selectedNode < 0 || selectedNode >= Count(model->GetNodes()))
	{
		ImGui::TextDisabled((const char*)u8"ノードまたはメッシュを選択してください");
		return;
	}
	if (selectedComponentType != AttachedComponentType::None && selectedComponentIndex >= 0)
	{
		DrawSelectedComponents();
		return;
	}

	// 選択ノードのローカルトランスフォーム
	VMDLModel::Node& node = model->GetNodes()[selectedNode];
	ImGui::Text((const char*)u8"ノード: %s", MakeNodeLabel(selectedNode, node.name).c_str());
	bool transformChanged =
		ImGui::DragFloat3((const char*)u8"ローカル位置", &node.position.x, 0.01f);
	Vector3 euler = node.rotation.ToEuler();
	euler.x = DEG(euler.x);
	euler.y = DEG(euler.y);
	euler.z = DEG(euler.z);
	if (ImGui::DragFloat3((const char*)u8"ローカル回転", &euler.x, 0.1f))
	{
		node.rotation =
			Quaternion::CreateFromYawPitchRoll(RAD(euler.y), RAD(euler.x), RAD(euler.z));
		node.rotation.Normalize();
		transformChanged = true;
	}
	transformChanged |= ImGui::DragFloat3((const char*)u8"ローカルスケール", &node.scale.x, 0.01f);
	if (transformChanged)
	{
		MarkDirty();
		if (animationRecording) RecordSelectedNodeKey();
	}

	// 選択メッシュの情報
	if (selectedMesh >= 0 && selectedMesh < Count(model->GetMeshes()))
	{
		VMDLModel::Mesh& mesh = model->GetMeshes()[selectedMesh];
		ImGui::SeparatorText((const char*)u8"メッシュ");
		ImGui::Text((const char*)u8"マテリアル: %s", mesh.material->name.c_str());
		ImGui::Text((const char*)u8"頂点数: %zu", mesh.vertices.size());
		ImGui::Text((const char*)u8"面数: %zu", mesh.indices.size() / 3);
		if (const auto* external = model->GetExternalMeshGroupForMesh(selectedMesh))
		{
			ImGui::TextColored(ImVec4(0.20f, 0.85f, 1.0f, 1.0f), "VMSH / Lazy Load");
			ImGui::TextWrapped((const char*)u8"外部メッシュ: %s", external->path.c_str());
			ImGui::TextDisabled((const char*)u8"頂点・インデックスはVMDLに保持されず、表示時だけ読み込みます");
		}
		if (ImGui::Checkbox((const char*)u8"表示", &mesh.isDraw)) MarkDirty();
		selectedMaterial = mesh.materialIndex;
		DrawMaterialEditor();
	}
	if (previewShadingMode == PreviewShadingMode::Solid)
		ImGui::ColorEdit4((const char*)u8"ソリッド色", &solidColor.x);
}

// 同種のコンポーネントだけを選択集合として保持する。ノードは主選択の表示に使う。
int VmdlEditorScene::GetComponentNodeIndex(int componentIndex) const
{
	if (!model || componentIndex < 0) return -1;
	const auto nodeIndex = [componentIndex](const auto& values) {
		return componentIndex < Count(values) ? values[componentIndex].nodeIndex : -1;
	};
	switch (selectedComponentType)
	{
	case AttachedComponentType::RigidBody: return nodeIndex(model->GetVmdlExtensionData().rigidBodies);
	case AttachedComponentType::Collider: return nodeIndex(model->GetVmdlExtensionData().colliders);
	case AttachedComponentType::Spring: return nodeIndex(model->GetVmdlExtensionData().springs);
	case AttachedComponentType::SpringCollider: return nodeIndex(model->GetVmdlExtensionData().springColliders);
	case AttachedComponentType::Trail: return nodeIndex(model->GetVmdlTrailData().trails);
	case AttachedComponentType::Particle: return nodeIndex(model->GetVmdlParticleData().emitters);
	case AttachedComponentType::SoundSource: return nodeIndex(model->GetVmdlSoundData().sources);
	case AttachedComponentType::PointLight: return nodeIndex(model->GetVmdlLightData().pointLights);
	case AttachedComponentType::CameraShake: return nodeIndex(model->GetVmdlPresentationData().cameraShakes);
	case AttachedComponentType::RadialBlur: return nodeIndex(model->GetVmdlPresentationData().radialBlurs);
	default: return -1;
	}
}

void VmdlEditorScene::DrawSelectedComponents()
{
	if (selectedComponentIndices.empty() && GetComponentNodeIndex(selectedComponentIndex) >= 0)
		selectedComponentIndices.push_back(selectedComponentIndex);
	std::erase_if(selectedComponentIndices, [this](int index) { return GetComponentNodeIndex(index) < 0; });
	if (selectedComponentIndices.empty()) return;
	selectedComponentIndex = selectedComponentIndices.back();
	selectedNode = GetComponentNodeIndex(selectedComponentIndex);
	if (selectedComponentIndices.size() > 1) DrawBatchComponentProperties();
	else DrawAttachedData(selectedNode);
}

void VmdlEditorScene::DrawBatchComponentProperties()
{
	ImGui::Text((const char*)u8"同種コンポーネントの一括編集: %zu個", selectedComponentIndices.size());
	ImGui::TextWrapped((const char*)u8"* は値が異なる項目です。最後に選んだ値を表示しています。操作した項目・軸だけを全選択へ反映します。「=」で表示中の値へ揃えられます。");
	ImGui::Separator();

	// 一括編集専用の入力。名前・所属ノード・再生トラックには触れない。
	const auto drawBatch = [this](auto& values, auto drawFields) {
		using Component = typename std::remove_reference_t<decltype(values)>::value_type;
		std::vector<Component*> targets;
		for (int index : selectedComponentIndices)
			if (index >= 0 && index < Count(values)) targets.push_back(&values[index]);
		if (targets.size() < 2) return;
		bool changed = false;
		const auto edit = [&](const char* label, auto field, float minimum = -100000.0f, float maximum = 100000.0f, float displayScale = 1.0f) {
			auto value = field(*targets.back());
			using Value = std::remove_cvref_t<decltype(value)>;
			const bool mixed = std::any_of(targets.begin(), targets.end(), [&](const auto* target) {
				return field(*target) != value;
			});
			const std::string display = std::string(label) + (mixed ? " *" : "") + "###" + label;
			bool edited = false;
			if constexpr (std::is_same_v<Value, bool>) edited = ImGui::Checkbox(display.c_str(), &value);
			else if constexpr (std::is_same_v<Value, int>)
			{
				edited = ImGui::DragInt(display.c_str(), &value, 1.0f, static_cast<int>(minimum), static_cast<int>(maximum));
				value = std::clamp(value, static_cast<int>(minimum), static_cast<int>(maximum));
			}
			else if constexpr (std::is_same_v<Value, float>)
			{
				value *= displayScale;
				edited = ImGui::DragFloat(display.c_str(), &value, 0.01f, minimum, maximum);
				value = std::clamp(value, minimum, maximum);
			}
			else if constexpr (std::is_same_v<Value, std::string>)
			{
				if (std::string_view(label) == (const char*)u8"エフェクト") edited = DrawEffectSelector(display.c_str(), value);
				else edited = DrawSoundPathSelector(display.c_str(), value);
			}
			// 代表値と同じ数値を全員へ揃える操作も用意する。
			ImGui::SameLine();
			ImGui::PushID(label);
			ImGui::BeginDisabled(!mixed);
			edited |= ImGui::SmallButton("=##Unify");
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip((const char*)u8"表示中の値を全選択へ適用");
			ImGui::EndDisabled();
			ImGui::PopID();
			if (!edited) return;
			for (auto* target : targets)
			{
				if constexpr (std::is_same_v<Value, float>) field(*target) = value / displayScale;
				else field(*target) = value;
			}
			changed = true;
		};
		const auto choice = [&](const char* label, auto field, const std::vector<std::pair<int, std::string>>& options) {
			const int current = field(*targets.back());
			const bool mixed = std::any_of(targets.begin(), targets.end(), [&](const auto* target) {
				return field(*target) != current;
			});
			const auto selected = std::find_if(options.begin(), options.end(), [current](const auto& option) {
				return option.first == current;
			});
			const std::string preview = mixed ? (const char*)u8"複数の値" :
				(selected != options.end() ? selected->second : std::to_string(current));
			if (!ImGui::BeginCombo(label, preview.c_str())) return;
			for (const auto& [index, name] : options)
			{
				if (!ImGui::Selectable(name.c_str(), !mixed && current == index)) continue;
				for (auto* target : targets) field(*target) = index;
				changed = true;
			}
			ImGui::EndCombo();
		};
		const auto vector = [&](const char* label, auto field, float minimum = -100000.0f, float maximum = 100000.0f, float displayScale = 1.0f) {
			ImGui::PushID(label);
			ImGui::TextUnformatted(label);
			edit("X", [&](auto& target) -> auto& { return field(target).x; }, minimum, maximum, displayScale);
			edit("Y", [&](auto& target) -> auto& { return field(target).y; }, minimum, maximum, displayScale);
			edit("Z", [&](auto& target) -> auto& { return field(target).z; }, minimum, maximum, displayScale);
			ImGui::PopID();
		};
		vector((const char*)u8"ローカル位置", [](auto& target) -> auto& { return target.transform.position; }, -100000.0f, 100000.0f, model->GetModelScale());
		vector((const char*)u8"ローカル回転 (度)", [](auto& target) -> auto& { return target.transform.rotation; });
		vector((const char*)u8"ローカルスケール", [](auto& target) -> auto& { return target.transform.scale; });
		drawFields(edit, vector, choice);
		if (changed)
		{
			for (auto* target : targets)
			{
				if constexpr (std::is_same_v<Component, VMDLModel::VmdlSpring>)
				{
					const Vector3 minimum = Vector3::Min(target->minAnglesDegrees, target->maxAnglesDegrees);
					target->maxAnglesDegrees = Vector3::Max(target->minAnglesDegrees, target->maxAnglesDegrees);
					target->minAnglesDegrees = minimum;
				}
				else if constexpr (std::is_same_v<Component, VMDLModel::VmdlSoundSource>)
				{
					target->pitchMax = std::max(target->pitchMin, target->pitchMax);
					target->maxDistance = std::max(target->minDistance + 0.01f, target->maxDistance);
				}
				else if constexpr (std::is_same_v<Component, VMDLModel::VmdlCollider>)
				{
					if (target->shape == 1) target->size.y = target->size.z = target->size.x;
				}
			}
			MarkDirty();
			if (selectedComponentType == AttachedComponentType::Particle) RebuildParticlePreview();
		}
	};

	switch (selectedComponentType)
	{
	case AttachedComponentType::RigidBody:
		drawBatch(model->GetVmdlExtensionData().rigidBodies, [&](auto edit, auto vector, auto choice) {
			edit((const char*)u8"静的", [](auto& target) -> auto& { return target.kinematic; });
			edit((const char*)u8"質量", [](auto& target) -> auto& { return target.mass; }, 0.001f, 100000.0f);
		});
		break;
	case AttachedComponentType::Collider:
		drawBatch(model->GetVmdlExtensionData().colliders, [&](auto edit, auto vector, auto choice) {
			std::vector<std::pair<int, std::string>> layers = {{-1, (const char*)u8"継承"}};
			for (int layer = 0; layer < EditableLayerCount; ++layer)
				layers.emplace_back(layer, PhysicsLayerManager::Instance().GetLayerDisplayName(static_cast<LayerId>(layer)));
			choice((const char*)u8"レイヤー", [](auto& target) -> auto& { return target.layer; }, layers);
			std::vector<std::pair<int, std::string>> shapes = {
				{0, (const char*)u8"ボックス"}, {1, (const char*)u8"球"}, {2, (const char*)u8"カプセル"}};
			for (int mesh = 0; mesh < Count(model->GetMeshes()); ++mesh)
				shapes.emplace_back(mesh + 3, MakeMeshLabel(*model, mesh));
			choice((const char*)u8"形状", [](auto& target) -> auto& { return target.shape; }, shapes);
			vector((const char*)u8"大きさ", [](auto& target) -> auto& { return target.size; }, 0.001f, 100000.0f, model->GetModelScale());
			edit((const char*)u8"トリガー", [](auto& target) -> auto& { return target.trigger; });
			edit((const char*)u8"NavMesh障害物", [](auto& target) -> auto& { return target.navMeshObstacle; });
		});
		break;
	case AttachedComponentType::Spring:
		drawBatch(model->GetVmdlExtensionData().springs, [&](auto edit, auto vector, auto choice) {
			edit((const char*)u8"硬さ", [](auto& target) -> auto& { return target.stiffness; }, 0.0f, 1.0f);
			edit((const char*)u8"抵抗", [](auto& target) -> auto& { return target.drag; }, 0.0f, 1.0f);
			edit((const char*)u8"全方向の最大角度 (度)", [](auto& target) -> auto& { return target.maxAngleDegrees; }, 0.0f, 180.0f);
			vector((const char*)u8"最小角度 XYZ (度)", [](auto& target) -> auto& { return target.minAnglesDegrees; }, -180.0f, 180.0f);
			vector((const char*)u8"最大角度 XYZ (度)", [](auto& target) -> auto& { return target.maxAnglesDegrees; }, -180.0f, 180.0f);
		});
		break;
	case AttachedComponentType::SpringCollider:
		drawBatch(model->GetVmdlExtensionData().springColliders, [&](auto edit, auto vector, auto choice) {
			edit((const char*)u8"半径", [](auto& target) -> auto& { return target.radius; }, 0.001f, 100000.0f);
		});
		break;
	case AttachedComponentType::Trail:
		drawBatch(model->GetVmdlTrailData().trails, [&](auto edit, auto vector, auto choice) {
			vector((const char*)u8"根元オフセット", [](auto& target) -> auto& { return target.rootOffset; });
			vector((const char*)u8"先端オフセット", [](auto& target) -> auto& { return target.tipOffset; });
			edit("R", [](auto& target) -> auto& { return target.color.x; }, 0.0f, 100000.0f);
			edit("G", [](auto& target) -> auto& { return target.color.y; }, 0.0f, 100000.0f);
			edit("B", [](auto& target) -> auto& { return target.color.z; }, 0.0f, 100000.0f);
			edit("A", [](auto& target) -> auto& { return target.color.w; }, 0.0f, 1.0f);
			edit((const char*)u8"先端比率", [](auto& target) -> auto& { return target.tipRatio; }, 0.0f, 4.0f);
			edit((const char*)u8"生存時間", [](auto& target) -> auto& { return target.lifeTime; }, 0.01f, 10.0f);
			edit((const char*)u8"最大頂点数", [](auto& target) -> auto& { return target.maxPoints; }, 2.0f, 1024.0f);
			vector((const char*)u8"角度オフセット", [](auto& target) -> auto& { return target.offsetAngle; });
		});
		break;
	case AttachedComponentType::Particle:
		drawBatch(model->GetVmdlParticleData().emitters, [&](auto edit, auto vector, auto choice) {
			edit((const char*)u8"ノードに追従", [](auto& target) -> auto& { return target.modelDerived; });
			edit((const char*)u8"ビルボード", [](auto& target) -> auto& { return target.billboard; });
			edit((const char*)u8"エフェクト", [](auto& target) -> auto& { return target.effectName; });
		});
		break;
	case AttachedComponentType::SoundSource:
		drawBatch(model->GetVmdlSoundData().sources, [&](auto edit, auto vector, auto choice) {
			edit((const char*)u8"サウンド", [](auto& target) -> auto& { return target.soundPath; });
			edit((const char*)u8"バリエーション番号 (-1: ランダム)", [](auto& target) -> auto& { return target.variant; }, -1.0f, 10000.0f);
			edit((const char*)u8"音量", [](auto& target) -> auto& { return target.volume; }, 0.0f, 4.0f);
			edit((const char*)u8"最小ピッチ", [](auto& target) -> auto& { return target.pitchMin; }, 0.125f, 8.0f);
			edit((const char*)u8"最大ピッチ", [](auto& target) -> auto& { return target.pitchMax; }, 0.125f, 8.0f);
			edit((const char*)u8"3Dサウンド", [](auto& target) -> auto& { return target.spatial; });
			edit((const char*)u8"減衰開始距離", [](auto& target) -> auto& { return target.minDistance; }, 0.01f, 1000.0f);
			edit((const char*)u8"再生限界距離", [](auto& target) -> auto& { return target.maxDistance; }, 0.02f, 1000.0f);
			edit((const char*)u8"近距離ローパス", [](auto& target) -> auto& { return target.lowPassHz; }, 20.0f, 20000.0f);
			edit((const char*)u8"遠距離ローパス", [](auto& target) -> auto& { return target.farLowPassHz; }, 20.0f, 20000.0f);
			edit((const char*)u8"残響", [](auto& target) -> auto& { return target.reverbMix; }, 0.0f, 1.0f);
		});
		break;
	case AttachedComponentType::PointLight:
		drawBatch(model->GetVmdlLightData().pointLights, [&](auto edit, auto vector, auto choice) {
			edit((const char*)u8"有効", [](auto& target) -> auto& { return target.active; });
			edit("R", [](auto& target) -> auto& { return target.color.x; }, 0.0f, 100000.0f);
			edit("G", [](auto& target) -> auto& { return target.color.y; }, 0.0f, 100000.0f);
			edit("B", [](auto& target) -> auto& { return target.color.z; }, 0.0f, 100000.0f);
			edit((const char*)u8"強度", [](auto& target) -> auto& { return target.intensity; }, 0.0f, 100000.0f);
			edit((const char*)u8"範囲", [](auto& target) -> auto& { return target.range; }, 0.0f, 10000.0f);
		});
		break;
	case AttachedComponentType::CameraShake:
		drawBatch(model->GetVmdlPresentationData().cameraShakes, [&](auto edit, auto vector, auto choice) {
			edit((const char*)u8"効果範囲", [](auto& target) -> auto& { return target.range; }, 0.01f, 10000.0f);
			edit((const char*)u8"距離に応じて減衰", [](auto& target) -> auto& { return target.distanceAttenuation; });
			edit((const char*)u8"継続時間", [](auto& target) -> auto& { return target.duration; }, 0.01f, 30.0f);
			edit((const char*)u8"揺れの強さ", [](auto& target) -> auto& { return target.intensity; }, 0.0f, 10.0f);
		});
		break;
	case AttachedComponentType::RadialBlur:
		drawBatch(model->GetVmdlPresentationData().radialBlurs, [&](auto edit, auto vector, auto choice) {
			edit((const char*)u8"効果範囲", [](auto& target) -> auto& { return target.range; }, 0.01f, 10000.0f);
			edit((const char*)u8"距離に応じて減衰", [](auto& target) -> auto& { return target.distanceAttenuation; });
			edit((const char*)u8"継続時間", [](auto& target) -> auto& { return target.duration; }, 0.01f, 30.0f);
			edit((const char*)u8"ブラー強度", [](auto& target) -> auto& { return target.power; }, 0.0f, 20.0f);
			edit((const char*)u8"立ち上がり割合", [](auto& target) -> auto& { return target.attackRate; }, 0.01f, 0.95f);
		});
		break;
	default: break;
	}

	// 初期有効状態はコンポーネント本体ではなく別の配列に保持されている。
	const auto initialActive = [this](auto get, auto set) {
		bool value = get(selectedComponentIndex);
		const bool mixed = std::any_of(selectedComponentIndices.begin(), selectedComponentIndices.end(),
			[&](int index) { return get(index) != value; });
		const char* label = mixed ? (const char*)u8"初期状態で有効 *" : (const char*)u8"初期状態で有効";
		bool changed = ImGui::Checkbox(label, &value);
		ImGui::SameLine();
		ImGui::BeginDisabled(!mixed);
		changed |= ImGui::SmallButton("=##InitialActive");
		ImGui::EndDisabled();
		if (!changed) return;
		for (int index : selectedComponentIndices) set(index, value);
		MarkDirty();
	};
	if (selectedComponentType == AttachedComponentType::Collider)
		initialActive([this](int index) { return model->GetColliderInitialActive(index); },
			[this](int index, bool active) {
				model->SetColliderInitialActive(index, active);
				if (Count(previewColliderActive) <= index) previewColliderActive.resize(index + 1, 1);
				previewColliderActive[index] = active ? 1 : 0;
			});
	else if (selectedComponentType == AttachedComponentType::Trail)
		initialActive([this](int index) { return model->GetTrailInitialActive(index); },
			[this](int index, bool active) {
				model->SetTrailInitialActive(index, active);
				if (Count(previewTrailActive) <= index) previewTrailActive.resize(index + 1, 1);
				previewTrailActive[index] = active ? 1 : 0;
			});
}

void VmdlEditorScene::DeleteAttachedComponent(AttachedComponentType type, int index)
{
	if (!model || index < 0) return;
	auto& data = model->GetVmdlExtensionData();
	auto& trailData = model->GetVmdlTrailData();
	auto& particleData = model->GetVmdlParticleData();
	auto& soundData = model->GetVmdlSoundData();
	auto& pointLights = model->GetVmdlLightData().pointLights;
	auto& presentation = model->GetVmdlPresentationData();
	switch (type)
	{
	case AttachedComponentType::RigidBody:
	{
		if (index >= Count(data.rigidBodies)) return;
		data.rigidBodies.erase(data.rigidBodies.begin() + index);
		if (selectedComponentType == AttachedComponentType::RigidBody)
		{
			if (selectedComponentIndex == index)
			{
				selectedComponentType = AttachedComponentType::None;
				selectedComponentIndex = -1;
				selectedComponentIndices.clear();
			}
			else if (selectedComponentIndex > index)
				--selectedComponentIndex;
		}
		MarkDirty();
		break;
	}
	case AttachedComponentType::Collider:
	{
		if (index >= Count(data.colliders)) return;
		data.colliders.erase(data.colliders.begin() + index);
		if (selectedComponentType == AttachedComponentType::Collider)
		{
			if (selectedComponentIndex == index)
			{
				selectedComponentType = AttachedComponentType::None;
				selectedComponentIndex = -1;
				selectedComponentIndices.clear();
			}
			else if (selectedComponentIndex > index)
				--selectedComponentIndex;
		}
		auto& control = model->GetVmdlAnimationControlData();
		if (index < Count(control.colliderInitialActive))
			control.colliderInitialActive.erase(
				control.colliderInitialActive.begin() + index);
		std::erase_if(control.colliderTracks,
			[index](const auto& track) { return track.colliderIndex == index; });
		for (auto& track : control.colliderTracks)
		{
			if (track.colliderIndex > index) --track.colliderIndex;
		}
		if (index < Count(previewColliderActive))
			previewColliderActive.erase(previewColliderActive.begin() + index);
		selectedColliderEventTarget = data.colliders.empty()
										  ? 0
										  : std::min(selectedColliderEventTarget,
												Count(data.colliders) - 1);
		if (timelineEventContextKind == 0) timelineEventContextKind = -1;
		MarkDirty();
		break;
	}
	case AttachedComponentType::Spring:
	{
		if (index >= Count(data.springs)) return;
		data.springs.erase(data.springs.begin() + index);
		if (selectedComponentType == AttachedComponentType::Spring)
		{
			if (selectedComponentIndex == index)
			{
				selectedComponentType = AttachedComponentType::None;
				selectedComponentIndex = -1;
				selectedComponentIndices.clear();
			}
			else if (selectedComponentIndex > index)
				--selectedComponentIndex;
		}
		MarkDirty();
		break;
	}
	case AttachedComponentType::SpringCollider:
	{
		if (index >= Count(data.springColliders)) return;
		data.springColliders.erase(data.springColliders.begin() + index);
		if (selectedComponentType == AttachedComponentType::SpringCollider)
		{
			if (selectedComponentIndex == index)
			{
				selectedComponentType = AttachedComponentType::None;
				selectedComponentIndex = -1;
				selectedComponentIndices.clear();
			}
			else if (selectedComponentIndex > index)
				--selectedComponentIndex;
		}
		MarkDirty();
		break;
	}
	case AttachedComponentType::Trail:
	{
		if (index >= Count(trailData.trails)) return;
		trailData.trails.erase(trailData.trails.begin() + index);
		if (selectedComponentType == AttachedComponentType::Trail)
		{
			if (selectedComponentIndex == index)
			{
				selectedComponentType = AttachedComponentType::None;
				selectedComponentIndex = -1;
				selectedComponentIndices.clear();
			}
			else if (selectedComponentIndex > index)
				--selectedComponentIndex;
		}
		if (index < Count(trailData.initialActive))
			trailData.initialActive.erase(trailData.initialActive.begin() + index);
		std::erase_if(trailData.tracks,
			[index](const auto& track) { return track.trailIndex == index; });
		for (auto& track : trailData.tracks)
		{
			if (track.trailIndex > index) --track.trailIndex;
		}
		if (index < Count(previewTrailActive))
			previewTrailActive.erase(previewTrailActive.begin() + index);
		selectedTrailEventTarget =
			trailData.trails.empty()
				? 0
				: std::min(selectedTrailEventTarget, Count(trailData.trails) - 1);
		if (timelineEventContextKind == 2) timelineEventContextKind = -1;
		MarkDirty();
		break;
	}
	case AttachedComponentType::Particle:
	{
		if (index >= Count(particleData.emitters)) return;
		particleData.emitters.erase(particleData.emitters.begin() + index);
		if (index < Count(particleData.initialActive))
			particleData.initialActive.erase(particleData.initialActive.begin() + index);
		std::erase_if(particleData.tracks,
			[index](const auto& track) { return track.emitterIndex == index; });
		for (auto& track : particleData.tracks)
			if (track.emitterIndex > index) --track.emitterIndex;
		if (selectedComponentType == AttachedComponentType::Particle)
		{
			if (selectedComponentIndex == index) { selectedComponentType = AttachedComponentType::None; selectedComponentIndex = -1; selectedComponentIndices.clear(); }
			else if (selectedComponentIndex > index) --selectedComponentIndex;
		}
		selectedParticleEventTarget = particleData.emitters.empty() ? 0 :
			std::min(selectedParticleEventTarget, Count(particleData.emitters) - 1);
		MarkDirty(); RebuildParticlePreview();
		break;
	}
	case AttachedComponentType::SoundSource:
	{
		if (index >= Count(soundData.sources)) return;
		soundData.sources.erase(soundData.sources.begin() + index);
		for (auto& track : soundData.tracks)
		{
			std::erase_if(track.keys, [index](const auto& key)
				{ return key.sourceIndex == index; });
			for (auto& key : track.keys)
				if (key.sourceIndex > index) --key.sourceIndex;
		}
		std::erase_if(soundData.tracks,
			[](const auto& track) { return track.keys.empty(); });
		if (selectedComponentType == AttachedComponentType::SoundSource)
		{
			if (selectedComponentIndex == index)
			{
				selectedComponentType = AttachedComponentType::None;
				selectedComponentIndex = -1;
				selectedComponentIndices.clear();
			}
			else if (selectedComponentIndex > index)
				--selectedComponentIndex;
		}
		MarkDirty();
		break;
	}
	case AttachedComponentType::PointLight:
	{
		if (index >= Count(pointLights)) return;
		pointLights.erase(pointLights.begin() + index);
		if (selectedComponentType == AttachedComponentType::PointLight)
		{
			if (selectedComponentIndex == index)
			{
				selectedComponentType = AttachedComponentType::None;
				selectedComponentIndex = -1;
				selectedComponentIndices.clear();
			}
			else if (selectedComponentIndex > index)
				--selectedComponentIndex;
		}
		MarkDirty();
		break;
	}
	case AttachedComponentType::CameraShake:
	{
		if (index >= Count(presentation.cameraShakes)) return;
		presentation.cameraShakes.erase(presentation.cameraShakes.begin() + index);
		for (auto& track : presentation.cameraShakeTracks)
		{
			std::erase_if(track.keys, [index](const auto& key) {
				return key.componentIndex == index;
			});
			for (auto& key : track.keys) if (key.componentIndex > index) --key.componentIndex;
		}
		std::erase_if(presentation.cameraShakeTracks,
			[](const auto& track) { return track.keys.empty(); });
		if (selectedComponentType == AttachedComponentType::CameraShake) { selectedComponentType = AttachedComponentType::None; selectedComponentIndex = -1; selectedComponentIndices.clear(); }
		MarkDirty();
		break;
	}
	case AttachedComponentType::RadialBlur:
	{
		if (index >= Count(presentation.radialBlurs)) return;
		presentation.radialBlurs.erase(presentation.radialBlurs.begin() + index);
		for (auto& track : presentation.radialBlurTracks)
		{
			std::erase_if(track.keys, [index](const auto& key) {
				return key.componentIndex == index;
			});
			for (auto& key : track.keys) if (key.componentIndex > index) --key.componentIndex;
		}
		std::erase_if(presentation.radialBlurTracks,
			[](const auto& track) { return track.keys.empty(); });
		if (selectedComponentType == AttachedComponentType::RadialBlur) { selectedComponentType = AttachedComponentType::None; selectedComponentIndex = -1; selectedComponentIndices.clear(); }
		MarkDirty();
		break;
	}
	default: break;
	}
}

void VmdlEditorScene::DrawAttachedData(int nodeIndex)
{
	bool duplicatePending = false;
	auto& data = model->GetVmdlExtensionData();
	const bool openSelectedComponent = focusSelectedComponent;
	focusSelectedComponent = false;

	// リジッドボディ
	int deleteRigidBody = -1;
	for (int i = 0; i < Count(data.rigidBodies); ++i)
	{
		auto& value = data.rigidBodies[i];
		if (value.nodeIndex != nodeIndex || selectedComponentType != AttachedComponentType::RigidBody ||
			selectedComponentIndex != i) continue;
		ImGui::PushID(1000 + i);
		const bool selected = selectedComponentType == AttachedComponentType::RigidBody &&
			selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)(ICON_FA_CUBE " " u8"リジッドボディ"),
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= DrawComponentTransform(*model, value.transform);
			const char* rigidBodyTypes[] = {(const char*)u8"静的", (const char*)u8"動的"};
			int rigidBodyType = value.kinematic ? 0 : 1;
			if (ImGui::Combo((const char*)u8"種類", &rigidBodyType,
					rigidBodyTypes, IM_ARRAYSIZE(rigidBodyTypes)))
			{
				value.kinematic = rigidBodyType == 0;
				changed = true;
			}
			if (rigidBodyType == 1)
				changed |= ImGui::DragFloat((const char*)u8"質量", &value.mass, 0.05f, 0.001f);
			if (changed) MarkDirty();
			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteRigidBody = i;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteRigidBody >= 0) DeleteAttachedComponent(AttachedComponentType::RigidBody, deleteRigidBody);

	// コライダー
	int deleteCollider = -1;
	for (int i = 0; i < Count(data.colliders); ++i)
	{
		auto& value = data.colliders[i];
		if (value.nodeIndex != nodeIndex || selectedComponentType != AttachedComponentType::Collider ||
			selectedComponentIndex != i) continue;
		ImGui::PushID(2000 + i);
		const bool selected = selectedComponentType == AttachedComponentType::Collider &&
			selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)(ICON_FA_SHAPES " " u8"コライダー"),
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			const char* shapes[] = {(const char*)u8"ボックス", (const char*)u8"球",
				(const char*)u8"カプセル", "Mesh"};
			bool initialActive = model->GetColliderInitialActive(i);
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			PhysicsLayerManager& layerManager = PhysicsLayerManager::Instance();
			const std::string layerPreview =
				value.layer < 0
					? (const char*)u8"-1: 継承"
					: layerManager.GetLayerDisplayName(static_cast<LayerId>(value.layer));
			if (ImGui::BeginCombo((const char*)u8"レイヤー", layerPreview.c_str()))
			{
				if (ImGui::Selectable((const char*)u8"-1: 継承", value.layer < 0))
				{
					value.layer = -1;
					changed = true;
				}
				for (int layer = 0; layer < EditableLayerCount; ++layer)
				{
					const std::string label =
						layerManager.GetLayerDisplayName(static_cast<LayerId>(layer));
					if (ImGui::Selectable(label.c_str(), value.layer == layer))
					{
						value.layer = layer;
						changed = true;
					}
				}
				ImGui::EndCombo();
			}
			int shape = value.shape >= 3 ? 3 : value.shape;
			if (ImGui::Combo((const char*)u8"形状", &shape, shapes, IM_ARRAYSIZE(shapes)))
			{
				value.shape = shape == 3 ? 3 : shape;
				if (shape == 1) value.size.y = value.size.z = value.size.x;
				changed = true;
			}
			Vector3 scaledSize = model->GetScaledAttachmentVector(value.size);
			changed |= DrawComponentTransform(*model, value.transform);
			if (shape == 3)
			{
				auto& meshes = model->GetMeshes();
				int meshIndex = value.shape - 3;
				if (meshes.empty())
				{
					ImGui::TextDisabled((const char*)u8"参照できるメッシュがありません");
				}
				else
				{
					meshIndex = std::clamp(meshIndex, 0, Count(meshes) - 1);
					if (value.shape != meshIndex + 3)
					{
						value.shape = meshIndex + 3;
						changed = true;
					}
					const std::string preview = MakeMeshLabel(*model, meshIndex);
					if (ImGui::BeginCombo((const char*)u8"参照メッシュ", preview.c_str()))
					{
						for (int candidate = 0; candidate < Count(meshes); ++candidate)
						{
							const std::string label = MakeMeshLabel(*model, candidate);
							if (ImGui::Selectable(label.c_str(), candidate == meshIndex))
							{
								meshIndex = candidate;
								value.shape = candidate + 3;
								changed = true;
							}
						}
						ImGui::EndCombo();
					}
				}
			}
			else if (shape == 1)
			{
				if (ImGui::DragFloat((const char*)u8"半径", &scaledSize.x, 0.01f, 0.001f))
				{
					scaledSize.x = std::max(0.001f, scaledSize.x);
					scaledSize.y = scaledSize.z = scaledSize.x;
					value.size = model->GetUnscaledAttachmentVector(scaledSize);
					changed = true;
				}
			}
			else if (value.shape == 2)
			{
				bool sizeChanged =
					ImGui::DragFloat((const char*)u8"半径", &scaledSize.x, 0.01f, 0.001f);
				sizeChanged |=
					ImGui::DragFloat((const char*)u8"高さ", &scaledSize.y, 0.01f, 0.001f);
				if (sizeChanged)
				{
					scaledSize.x = std::max(0.001f, scaledSize.x);
					scaledSize.y = std::max(0.001f, scaledSize.y);
					value.size = model->GetUnscaledAttachmentVector(scaledSize);
					changed = true;
				}
			}
			else if (ImGui::DragFloat3((const char*)u8"大きさ", &scaledSize.x, 0.01f))
			{
				value.size = model->GetUnscaledAttachmentVector(scaledSize);
				changed = true;
			}
			changed |= ImGui::Checkbox((const char*)u8"トリガー", &value.trigger);
			changed |= ImGui::Checkbox((const char*)u8"NavMesh障害物", &value.navMeshObstacle);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip((const char*)u8"このコライダーの範囲を、経路探索で避ける障害物にします");
			if (changed) MarkDirty();
			if (ImGui::Checkbox((const char*)u8"初期状態で有効", &initialActive))
			{
				model->SetColliderInitialActive(i, initialActive);
				if (Count(previewColliderActive) <= i)
					previewColliderActive.resize(i + 1, 1);
				previewColliderActive[i] = initialActive ? 1 : 0;
				MarkDirty();
			}

			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteCollider = i;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteCollider >= 0) DeleteAttachedComponent(AttachedComponentType::Collider, deleteCollider);

	// スプリング
	int deleteSpring = -1;
	for (int i = 0; i < Count(data.springs); ++i)
	{
		auto& value = data.springs[i];
		if (value.nodeIndex != nodeIndex || selectedComponentType != AttachedComponentType::Spring ||
			selectedComponentIndex != i) continue;
		ImGui::PushID(3000 + i);
		const bool selected = selectedComponentType == AttachedComponentType::Spring &&
			selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)(ICON_FA_LINK " " u8"スプリング"),
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= DrawComponentTransform(*model, value.transform);
			changed |= ImGui::DragFloat((const char*)u8"硬さ", &value.stiffness, 0.01f, 0.0f, 1.0f);
			changed |= ImGui::DragFloat((const char*)u8"抵抗", &value.drag, 0.01f, 0.0f, 1.0f);
			changed |= ImGui::SliderFloat((const char*)u8"最大角度 (度)",
				&value.maxAngleDegrees, 0.0f, 180.0f, "%.1f");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip(
				(const char*)u8"アニメーションの向きを中心に揺れる範囲を制限します。0度で揺れなし、180度で制限なし。コライダーと両立できない場合は角度制限を優先します。");
			bool angleChanged = ImGui::SliderFloat3((const char*)u8"最小角度 XYZ (度)",
				&value.minAnglesDegrees.x, -180.0f, 180.0f, "%.1f");
			angleChanged |= ImGui::SliderFloat3((const char*)u8"最大角度 XYZ (度)",
				&value.maxAnglesDegrees.x, -180.0f, 180.0f, "%.1f");
			if (angleChanged)
			{
				const Vector3 minimum = Vector3::Min(value.minAnglesDegrees, value.maxAnglesDegrees);
				value.maxAnglesDegrees = Vector3::Max(value.minAnglesDegrees, value.maxAnglesDegrees);
				value.minAnglesDegrees = minimum;
			}
			ImGui::TextDisabled((const char*)u8"アニメーション姿勢からのローカル追加回転。X=Pitch / Y=Yaw / Z=Roll");
			changed |= angleChanged;
			if (changed) MarkDirty();

			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteSpring = i;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteSpring >= 0) DeleteAttachedComponent(AttachedComponentType::Spring, deleteSpring);

	// スプリングコライダー
	int deleteSpringCollider = -1;
	for (int i = 0; i < Count(data.springColliders); ++i)
	{
		auto& value = data.springColliders[i];
		if (value.nodeIndex != nodeIndex ||
			selectedComponentType != AttachedComponentType::SpringCollider ||
			selectedComponentIndex != i) continue;
		ImGui::PushID(4000 + i);
		const bool selected = selectedComponentType == AttachedComponentType::SpringCollider &&
			selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)(ICON_FA_BULLSEYE " " u8"スプリングコライダー"),
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= DrawComponentTransform(*model, value.transform);
			changed |= ImGui::DragFloat((const char*)u8"半径", &value.radius, 0.01f, 0.001f);
			if (changed) MarkDirty();

			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteSpringCollider = i;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteSpringCollider >= 0) DeleteAttachedComponent(AttachedComponentType::SpringCollider, deleteSpringCollider);

	// トレイル
	auto& trailData = model->GetVmdlTrailData();
	int deleteTrail = -1;
	for (int i = 0; i < Count(trailData.trails); ++i)
	{
		auto& value = trailData.trails[i];
		if (value.nodeIndex != nodeIndex || selectedComponentType != AttachedComponentType::Trail ||
			selectedComponentIndex != i) continue;
		ImGui::PushID(5000 + i);
		const bool selected = selectedComponentType == AttachedComponentType::Trail &&
			selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)(ICON_FA_WIND " " u8"トレイル"),
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool initialActive = model->GetTrailInitialActive(i);
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= DrawComponentTransform(*model, value.transform);
			changed |=
				ImGui::DragFloat3((const char*)u8"根元オフセット", &value.rootOffset.x, 0.01f);
			changed |=
				ImGui::DragFloat3((const char*)u8"先端オフセット", &value.tipOffset.x, 0.01f);
			changed |= ImGui::ColorEdit4((const char*)u8"色", &value.color.x,
				ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
			changed |=
				ImGui::DragFloat((const char*)u8"先端比率", &value.tipRatio, 0.01f, 0.0f, 4.0f);
			changed |= ImGui::DragFloat(
				(const char*)u8"生存時間", &value.lifeTime, 0.01f, 0.01f, 10.0f, "%.3f sec");
			changed |= ImGui::DragInt((const char*)u8"最大頂点数", &value.maxPoints, 1.0f, 2, 1024);
			changed |=
				ImGui::DragFloat3((const char*)u8"角度オフセット", &value.offsetAngle.x, 0.01f);
			value.tipRatio = std::clamp(value.tipRatio, 0.0f, 4.0f);
			value.lifeTime = std::clamp(value.lifeTime, 0.01f, 10.0f);
			value.maxPoints = std::clamp(value.maxPoints, 2, 1024);
			if (changed) MarkDirty();
			if (ImGui::Checkbox((const char*)u8"初期状態で有効", &initialActive))
			{
				model->SetTrailInitialActive(i, initialActive);
				if (Count(previewTrailActive) <= i)
					previewTrailActive.resize(i + 1, 1);
				previewTrailActive[i] = initialActive ? 1 : 0;
				MarkDirty();
			}
			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteTrail = i;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteTrail >= 0) DeleteAttachedComponent(AttachedComponentType::Trail, deleteTrail);

	// パーティクルエミッタ
	auto& particleData = model->GetVmdlParticleData();
	int deleteParticle = -1;
	for (int i = 0; i < Count(particleData.emitters); ++i)
	{
		auto& value = particleData.emitters[i];
		if (value.nodeIndex != nodeIndex || selectedComponentType != AttachedComponentType::Particle ||
			selectedComponentIndex != i) continue;
		ImGui::PushID(5500 + i);
		const bool selected = selectedComponentType == AttachedComponentType::Particle &&
			selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)(ICON_FA_MAGIC " " u8"Effekseer"),
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= ImGui::Checkbox((const char*)u8"ノードに追従", &value.modelDerived);
			changed |= ImGui::Checkbox((const char*)u8"ビルボード", &value.billboard);
			changed |= DrawComponentTransform(*model, value.transform);
			changed |= DrawEffectSelector((const char*)u8"エフェクト", value.effectName);
			ImGui::BeginDisabled(value.effectName.empty());
			if (ImGui::Button(ICON_FA_PLAY "##TestPlay"))
			{
				showParticle = true;
				unifiedPreviewActive = true;
				if (!particlePreviewOwner || particlePreviewComponents.size() !=
					particleData.emitters.size())
					RebuildParticlePreview();
				if (i < Count(particlePreviewComponents) &&
					particlePreviewComponents[i])
				{
					manualParticlePreviewIndex = i;
					particlePreviewComponents[i]->Play();
				}
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip((const char*)u8"テスト再生");
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteParticle = i;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"複製");
			if (changed) { MarkDirty(); RebuildParticlePreview(); }
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteParticle >= 0) DeleteAttachedComponent(AttachedComponentType::Particle, deleteParticle);

	// サウンドソース
	auto& soundData = model->GetVmdlSoundData();
	int deleteSoundSource = -1;
	for (int i = 0; i < Count(soundData.sources); ++i)
	{
		auto& value = soundData.sources[i];
		if (value.nodeIndex != nodeIndex ||
			selectedComponentType != AttachedComponentType::SoundSource ||
			selectedComponentIndex != i) continue;
		ImGui::PushID(6000 + i);
		const bool selected = selectedComponentType == AttachedComponentType::SoundSource &&
			selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)(ICON_FA_VOLUME_UP " " u8"サウンドソース"),
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= DrawComponentTransform(*model, value.transform);
			if (DrawSoundPathSelector((const char*)u8"サウンド", value.soundPath)) changed = true;
			bool randomVariant = value.variant < 0;
			if (ImGui::Checkbox((const char*)u8"バリエーションをランダム選択", &randomVariant))
			{
				value.variant = randomVariant ? -1 : 0;
				changed = true;
			}
			if (!randomVariant)
				changed |= ImGui::DragInt(
					(const char*)u8"バリエーション番号", &value.variant, 1.0f, 0, 10000);
			changed |= ImGui::DragFloat((const char*)u8"音量", &value.volume, 0.01f, 0.0f, 4.0f);
			changed |= ImGui::DragFloatRange2((const char*)u8"ランダムピッチ",
				&value.pitchMin, &value.pitchMax, 0.01f, 0.125f, 8.0f, "%.2f", "%.2f");
			changed |= ImGui::Checkbox((const char*)u8"3Dサウンド", &value.spatial);
			if (value.spatial)
			{
				changed |= ImGui::DragFloat(
					(const char*)u8"減衰開始距離", &value.minDistance, 0.1f, 0.01f, 1000.0f);
				changed |= ImGui::DragFloat(
					(const char*)u8"再生限界距離", &value.maxDistance, 0.1f, 0.02f, 1000.0f);
				changed |= ImGui::DragFloat(
					(const char*)u8"近距離ローパス", &value.lowPassHz, 10.0f, 20.0f, 20000.0f);
				changed |= ImGui::DragFloat(
					(const char*)u8"遠距離ローパス", &value.farLowPassHz, 10.0f, 20.0f, 20000.0f);
				changed |= ImGui::SliderFloat(
					(const char*)u8"残響", &value.reverbMix, 0.0f, 1.0f);
			}
			value.volume = std::clamp(value.volume, 0.0f, 4.0f);
			value.pitchMin = std::clamp(value.pitchMin, 0.125f, 8.0f);
			value.pitchMax = std::clamp(value.pitchMax, value.pitchMin, 8.0f);
			value.minDistance = std::max(0.01f, value.minDistance);
			value.maxDistance = std::max(value.minDistance + 0.01f, value.maxDistance);
			value.lowPassHz = std::clamp(value.lowPassHz, 20.0f, 20000.0f);
			value.farLowPassHz = std::clamp(value.farLowPassHz, 20.0f, 20000.0f);
			value.reverbMix = std::clamp(value.reverbMix, 0.0f, 1.0f);
			if (changed) MarkDirty();

			ImGui::BeginDisabled(value.soundPath.empty());
			if (ImGui::Button(ICON_FA_PLAY "##TestSound")) PlaySoundSourcePreview(i);
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip((const char*)u8"テスト再生");
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteSoundSource = i;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteSoundSource >= 0) DeleteAttachedComponent(AttachedComponentType::SoundSource, deleteSoundSource);

	// ポイントライト
	auto& pointLights = model->GetVmdlLightData().pointLights;
	int deletePointLight = -1;
	for (int i = 0; i < Count(pointLights); ++i)
	{
		auto& value = pointLights[i];
		if (value.nodeIndex != nodeIndex ||
			selectedComponentType != AttachedComponentType::PointLight ||
			selectedComponentIndex != i) continue;
		ImGui::PushID(6500 + i);
		const bool selected = selectedComponentType == AttachedComponentType::PointLight &&
			selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)(ICON_FA_LIGHTBULB " " u8"ポイントライト"),
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= DrawComponentTransform(*model, value.transform);
			changed |= ImGui::Checkbox((const char*)u8"有効", &value.active);
			changed |= ImGui::ColorEdit3((const char*)u8"色", &value.color.x,
				ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
			changed |= ImGui::DragFloat(
				(const char*)u8"強度", &value.intensity, 0.1f, 0.0f, 100000.0f);
			changed |= ImGui::DragFloat(
				(const char*)u8"範囲", &value.range, 0.1f, 0.0f, 10000.0f);
			value.intensity = std::max(0.0f, value.intensity);
			value.range = std::max(0.0f, value.range);
			if (changed) MarkDirty();
			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deletePointLight = i;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deletePointLight >= 0) DeleteAttachedComponent(AttachedComponentType::PointLight, deletePointLight);

	auto& presentation = model->GetVmdlPresentationData();
	auto drawDistanceMode = [](bool& attenuate) {
		bool fixed = !attenuate;
		if (ImGui::Checkbox((const char*)u8"範囲内は固定強度", &fixed) && fixed)
			attenuate = false;
		bool distance = attenuate;
		if (ImGui::Checkbox((const char*)u8"プレイヤーとの距離に応じて減衰", &distance) && distance)
			attenuate = true;
	};
	int deleteCameraShake = -1;
	for (int i = 0; i < Count(presentation.cameraShakes); ++i)
	{
		auto& value = presentation.cameraShakes[i];
		if (value.nodeIndex != nodeIndex ||
			selectedComponentType != AttachedComponentType::CameraShake ||
			selectedComponentIndex != i) continue;
		ImGui::PushID(7000 + i);
		const bool selected = selectedComponentType == AttachedComponentType::CameraShake && selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)(ICON_FA_CAMERA " " u8"カメラシェイク"), selected ? ImGuiTreeNodeFlags_Selected : 0))
		{
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= DrawComponentTransform(*model, value.transform);
			changed |= ImGui::DragFloat((const char*)u8"効果範囲", &value.range, 0.1f, 0.01f, 10000.0f);
			const bool oldMode = value.distanceAttenuation; drawDistanceMode(value.distanceAttenuation);
			changed |= oldMode != value.distanceAttenuation;
			changed |= ImGui::DragFloat((const char*)u8"継続時間", &value.duration, 0.05f, 0.01f, 30.0f);
			changed |= ImGui::DragFloat((const char*)u8"揺れの強さ", &value.intensity, 0.005f, 0.0f, 10.0f);
			value.range = std::max(0.01f, value.range); value.duration = std::max(0.01f, value.duration);
			value.intensity = std::max(0.0f, value.intensity);
			if (changed) MarkDirty();
			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteCameraShake = i;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteCameraShake >= 0) DeleteAttachedComponent(AttachedComponentType::CameraShake, deleteCameraShake);

	int deleteRadialBlur = -1;
	for (int i = 0; i < Count(presentation.radialBlurs); ++i)
	{
		auto& value = presentation.radialBlurs[i];
		if (value.nodeIndex != nodeIndex || selectedComponentType != AttachedComponentType::RadialBlur ||
			selectedComponentIndex != i) continue;
		ImGui::PushID(8000 + i);
		const bool selected = selectedComponentType == AttachedComponentType::RadialBlur && selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)(ICON_FA_SUN " " u8"ラジアルブラー"), selected ? ImGuiTreeNodeFlags_Selected : 0))
		{
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= DrawComponentTransform(*model, value.transform);
			changed |= ImGui::DragFloat((const char*)u8"効果範囲", &value.range, 0.1f, 0.01f, 10000.0f);
			const bool oldMode = value.distanceAttenuation; drawDistanceMode(value.distanceAttenuation);
			changed |= oldMode != value.distanceAttenuation;
			changed |= ImGui::DragFloat((const char*)u8"継続時間", &value.duration, 0.05f, 0.01f, 30.0f);
			changed |= ImGui::DragFloat((const char*)u8"ブラー強度", &value.power, 0.05f, 0.0f, 20.0f);
			changed |= ImGui::SliderFloat((const char*)u8"立ち上がり割合", &value.attackRate, 0.01f, 0.95f);
			value.range = std::max(0.01f, value.range); value.duration = std::max(0.01f, value.duration);
			value.power = std::max(0.0f, value.power);
			if (changed) MarkDirty();
			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteRadialBlur = i;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip((const char*)u8"複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteRadialBlur >= 0) DeleteAttachedComponent(AttachedComponentType::RadialBlur, deleteRadialBlur);
	if (duplicatePending) DuplicateSelectedAttachedComponent();
}

void VmdlEditorScene::DrawTimeline()
{
	if (!model)
	{
		ImGui::TextDisabled((const char*)u8"モデルが読み込まれていません");
		return;
	}

	// アニメーションの選択と削除
	auto& animations = model->GetAnimations();
	const char* preview =
		selectedAnimation >= 0 && selectedAnimation < Count(animations)
			? animations[selectedAnimation].name.c_str()
			: (const char*)u8"（なし）";
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted((const char*)u8"アニメーション");
	ImGui::SameLine();
	const float deleteAnimationButtonWidth = ImGui::GetFrameHeight();
	ImGui::SetNextItemWidth(
		std::max(80.0f, ImGui::GetContentRegionAvail().x - deleteAnimationButtonWidth -
							ImGui::GetStyle().ItemSpacing.x));
	if (ImGui::BeginCombo("##Animation", preview))
	{
		for (int i = 0; i < Count(animations); ++i)
		{
			const bool selected =
				ImGui::Selectable(animations[i].name.c_str(), selectedAnimation == i);
			const ImVec2 itemMin = ImGui::GetItemRectMin();
			const ImVec2 itemMax = ImGui::GetItemRectMax();
			if (selected)
			{
				selectedAnimation = i;
				animationTime = 0.0f;
				animationPlaying = false;
				ApplyAnimationPreview();
			}
			if (selectedAnimation == i)
			{
				ImGui::GetWindowDrawList()->AddRect(
					itemMin, itemMax, ImGuiTheme::SelectedOutline, 2.0f, 0, 2.0f);
				ImGui::GetWindowDrawList()->AddRectFilled(
					itemMin, ImVec2(itemMin.x + 4.0f, itemMax.y), ImGuiTheme::SelectedAccent);
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	const bool canDeleteAnimation =
		selectedAnimation >= 0 && selectedAnimation < Count(animations);
	ImGui::BeginDisabled(!canDeleteAnimation);
	if (ImGui::Button(
			(const char*)u8"×##DeleteAnimation", ImVec2(deleteAnimationButtonWidth, 0.0f)))
		ImGui::OpenPopup((const char*)u8"アニメーションを削除");
	ImGui::EndDisabled();

	if (ImGui::BeginPopupModal(
			(const char*)u8"アニメーションを削除", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (canDeleteAnimation)
		{
			ImGui::Text(
				(const char*)u8"「%s」を削除しますか", animations[selectedAnimation].name.c_str());
			ImGui::TextDisabled((const char*)u8"関連するIKウェイトとイベント設定も削除されます");
			if (ImGui::Button((const char*)u8"削除", ImVec2(120.0f, 0.0f)))
			{
				const std::string deletedName = animations[selectedAnimation].name;
				animations.erase(animations.begin() + selectedAnimation);
				const bool sameNameRemains = std::any_of(
					animations.begin(), animations.end(), [&deletedName](const auto& animation) {
						return animation.name == deletedName;
					});
				if (!sameNameRemains)
				{
					const std::string footWeightPrefix = deletedName + "::FootWeight:";
					auto& editorData = model->GetVmdlAnimationEditorData();
					std::erase_if(editorData.ikContactOffsetTracks, [&deletedName](const auto& track) {
						return track.animationName == deletedName;
					});
					std::erase_if(
						editorData.footWeightTracks, [&footWeightPrefix](const auto& track) {
							return track.animationName.starts_with(footWeightPrefix);
						});
					auto& controlData = model->GetVmdlAnimationControlData();
					std::erase_if(controlData.colliderTracks, [&deletedName](const auto& track) {
						return track.animationName == deletedName;
					});
					std::erase_if(model->GetMaterialAnimationTracks(), [&deletedName](const auto& track) {
						return track.animationName == deletedName;
					});
					std::erase_if(controlData.morphTracks, [&deletedName](const auto& track) {
						return track.animationName == deletedName;
					});
					auto& trailData = model->GetVmdlTrailData();
					std::erase_if(trailData.tracks, [&deletedName](const auto& track) {
						return track.animationName == deletedName;
					});
					auto& particleData = model->GetVmdlParticleData();
					std::erase_if(particleData.tracks, [&deletedName](const auto& track) {
						return track.animationName == deletedName;
					});
					auto& presentation = model->GetVmdlPresentationData();
					std::erase_if(presentation.cameraShakeTracks, [&deletedName](const auto& track) {
						return track.animationName == deletedName;
					});
					std::erase_if(presentation.radialBlurTracks, [&deletedName](const auto& track) {
						return track.animationName == deletedName;
					});
				}
				selectedAnimation =
					animations.empty()
						? -1
						: std::min(selectedAnimation, Count(animations) - 1);
				animationTime = 0.0f;
				animationPlaying = false;
				animationRecording = false;
				draggingAnimationKey = false;
				scrubbingAnimationTime = false;
				paintingFootWeight = false;
				paintingFootWeightIndex = -1;
				paintingFootWeightLastSample = -1;
				selectedKeyTrack = -1;
				selectedKeyIndex = -1;
				timelineEventContextKind = -1;
				ResetAnimationControlPreview();
				if (selectedAnimation >= 0) ApplyAnimationPreview();
				MarkDirty();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
		}
		if (ImGui::Button((const char*)u8"キャンセル", ImVec2(120.0f, 0.0f)))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	if (selectedAnimation < 0 || selectedAnimation >= Count(animations)) return;

	VMDLModel::Animation& animation = animations[selectedAnimation];

	// 録画と再生操作
	if (animationRecording)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
	}

	const bool recordClicked = ImGui::Button(ICON_FA_CAMERA "##Record", ImVec2(38.0f, 0.0f));

	if (animationRecording)
	{
		ImGui::PopStyleColor(3);
	}

	if (recordClicked) animationRecording = !animationRecording;
	ImGui::SameLine();
	const float progress = animation.secondsLength > 0.0f
		? std::clamp(animationTime / animation.secondsLength, 0.0f, 1.0f) : 0.0f;
	ImGui::Text("%.3f / %.3f sec  (%.3f / 1.000)",
		animationTime, animation.secondsLength, progress);

	// IK・イベントのトラックはノード未選択でも表示する。
	DrawAnimationCurves();
}

void VmdlEditorScene::DrawAnimationCurves()
{
	auto& io = ImGui::GetIO();
	if (!model || selectedAnimation < 0 || selectedAnimation >= Count(model->GetAnimations())) return;
	auto& animation = model->GetAnimations()[selectedAnimation];
	const bool hasNode = selectedNode >= 0 && selectedNode < Count(animation.nodeAnims) &&
		selectedNode < Count(model->GetNodes());
	VMDLModel::NodeAnim emptyKeys;
	auto& keys = hasNode ? animation.nodeAnims[selectedNode] : emptyKeys;
	if (!hasNode)
	{
		draggingNumericTrack = -1;
		draggingAnimationKey = false;
		selectedKeyTrack = -1;
		selectedKeyIndex = -1;
	}
	const float length = std::max(0.001f, animation.secondsLength);
	const auto& ikSettings = model->GetVmdlIKSettings();
	const int footWeightCount = ikSettings.type == 0
		? 0
		: std::min(Count(ikSettings.legs), VMDLModel::VmdlIKSettings::MaxLegCount);
	const auto footWeightLabel = [&](int index) { return ikSettings.legs[index].name.c_str(); };
	std::vector<VMDLModel::VmdlFootWeightTrack*> footWeightTracks(footWeightCount, nullptr);
	for (int i = 0; i < footWeightCount; ++i)
		footWeightTracks[i] = model->FindFootWeightTrack(animation.name, i);

	struct DopeRow
	{
		const char* label;
		int track;
		bool child;
		int component;
	};
	std::vector<DopeRow> rows;
	const auto addRows = [&rows](const char* label, int track, bool expanded, int componentCount) {
		rows.push_back({label, track, false, -1});
		if (!expanded) return;
		constexpr const char* components[] = {"X", "Y", "Z", "W"};
		for (int i = 0; i < componentCount; ++i) rows.push_back({components[i], track, true, i});
	};
	if (hasNode)
	{
		addRows((const char*)u8"  位置", 0, positionTrackExpanded, 3);
		addRows((const char*)u8"  回転", 1, rotationTrackExpanded, 4);
		addRows((const char*)u8"  スケール", 2, scaleTrackExpanded, 3);
	}
	struct KeyHit
	{
		ImVec2 position;
		int track;
		int index;
	};
	std::vector<KeyHit> hitKeys;

	ImGui::SeparatorText((const char*)u8"ドープシート");
	if (timelineSelectionAnimation != selectedAnimation || timelineSelectionNode != selectedNode ||
		timelineSelectionMesh != selectedMesh)
	{
		timelineSelectedKeys.clear();
		timelineKeysDragging = false;
		timelineBoxSelecting = false;
		timelineSelectionAnimation = selectedAnimation;
		timelineSelectionNode = selectedNode;
		timelineSelectionMesh = selectedMesh;
		paintingFootWeight = false;
		draggingNumericTrack = -1;
	}
	if (timelineSelectedKeys.size() == 1 && timelineSelectedKeys.front().kind < 3)
	{
		selectedKeyTrack = timelineSelectedKeys.front().kind;
		selectedKeyIndex = timelineSelectedKeys.front().index;
	}
	else selectedKeyTrack = selectedKeyIndex = -1;
	ImGui::BeginDisabled(timelineSelectedKeys.empty());
	const bool deleteTimelineKeys = ImGui::SmallButton((const char*)u8"選択キーを削除");
	ImGui::EndDisabled();
	const float rulerHeight = 26.0f;
	const float footWeightHeight = 20.0f;
	const float rowHeight = 20.0f;
	const float labelWidth = 250.0f;
	const float footWeightsHeight = footWeightHeight * footWeightCount;
	const float ikContactOffsetRowHeight = rowHeight;
	const float rowsTopOffset = rulerHeight + ikContactOffsetRowHeight + footWeightsHeight;
	const int colliderRowCount = Count(model->GetVmdlExtensionData().colliders);
	const int trailRowCount = Count(model->GetVmdlTrailData().trails);
	const int particleRowCount = Count(model->GetVmdlParticleData().emitters);
	const int morphRowCount = Count(model->GetVmdlExtensionData().morphs);
	const int soundRowCount = Count(model->GetVmdlSoundData().sources);
	const int cameraShakeRowCount =
		Count(model->GetVmdlPresentationData().cameraShakes);
	const int radialBlurRowCount =
		Count(model->GetVmdlPresentationData().radialBlurs);
	const int materialRowCount = selectedMesh >= 0 && selectedMaterial >= 0 &&
		selectedMaterial < Count(model->GetMaterials()) ? 1 : 0;
	const int eventRowCount = colliderRowCount + trailRowCount + particleRowCount + morphRowCount +
		soundRowCount + cameraShakeRowCount + radialBlurRowCount + materialRowCount;
	const float eventRowsTopOffset = rowsTopOffset + rowHeight * rows.size();
	const float sheetHeight = eventRowsTopOffset + rowHeight * eventRowCount;
	ImGui::InvisibleButton(
		"Animation Dope Sheet", ImVec2(-1.0f, sheetHeight), ImGuiButtonFlags_MouseButtonLeft);
	const ImVec2 sheetMin = ImGui::GetItemRectMin();
	const ImVec2 sheetMax = ImGui::GetItemRectMax();
	const bool hovered = ImGui::IsItemHovered();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const float timeLeft = sheetMin.x + labelWidth + 8.0f;
	const float timeRight = sheetMax.x - 8.0f;
	const float timeWidth = std::max(1.0f, timeRight - timeLeft);
	const auto timeToX = [&](float seconds) {
		return timeLeft + std::clamp(seconds / length, 0.0f, 1.0f) * timeWidth;
	};
	const auto xToTime = [&](float x) {
		return std::clamp((x - timeLeft) / timeWidth, 0.0f, 1.0f) * length;
	};

	drawList->AddRectFilled(sheetMin, sheetMax, IM_COL32(42, 42, 45, 255));
	drawList->AddRectFilled(
		sheetMin, ImVec2(sheetMin.x + labelWidth, sheetMax.y), IM_COL32(52, 52, 56, 255));
	drawList->AddLine(ImVec2(sheetMin.x + labelWidth, sheetMin.y),
		ImVec2(sheetMin.x + labelWidth, sheetMax.y), IM_COL32(90, 90, 94, 255));
	for (int tick = 0; tick <= 10; ++tick)
	{
		const float ratio = tick / 10.0f;
		const float x = timeLeft + ratio * timeWidth;
		const ImU32 color =
			tick % 5 == 0 ? IM_COL32(105, 105, 110, 255) : IM_COL32(70, 70, 74, 255);
		drawList->AddLine(ImVec2(x, sheetMin.y + rulerHeight), ImVec2(x, sheetMax.y), color);
		const std::string timeLabel = std::format("{:.2f}", length * ratio);
		drawList->AddText(
			ImVec2(x + 2.0f, sheetMin.y + 4.0f), IM_COL32(185, 185, 190, 255),
			timeLabel.c_str());
	}
	drawList->AddLine(ImVec2(sheetMin.x, sheetMin.y + rulerHeight),
		ImVec2(sheetMax.x, sheetMin.y + rulerHeight), IM_COL32(95, 95, 100, 255));
	const auto weightColor = [](float weight) {
		const float value = std::clamp(weight, 0.0f, 1.0f);
		const ImVec4 stops[] = {ImVec4(0.0f, 0.0f, 0.0f, 1.0f), ImVec4(0.0f, 0.18f, 0.85f, 1.0f),
			ImVec4(1.0f, 0.9f, 0.0f, 1.0f), ImVec4(1.0f, 0.42f, 0.0f, 1.0f),
			ImVec4(0.95f, 0.02f, 0.0f, 1.0f)};
		const float scaled = value * 4.0f;
		const int index = std::min(static_cast<int>(scaled), 3);
		const float t = scaled - index;
		ImVec4 color(std::lerp(stops[index].x, stops[index + 1].x, t),
			std::lerp(stops[index].y, stops[index + 1].y, t),
			std::lerp(stops[index].z, stops[index + 1].z, t), 1.0f);
		return ImGui::ColorConvertFloat4ToU32(color);
	};
	const float ikRowTop = sheetMin.y + rulerHeight;
	const float ikRowCenter = ikRowTop + ikContactOffsetRowHeight * 0.5f;
	drawList->AddText(ImVec2(sheetMin.x + 8.0f, ikRowTop + 2.0f), IM_COL32(100, 220, 185, 255),
		(const char*)u8"IK接地オフセット");
	drawList->AddText(ImVec2(sheetMin.x + labelWidth - 20.0f, ikRowTop + 2.0f), IM_COL32(100, 220, 185, 255), "+");
	if (auto* heightTrack = model->FindIkContactOffsetTrack(animation.name))
	{
		for (const auto& key : heightTrack->keys)
		{
			const ImVec2 p(timeToX(key.seconds), ikRowCenter);
			const ImVec2 diamond[] = {ImVec2(p.x, p.y - 5.0f), ImVec2(p.x + 5.0f, p.y),
				ImVec2(p.x, p.y + 5.0f), ImVec2(p.x - 5.0f, p.y)};
			drawList->AddConvexPolyFilled(diamond, 4, IM_COL32(100, 220, 185, 255));
		}
	}
	const float weightTop = sheetMin.y + rulerHeight + ikContactOffsetRowHeight;
	const float weightBottom = weightTop + footWeightsHeight;
	for (int footIndex = 0; footIndex < footWeightCount; ++footIndex)
	{
		const float rowTop = weightTop + footIndex * footWeightHeight;
		const float rowBottom = rowTop + footWeightHeight;
		drawList->AddRectFilled(ImVec2(sheetMin.x, rowTop),
			ImVec2(sheetMin.x + labelWidth, rowBottom), IM_COL32(45, 45, 48, 255));
		std::string label = std::string((const char*)u8"足ウェイト／") + footWeightLabel(footIndex);
		drawList->AddText(
			ImVec2(sheetMin.x + 8.0f, rowTop + 2.0f), IM_COL32(210, 210, 215, 255), label.c_str());
		const ImVec2 noneButtonMin(sheetMin.x + labelWidth - 42.0f, rowTop + 2.0f);
		const ImVec2 noneButtonMax(sheetMin.x + labelWidth - 24.0f, rowBottom - 2.0f);
		const ImVec2 applyButtonMin(sheetMin.x + labelWidth - 22.0f, rowTop + 2.0f);
		const ImVec2 applyButtonMax(sheetMin.x + labelWidth - 4.0f, rowBottom - 2.0f);
		const bool noneButtonHovered = ImGui::IsMouseHoveringRect(noneButtonMin, noneButtonMax);
		const bool applyButtonHovered = ImGui::IsMouseHoveringRect(applyButtonMin, applyButtonMax);
		drawList->AddRectFilled(noneButtonMin, noneButtonMax,
			noneButtonHovered ? IM_COL32(185, 105, 125, 255) : IM_COL32(75, 78, 85, 255), 3.0f);
		drawList->AddRect(noneButtonMin, noneButtonMax, IM_COL32(130, 135, 145, 255), 3.0f);
		drawList->AddText(
			ImVec2(noneButtonMin.x + 5.0f, noneButtonMin.y), IM_COL32(240, 240, 245, 255), "N");
		drawList->AddRectFilled(applyButtonMin, applyButtonMax,
			applyButtonHovered ? IM_COL32(100, 150, 215, 255) : IM_COL32(75, 78, 85, 255), 3.0f);
		drawList->AddRect(applyButtonMin, applyButtonMax, IM_COL32(130, 135, 145, 255), 3.0f);
		drawList->AddText(
			ImVec2(applyButtonMin.x + 5.0f, applyButtonMin.y), IM_COL32(240, 240, 245, 255), "A");
		drawList->AddRectFilled(ImVec2(timeLeft, rowTop + 1.0f),
			ImVec2(timeRight, rowBottom - 1.0f), IM_COL32(0, 0, 0, 255));
		auto* track = footWeightTracks[footIndex];
		if (track && !track->weights.empty())
		{
			const float sampleWidth = timeWidth / track->weights.size();
			for (int i = 0; i < Count(track->weights); ++i)
			{
				const float x0 = timeLeft + i * sampleWidth;
				const float x1 = timeLeft + (i + 1) * sampleWidth + 1.0f;
				drawList->AddRectFilled(ImVec2(x0, rowTop + 1.0f), ImVec2(x1, rowBottom - 1.0f),
					weightColor(track->weights[i]));
			}
		}
		drawList->AddLine(ImVec2(sheetMin.x, rowBottom), ImVec2(sheetMax.x, rowBottom),
			IM_COL32(95, 95, 100, 255));
	}

	const ImU32 trackColors[] = {
		IM_COL32(255, 180, 65, 255), IM_COL32(100, 190, 255, 255), IM_COL32(110, 225, 120, 255)};
	const auto numericValue = [&](int track, int component) {
		if (track == 0)
		{
			if (selectedKeyTrack == track && selectedKeyIndex >= 0 &&
				selectedKeyIndex < Count(keys.positionKeyframes))
				return (&keys.positionKeyframes[selectedKeyIndex].value.x)[component];
			return (&model->GetNodes()[selectedNode].position.x)[component];
		}
		if (track == 1)
		{
			if (selectedKeyTrack == track && selectedKeyIndex >= 0 &&
				selectedKeyIndex < Count(keys.rotationKeyframes))
				return (&keys.rotationKeyframes[selectedKeyIndex].value.x)[component];
			return (&model->GetNodes()[selectedNode].rotation.x)[component];
		}
		if (selectedKeyTrack == track && selectedKeyIndex >= 0 &&
			selectedKeyIndex < Count(keys.scaleKeyframes))
			return (&keys.scaleKeyframes[selectedKeyIndex].value.x)[component];
		return (&model->GetNodes()[selectedNode].scale.x)[component];
	};
	for (int row = 0; row < Count(rows); ++row)
	{
		const float y0 = sheetMin.y + rowsTopOffset + row * rowHeight;
		const float centerY = y0 + rowHeight * 0.5f;
		if ((row & 1) != 0)
			drawList->AddRectFilled(ImVec2(sheetMin.x, y0), ImVec2(sheetMax.x, y0 + rowHeight),
				IM_COL32(48, 48, 51, 255));
		drawList->AddLine(ImVec2(sheetMin.x, y0 + rowHeight), ImVec2(sheetMax.x, y0 + rowHeight),
			IM_COL32(61, 61, 65, 255));
		const float indent = rows[row].child ? 28.0f : 8.0f;
		if (!rows[row].child)
		{
			const bool expanded = rows[row].track == 0	 ? positionTrackExpanded
								  : rows[row].track == 1 ? rotationTrackExpanded
														 : scaleTrackExpanded;
			drawList->AddText(ImVec2(sheetMin.x + 7.0f, y0 + 2.0f), trackColors[rows[row].track],
				expanded ? "v" : ">");
		}
		drawList->AddText(ImVec2(sheetMin.x + indent, y0 + 2.0f),
			rows[row].child ? IM_COL32(205, 205, 210, 255) : IM_COL32(240, 240, 242, 255),
			rows[row].label);
		if (rows[row].child)
		{
			const std::string valueText = std::format(
				"{:.3f}", numericValue(rows[row].track, rows[row].component));
			drawList->AddRectFilled(ImVec2(sheetMin.x + 92.0f, y0 + 1.0f),
				ImVec2(sheetMin.x + labelWidth - 7.0f, y0 + rowHeight - 1.0f),
				IM_COL32(63, 63, 67, 255), 3.0f);
			drawList->AddText(
				ImVec2(sheetMin.x + 104.0f, y0 + 2.0f), IM_COL32(220, 220, 225, 255),
				valueText.c_str());
		}

		const auto drawTrackKeys = [&](const auto& trackKeys) {
			for (int keyIndex = 0; keyIndex < Count(trackKeys); ++keyIndex)
			{
				const ImVec2 position(timeToX(trackKeys[keyIndex].seconds), centerY);
				const bool selected =
					selectedKeyTrack == rows[row].track && selectedKeyIndex == keyIndex;
				const ImU32 color =
					selected ? IM_COL32(255, 235, 90, 255) : trackColors[rows[row].track];
				const ImVec2 diamond[] = {ImVec2(position.x, position.y - 5.0f),
					ImVec2(position.x + 5.0f, position.y), ImVec2(position.x, position.y + 5.0f),
					ImVec2(position.x - 5.0f, position.y)};
				drawList->AddConvexPolyFilled(diamond, 4, color);
				drawList->AddPolyline(
					diamond, 4, IM_COL32(25, 25, 25, 255), ImDrawFlags_Closed, 1.0f);
				hitKeys.push_back({position, rows[row].track, keyIndex});
			}
		};
		if (rows[row].track == 0) drawTrackKeys(keys.positionKeyframes);
		else if (rows[row].track == 1) drawTrackKeys(keys.rotationKeyframes);
		else drawTrackKeys(keys.scaleKeyframes);
	}

	const auto& colliders = model->GetVmdlExtensionData().colliders;
	const auto& trails = model->GetVmdlTrailData().trails;
	const auto& particles = model->GetVmdlParticleData().emitters;
	const auto& morphs = model->GetVmdlExtensionData().morphs;
	auto& controlData = model->GetVmdlAnimationControlData();
	auto& particleData = model->GetVmdlParticleData();
	auto& soundData = model->GetVmdlSoundData();
	auto& presentation = model->GetVmdlPresentationData();
	const auto drawEventDiamond = [&](float seconds, float centerY, ImU32 color) {
		const ImVec2 position(timeToX(seconds), centerY);
		const ImVec2 diamond[] = {ImVec2(position.x, position.y - 5.0f),
			ImVec2(position.x + 5.0f, position.y), ImVec2(position.x, position.y + 5.0f),
			ImVec2(position.x - 5.0f, position.y)};
		drawList->AddConvexPolyFilled(diamond, 4, color);
		drawList->AddPolyline(diamond, 4, IM_COL32(25, 25, 25, 255), ImDrawFlags_Closed, 1.0f);
	};
	for (int eventRow = 0; eventRow < eventRowCount; ++eventRow)
	{
		const float y0 = sheetMin.y + eventRowsTopOffset + eventRow * rowHeight;
		const float centerY = y0 + rowHeight * 0.5f;
		if ((eventRow & 1) == 0)
			drawList->AddRectFilled(ImVec2(sheetMin.x, y0), ImVec2(sheetMax.x, y0 + rowHeight),
				IM_COL32(48, 48, 51, 255));
		drawList->AddLine(ImVec2(sheetMin.x, y0 + rowHeight), ImVec2(sheetMax.x, y0 + rowHeight),
			IM_COL32(61, 61, 65, 255));

		if (eventRow < colliderRowCount)
		{
			const int colliderIndex = eventRow;
			const std::string label = (const char*)u8"コライダー／" + colliders[colliderIndex].name;
			drawList->AddText(
				ImVec2(sheetMin.x + 8.0f, y0 + 2.0f), IM_COL32(100, 220, 235, 255), label.c_str());
			for (const auto& track : controlData.colliderTracks)
			{
				if (track.animationName != animation.name || track.colliderIndex != colliderIndex)
					continue;
				for (const auto& key : track.keys)
					drawEventDiamond(key.seconds, centerY,
						key.value ? IM_COL32(80, 235, 115, 255) : IM_COL32(235, 75, 75, 255));
				break;
			}
		}
		else if (eventRow < colliderRowCount + trailRowCount)
		{
			const int trailIndex = eventRow - colliderRowCount;
			const std::string label = (const char*)u8"トレイル／" + trails[trailIndex].name;
			drawList->AddText(
				ImVec2(sheetMin.x + 8.0f, y0 + 2.0f), IM_COL32(255, 155, 55, 255), label.c_str());
			for (const auto& track : model->GetVmdlTrailData().tracks)
			{
				if (track.animationName != animation.name || track.trailIndex != trailIndex)
					continue;
				for (const auto& key : track.keys)
					drawEventDiamond(key.seconds, centerY,
						key.value ? IM_COL32(80, 235, 115, 255) : IM_COL32(235, 75, 75, 255));
				break;
			}
		}
		else if (eventRow < colliderRowCount + trailRowCount + particleRowCount)
		{
			const int particleIndex = eventRow - colliderRowCount - trailRowCount;
			const std::string label = (const char*)u8"パーティクル／" + particles[particleIndex].name;
			drawList->AddText(
				ImVec2(sheetMin.x + 8.0f, y0 + 2.0f), IM_COL32(95, 210, 255, 255), label.c_str());
			for (const auto& track : particleData.tracks)
			{
				if (track.animationName != animation.name || track.emitterIndex != particleIndex) continue;
				for (const auto& key : track.keys)
					drawEventDiamond(key.seconds, centerY,
						key.value ? IM_COL32(80, 235, 115, 255) : IM_COL32(235, 75, 75, 255));
				break;
			}
		}
		else if (eventRow < colliderRowCount + trailRowCount + particleRowCount + morphRowCount)
		{
			const int morphIndex = eventRow - colliderRowCount - trailRowCount - particleRowCount;
			const std::string label = (const char*)u8"モーフ／" + morphs[morphIndex].name;
			drawList->AddText(
				ImVec2(sheetMin.x + 8.0f, y0 + 2.0f), IM_COL32(210, 125, 255, 255), label.c_str());
			for (const auto& track : controlData.morphTracks)
			{
				if (track.animationName != animation.name) continue;
				for (const auto& key : track.keys)
				{
					if (key.morphIndex == morphIndex)
						drawEventDiamond(key.seconds, centerY, IM_COL32(210, 125, 255, 255));
				}
				break;
			}
		}
		else if (eventRow < colliderRowCount + trailRowCount + particleRowCount + morphRowCount + soundRowCount)
		{
			const int soundIndex =
				eventRow - colliderRowCount - trailRowCount - particleRowCount - morphRowCount;
			const std::string label =
				(const char*)u8"サウンド／" + soundData.sources[soundIndex].name;
			drawList->AddText(
				ImVec2(sheetMin.x + 8.0f, y0 + 2.0f), IM_COL32(245, 110, 190, 255), label.c_str());
			for (const auto& track : soundData.tracks)
			{
				if (track.animationName != animation.name) continue;
				for (const auto& key : track.keys)
				{
					if (key.sourceIndex == soundIndex)
						drawEventDiamond(key.seconds, centerY, IM_COL32(245, 110, 190, 255));
				}
				break;
			}
		}
		else if (eventRow < colliderRowCount + trailRowCount + particleRowCount + morphRowCount +
			soundRowCount + cameraShakeRowCount)
		{
			const int index = eventRow - colliderRowCount - trailRowCount - particleRowCount -
				morphRowCount - soundRowCount;
			const std::string label = (const char*)u8"カメラシェイク／" + presentation.cameraShakes[index].name;
			drawList->AddText(ImVec2(sheetMin.x + 8.0f, y0 + 2.0f),
				IM_COL32(255, 180, 65, 255), label.c_str());
			for (const auto& track : presentation.cameraShakeTracks)
				if (track.animationName == animation.name)
					for (const auto& key : track.keys)
						if (key.componentIndex == index)
							drawEventDiamond(key.seconds, centerY, IM_COL32(255, 180, 65, 255));
		}
		else if (eventRow < eventRowCount - materialRowCount)
		{
			const int index = eventRow - colliderRowCount - trailRowCount - particleRowCount -
				morphRowCount - soundRowCount - cameraShakeRowCount;
			const std::string label = (const char*)u8"ラジアルブラー／" + presentation.radialBlurs[index].name;
			drawList->AddText(ImVec2(sheetMin.x + 8.0f, y0 + 2.0f),
				IM_COL32(190, 105, 255, 255), label.c_str());
			for (const auto& track : presentation.radialBlurTracks)
				if (track.animationName == animation.name)
					for (const auto& key : track.keys)
						if (key.componentIndex == index)
							drawEventDiamond(key.seconds, centerY, IM_COL32(190, 105, 255, 255));
		}
		else
		{
			const std::string label = (const char*)u8"マテリアル／" + model->GetMaterials()[selectedMaterial].name;
			drawList->AddText(ImVec2(sheetMin.x + 8.0f, y0 + 2.0f), IM_COL32(255, 205, 95, 255), label.c_str());
			for (const auto& track : model->GetMaterialAnimationTracks())
				if (track.animationName == animation.name && track.materialIndex == selectedMaterial)
					for (const auto& key : track.keys)
						drawEventDiamond(key.seconds, centerY, IM_COL32(255, 205, 95, 255));
		}

	}

	const float playheadX = timeToX(animationTime);
	drawList->AddLine(ImVec2(playheadX, sheetMin.y), ImVec2(playheadX, sheetMax.y),
		IM_COL32(90, 190, 255, 255), 2.0f);
	const ImVec2 playheadTriangle[] = {ImVec2(playheadX - 5.0f, sheetMin.y),
		ImVec2(playheadX + 5.0f, sheetMin.y), ImVec2(playheadX, sheetMin.y + 7.0f)};
	drawList->AddConvexPolyFilled(playheadTriangle, 3, IM_COL32(90, 190, 255, 255));

	// すべてのキーを同じ識別子・選択・ドラッグ処理で扱う。
	const auto visitTrack = [&](const TimelineKeyId& id, auto&& action) {
		const auto visitGroup = [&](auto& tracks) {
			if (id.track >= 0 && id.track < Count(tracks)) action(tracks[id.track].keys);
		};
		switch (id.kind)
		{
		case 0: if (hasNode && id.track == selectedNode) action(keys.positionKeyframes); break;
		case 1: if (hasNode && id.track == selectedNode) action(keys.rotationKeyframes); break;
		case 2: if (hasNode && id.track == selectedNode) action(keys.scaleKeyframes); break;
		case 3: visitGroup(model->GetVmdlAnimationEditorData().ikContactOffsetTracks); break;
		case 4: visitGroup(controlData.colliderTracks); break;
		case 5: visitGroup(model->GetVmdlTrailData().tracks); break;
		case 6: visitGroup(particleData.tracks); break;
		case 7: visitGroup(controlData.morphTracks); break;
		case 8: visitGroup(soundData.tracks); break;
		case 9: visitGroup(presentation.cameraShakeTracks); break;
		case 10: visitGroup(presentation.radialBlurTracks); break;
		case 11: visitGroup(model->GetMaterialAnimationTracks()); break;
		}
	};
	const auto isSelected = [&](const TimelineKeyId& id) {
		return std::find(timelineSelectedKeys.begin(), timelineSelectedKeys.end(), id) != timelineSelectedKeys.end();
	};
	const auto sortTrack = [&](TimelineKeyId id) {
		visitTrack(id, [&](auto& trackKeys) {
			std::vector<int> order(trackKeys.size());
			for (int i = 0; i < Count(order); ++i) order[i] = i;
			std::stable_sort(order.begin(), order.end(), [&](int left, int right) {
				return trackKeys[left].seconds < trackKeys[right].seconds;
			});
			auto sorted = trackKeys;
			for (int i = 0; i < Count(order); ++i) sorted[i] = trackKeys[order[i]];
			const auto remap = [&](TimelineKeyId& keyId) {
				if (keyId.kind != id.kind || keyId.track != id.track) return;
				const auto found = std::find(order.begin(), order.end(), keyId.index);
				keyId.index = found == order.end() ? -1 : static_cast<int>(found - order.begin());
			};
			for (auto& selected : timelineSelectedKeys) remap(selected);
			remap(timelineDetailKey);
			trackKeys = std::move(sorted);
		});
	};
	struct VisibleKey { TimelineKeyId id; ImVec2 position; float seconds; };
	std::vector<VisibleKey> visibleKeys;
	for (const auto& hit : hitKeys)
	{
		const TimelineKeyId id{hit.track, selectedNode, hit.index};
		visitTrack(id, [&](const auto& trackKeys) {
			if (id.index >= 0 && id.index < Count(trackKeys))
				visibleKeys.push_back({id, hit.position, trackKeys[id.index].seconds});
		});
	}
	const auto gatherTracks = [&](int kind, const auto& tracks, auto&& rowForKey) {
		for (int trackIndex = 0; trackIndex < Count(tracks); ++trackIndex)
		{
			const auto& track = tracks[trackIndex];
			if (track.animationName != animation.name) continue;
			for (int i = 0; i < Count(track.keys); ++i)
			{
				const auto& key = track.keys[i];
				const int row = rowForKey(track, key);
				if (row < 0 || row >= eventRowCount) continue;
				visibleKeys.push_back({{kind, trackIndex, i},
					ImVec2(timeToX(key.seconds), sheetMin.y + eventRowsTopOffset + (row + 0.5f) * rowHeight), key.seconds});
			}
		}
	};
	const auto& heightTracks = model->GetVmdlAnimationEditorData().ikContactOffsetTracks;
	for (int t = 0; t < Count(heightTracks); ++t)
		if (heightTracks[t].animationName == animation.name)
			for (int i = 0; i < Count(heightTracks[t].keys); ++i)
				visibleKeys.push_back({{3, t, i}, ImVec2(timeToX(heightTracks[t].keys[i].seconds), ikRowCenter), heightTracks[t].keys[i].seconds});
	gatherTracks(4, controlData.colliderTracks, [&](const auto& track, const auto&) {
		return track.colliderIndex >= 0 && track.colliderIndex < colliderRowCount ? track.colliderIndex : -1;
	});
	gatherTracks(5, model->GetVmdlTrailData().tracks, [&](const auto& track, const auto&) {
		return track.trailIndex >= 0 && track.trailIndex < trailRowCount ? colliderRowCount + track.trailIndex : -1;
	});
	gatherTracks(6, particleData.tracks, [&](const auto& track, const auto&) {
		return track.emitterIndex >= 0 && track.emitterIndex < particleRowCount ? colliderRowCount + trailRowCount + track.emitterIndex : -1;
	});
	const int morphStart = colliderRowCount + trailRowCount + particleRowCount;
	const int soundStart = morphStart + morphRowCount;
	const int shakeStart = soundStart + soundRowCount;
	const int blurStart = shakeStart + cameraShakeRowCount;
	gatherTracks(7, controlData.morphTracks, [&](const auto&, const auto& key) {
		return key.morphIndex >= 0 && key.morphIndex < morphRowCount ? morphStart + key.morphIndex : -1;
	});
	gatherTracks(8, soundData.tracks, [&](const auto&, const auto& key) {
		return key.sourceIndex >= 0 && key.sourceIndex < soundRowCount ? soundStart + key.sourceIndex : -1;
	});
	gatherTracks(9, presentation.cameraShakeTracks, [&](const auto&, const auto& key) {
		return key.componentIndex >= 0 && key.componentIndex < cameraShakeRowCount ? shakeStart + key.componentIndex : -1;
	});
	gatherTracks(10, presentation.radialBlurTracks, [&](const auto&, const auto& key) {
		return key.componentIndex >= 0 && key.componentIndex < radialBlurRowCount ? blurStart + key.componentIndex : -1;
	});
	gatherTracks(11, model->GetMaterialAnimationTracks(), [&](const auto& track, const auto&) {
		return materialRowCount && track.materialIndex == selectedMaterial ? eventRowCount - 1 : -1;
	});
	int hitKey = -1;
	float hitDistance = 10.0f;
	for (int i = 0; i < Count(visibleKeys); ++i)
	{
		const auto& key = visibleKeys[i];
		const float distance = std::abs(io.MousePos.x - key.position.x) + std::abs(io.MousePos.y - key.position.y);
		if (hovered && distance < hitDistance) { hitDistance = distance; hitKey = i; }
		if (isSelected(key.id))
			drawList->AddCircle(key.position, 7.0f, IM_COL32(255, 230, 80, 255), 4, 2.0f);
	}
	if (hitKey >= 0) ImGui::SetTooltip("%.3f sec", visibleKeys[hitKey].seconds);
	const auto eraseSelected = [&]() {
		auto deleting = timelineSelectedKeys;
		std::sort(deleting.begin(), deleting.end(), [](const auto& left, const auto& right) {
			if (left.kind != right.kind) return left.kind < right.kind;
			if (left.track != right.track) return left.track < right.track;
			return left.index > right.index;
		});
		for (const auto& id : deleting)
			visitTrack(id, [&](auto& trackKeys) {
				if (id.index >= 0 && id.index < Count(trackKeys)) trackKeys.erase(trackKeys.begin() + id.index);
			});
		timelineSelectedKeys.clear();
		timelineKeysDragging = false;
		selectedKeyTrack = selectedKeyIndex = -1;
		MarkDirty();
		ApplyAnimationPreview();
	};
	const auto sortSelectedTracks = [&]() {
		std::vector<TimelineKeyId> tracks;
		for (const auto& id : timelineSelectedKeys)
			if (std::none_of(tracks.begin(), tracks.end(), [&](const auto& other) {
				return other.kind == id.kind && other.track == id.track;
			})) tracks.push_back(id);
		for (const auto& id : tracks) sortTrack(id);
	};
	if (deleteTimelineKeys || (!timelineSelectedKeys.empty() && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
		!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false))) eraseSelected();
	if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (io.MousePos.x >= timeLeft && io.MousePos.y >= weightTop && io.MousePos.y < weightBottom && footWeightCount > 0)
		{
			timelineBoxSelecting = false;
			timelineKeysDragging = false;
			scrubbingAnimationTime = false;
			paintingFootWeight = true;
			paintingFootWeightIndex = std::clamp(static_cast<int>((io.MousePos.y - weightTop) / footWeightHeight), 0, footWeightCount - 1);
			paintingFootWeightLastSample = -1;
		}
		else if ((io.KeyShift || io.KeyCtrl) && io.MousePos.x >= timeLeft)
		{
			timelineBoxSelecting = true;
			timelineBoxRemoving = io.KeyCtrl;
			timelineDragStart = {io.MousePos.x, io.MousePos.y};
			if (io.KeyCtrl && hitKey >= 0) std::erase(timelineSelectedKeys, visibleKeys[hitKey].id);
		}
		else if (hitKey >= 0)
		{
			const auto id = visibleKeys[hitKey].id;
			if (!isSelected(id)) { timelineSelectedKeys.clear(); timelineSelectedKeys.push_back(id); }
			selectedKeyTrack = id.kind < 3 ? id.kind : -1;
			selectedKeyIndex = id.kind < 3 ? id.index : -1;
			timelineKeysDragging = true;
			timelineDragStart = {io.MousePos.x, io.MousePos.y};
			timelineDragStartTimes.clear();
			for (const auto& selected : timelineSelectedKeys)
			{
				float seconds = 0.0f;
				visitTrack(selected, [&](const auto& trackKeys) {
					if (selected.index >= 0 && selected.index < Count(trackKeys)) seconds = trackKeys[selected.index].seconds;
				});
				timelineDragStartTimes.push_back(seconds);
			}
		}
		else if (io.MousePos.x >= timeLeft)
		{
			timelineSelectedKeys.clear();
			animationPlaying = false;
			scrubbingAnimationTime = true;
		}
		else if (io.MousePos.y >= ikRowTop && io.MousePos.y < weightTop)
		{
			timelineAddKind = 3; timelineAddTarget = -1; timelineAddTime = animationTime;
			ImGui::OpenPopup("Timeline Add Key");
		}
		else if (io.MousePos.y >= weightTop && io.MousePos.y < weightBottom && footWeightCount > 0)
		{
			const int footIndex = std::clamp(static_cast<int>((io.MousePos.y - weightTop) / footWeightHeight), 0, footWeightCount - 1);
			auto& track = model->GetOrCreateFootWeightTrack(animation.name, footIndex);
			const int samples = std::max(2, static_cast<int>(std::ceil(length * track.sampleRate)) + 1);
			const bool clear = io.MousePos.x < sheetMin.x + labelWidth - 24.0f;
			track.weights.assign(samples, clear ? 0.0f : 1.0f);
			MarkDirty(); ApplyAnimationPreview();
		}
		else if (io.MousePos.y >= sheetMin.y + rowsTopOffset && io.MousePos.y < sheetMin.y + eventRowsTopOffset && !rows.empty())
		{
			const int row = std::clamp(static_cast<int>((io.MousePos.y - sheetMin.y - rowsTopOffset) / rowHeight), 0, Count(rows) - 1);
			if (!rows[row].child)
			{
				if (rows[row].track == 0) positionTrackExpanded = !positionTrackExpanded;
				else if (rows[row].track == 1) rotationTrackExpanded = !rotationTrackExpanded;
				else scaleTrackExpanded = !scaleTrackExpanded;
			}
			else if (io.MousePos.x >= sheetMin.x + 92.0f)
			{
				draggingNumericTrack = rows[row].track;
				draggingNumericComponent = rows[row].component;
			}
		}
	}
	if (paintingFootWeight && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		if (paintingFootWeightIndex < 0 || paintingFootWeightIndex >= footWeightCount) return;
		auto* footWeightTrack =
			&model->GetOrCreateFootWeightTrack(animation.name, paintingFootWeightIndex);
		const int sampleCount =
			std::max(2, static_cast<int>(std::ceil(length * footWeightTrack->sampleRate)) + 1);
		if (footWeightTrack->weights.size() != sampleCount)
			footWeightTrack->weights.resize(sampleCount, 0.0f);
		const float ratio = std::clamp((io.MousePos.x - timeLeft) / timeWidth, 0.0f, 1.0f);
		const int center =
			std::clamp(static_cast<int>(ratio * (sampleCount - 1)), 0, sampleCount - 1);
		const int previousCenter =
			paintingFootWeightLastSample < 0 ? center : paintingFootWeightLastSample;
		const int strokeStart = std::min(previousCenter, center);
		const int strokeEnd = std::max(previousCenter, center);
		const int radius =
			std::max(1, static_cast<int>(std::round(footWeightTrack->sampleRate * 0.0125f)));
		const bool erase = io.KeyShift;
		const float amount = io.DeltaTime * 1.8f;
		for (int index = strokeStart - radius; index <= strokeEnd + radius; ++index)
		{
			if (index < 0 || index >= sampleCount) continue;
			const int distance = index < strokeStart ? strokeStart - index
								 : index > strokeEnd ? index - strokeEnd
													 : 0;
			const float falloff =
				1.0f - static_cast<float>(distance) / static_cast<float>(radius + 1);
			float& weight = footWeightTrack->weights[index];
			if (erase)
			{
				const float eraseAmount = amount * falloff;
				if (weight > 0.0f) weight = std::max(0.0f, weight - eraseAmount);
				else weight = std::min(0.0f, weight + eraseAmount);
			}
			else weight = std::clamp(weight + amount * falloff, 0.0f, 1.0f);
		}
		paintingFootWeightLastSample = center;
		MarkDirty();
	}
	if (paintingFootWeight && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		paintingFootWeight = false;
		paintingFootWeightIndex = -1;
		paintingFootWeightLastSample = -1;
	}

	if (draggingNumericTrack >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		const float delta = io.MouseDelta.x * 0.01f;
		VMDLModel::Node& node = model->GetNodes()[selectedNode];
		if (draggingNumericTrack == 0)
		{
			if (selectedKeyTrack == 0 && selectedKeyIndex >= 0 &&
				selectedKeyIndex < Count(keys.positionKeyframes))
				(&keys.positionKeyframes[selectedKeyIndex].value.x)[draggingNumericComponent] +=
					delta;
			else (&node.position.x)[draggingNumericComponent] += delta;
		}
		else if (draggingNumericTrack == 1)
		{
			if (selectedKeyTrack == 1 && selectedKeyIndex >= 0 &&
				selectedKeyIndex < Count(keys.rotationKeyframes))
			{
				(&keys.rotationKeyframes[selectedKeyIndex].value.x)[draggingNumericComponent] +=
					delta;
				keys.rotationKeyframes[selectedKeyIndex].value.Normalize();
			}
			else
			{
				(&node.rotation.x)[draggingNumericComponent] += delta;
				node.rotation.Normalize();
			}
		}
		else
		{
			if (selectedKeyTrack == 2 && selectedKeyIndex >= 0 &&
				selectedKeyIndex < Count(keys.scaleKeyframes))
				(&keys.scaleKeyframes[selectedKeyIndex].value.x)[draggingNumericComponent] += delta;
			else (&node.scale.x)[draggingNumericComponent] += delta;
		}
		MarkDirty();
		if (selectedKeyTrack == draggingNumericTrack) ApplyAnimationPreview();
		else if (animationRecording) RecordSelectedNodeKey();
	}
	if (draggingNumericTrack >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		draggingNumericTrack = -1;
		draggingNumericComponent = -1;
	}

	if (timelineBoxSelecting)
	{
		const ImVec2 boxMin(std::min(timelineDragStart.x, io.MousePos.x), std::min(timelineDragStart.y, io.MousePos.y));
		const ImVec2 boxMax(std::max(timelineDragStart.x, io.MousePos.x), std::max(timelineDragStart.y, io.MousePos.y));
		drawList->AddRectFilled(boxMin, boxMax, timelineBoxRemoving ? IM_COL32(230, 80, 80, 40) : IM_COL32(80, 160, 255, 40));
		drawList->AddRect(boxMin, boxMax, IM_COL32(150, 190, 255, 255));
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			for (const auto& key : visibleKeys)
				if (key.position.x >= boxMin.x && key.position.x <= boxMax.x && key.position.y >= boxMin.y && key.position.y <= boxMax.y)
				{
					if (timelineBoxRemoving) std::erase(timelineSelectedKeys, key.id);
					else if (!isSelected(key.id)) timelineSelectedKeys.push_back(key.id);
				}
			timelineBoxSelecting = false;
		}
	}
	if (timelineKeysDragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f))
	{
		float delta = (io.MousePos.x - timelineDragStart.x) / timeWidth * length;
		float minimum = -length, maximum = length;
		for (float time : timelineDragStartTimes) { minimum = std::max(minimum, -time); maximum = std::min(maximum, length - time); }
		delta = std::clamp(delta, minimum, maximum);
		for (int i = 0; i < Count(timelineSelectedKeys); ++i)
		{
			const auto& id = timelineSelectedKeys[i];
			visitTrack(id, [&](auto& trackKeys) {
				if (id.index >= 0 && id.index < Count(trackKeys)) trackKeys[id.index].seconds = timelineDragStartTimes[i] + delta;
			});
		}
		animationPlaying = false;
		sortSelectedTracks();
		MarkDirty();
		ApplyAnimationPreview();
	}
	if (timelineKeysDragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		sortSelectedTracks();
		timelineKeysDragging = false;
	}
	if (scrubbingAnimationTime && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		animationTime = xToTime(io.MousePos.x); ApplyAnimationPreview();
	}
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) scrubbingAnimationTime = false;
	if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		if (hitKey >= 0 && timelineSelectedKeys.size() == 1 && isSelected(visibleKeys[hitKey].id))
		{
			timelineDetailKey = timelineSelectedKeys.front();
			ImGui::OpenPopup("Timeline Key Details");
		}
		else if (hitKey < 0 && io.MousePos.x >= timeLeft)
		{
			timelineAddTime = xToTime(io.MousePos.x);
			timelineAddKind = -1;
			if (io.MousePos.y >= ikRowTop && io.MousePos.y < weightTop) timelineAddKind = 3;
			else if (io.MousePos.y >= sheetMin.y + eventRowsTopOffset)
			{
				const int row = static_cast<int>((io.MousePos.y - sheetMin.y - eventRowsTopOffset) / rowHeight);
				if (row < colliderRowCount) { timelineAddKind = 4; timelineAddTarget = row; }
				else if (row < colliderRowCount + trailRowCount) { timelineAddKind = 5; timelineAddTarget = row - colliderRowCount; }
				else if (row < morphStart) { timelineAddKind = 6; timelineAddTarget = row - colliderRowCount - trailRowCount; }
				else if (row < soundStart) { timelineAddKind = 7; timelineAddTarget = row - morphStart; }
				else if (row < shakeStart) { timelineAddKind = 8; timelineAddTarget = row - soundStart; }
				else if (row < blurStart) { timelineAddKind = 9; timelineAddTarget = row - shakeStart; }
				else if (row < blurStart + radialBlurRowCount) { timelineAddKind = 10; timelineAddTarget = row - blurStart; }
				else if (materialRowCount) { timelineAddKind = 11; timelineAddTarget = selectedMaterial; }
			}
			else if (io.MousePos.y >= sheetMin.y + rowsTopOffset && !rows.empty())
			{
				const int row = std::clamp(static_cast<int>((io.MousePos.y - sheetMin.y - rowsTopOffset) / rowHeight), 0, Count(rows) - 1);
				timelineAddKind = rows[row].track; timelineAddTarget = selectedNode;
			}
			if (timelineAddKind >= 0) ImGui::OpenPopup("Timeline Add Key");
		}
	}
	if (ImGui::BeginPopup("Timeline Key Details"))
	{
		if (timelineSelectedKeys.size() != 1 || timelineSelectedKeys.front() != timelineDetailKey)
			ImGui::CloseCurrentPopup();
		else
		{
			bool changed = false, remove = false;
			ImGui::TextUnformatted((const char*)u8"キー詳細");
			visitTrack(timelineDetailKey, [&](auto& trackKeys) {
				if (timelineDetailKey.index < 0 || timelineDetailKey.index >= Count(trackKeys)) return;
				auto& key = trackKeys[timelineDetailKey.index];
				changed |= ImGui::DragFloat((const char*)u8"時間", &key.seconds, 0.01f, 0.0f, length, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
				if constexpr (requires { key.contactOffset; })
					changed |= ImGui::DragFloat((const char*)u8"接地オフセット", &key.contactOffset, 0.005f);
				if constexpr (requires { key.value; })
				{
					if constexpr (std::is_same_v<decltype(key.value), bool>)
						changed |= ImGui::Checkbox((const char*)u8"有効", &key.value);
					else if constexpr (requires { key.value.w; })
					{
						if (ImGui::DragFloat4((const char*)u8"回転", &key.value.x, 0.01f))
						{
							key.value.Normalize(); changed = true;
						}
					}
					else changed |= ImGui::DragFloat3((const char*)u8"値", &key.value.x, 0.01f);
				}
				if constexpr (requires { key.sourceIndex; })
				{
					if (key.sourceIndex >= 0 && key.sourceIndex < Count(soundData.sources))
						ImGui::TextUnformatted(soundData.sources[key.sourceIndex].name.c_str());
					changed |= ImGui::DragInt((const char*)u8"バリエーション", &key.variant, 1.0f, -1, 100);
					changed |= ImGui::DragFloat((const char*)u8"音量", &key.volume, 0.01f, 0.0f, 4.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
					changed |= ImGui::DragFloat((const char*)u8"ピッチ最小", &key.pitchMin, 0.01f, 0.125f, 8.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
					changed |= ImGui::DragFloat((const char*)u8"ピッチ最大", &key.pitchMax, 0.01f, key.pitchMin, 8.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
					if (ImGui::Button((const char*)u8"試聴")) PlaySoundSourcePreview(key.sourceIndex);
				}
				if constexpr (requires { key.morphIndex; })
					if (key.morphIndex >= 0 && key.morphIndex < Count(morphs)) ImGui::TextUnformatted(morphs[key.morphIndex].name.c_str());
				if constexpr (requires { key.baseColor; })
				{
					changed |= ImGui::ColorEdit4((const char*)u8"基本色", &key.baseColor.x);
					changed |= ImGui::ColorEdit4((const char*)u8"発光色", &key.emissiveColor.x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
					changed |= ImGui::SliderFloat((const char*)u8"メタリック", &key.metalness, 0.0f, 1.0f);
					changed |= ImGui::SliderFloat((const char*)u8"粗さ", &key.roughness, 0.0001f, 1.0f);
				}
				remove = ImGui::Button((const char*)u8"キーを削除");
			});
			if (changed)
			{
				sortTrack(timelineDetailKey); MarkDirty(); ApplyAnimationPreview();
			}
			if (remove) { eraseSelected(); ImGui::CloseCurrentPopup(); }
		}
		ImGui::EndPopup();
	}
	if (ImGui::BeginPopup("Timeline Add Key"))
	{
		ImGui::Text("%.3f sec", timelineAddTime);
		const bool booleanKey = timelineAddKind == 4 || timelineAddKind == 5 || timelineAddKind == 6;
		const bool addOn = ImGui::MenuItem(booleanKey ? (const char*)u8"ONキーを追加" : (const char*)u8"キーを追加");
		const bool addOff = booleanKey && ImGui::MenuItem((const char*)u8"OFFキーを追加");
		if (addOn || addOff)
		{
			TimelineKeyId added{timelineAddKind, -1, -1};
			const auto addKey = [&](auto& trackKeys, const auto& key) {
				added.index = Count(trackKeys); trackKeys.push_back(key);
			};
			const auto animationTrack = [&](auto& tracks) -> auto& {
				for (int i = 0; i < Count(tracks); ++i)
					if (tracks[i].animationName == animation.name) { added.track = i; return tracks[i]; }
				added.track = Count(tracks);
				auto& track = tracks.emplace_back(); track.animationName = animation.name; return track;
			};
			switch (timelineAddKind)
			{
			case 0: if (hasNode) { added.track = selectedNode; addKey(keys.positionKeyframes, VMDLModel::VectorKeyframe{timelineAddTime, model->GetNodes()[selectedNode].position}); } break;
			case 1: if (hasNode) { added.track = selectedNode; addKey(keys.rotationKeyframes, VMDLModel::QuaternionKeyframe{timelineAddTime, model->GetNodes()[selectedNode].rotation}); } break;
			case 2: if (hasNode) { added.track = selectedNode; addKey(keys.scaleKeyframes, VMDLModel::VectorKeyframe{timelineAddTime, model->GetNodes()[selectedNode].scale}); } break;
			case 3:
			{
				const float offset = model->EvaluateIkContactOffset(selectedAnimation, timelineAddTime);
				auto& track = animationTrack(model->GetVmdlAnimationEditorData().ikContactOffsetTracks);
				addKey(track.keys, VMDLModel::VmdlIkContactOffsetKeyframe{timelineAddTime, offset}); break;
			}
			case 4:
			{
				auto& track = model->GetOrCreateColliderAnimationTrack(animation.name, timelineAddTarget);
				added.track = static_cast<int>(&track - controlData.colliderTracks.data());
				addKey(track.keys, VMDLModel::VmdlBoolKeyframe{timelineAddTime, addOn}); break;
			}
			case 5:
			{
				auto& tracks = model->GetVmdlTrailData().tracks;
				auto& track = model->GetOrCreateTrailAnimationTrack(animation.name, timelineAddTarget);
				added.track = static_cast<int>(&track - tracks.data());
				addKey(track.keys, VMDLModel::VmdlBoolKeyframe{timelineAddTime, addOn}); break;
			}
			case 6:
			{
				auto& track = model->GetOrCreateParticleAnimationTrack(animation.name, timelineAddTarget);
				added.track = static_cast<int>(&track - particleData.tracks.data());
				addKey(track.keys, VMDLModel::VmdlBoolKeyframe{timelineAddTime, addOn}); break;
			}
			case 7:
			{
				auto& track = animationTrack(controlData.morphTracks);
				addKey(track.keys, VMDLModel::VmdlMorphKeyframe{timelineAddTime, timelineAddTarget}); break;
			}
			case 8:
			{
				auto& track = animationTrack(soundData.tracks);
				VMDLModel::VmdlSoundKeyframe key; key.seconds = timelineAddTime; key.sourceIndex = timelineAddTarget;
				addKey(track.keys, key); break;
			}
			case 9: case 10:
			{
				auto& tracks = timelineAddKind == 9 ? presentation.cameraShakeTracks : presentation.radialBlurTracks;
				auto& track = animationTrack(tracks);
				addKey(track.keys, VMDLModel::VmdlPresentationKeyframe{timelineAddTime, timelineAddTarget}); break;
			}
			case 11:
			{
				model->RecordMaterialKey(selectedAnimation, selectedMaterial, timelineAddTime);
				auto& tracks = model->GetMaterialAnimationTracks();
				for (int t = 0; t < Count(tracks); ++t)
					if (tracks[t].animationName == animation.name && tracks[t].materialIndex == selectedMaterial)
						for (int i = 0; i < Count(tracks[t].keys); ++i)
							if (std::abs(tracks[t].keys[i].seconds - timelineAddTime) < 0.0005f) added = {11, t, i};
				break;
			}
			}
			timelineSelectedKeys.clear();
			if (added.index >= 0) { timelineSelectedKeys.push_back(added); sortTrack(added); }
			MarkDirty(); ApplyAnimationPreview(); ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void VmdlEditorScene::ApplyAnimationPreview()
{
	if (!model || selectedAnimation < 0 ||
		selectedAnimation >= Count(model->GetAnimations()))
		return;
	const auto& animation = model->GetAnimations()[selectedAnimation];
	auto& nodes = model->GetNodes();
	const int count = std::min(Count(nodes), Count(animation.nodeAnims));
	for (int i = 0; i < count; ++i)
	{
		VMDLModel::NodePose pose{nodes[i].position, nodes[i].rotation, nodes[i].scale};
		model->ComputeAnimation(selectedAnimation, i, animationTime, pose);
		nodes[i].position = pose.position;
		nodes[i].rotation = pose.rotation;
		nodes[i].scale = pose.scale;
	}
	if (previewColliderActive.size() != model->GetVmdlExtensionData().colliders.size())
		previewColliderActive.resize(model->GetVmdlExtensionData().colliders.size(), 1);
	for (int i = 0; i < Count(previewColliderActive); ++i)
		previewColliderActive[i] =
			model->EvaluateColliderActive(selectedAnimation, animationTime, i) ? 1 : 0;
	previewTrailActive.resize(model->GetVmdlTrailData().trails.size(), 1);
	for (int i = 0; i < Count(previewTrailActive); ++i)
		previewTrailActive[i] =
			model->EvaluateTrailActive(selectedAnimation, animationTime, i) ? 1 : 0;
	model->ApplyMorphAnimation(selectedAnimation, animationTime);
}

void VmdlEditorScene::PlaySoundSourcePreview(int sourceIndex)
{
	if (!model || sourceIndex < 0 || sourceIndex >= Count(model->GetVmdlSoundData().sources)) return;
	const auto& source = model->GetVmdlSoundData().sources[sourceIndex];
	SoundSystem::PlayOptions options;
	options.volume = source.volume;
	options.pitch = source.pitchMax > source.pitchMin
		? Random::Range(source.pitchMin, source.pitchMax) : source.pitchMin;
	if (source.spatial && source.nodeIndex >= 0 && source.nodeIndex < Count(model->GetNodes()))
	{
		SoundSystem::SpatialOptions spatial;
		static_cast<SoundSystem::PlayOptions&>(spatial) = options;
		spatial.minDistance = source.minDistance;
		spatial.maxDistance = source.maxDistance;
		spatial.lowPassHz = source.lowPassHz;
		spatial.farLowPassHz = source.farLowPassHz;
		spatial.reverbMix = source.reverbMix;
		const Vector3 position = model->GetScaledAttachmentTransform(
			source.transform.ToMatrix() * model->GetNodes()[source.nodeIndex].worldTransform).Translation();
		SoundSystem::Instance().Play3DAt(source.soundPath, position, spatial, source.variant);
	}
	else SoundSystem::Instance().Play(source.soundPath, options, source.variant);
}

void VmdlEditorScene::PlayAnimationSoundPreview(
	int animationIndex, float beginTime, float endTime)
{
	if (!model || animationIndex < 0 ||
		animationIndex >= Count(model->GetAnimations()) || endTime < beginTime)
		return;

	const auto& animation = model->GetAnimations()[animationIndex];
	const auto& soundData = model->GetVmdlSoundData();
	for (const auto& track : soundData.tracks)
	{
		if (track.animationName != animation.name) continue;
		for (const auto& key : track.keys)
		{
			if (key.seconds <= beginTime || key.seconds > endTime + 0.0001f ||
				key.sourceIndex < 0 || key.sourceIndex >= Count(soundData.sources))
				continue;

			PlaySoundSourcePreview(key.sourceIndex);
		}
	}
}

void VmdlEditorScene::ResetAnimationControlPreview()
{
	if (!model) return;
	previewColliderActive.resize(model->GetVmdlExtensionData().colliders.size(), 1);
	for (int i = 0; i < Count(previewColliderActive); ++i)
		previewColliderActive[i] = model->GetColliderInitialActive(i) ? 1 : 0;
	previewTrailActive.resize(model->GetVmdlTrailData().trails.size(), 1);
	for (int i = 0; i < Count(previewTrailActive); ++i)
		previewTrailActive[i] = model->GetTrailInitialActive(i) ? 1 : 0;
	previewParticleActive.resize(model->GetVmdlParticleData().emitters.size(), 0);
	for (int i = 0; i < Count(previewParticleActive); ++i)
		previewParticleActive[i] = model->GetParticleInitialActive(i) ? 1 : 0;
	model->RestoreRuntimeMorphVisibility();
}

void VmdlEditorScene::RebuildSpringPreview()
{
	springPreviewOwner.reset();
	springPreviewComponents.clear();
	springPreviewAnimation = -1;
	springPreviewAnimationTime = 0.0f;
	if (!model) return;

	const auto& data = model->GetVmdlExtensionData();
	std::vector<SpringBone::SpringCapsule> springColliders;
	springColliders.reserve(data.springColliders.size());
	for (const auto& value : data.springColliders)
	{
		SpringBone::SpringCapsule collider;
		collider.start = value.transform.position;
		collider.end = value.transform.position;
		collider.radius = value.radius * std::max({std::abs(value.transform.scale.x),
			std::abs(value.transform.scale.y), std::abs(value.transform.scale.z)});
		collider.nodeIndex = value.nodeIndex;
		springColliders.push_back(collider);
	}

	springPreviewOwner = std::make_unique<Object>("VMDL Spring Preview");
	for (const auto& value : data.springs)
	{
		if (value.nodeIndex < 0 || value.nodeIndex >= Count(model->GetNodes()))
			continue;
		SpringBone* spring = springPreviewOwner->AddComponent<SpringBone>(
			0, model.get(), value.nodeIndex, springColliders, value.stiffness, value.drag);
		spring->SetName(value.name);
		spring->SetUseUnscaledTime(true);
		spring->SetMaxAngle(value.maxAngleDegrees);
		spring->SetAngleLimits(value.minAnglesDegrees, value.maxAnglesDegrees);
		springPreviewComponents.push_back(spring);
	}
	springPreviewOwner->Awake();
}

bool VmdlEditorScene::UpdateSpringPreview()
{
	if (!animationPlaying || !model || model->GetVmdlExtensionData().springs.empty())
	{
		springPreviewOwner.reset();
		springPreviewComponents.clear();
		springPreviewSignature.clear();
		return false;
	}

	const auto& data = model->GetVmdlExtensionData();
	std::string signature;
	for (const auto& value : data.springs)
	{
		signature += std::format("{}|{},{},{}|{},{},{}|{},{},{}|{}|{}|", value.nodeIndex,
			value.transform.position.x, value.transform.position.y, value.transform.position.z,
			value.transform.rotation.x, value.transform.rotation.y, value.transform.rotation.z,
			value.transform.scale.x, value.transform.scale.y, value.transform.scale.z,
			value.stiffness, value.drag);
		signature += std::format("{}|{},{},{}|{},{},{}|", value.maxAngleDegrees,
			value.minAnglesDegrees.x, value.minAnglesDegrees.y, value.minAnglesDegrees.z,
			value.maxAnglesDegrees.x, value.maxAnglesDegrees.y, value.maxAnglesDegrees.z);
	}
	for (const auto& value : data.springColliders)
	{
		signature += std::format("{}|{},{},{}|{},{},{}|{}|", value.nodeIndex,
			value.transform.position.x, value.transform.position.y, value.transform.position.z,
			value.transform.scale.x, value.transform.scale.y, value.transform.scale.z, value.radius);
	}
	if (signature != springPreviewSignature)
	{
		springPreviewSignature = std::move(signature);
		RebuildSpringPreview();
	}
	if (!springPreviewOwner || springPreviewComponents.empty()) return false;

	const float timeDelta = animationTime - springPreviewAnimationTime;
	const float expectedAdvance =
		Game::Time::unscaledDeltaTime * std::max(std::abs(playbackSpeed), 1.0f);
	const bool timelineJumped = selectedAnimation != springPreviewAnimation ||
		timeDelta < -0.0001f || timeDelta > expectedAdvance * 2.0f + 0.01f;
	if (timelineJumped)
	{
		for (SpringBone* spring : springPreviewComponents) spring->Reset();
	}
	springPreviewAnimation = selectedAnimation;
	springPreviewAnimationTime = animationTime;
	springPreviewOwner->LateUpdate();
	return true;
}

void VmdlEditorScene::RebuildTrailPreview()
{
	trailPreviewOwner.reset();
	trailPreviewComponents.clear();
	trailPreviewAnimation = -1;
	trailPreviewAnimationTime = 0.0f;
	if (!model) return;

	const auto& trails = model->GetVmdlTrailData().trails;
	trailPreviewOwner = std::make_unique<Object>("VMDL Trail Preview");
	trailPreviewComponents.resize(trails.size(), nullptr);
	for (int i = 0; i < Count(trails); ++i)
	{
		const auto& value = trails[i];
		if (value.nodeIndex < 0 || value.nodeIndex >= Count(model->GetNodes()))
			continue;
		TrailRenderComponent* trail = trailPreviewOwner->AddComponent<TrailRenderComponent>(
			model.get(), value.nodeIndex, value.rootOffset, value.tipOffset, value.color,
			value.tipRatio, std::max(0.01f, value.lifeTime), std::max(2, value.maxPoints),
			value.offsetAngle, value.transform.ToMatrix());
		trail->SetUseUnscaledTime(true);
		trailPreviewComponents[i] = trail;
	}
	trailPreviewOwner->Awake();
}

void VmdlEditorScene::UpdateTrailPreview(const RenderContext& rc)
{
	if (!unifiedPreviewActive || !model || model->GetVmdlTrailData().trails.empty())
	{
		trailPreviewOwner.reset();
		trailPreviewComponents.clear();
		trailPreviewSignature.clear();
		return;
	}

	const auto& trails = model->GetVmdlTrailData().trails;
	std::string signature;
	for (const auto& value : trails)
	{
			signature += std::format(
			"{}|{},{},{}|{},{},{}|{},{},{},{}|{}|{}|{}|{},{},{}|{},{},{}|{},{},{}|{},{},{}|", value.nodeIndex,
			value.rootOffset.x, value.rootOffset.y, value.rootOffset.z, value.tipOffset.x,
			value.tipOffset.y, value.tipOffset.z, value.color.x, value.color.y, value.color.z,
			value.color.w, value.tipRatio, value.lifeTime, value.maxPoints, value.offsetAngle.x,
			value.offsetAngle.y, value.offsetAngle.z, value.transform.position.x,
			value.transform.position.y, value.transform.position.z, value.transform.rotation.x,
			value.transform.rotation.y, value.transform.rotation.z, value.transform.scale.x,
			value.transform.scale.y, value.transform.scale.z);
	}
	if (signature != trailPreviewSignature)
	{
		trailPreviewSignature = std::move(signature);
		RebuildTrailPreview();
	}
	if (!trailPreviewOwner) return;

	const bool timelineAdvanced = selectedAnimation == trailPreviewAnimation &&
		animationTime > trailPreviewAnimationTime + 0.0001f;
	const bool resetTrail = selectedAnimation != trailPreviewAnimation ||
		animationTime + 0.0001f < trailPreviewAnimationTime;
	if (resetTrail)
	{
		for (TrailRenderComponent* trail : trailPreviewComponents)
		{
			if (trail) trail->ResetTrail();
		}
	}
	trailPreviewAnimation = selectedAnimation;
	trailPreviewAnimationTime = animationTime;

	for (int i = 0; i < Count(trailPreviewComponents); ++i)
	{
		TrailRenderComponent* trail = trailPreviewComponents[i];
		if (!trail) continue;
		const bool eventActive =
			i < Count(previewTrailActive)
				? previewTrailActive[i] != 0
				: model->GetTrailInitialActive(i);
		if (showTrail && eventActive && (animationPlaying || timelineAdvanced)) trail->StartTrail();
		else trail->StopTrail();
	}
	trailPreviewOwner->LateUpdate();
	if (!showTrail) return;
	for (TrailRenderComponent* trail : trailPreviewComponents)
	{
		if (trail) trail->RenderTrail(rc);
	}
}

void VmdlEditorScene::RebuildParticlePreview()
{
	particlePreviewOwner.reset();
	particlePreviewComponents.clear();
	manualParticlePreviewIndex = -1;
	particlePreviewAnimation = -1;
	particlePreviewAnimationTime = 0.0f;
	if (!model) return;
	particlePreviewOwner = std::make_unique<Object>("VMDL Particle Preview");
	const auto& data = model->GetVmdlParticleData();
	particlePreviewComponents.assign(data.emitters.size(), nullptr);
	for (int i = 0; i < Count(data.emitters); ++i)
	{
		const auto& value = data.emitters[i];
		if (value.nodeIndex < 0 || value.nodeIndex >= Count(model->GetNodes()))
			continue;
		particlePreviewComponents[i] = particlePreviewOwner->AddComponent<VMDLParticleEmitterComponent>(
			model.get(), value, model->GetParticleInitialActive(i));
	}
	particlePreviewOwner->Awake();
	particlePreviewOwner->Start();
}

void VmdlEditorScene::UpdateParticlePreview(const RenderContext& rc)
{
	if (!unifiedPreviewActive || !showParticle || !model)
	{
		particlePreviewOwner.reset();
		particlePreviewComponents.clear();
		return;
	}
	if (!particlePreviewOwner || particlePreviewComponents.size() !=
		model->GetVmdlParticleData().emitters.size())
		RebuildParticlePreview();
	if (!particlePreviewOwner) return;
	previewParticleActive.resize(particlePreviewComponents.size(), 0);
	for (int i = 0; i < Count(particlePreviewComponents); ++i)
	{
		if (!particlePreviewComponents[i]) continue;
		const bool manualPreview = manualParticlePreviewIndex == i &&
			particlePreviewComponents[i]->IsPlaying();
		if (manualParticlePreviewIndex == i && !manualPreview)
			manualParticlePreviewIndex = -1;
		const bool eventActive = selectedAnimation >= 0
			? model->EvaluateParticleActive(selectedAnimation, animationTime, i)
			: model->GetParticleInitialActive(i);
		const bool active = animationPlaying && eventActive;
		previewParticleActive[i] = active ? 1 : 0;
		if (!manualPreview) particlePreviewComponents[i]->SetEmitting(active);
		if (particlePreviewPlayPending && active) particlePreviewComponents[i]->Play();
	}
	particlePreviewPlayPending = false;
	particlePreviewOwner->LateUpdate();
	for (VMDLParticleEmitterComponent* emitter : particlePreviewComponents)
		if (emitter) emitter->RenderParticles(rc);
	particlePreviewAnimation = selectedAnimation;
	particlePreviewAnimationTime = animationTime;
}

void VmdlEditorScene::DrawAnimationEventEditor()
{
	if (!model || selectedAnimation < 0 ||
		selectedAnimation >= Count(model->GetAnimations()))
		return;
	auto& animation = model->GetAnimations()[selectedAnimation];
	auto& colliders = model->GetVmdlExtensionData().colliders;
	auto& morphs = model->GetVmdlExtensionData().morphs;
	auto& controlData = model->GetVmdlAnimationControlData();
	const auto byTime = [](const auto& left, const auto& right) {
		return left.seconds < right.seconds;
	};

	ImGui::SeparatorText((const char*)u8"アニメーションイベント");

	// コライダーのON/OFFキー
	if (ImGui::TreeNodeEx((const char*)u8"コライダー有効状態", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (colliders.empty())
			ImGui::TextDisabled((const char*)u8"このVMDLにはコライダーが登録されていません");
		else
		{
			selectedColliderEventTarget =
				std::clamp(selectedColliderEventTarget, 0, Count(colliders) - 1);
			if (ImGui::BeginCombo((const char*)u8"コライダー",
					colliders[selectedColliderEventTarget].name.c_str()))
			{
				for (int i = 0; i < Count(colliders); ++i)
				{
					if (ImGui::Selectable(
							colliders[i].name.c_str(), selectedColliderEventTarget == i))
						selectedColliderEventTarget = i;
				}
				ImGui::EndCombo();
			}
			bool initialActive = model->GetColliderInitialActive(selectedColliderEventTarget);
			if (ImGui::Checkbox((const char*)u8"初期状態で有効", &initialActive))
			{
				model->SetColliderInitialActive(selectedColliderEventTarget, initialActive);
				MarkDirty();
				ApplyAnimationPreview();
			}
			if (ImGui::Button((const char*)u8"ONキーを追加"))
			{
				auto& track = model->GetOrCreateColliderAnimationTrack(
					animation.name, selectedColliderEventTarget);
				track.keys.push_back({animationTime, true});
				std::sort(track.keys.begin(), track.keys.end(), byTime);
				MarkDirty();
				ApplyAnimationPreview();
			}
			ImGui::SameLine();
			if (ImGui::Button((const char*)u8"OFFキーを追加"))
			{
				auto& track = model->GetOrCreateColliderAnimationTrack(
					animation.name, selectedColliderEventTarget);
				track.keys.push_back({animationTime, false});
				std::sort(track.keys.begin(), track.keys.end(), byTime);
				MarkDirty();
				ApplyAnimationPreview();
			}

			VMDLModel::VmdlColliderAnimationTrack* selectedTrack = nullptr;
			for (auto& track : controlData.colliderTracks)
			{
				if (track.animationName == animation.name &&
					track.colliderIndex == selectedColliderEventTarget)
				{
					selectedTrack = &track;
					break;
				}
			}
			if (selectedTrack)
			{
				int removeIndex = -1;
				bool sortKeys = false;
				bool valueChanged = false;
				for (int i = 0; i < Count(selectedTrack->keys); ++i)
				{
					auto& key = selectedTrack->keys[i];
					ImGui::PushID(i);
					ImGui::SetNextItemWidth(150.0f);
					if (ImGui::DragFloat("##Time", &key.seconds, 0.01f, 0.0f,
							animation.secondsLength, "%.3f sec"))
						sortKeys = true;
					ImGui::SameLine();
					if (ImGui::Checkbox((const char*)u8"有効", &key.value)) valueChanged = true;
					ImGui::SameLine();
					if (ImGui::SmallButton((const char*)u8"削除")) removeIndex = i;
					ImGui::PopID();
				}
				if (removeIndex >= 0)
				{
					selectedTrack->keys.erase(selectedTrack->keys.begin() + removeIndex);
					MarkDirty();
				}
				if (sortKeys)
				{
					std::sort(selectedTrack->keys.begin(), selectedTrack->keys.end(), byTime);
					MarkDirty();
				}
				if (valueChanged) MarkDirty();
				if (removeIndex >= 0 || sortKeys || valueChanged) ApplyAnimationPreview();
			}
		}
		ImGui::TreePop();
	}

	// パーティクルON/OFFキー
	auto& particleData = model->GetVmdlParticleData();
	if (ImGui::TreeNodeEx((const char*)u8"パーティクル発生", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (particleData.emitters.empty())
			ImGui::TextDisabled((const char*)u8"このVMDLにはパーティクルが登録されていません");
		else
		{
			selectedParticleEventTarget = std::clamp(selectedParticleEventTarget, 0,
				Count(particleData.emitters) - 1);
			if (ImGui::BeginCombo((const char*)u8"エミッタ",
					particleData.emitters[selectedParticleEventTarget].name.c_str()))
			{
				for (int i = 0; i < Count(particleData.emitters); ++i)
					if (ImGui::Selectable(particleData.emitters[i].name.c_str(),
						selectedParticleEventTarget == i)) selectedParticleEventTarget = i;
				ImGui::EndCombo();
			}
			bool initialActive = model->GetParticleInitialActive(selectedParticleEventTarget);
			if (ImGui::Checkbox((const char*)u8"初期状態で発生", &initialActive))
			{
				model->SetParticleInitialActive(selectedParticleEventTarget, initialActive);
				MarkDirty();
			}
			if (ImGui::Button((const char*)u8"ONキーを追加"))
			{
				auto& track = model->GetOrCreateParticleAnimationTrack(
					animation.name, selectedParticleEventTarget);
				track.keys.push_back({animationTime, true});
				std::sort(track.keys.begin(), track.keys.end(), byTime); MarkDirty();
			}
			ImGui::SameLine();
			if (ImGui::Button((const char*)u8"OFFキーを追加"))
			{
				auto& track = model->GetOrCreateParticleAnimationTrack(
					animation.name, selectedParticleEventTarget);
				track.keys.push_back({animationTime, false});
				std::sort(track.keys.begin(), track.keys.end(), byTime); MarkDirty();
			}
			VMDLModel::VmdlParticleAnimationTrack* selectedTrack = nullptr;
			for (auto& track : particleData.tracks)
				if (track.animationName == animation.name &&
					track.emitterIndex == selectedParticleEventTarget) { selectedTrack = &track; break; }
			if (selectedTrack)
			{
				int remove = -1; bool changed = false;
				for (int i = 0; i < Count(selectedTrack->keys); ++i)
				{
					auto& key = selectedTrack->keys[i]; ImGui::PushID(7000 + i);
					ImGui::SetNextItemWidth(150.0f);
					changed |= ImGui::DragFloat("##ParticleTime", &key.seconds, 0.01f,
						0.0f, animation.secondsLength, "%.3f sec");
					ImGui::SameLine(); changed |= ImGui::Checkbox((const char*)u8"発生", &key.value);
					ImGui::SameLine(); if (ImGui::SmallButton((const char*)u8"削除")) remove = i;
					ImGui::PopID();
				}
				if (remove >= 0) selectedTrack->keys.erase(selectedTrack->keys.begin() + remove);
				if (changed) std::sort(selectedTrack->keys.begin(), selectedTrack->keys.end(), byTime);
				if (changed || remove >= 0) MarkDirty();
			}
		}
		ImGui::TreePop();
	}

	// モーフ切り替えキー
	if (ImGui::TreeNodeEx((const char*)u8"モーフ適用", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (morphs.empty())
			ImGui::TextDisabled((const char*)u8"このVMDLにはモーフが登録されていません");
		else
		{
			selectedMorphEventTarget =
				std::clamp(selectedMorphEventTarget, 0, Count(morphs) - 1);
			if (ImGui::BeginCombo(
					(const char*)u8"モーフ", morphs[selectedMorphEventTarget].name.c_str()))
			{
				for (int i = 0; i < Count(morphs); ++i)
				{
					if (ImGui::Selectable(morphs[i].name.c_str(), selectedMorphEventTarget == i))
						selectedMorphEventTarget = i;
					const ImVec2 itemMin = ImGui::GetItemRectMin();
					const ImVec2 itemMax = ImGui::GetItemRectMax();
					if (selectedMorphEventTarget == i)
					{
						ImGui::GetWindowDrawList()->AddRect(
							itemMin, itemMax, ImGuiTheme::SelectedOutline, 2.0f, 0, 2.0f);
						ImGui::GetWindowDrawList()->AddRectFilled(itemMin,
							ImVec2(itemMin.x + 4.0f, itemMax.y), ImGuiTheme::SelectedAccent);
					}
				}
				ImGui::EndCombo();
			}
			if (ImGui::Button((const char*)u8"モーフキーを追加"))
			{
				auto& track = model->GetOrCreateMorphAnimationTrack(animation.name);
				track.keys.push_back({animationTime, selectedMorphEventTarget});
				std::sort(track.keys.begin(), track.keys.end(), byTime);
				MarkDirty();
				ApplyAnimationPreview();
			}

			VMDLModel::VmdlMorphAnimationTrack* selectedTrack = nullptr;
			for (auto& track : controlData.morphTracks)
			{
				if (track.animationName == animation.name)
				{
					selectedTrack = &track;
					break;
				}
			}
			if (selectedTrack)
			{
				int removeIndex = -1;
				bool changed = false;
				for (int i = 0; i < Count(selectedTrack->keys); ++i)
				{
					auto& key = selectedTrack->keys[i];
					ImGui::PushID(i);
					ImGui::SetNextItemWidth(150.0f);
					if (ImGui::DragFloat("##Time", &key.seconds, 0.01f, 0.0f,
							animation.secondsLength, "%.3f sec"))
						changed = true;
					ImGui::SameLine();
					const char* keyMorphName =
						key.morphIndex >= 0 && key.morphIndex < Count(morphs)
							? morphs[key.morphIndex].name.c_str()
							: "(missing)";
					ImGui::SetNextItemWidth(220.0f);
					if (ImGui::BeginCombo("##Morph", keyMorphName))
					{
						for (int morphIndex = 0; morphIndex < Count(morphs);
							++morphIndex)
						{
							if (ImGui::Selectable(
									morphs[morphIndex].name.c_str(), key.morphIndex == morphIndex))
							{
								key.morphIndex = morphIndex;
								changed = true;
							}
						}
						ImGui::EndCombo();
					}
					ImGui::SameLine();
					if (ImGui::SmallButton((const char*)u8"削除")) removeIndex = i;
					ImGui::PopID();
				}
				if (removeIndex >= 0)
					selectedTrack->keys.erase(selectedTrack->keys.begin() + removeIndex);
				if (changed)
					std::sort(selectedTrack->keys.begin(), selectedTrack->keys.end(), byTime);
				if (changed || removeIndex >= 0)
				{
					MarkDirty();
					ApplyAnimationPreview();
				}
			}
		}
		ImGui::TreePop();
	}

}

bool VmdlEditorScene::DrawIkVector3(const char* label, Vector3& value, float speed,
	int legIndex, IkGizmoTarget target)
{
	ImGui::PushID(label);
	ImGui::TextUnformatted(label);
	const float buttonWidth = ImGui::CalcTextSize("Pickup").x + ImGui::GetStyle().FramePadding.x * 2.0f;
	ImGui::SetNextItemWidth(std::min(210.0f, std::max(90.0f,
		ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemSpacing.x)));
	const bool changed = ImGui::DragFloat3("##Value", &value.x, speed);
	ImGui::SameLine();
	if (ImGui::Button("Pickup"))
	{
		ikGizmoLeg = legIndex;
		ikGizmoTarget = target;
		showSelectionGizmo = false;
		showDebugOverlays = true;
		showFootIkDebug = true;
		if (target == IkGizmoTarget::Pole) showIkPole = true;
	}
	ImGui::PopID();
	return changed;
}

void VmdlEditorScene::DrawIkSettings()
{
	if (!model)
	{
		ImGui::TextDisabled((const char*)u8"設定なし");
		return;
	}

	auto& settings = model->GetVmdlIKSettings();
	auto& poles = model->GetVmdlIKPoles();
	auto& raySettings = model->GetVmdlIKRaySettings();
	auto& interpolation = model->GetVmdlMultiLegIKSettings();
	const char* easingNames[] = {"None", "Linear", "InQuad", "OutQuad", "InOutQuad",
		"InCubic", "OutCubic", "InOutCubic", "InSine", "OutSine", "InOutSine"};
	int easingIndex = std::clamp(interpolation.interpolationEasing + 1, 0,
		static_cast<int>(Easing::Type::Count));
	if (ImGui::Combo((const char*)u8"補間イージング (Interpolation Easing)",
			&easingIndex, easingNames, static_cast<int>(Easing::Type::Count) + 1))
	{
		interpolation.interpolationEasing = easingIndex - 1;
		MarkDirty();
	}

	// IKの種類と自動割り当て
	const char* types[] = {
		(const char*)u8"なし (None)",
		(const char*)u8"人型足IK (Humanoid Foot IK)",
		(const char*)u8"四足IK (Quadruped IK)",
		(const char*)u8"昆虫IK (Insect IK)",
	};
	int selectedType = settings.type;
	if (ImGui::Combo((const char*)u8"IK種類 (IK Type)", &selectedType, types, IM_ARRAYSIZE(types)))
	{
		settings.type = selectedType;
		model->ResetVmdlIKLegsForType();
		MarkDirty();
	}
	if (settings.type == 0) return;
	if (raySettings.size() < settings.legs.size()) raySettings.resize(settings.legs.size());
	if (ImGui::Button((const char*)u8"名前から自動設定 (Auto Assign)"))
	{
		if (model->AutoAssignVmdlIKNodes()) MarkDirty();
	}

	const auto nodeCombo = [&](const char* label, std::string& name, bool allowNone = false) {
		bool changed = false;
		const int selectedNodeIndex = name.empty() ? -1 : model->GetNodeIndex(name.c_str());
		const std::string preview =
			selectedNodeIndex >= 0
				? MakeNodeLabel(selectedNodeIndex, model->GetNodes()[selectedNodeIndex].name)
			: name.empty() ? (const char*)u8"（なし）"
						   : name;
		if (ImGui::BeginCombo(label, preview.c_str()))
		{
			if (allowNone && ImGui::Selectable((const char*)u8"（なし）", name.empty()))
			{
				name.clear();
				changed = true;
			}
			const auto& nodes = model->GetNodes();
			for (int nodeIndex = 0; nodeIndex < Count(nodes); ++nodeIndex)
			{
				const auto& node = nodes[nodeIndex];
				const std::string nodeLabel = MakeNodeLabel(nodeIndex, node.name);
				if (ImGui::Selectable(nodeLabel.c_str(), nodeIndex == selectedNodeIndex))
				{
					name = nodeLabel;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		if (!name.empty() && model->GetNodeIndex(name.c_str()) < 0)
		{
			ImGui::SameLine();
			ImGui::TextUnformatted((const char*)u8"不足");
		}
		return changed;
	};

	// IK全体の基準ノード
	if (nodeCombo(settings.type == 1 ? (const char*)u8"骨盤 (Pelvis)"
									 : (const char*)u8"胴体中心 (Body Center)",
			settings.centerNode))
		MarkDirty();

	if (settings.type == 2 || settings.type == 3)
	{
		auto& solver = model->GetVmdlMultiLegIKSettings();
		ImGui::SeparatorText((const char*)u8"多脚IKの補正 (Multi-Leg IK Correction)");
		bool changed = ImGui::DragFloat(
			(const char*)u8"胴体の高さ補正 (Body Height Offset)",
			&solver.bodyHeightOffset, 0.005f, -5.0f, 5.0f);
		changed |= ImGui::DragFloat(
			(const char*)u8"接地オフセット (Contact Offset)",
			&solver.contactOffset, 0.001f, -5.0f, 5.0f);
		changed |= ImGui::DragFloat(
			(const char*)u8"上方の最大補正 (Max Up Correction)",
			&solver.maxUpCorrection, 0.01f, 0.0f, 20.0f);
		changed |= ImGui::DragFloat(
			(const char*)u8"下方の最大補正 (Max Down Correction)",
			&solver.maxDownCorrection, 0.01f, 0.0f, 20.0f);
		solver.maxUpCorrection = std::max(solver.maxUpCorrection, 0.0f);
		solver.maxDownCorrection = std::max(solver.maxDownCorrection, 0.0f);
		if (changed) MarkDirty();
	}

	// 各脚のボーン、ポール、レイ
	for (int i = 0; i < Count(settings.legs); ++i)
	{
		auto& leg = settings.legs[i];
		ImGui::PushID(i);
		const std::string header =
			leg.name.empty() ? (const char*)u8"脚 " + std::to_string(i + 1) : leg.name;
		if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			bool changed = ImGui::InputText((const char*)u8"名前 (Name)", &leg.name);
			changed |= nodeCombo((const char*)u8"付け根 (Root)", leg.root);
			changed |= nodeCombo((const char*)u8"中間 (Middle)", leg.mid);
			changed |= nodeCombo((const char*)u8"先端 (Foot / Tip)", leg.tip);
			changed |= nodeCombo((const char*)u8"接地点 (Contact)", leg.contact, true);

			auto& pole = poles[i];
			const bool wasCustom = pole.custom;
			if (ImGui::Checkbox((const char*)u8"カスタムポール (Custom Pole)", &pole.custom))
			{
				changed = true;
				if (!pole.custom) pole = VMDLModel::VmdlIKPole{};
				if (!wasCustom && pole.custom)
				{
					const int rootIndex = model->GetNodeIndex(leg.root.c_str());
					const int midIndex = model->GetNodeIndex(leg.mid.c_str());
					const int tipIndex = model->GetNodeIndex(leg.tip.c_str());
					if (rootIndex >= 0 && midIndex >= 0 && tipIndex >= 0)
					{
						const auto& nodes = model->GetNodes();
						const Vector3 rootPosition = nodes[rootIndex].globalTransform.Translation();
						const Vector3 midPosition = nodes[midIndex].globalTransform.Translation();
						const Vector3 tipPosition = nodes[tipIndex].globalTransform.Translation();
						Vector3 rootToTip = tipPosition - rootPosition;
						Vector3 poleDirection = Vector3::UnitZ;
						if (rootToTip.LengthSquared() > eps)
						{
							rootToTip.Normalize();
							const Vector3 projectedMid =
								rootPosition +
								rootToTip * (midPosition - rootPosition).Dot(rootToTip);
							poleDirection = midPosition - projectedMid;
							if (poleDirection.LengthSquared() <= eps)
								poleDirection = Vector3::UnitZ;
							else poleDirection.Normalize();
						}
						const float poleLift = settings.type == 1 ? 0.35f : 0.0f;
						pole.position = midPosition + poleDirection * 0.5f + Vector3::Up * poleLift;
					}
				}
			}
			if (pole.custom)
			{
				changed |= DrawIkVector3((const char*)u8"ポール位置 (Pole Position)",
					pole.position, 0.01f, i, IkGizmoTarget::Pole);
				ImGui::TextDisabled((const char*)u8"モデルのローカル座標");
			}

			auto& ray = raySettings[i];
			if (ImGui::Checkbox((const char*)u8"カスタムレイ (Custom Ray)", &ray.custom))
			{
				changed = true;
				if (!ray.custom)
				{
					ray = VMDLModel::VmdlIKRaySettings{};
					if (settings.type != 1)
					{
						ray.startOffset = Vector3(0.0f, 1.0f, 0.0f);
						ray.length = 6.0f;
					}
				}
			}
			if (ray.custom)
			{
				changed |= DrawIkVector3((const char*)u8"開始位置 (Start Offset)",
					ray.startOffset, 0.01f, i, IkGizmoTarget::RayStart);
				changed |= DrawIkVector3((const char*)u8"同期オフセット (Sync Offset)",
					ray.syncOffset, 0.001f, i, IkGizmoTarget::RaySync);
				ImGui::TextDisabled((const char*)u8"ボーンからレイをずらす量（モデルのローカル座標）");
				changed |= ImGui::DragFloat(
					(const char*)u8"レイの長さ (Ray Length)", &ray.length, 0.01f, 0.01f, 100.0f);
				ray.length = std::max(0.01f, ray.length);
			}
			if (changed) MarkDirty();
		}
		ImGui::PopID();
	}
}

void VmdlEditorScene::DrawMorphEditor()
{
	ImGui::TextDisabled((const char*)u8"+ は表示、- は非表示、中点は変更なしです");
	if (!model) return;

	auto& morphs = model->GetVmdlExtensionData().morphs;
	const bool hasSelectedMorph =
		selectedMorph >= 0 && selectedMorph < Count(morphs);

	// モーフの追加と操作
	if (ImGui::Button((const char*)u8"現在のモーフを登録"))
	{
		auto& morph = morphs.emplace_back();
		morph.name = MakeUniqueMorphName("MORPH " + std::to_string(morphs.size()));
		morph.meshVisibility.reserve(model->GetMeshes().size());
		for (const VMDLModel::Mesh& mesh : model->GetMeshes())
			morph.meshVisibility.push_back(mesh.isDraw ? 1 : 0);
		selectedMorph = Count(morphs) - 1;
		MarkDirty();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(!hasSelectedMorph);
	if (ImGui::Button((const char*)u8"現在の表示状態を反映") && hasSelectedMorph)
	{
		auto& visibility = morphs[selectedMorph].meshVisibility;
		visibility.resize(model->GetMeshes().size());
		for (size_t i = 0; i < visibility.size(); ++i)
			visibility[i] = model->GetMeshes()[i].isDraw ? 1 : 0;
		MarkDirty();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button((const char*)u8"モーフを適用") && hasSelectedMorph)
	{
		model->ApplyMorph(selectedMorph);
	}
	ImGui::SameLine();
	const bool canDeleteMorph = hasSelectedMorph;
	ImGui::BeginDisabled(!canDeleteMorph);
	if (ImGui::Button((const char*)u8"モーフを複製") && canDeleteMorph)
	{
		VMDLModel::VmdlMorph duplicate = morphs[selectedMorph];
		duplicate.name = MakeUniqueMorphName(duplicate.name + " COPY");
		duplicate.applyOnInitialize = false;
		morphs.push_back(std::move(duplicate));
		selectedMorph = Count(morphs) - 1;
		MarkDirty();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!canDeleteMorph);
	if (ImGui::Button((const char*)u8"モーフを削除") && canDeleteMorph)
	{
		morphs.erase(morphs.begin() + selectedMorph);
		for (auto& track : model->GetVmdlAnimationControlData().morphTracks)
		{
			std::erase_if(
				track.keys, [&](const auto& key) { return key.morphIndex == selectedMorph; });
			for (auto& key : track.keys)
			{
				if (key.morphIndex > selectedMorph) --key.morphIndex;
			}
		}
		if (morphs.empty()) selectedMorph = -1;
		else selectedMorph = std::min(selectedMorph, Count(morphs) - 1);
		MarkDirty();
	}
	ImGui::EndDisabled();

	constexpr ImGuiTableFlags morphTableFlags = ImGuiTableFlags_Resizable |
												ImGuiTableFlags_BordersInnerV |
												ImGuiTableFlags_SizingStretchProp;
	if (ImGui::BeginTable("Morph Editor Layout", 2, morphTableFlags, ImVec2(0.0f, 0.0f)))
	{
		ImGui::TableSetupColumn("Morph List Column", ImGuiTableColumnFlags_WidthStretch, 0.25f);
		ImGui::TableSetupColumn("Morph Property Column", ImGuiTableColumnFlags_WidthStretch, 0.75f);
		ImGui::TableNextRow();

		// 左側のモーフ一覧
		ImGui::TableSetColumnIndex(0);
		ImGui::BeginChild("Morph List", ImVec2(0.0f, 0.0f), true);
		for (int i = 0; i < Count(morphs); ++i)
		{
			ImGui::PushID(i);
			if (ImGui::Selectable(morphs[i].name.c_str(), selectedMorph == i)) selectedMorph = i;
			const ImVec2 itemMin = ImGui::GetItemRectMin();
			const ImVec2 itemMax = ImGui::GetItemRectMax();
			if (selectedMorph == i)
			{
				ImGui::GetWindowDrawList()->AddRect(
					itemMin, itemMax, ImGuiTheme::SelectedOutline, 2.0f, 0, 2.0f);
				ImGui::GetWindowDrawList()->AddRectFilled(
					itemMin, ImVec2(itemMin.x + 4.0f, itemMax.y), ImGuiTheme::SelectedAccent);
			}
			if (ImGui::BeginPopupContextItem("Morph Actions"))
			{
				selectedMorph = i;
				bool hasResidentMesh = false;
				for (int meshIndex = 0;
					meshIndex < Count(morphs[i].meshVisibility) &&
					meshIndex < Count(model->GetMeshes()); ++meshIndex)
				{
					if (morphs[i].meshVisibility[meshIndex] == 1 &&
						!model->IsExternalMesh(meshIndex))
					{
						hasResidentMesh = true;
						break;
					}
				}
				if (ImGui::MenuItem((const char*)u8"モーフに関連するメッシュを分離",
						nullptr, false, hasResidentMesh))
					SeparateMorphMeshes(i);
				if (!hasResidentMesh)
					ImGui::TextDisabled((const char*)u8"分離できる表示メッシュがありません");
				ImGui::EndPopup();
			}
			ImGui::PopID();
		}
		ImGui::EndChild();

		// 右側のモーフ設定
		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("Morph Property", ImVec2(0.0f, 0.0f), true);
		if (selectedMorph >= 0 && selectedMorph < Count(morphs))
		{
			auto& morph = morphs[selectedMorph];
			std::string name = morph.name;
			if (ImGui::InputText((const char*)u8"名前", &name))
			{
				const std::string newName = ToUpperString(name.c_str());
				const int duplicateIndex = model->GetMorphIndex(newName.c_str());
				if (!newName.empty() && (duplicateIndex < 0 || duplicateIndex == selectedMorph))
				{
					morph.name = newName;
					MarkDirty();
				}
			}
			if (ImGui::Checkbox((const char*)u8"初期状態", &morph.applyOnInitialize)) MarkDirty();
			if (morph.meshVisibility.size() != model->GetMeshes().size())
				morph.meshVisibility.resize(model->GetMeshes().size(), 2);
			ImGui::TextUnformatted((const char*)u8"+ 表示");
			ImGui::SameLine();
			ImGui::TextUnformatted((const char*)u8"- 非表示");
			ImGui::SameLine();
			ImGui::TextDisabled((const char*)u8"中点：変更なし");
			ImGui::Separator();
			for (int i = 0; i < Count(model->GetMeshes()); ++i)
			{
				uint8_t& state = morph.meshVisibility[i];
				if (state > 2) state = 2;
				const VMDLModel::Mesh& mesh = model->GetMeshes()[i];
				const std::string label =
					(const char*)u8"メッシュ " + std::to_string(i) + " : " + mesh.material->name;
				ImGui::PushID(i);
				bool changed = false;
				if (ImGui::RadioButton("+", state == 1))
				{
					state = 1;
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::RadioButton("-", state == 0))
				{
					state = 0;
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::RadioButton("\xC2\xB7", state == 2))
				{
					state = 2;
					changed = true;
				}
				ImGui::SameLine();
				ImGui::TextUnformatted(label.c_str());
				if (changed) MarkDirty();
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
		ImGui::EndTable();
	}
}

void VmdlEditorScene::DrawMaterialEditor()
{
	if (!model) return;

	auto& materials = model->GetMaterials();
	if (materials.empty())
	{
		ImGui::TextDisabled((const char*)u8"このモデルにはマテリアルがありません");
		return;
	}
	if (selectedMaterial < 0 || selectedMaterial >= Count(materials)) return;
	ImGui::SeparatorText((const char*)(ICON_FA_PALETTE " " u8"マテリアル"));
	auto& material = materials[selectedMaterial];
	ImGui::Text((const char*)u8"マテリアル: %s", material.name.c_str());
	bool changed = false;
	if (ImGui::Button((const char*)u8"GLB設定に初期化") &&
		model->ResetMaterialToGLB(static_cast<size_t>(selectedMaterial)))
	{
		changed = true;
	}

	// PBRパラメーター
	ImGui::SeparatorText("PBR");
	changed |= ImGui::ColorEdit4((const char*)u8"基本色", &material.baseColor.x);
	changed |= ImGui::ColorEdit4((const char*)u8"発光色", &material.emissiveColor.x,
		ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
	changed |= ImGui::SliderFloat((const char*)u8"メタリック", &material.metalness, 0.0f, 1.0f);
	changed |= ImGui::SliderFloat((const char*)u8"粗さ", &material.roughness, 0.0001f, 1.0f);
	changed |= ImGui::SliderFloat((const char*)u8"オクルージョン", &material.occlusion, 0.0f, 1.0f);
	changed |= ImGui::SliderFloat(
		(const char*)u8"オクルージョン強度", &material.occlusionStrength, 0.0f, 1.0f);
	changed |= ImGui::SliderFloat((const char*)u8"影の強度", &material.shadowStrength, 0.0f, 1.0f);

	ImGui::SeparatorText((const char*)u8"リフレクション");
	bool crystalEnabled = material.transmission > 0.0f;
	if (ImGui::Checkbox((const char*)u8"リフレクション", &crystalEnabled))
	{
		material.transmission = crystalEnabled ? 0.88f : 0.0f;
		changed = true;
	}
	if (crystalEnabled)
	{
		changed |= ImGui::SliderFloat((const char*)u8"透過率", &material.transmission, 0.001f, 1.0f);
		changed |= ImGui::SliderFloat((const char*)u8"屈折率", &material.indexOfRefraction, 1.0f, 2.5f);
		changed |= ImGui::DragFloat((const char*)u8"屈折の距離", &material.refractionDistance,
			0.005f, 0.0f, 5.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		changed |= ImGui::SliderFloat((const char*)u8"リフレクションの粗さ", &material.crystalRoughness, 0.0001f, 1.0f);
	}

	const char* alphaModes[] = {
		(const char*)u8"不透明", (const char*)u8"マスク", (const char*)u8"ブレンド"};
	int alphaMode = static_cast<int>(material.alphaMode);
	if (ImGui::Combo(
			(const char*)u8"アルファモード", &alphaMode, alphaModes, IM_ARRAYSIZE(alphaModes)))
	{
		material.alphaMode = static_cast<VMDLModel::AlphaMode>(alphaMode);
		changed = true;
	}
	if (material.alphaMode == VMDLModel::AlphaMode::Mask)
		changed |= ImGui::SliderFloat(
			(const char*)u8"アルファしきい値", &material.alphaCutoff, 0.0f, 1.0f);

	// テクスチャの書き出しと置換
	ImGui::SeparatorText((const char*)u8"テクスチャ");
	const auto textureRow = [&](const char* label, VMDLModel::MaterialTextureSlot slot,
								const std::string& filename, const std::vector<uint8_t>& embedded) {
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(label);
		ImGui::TableSetColumnIndex(1);

		if (filename.empty())
			ImGui::TextDisabled(
				embedded.empty() ? (const char*)u8"なし" : (const char*)u8"埋め込み");
		else
			ImGui::Text(
				"%s%s", filename.c_str(), embedded.empty() ? "" : (const char*)u8"（埋め込み）");

		ImGui::TableSetColumnIndex(2);
		ImGui::PushID(label);

		if (ImGui::SmallButton((const char*)u8"書き出し"))
		{
			std::string filepath;
			const char* filter = "PNG Image (*.png)\0"
								 "*.png\0"
								 "DDS Texture (*.dds)\0"
								 "*.dds\0"
								 "\0";
			if (Dialog::SaveFileName(filepath, filter, "Export Material Texture") ==
				DialogResult::OK)
			{
				if (model->ExportMaterialTexture(
						static_cast<size_t>(selectedMaterial), slot, filepath))
				{
					changed = true;
				}
				else
				{
					ErrorMessage("Failed to export the material texture.");
				}
			}
		}
		ImGui::SameLine();

		if (ImGui::SmallButton((const char*)u8"置換"))
		{
			std::string filepath;
			const char* filter = "PNG Image (*.png)\0"
								 "*.png\0"
								 "DDS Texture (*.dds)\0"
								 "*.dds\0"
								 "\0";
			if (Dialog::OpenFileName(filepath, filter, "Replace Material Texture") ==
				DialogResult::OK)
			{
				if (model->ReplaceMaterialTexture(
						static_cast<size_t>(selectedMaterial), slot, filepath))
				{
					changed = true;
				}
				else
				{
					ErrorMessage("Failed to replace the material texture.");
				}
			}
		}
		ImGui::SameLine();

		if (ImGui::SmallButton((const char*)u8"クリア") &&
			model->ClearMaterialTexture(static_cast<size_t>(selectedMaterial), slot))
		{
			changed = true;
		}
		ImGui::PopID();
	};
	if (ImGui::BeginTable("Material Textures", 3,
			ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn((const char*)u8"種類", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn((const char*)u8"参照元", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn((const char*)u8"操作", ImGuiTableColumnFlags_WidthStretch);
		textureRow("Base Color", VMDLModel::MaterialTextureSlot::BaseColor,
			material.baseTextureFileName, material.baseTextureDDS);
		textureRow("Normal", VMDLModel::MaterialTextureSlot::Normal, material.normalTextureFileName,
			material.normalTextureDDS);
		textureRow("Metalness / Roughness", VMDLModel::MaterialTextureSlot::MetalnessRoughness,
			material.metalnessRoughnessTextureFileName, material.metalnessRoughnessTextureDDS);
		textureRow("Occlusion", VMDLModel::MaterialTextureSlot::Occlusion,
			material.occlusionTextureFileName, material.occlusionTextureDDS);
		textureRow("Emissive", VMDLModel::MaterialTextureSlot::Emissive,
			material.emissiveTextureFileName, material.emissiveTextureDDS);
		ImGui::EndTable();
	}
	if (changed)
	{
		for (auto& [groupIndex, cache] : externalMeshPreviewCaches)
			if (cache) cache->SyncMaterialsFrom(*model);
		MarkDirty();
		if (animationRecording) model->RecordMaterialKey(selectedAnimation, selectedMaterial, animationTime);
	}
	DrawMaterialAnimationKeys();
}

void VmdlEditorScene::DrawMaterialAnimationKeys()
{
	if (selectedAnimation < 0 || selectedAnimation >= Count(model->GetAnimations()))
	{
		ImGui::TextDisabled((const char*)u8"アニメーションを選択するとキー登録できます");
		return;
	}
	const auto& animation = model->GetAnimations()[selectedAnimation];
	ImGui::SeparatorText((const char*)(ICON_FA_KEY " " u8"マテリアルキー"));
	ImGui::Text("%s / %.3f sec", animation.name.c_str(), animationTime);
	if (ImGui::Button((const char*)u8"現在のマテリアルをキー登録"))
	{
		model->RecordMaterialKey(selectedAnimation, selectedMaterial, animationTime);
		MarkDirty();
	}
	ImGui::TextDisabled((const char*)u8"基本色・発光色・メタリック・粗さを補間します");
	for (auto& track : model->GetMaterialAnimationTracks())
	{
		if (track.animationName != animation.name || track.materialIndex != selectedMaterial) continue;
		bool changed = false;
		int remove = -1;
		for (int i = 0; i < Count(track.keys); ++i)
		{
			auto& key = track.keys[i];
			ImGui::PushID(i);
			if (ImGui::TreeNode(std::format("{:.3f} sec###MaterialKey", key.seconds).c_str()))
			{
				changed |= ImGui::DragFloat((const char*)u8"時間", &key.seconds, 0.01f,
					0.0f, animation.secondsLength, "%.3f", ImGuiSliderFlags_AlwaysClamp);
				changed |= ImGui::ColorEdit4((const char*)u8"基本色", &key.baseColor.x);
				changed |= ImGui::ColorEdit4((const char*)u8"発光色", &key.emissiveColor.x,
					ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
				changed |= ImGui::SliderFloat((const char*)u8"メタリック", &key.metalness, 0.0f, 1.0f);
				changed |= ImGui::SliderFloat((const char*)u8"粗さ", &key.roughness, 0.0001f, 1.0f);
				if (ImGui::SmallButton((const char*)u8"このキーへ移動"))
				{
					animationPlaying = false;
					animationTime = key.seconds;
					ApplyAnimationPreview();
				}
				ImGui::SameLine();
				if (ImGui::SmallButton((const char*)u8"削除")) remove = i;
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if (remove >= 0) track.keys.erase(track.keys.begin() + remove);
		if (changed || remove >= 0)
		{
			std::stable_sort(track.keys.begin(), track.keys.end(),
				[](const auto& left, const auto& right) { return left.seconds < right.seconds; });
			MarkDirty();
		}
		break;
	}
}

void VmdlEditorScene::RecordSelectedNodeKey()
{
	if (!model || selectedAnimation < 0 || selectedNode < 0) return;
	auto& animation = model->GetAnimations()[selectedAnimation];
	if (animation.nodeAnims.size() < model->GetNodes().size())
		animation.nodeAnims.resize(model->GetNodes().size());
	const VMDLModel::Node& node = model->GetNodes()[selectedNode];
	auto& nodeAnimation = animation.nodeAnims[selectedNode];
	const auto upsert = [&](auto& keyframes, const auto& value) {
		for (auto& key : keyframes)
		{
			if (std::abs(key.seconds - animationTime) > 0.0005f) continue;
			key.value = value;
			return;
		}
		keyframes.push_back({animationTime, value});
	};
	upsert(nodeAnimation.positionKeyframes, node.position);
	upsert(nodeAnimation.rotationKeyframes, node.rotation);
	upsert(nodeAnimation.scaleKeyframes, node.scale);
	auto byTime = [](const auto& left, const auto& right) { return left.seconds < right.seconds; };
	std::sort(
		nodeAnimation.positionKeyframes.begin(), nodeAnimation.positionKeyframes.end(), byTime);
	std::sort(
		nodeAnimation.rotationKeyframes.begin(), nodeAnimation.rotationKeyframes.end(), byTime);
	std::sort(nodeAnimation.scaleKeyframes.begin(), nodeAnimation.scaleKeyframes.end(), byTime);
	MarkDirty();
}

void VmdlEditorScene::MarkDirty()
{
	dirty = true;
}

bool VmdlEditorScene::OnRequestExit()
{
	if (!dirty) return true;
	const int result = MessageBoxW(Game::Graphics::Instance().GetWindowHandle(),
		L"変更を保存しますか？", L"VEER", MB_YESNOCANCEL | MB_ICONQUESTION);
	if (result == IDNO) return true;
	if (result != IDYES) return false;
	SaveVmdl();
	return !dirty;
}

void VmdlEditorScene::UpdateModelFraming()
{
	if (!model) return;

	model->UpdateTransform(Matrix::Identity);
	const Matrix renderScaleTransform = model->GetRenderScaleTransform();

	Vector3 minPosition(FLT_MAX, FLT_MAX, FLT_MAX);
	Vector3 maxPosition(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	bool hasVertex = false;

	for (const VMDLModel::Mesh& mesh : model->GetMeshes())
	{
		if (!mesh.node) continue;

		for (const VMDLModel::Vertex& vertex : mesh.vertices)
		{
			const Vector3 position = Vector3::Transform(
				vertex.position, mesh.node->globalTransform * renderScaleTransform);

			minPosition.x = std::min(minPosition.x, position.x);
			minPosition.y = std::min(minPosition.y, position.y);
			minPosition.z = std::min(minPosition.z, position.z);

			maxPosition.x = std::max(maxPosition.x, position.x);
			maxPosition.y = std::max(maxPosition.y, position.y);
			maxPosition.z = std::max(maxPosition.z, position.z);

			hasVertex = true;
		}
	}

	if (!hasVertex) return;

	const Vector3 center = (minPosition + maxPosition) * 0.5f;
	const Vector3 size = maxPosition - minPosition;

	cameraFocusOffset = center - Vector3(0.0f, 1.0f, 0.0f);
	cameraOrbitPivot = center;
	previewCameraViewInitialized = false;

	cameraDistance = std::clamp(size.Length() * 1.25f, 2.0f, 50.0f);
	targetCameraDistance = cameraDistance;
}

std::string VmdlEditorScene::MakeUniqueMorphName(const std::string& baseName) const
{
	const std::string base = ToUpperString(baseName);
	if (model->GetMorphIndex(base.c_str()) < 0) return base;
	for (int suffix = 2;; ++suffix)
	{
		const std::string candidate = base + " " + std::to_string(suffix);
		if (model->GetMorphIndex(candidate.c_str()) < 0) return candidate;
	}
}

std::string VmdlEditorScene::MakeNodeLabel(int nodeIndex, const std::string& nodeName)
{
	return std::to_string(nodeIndex) + ":" + nodeName;
}

void VmdlEditorScene::ResetLightingEnvironment()
{
	auto& graphics = Game::Graphics::Instance();
	if (!graphics.LoadSkyMap("Default"))
	{
		ErrorMessage("Default lighting environment could not be loaded.");
		return;
	}
	graphics.GetSkyBoxRenderer()->SetIntensity(1.0f);

	const LightManager defaults;
	editorLights.SetAmbientColor(defaults.GetAmbientColor());
	editorLights.ClearLights();
	editorLights.SetDirectionalLight(defaults.GetDirectionalLight());
	editorLightDirection = {0.6f, -0.7f, 0.0f};
	editorLights.GetDirectionalLight().transform.SetAngle(editorLightDirection);
	usesCustomLighting = false;
}

void VmdlEditorScene::ApplyVstgLighting()
{
	const std::string initialDirectory = (ResourceManager::FindSourceResourceRoot() / "Stage").string();
	std::string filepath;
	if (Dialog::OpenFileName(filepath, "VSTG (*.vstg)\0*.vstg\0\0",
			"Apply VSTG lighting environment", initialDirectory.c_str()) != DialogResult::OK) return;
	try
	{
		VSTG stage;
		if (!stage.Load(filepath) || !stage.ApplyLighting(editorLights))
		{
			ErrorMessage(stage.GetError());
			return;
		}
		usesCustomLighting = true;
	}
	catch (const std::exception& exception)
	{
		ErrorMessage(std::string("VSTG lighting import failed: ") + exception.what());
	}
}

void VmdlEditorScene::OpenVmdl()
{
	const std::string initialDirectory = (ResourceManager::FindSourceResourceRoot() / "Model").string();
	std::string filepath;
	if (Dialog::OpenFileName(filepath, "VMDL (*.vmdl)\0*.vmdl\0", "Open VMDL",
			initialDirectory.c_str()) != DialogResult::OK)
		return;
	LoadModel(filepath);
}

void VmdlEditorScene::ImportGlb()
{
	const std::string initialDirectory = (ResourceManager::FindSourceResourceRoot() / "Model").string();
	std::string filepath;
	if (Dialog::OpenFileName(filepath, "glTF Binary (*.glb)\0*.glb\0", "Import GLB",
			initialDirectory.c_str()) != DialogResult::OK)
		return;

	std::filesystem::path proposedPath = filepath;
	proposedPath.replace_extension(".vmdl");
	std::string destination = proposedPath.string();
	if (Dialog::SaveFileName(destination, "VMDL (*.vmdl)\0*.vmdl\0",
			(const char*)u8"VMDLの保存先", "vmdl") != DialogResult::OK)
		return;

	LoadModel(filepath, destination);
}

void VmdlEditorScene::ExportGlb()
{
	if (!model) return;
	std::filesystem::path proposedPath = documentPath;
	if (proposedPath.empty()) proposedPath = "model.vmdl";
	proposedPath.replace_extension(".glb");
	std::string filepath = proposedPath.string();
	if (Dialog::SaveFileName(filepath, "glTF Binary (*.glb)\0*.glb\0", "Export GLB", "glb") != DialogResult::OK)
		return;
	std::filesystem::path destination = filepath;
	destination.replace_extension(".glb");
	std::string error;
	if (!model->ExportGlb(destination, &error)) ErrorMessage("GLB export failed: " + error);
}

void VmdlEditorScene::AppendAnimationGlb()
{
	if (!model) return;

	const std::string initialDirectory = (ResourceManager::FindSourceResourceRoot() / "Model").string();
	std::string filepath;
	if (Dialog::OpenFileName(filepath, "glTF Binary (*.glb)\0*.glb\0",
			"Append Animation GLB", initialDirectory.c_str()) != DialogResult::OK)
	{
		return;
	}

	try
	{
		const int firstAppendedIndex = Count(model->GetAnimations());
		model->AppendAnimations(filepath.c_str());
		if (firstAppendedIndex >= Count(model->GetAnimations()))
		{
			ErrorMessage("The selected GLB contains no animations.");
			return;
		}

		selectedAnimation = firstAppendedIndex;
		animationTime = 0.0f;
		animationPlaying = false;
		selectedKeyTrack = -1;
		selectedKeyIndex = -1;
		ResetAnimationControlPreview();
		ApplyAnimationPreview();
		MarkDirty();
	}
	catch (const std::exception& exception)
	{
		ErrorMessage(std::string("Animation append failed: ") + exception.what());
	}
}

void VmdlEditorScene::ReplaceGlbCache()
{
	// GLB部分のみ交換
	if (!model) return;

	const std::string initialDirectory = (ResourceManager::FindSourceResourceRoot() / "Model").string();
	std::string filepath;
	if (Dialog::OpenFileName(filepath, "glTF Binary (*.glb)\0*.glb\0",
			"Replace GLB Cache", initialDirectory.c_str()) != DialogResult::OK)
		return;

	footIkPreviewOwner.reset();
	footIkPreviewAnimator = nullptr;
	footIkPreviewSignature.clear();
	springPreviewOwner.reset();
	springPreviewComponents.clear();
	springPreviewSignature.clear();
	trailPreviewOwner.reset();
	trailPreviewComponents.clear();
	trailPreviewSignature.clear();
	particlePreviewOwner.reset();
	particlePreviewComponents.clear();
	editorPointLights.clear();
	editorLights.ClearAttachedPointLights();
	externalMeshPreviewCaches.clear();
	try
	{
		std::string replaceError;
		if (!model->ReplaceGLBCache(filepath, 60.0f, &replaceError))
		{
			ErrorMessage(replaceError.empty()
				? "Failed to replace the GLB cache."
				: "Failed to replace the GLB cache: " + replaceError);
			return;
		}
		previewStageSelected = false;
		selectedNode = model->GetNodes().empty() ? -1 : 0;
		ikGizmoTarget = IkGizmoTarget::None;
		ikGizmoLeg = -1;
		showSelectionGizmo = true;
		selectedNodes.clear();
		if (selectedNode >= 0) selectedNodes.push_back(selectedNode);
		selectedMesh = -1;
		selectedMeshes.clear();
		selectedComponentType = AttachedComponentType::None;
		selectedComponentIndex = -1;
		selectedComponentIndices.clear();
		focusSelectedComponent = false;
		selectedMaterial = model->GetMaterials().empty() ? -1 : 0;
		selectedAnimation = model->GetAnimations().empty() ? -1 : 0;
		selectedMorph = model->GetVmdlExtensionData().morphs.empty() ? -1 : 0;
		for (int i = 0; i < Count(model->GetVmdlExtensionData().morphs); ++i)
		{
			if (!model->GetVmdlExtensionData().morphs[i].applyOnInitialize) continue;
			selectedMorph = i;
			break;
		}
		animationTime = 0.0f;
		ResetAnimationControlPreview();
		model->ApplyInitialMorphs();
		ApplyAnimationPreview();
		UpdateModelFraming();
		MarkDirty();
	}
	catch (const std::exception& exception)
	{
		ErrorMessage(std::string("GLB cache replacement failed: ") + exception.what());
	}
}

std::filesystem::path VmdlEditorScene::MakeMeshCachePath(
	int meshIndex, const std::string& morphName) const
{
	if (!model || meshIndex < 0 ||
		meshIndex >= Count(model->GetMeshes())) return {};
	const auto& mesh = model->GetMeshes()[meshIndex];
	std::string meshLabel;
	if (mesh.materialIndex >= 0 &&
		mesh.materialIndex < Count(model->GetMaterials()))
		meshLabel = model->GetMaterials()[mesh.materialIndex].name;
	if (meshLabel.empty() && mesh.nodeIndex >= 0 &&
		mesh.nodeIndex < Count(model->GetNodes()))
		meshLabel = model->GetNodes()[mesh.nodeIndex].name;

	std::string suffix;
	if (!morphName.empty()) suffix += "_morph_" + MakeVmshFileLabel(morphName);
	suffix += "_mesh_" + std::to_string(meshIndex) + "_" +
		MakeVmshFileLabel(meshLabel) + ".vmsh";
	if (!documentPath.empty())
		return documentPath.parent_path() / (documentPath.stem().string() + suffix);
	return ResourceManager::FindSourceResourceRoot() / "Model" / ("model" + suffix);
}

// 右クリックした1メッシュをVMSHへ分離する
void VmdlEditorScene::SeparateMeshToCache(int meshIndex)
{
	if (!model || meshIndex < 0 ||
		meshIndex >= Count(model->GetMeshes())) return;
	const std::vector<int> meshIndices = {meshIndex};
	if (selectedMorph < 0 ||
		selectedMorph >= Count(model->GetVmdlExtensionData().morphs))
	{
		ErrorMessage("Select the activation morph before externalizing meshes.");
		return;
	}
	const std::wstring message =
		L"選択メッシュの頂点実体をVMSHへ分離します。\n"
		L"現在選択中のモーフで表示されたときだけ遅延読み込みされます。\n"
		L"VMDL本体への変更は、VMDLを保存するまで確定しません。\n\n"
		L"続行しますか？";
	if (MessageBoxW(Game::Graphics::Instance().GetWindowHandle(), message.c_str(),
			L"VEER", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
		return;

	const std::filesystem::path proposedPath = MakeMeshCachePath(meshIndex);
	std::string filepath = proposedPath.string();
	if (Dialog::SaveFileName(filepath, "VMDL Mesh Cache (*.vmsh)\0*.vmsh\0",
			(const char*)u8"VMSHの分離先", "vmsh") != DialogResult::OK)
		return;
	for (const auto& group : model->GetExternalMeshGroups())
	{
		const std::filesystem::path existingPath =
			ResourceManager::ResolveSourcePath(group.path);
		if (!IsSameFilePath(existingPath, filepath)) continue;
		ErrorMessage((const char*)u8"このVMSHは別の分離メッシュが使用しています。別のファイル名を指定してください。");
		return;
	}

	std::string error;
	if (!MeshCache::Save(filepath, *model, meshIndices, &error))
	{
		ErrorMessage("Failed to save the outfit mesh cache: " + error);
		return;
	}

	if (!ResourceManager::Instance().RefreshResources(filepath))
		ErrorMessage("Mesh saved, but runtime cache refresh failed.");
	const std::string portablePath = PortableResourcePath(filepath);
	if (!model->ExternalizeMeshes(portablePath, meshIndices, selectedMorph))
	{
		ErrorMessage("The VMSH was saved, but the meshes could not be externalized.");
		return;
	}

	externalMeshPreviewCaches.clear();
	UpdateModelFraming();
	MarkDirty();
}

void VmdlEditorScene::SeparateMorphMeshes(int morphIndex)
{
	if (!model || morphIndex < 0 ||
		morphIndex >= Count(model->GetVmdlExtensionData().morphs)) return;
	const auto& morph = model->GetVmdlExtensionData().morphs[morphIndex];
	std::vector<int> meshIndices;
	for (int meshIndex = 0;
		meshIndex < Count(morph.meshVisibility) && meshIndex < Count(model->GetMeshes());
		++meshIndex)
	{
		// 「+ 表示」のメッシュが、このモーフを適用したときに必要となる実体
		if (morph.meshVisibility[meshIndex] == 1 &&
			!model->IsExternalMesh(meshIndex))
			meshIndices.push_back(meshIndex);
	}
	if (meshIndices.empty()) return;

	const std::wstring message =
		L"モーフ「" + Utf8ToWide(morph.name) + L"」で表示する" +
		std::to_wstring(meshIndices.size()) +
		L"個のメッシュを、それぞれ個別のVMSHへ分離します。\n"
		L"既存のVMSHファイルは上書きしません。\n\n続行しますか？";
	if (MessageBoxW(Game::Graphics::Instance().GetWindowHandle(), message.c_str(),
			L"VEER", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
		return;

	int separatedCount = 0;
	std::string failures;
	for (int meshIndex : meshIndices)
	{
		std::filesystem::path filepath = MakeMeshCachePath(meshIndex, morph.name);
		const std::filesystem::path originalPath = filepath;
		for (int suffix = 2; std::filesystem::exists(filepath); ++suffix)
			filepath = originalPath.parent_path() /
				(originalPath.stem().string() + "_" + std::to_string(suffix) + ".vmsh");

		std::string error;
		if (!MeshCache::Save(filepath, *model, {meshIndex}, &error))
		{
			failures += "mesh " + std::to_string(meshIndex) + ": " + error + "\n";
			continue;
		}
		if (!ResourceManager::Instance().RefreshResources(filepath))
		{
			failures += "mesh " + std::to_string(meshIndex) + ": cache refresh failed\n";
		}
		if (!model->ExternalizeMeshes(
				PortableResourcePath(filepath), {meshIndex}, morphIndex))
		{
			failures += "mesh " + std::to_string(meshIndex) + ": externalize failed\n";
			std::error_code removeError;
			std::filesystem::remove(filepath, removeError);
			continue;
		}
		++separatedCount;
	}

	if (separatedCount > 0)
	{
		externalMeshPreviewCaches.clear();
		UpdateModelFraming();
		MarkDirty();
	}
	if (!failures.empty())
		ErrorMessage("Some morph meshes could not be separated:\n" + failures);
}

void VmdlEditorScene::RestoreExternalMesh(int meshIndex)
{
	if (!model) return;
	const auto* group = model->GetExternalMeshGroupForMesh(meshIndex);
	if (!group) return;
	const std::wstring message =
		L"選択したメッシュをVMDL本体へ結合します\n"
		L"ほかのメッシュが使用していなければ、結合内容をVMDLへ保存して\n"
		L"VMSHファイルも削除します\n\n"
		L"続行しますか？";
	if (MessageBoxW(Game::Graphics::Instance().GetWindowHandle(), message.c_str(),
			L"VEER", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
		return;

	std::filesystem::path resolved;
	if (!ResolveExternalMeshPath(meshIndex, resolved)) return;
	// 代替VMSHが選ばれた場合も含め、結合後の参照確認に使うパスを保持する
	group = model->GetExternalMeshGroupForMesh(meshIndex);
	if (!group) return;
	const std::string linkedPath = group->path;
	std::string error;
	if (!model->RestoreExternalMeshes(meshIndex, resolved, &error))
	{
		ErrorMessage(error.empty() ? "Failed to restore the VMSH meshes." : error);
		return;
	}
	externalMeshPreviewCaches.clear();
	UpdateModelFraming();
	MarkDirty();

	bool stillReferenced = false;
	for (const auto& remainingGroup : model->GetExternalMeshGroups())
	{
		if (remainingGroup.path != linkedPath &&
			!IsSameFilePath(ResourceManager::ResolveSourcePath(remainingGroup.path), resolved))
			continue;
		stillReferenced = true;
		break;
	}
	if (!stillReferenced)
	{
		// VMDLが古い外部参照を保持したままVMSHだけ消える状態を防ぐ
		if (documentPath.empty())
		{
			MessageBoxW(Game::Graphics::Instance().GetWindowHandle(),
				L"結合は完了しましたが、VMDLの保存先が未設定のためVMSHは削除しませんでした。",
				L"VEER", MB_OK | MB_ICONWARNING);
			return;
		}
		documentPath = ResourceManager::ResolveSourcePath(documentPath);
		if (!model->SaveVmdl(documentPath) ||
			!ResourceManager::Instance().RefreshResources(documentPath))
		{
			MessageBoxW(Game::Graphics::Instance().GetWindowHandle(),
				L"VMDLの保存に失敗したため、復旧用のVMSHは削除しませんでした。",
				L"VEER", MB_OK | MB_ICONWARNING);
			return;
		}
		dirty = false;

		std::error_code removeError;
		const bool removed = std::filesystem::remove(resolved, removeError);
		if (removeError || (!removed && std::filesystem::exists(resolved)))
		{
			const std::wstring warning =
				L"VMDLへの結合は完了しましたが、VMSHファイルを削除できませんでした。\n\n" +
				resolved.wstring();
			MessageBoxW(Game::Graphics::Instance().GetWindowHandle(), warning.c_str(),
				L"VEER", MB_OK | MB_ICONWARNING);
		}
		else
		{
			ResourceManager::Instance().RefreshResources();
		}
	}
}

bool VmdlEditorScene::ResolveExternalMeshPath(
	int meshIndex, std::filesystem::path& resolvedPath)
{
	if (!model) return false;
	const auto* group = model->GetExternalMeshGroupForMesh(meshIndex);
	if (!group) return false;
	// エディターでは実行用キャッシュではなくResources側の正本が移動していないかを確認する
	resolvedPath = ResourceManager::ResolveSourcePath(group->path);
	if (std::filesystem::is_regular_file(resolvedPath)) return true;

	const std::wstring message =
		L"VMSHリソースが見つかりません。\n\n" +
		Utf8ToWide(group->path) +
		L"\n\n代わりとなるVMSHを選択してください";
	MessageBoxW(Game::Graphics::Instance().GetWindowHandle(), message.c_str(),
		L"VMSH リソースの復旧", MB_OK | MB_ICONWARNING);

	const std::filesystem::path modelRoot = ResourceManager::FindSourceResourceRoot() / "Model";
	std::string replacementFile;
	if (Dialog::OpenFileName(replacementFile, "VMSH (*.vmsh)\0*.vmsh\0\0",
			(const char*)u8"代わりとなるVMSHを選択", modelRoot.string().c_str()) !=
		DialogResult::OK)
		return false;
	if (!std::filesystem::is_regular_file(replacementFile))
	{
		ErrorMessage("The selected VMSH file does not exist.");
		return false;
	}

	const std::string portablePath = PortableResourcePath(replacementFile);
	if (!model->SetExternalMeshPath(meshIndex, portablePath)) return false;
	ResourceManager::Instance().RefreshResources(replacementFile);
	resolvedPath = replacementFile;
	externalMeshPreviewCaches.clear();
	MarkDirty();
	return true;
}

void VmdlEditorScene::ResolveMissingExternalMeshes()
{
	if (!model) return;
	std::vector<int> representatives;
	for (const auto& group : model->GetExternalMeshGroups())
		if (!group.meshIndices.empty()) representatives.push_back(group.meshIndices.front());
	for (int meshIndex : representatives)
	{
		std::filesystem::path resolved;
		ResolveExternalMeshPath(meshIndex, resolved);
	}
}

void VmdlEditorScene::SaveVmdl()
{
	if (!model) return;
	if (documentPath.empty())
	{
		SaveVmdlAs();
		return;
	}
	documentPath = ResourceManager::ResolveSourcePath(documentPath);
	if (model->SaveVmdl(documentPath))
	{
		if (!ResourceManager::Instance().RefreshResources(documentPath))
		{
			ErrorMessage("VMDL saved, but runtime cache refresh failed. Save again to retry.");
			return;
		}
		dirty = false;
	}
	else
	{
		ErrorMessage("Failed to save the VMDL file.");
	}
}

void VmdlEditorScene::SaveVmdlAs()
{
	if (!model) return;
	std::string filepath = documentPath.string();
	if (Dialog::SaveFileName(filepath, "VMDL (*.vmdl)\0*.vmdl\0", "Save VMDL", "vmdl") !=
		DialogResult::OK)
		return;
	documentPath = filepath;
	documentPath = ResourceManager::ResolveSourcePath(documentPath);
	if (model->SaveVmdl(documentPath))
	{
		if (!ResourceManager::Instance().RefreshResources(documentPath))
		{
			ErrorMessage("VMDL saved, but runtime cache refresh failed. Save again to retry.");
			return;
		}
		dirty = false;
	}
	else
	{
		ErrorMessage("Failed to save the VMDL file.");
	}
}

// ファイルの存在を確認してからモデルを読み込み、読み込み成功時に履歴を更新
void VmdlEditorScene::LoadModel(
	const std::filesystem::path& requestedPath, const std::filesystem::path& requestedDestination)
{
	const auto filepath = ResourceManager::ResolveSourcePath(ResourcePath::Path(ResourceManager::Instance().ResolveResourcePath(ResourcePath::Text(requestedPath), ResourceManager::ResourceType::Model)));
	const auto importDestination = ResourceManager::ResolveSourcePath(requestedDestination);
	if (!std::filesystem::is_regular_file(filepath))
	{
		ErrorMessage("Model file not found. Open an existing file from Resources/Model.");
		return;
	}
	if (model && !OnRequestExit()) return;
	footIkPreviewOwner.reset();
	footIkPreviewAnimator = nullptr;
	footIkPreviewSignature.clear();
	springPreviewOwner.reset();
	springPreviewComponents.clear();
	springPreviewSignature.clear();
	trailPreviewOwner.reset();
	trailPreviewComponents.clear();
	trailPreviewSignature.clear();
	particlePreviewOwner.reset();
	particlePreviewComponents.clear();
	editorPointLights.clear();
	editorLights.ClearAttachedPointLights();
	externalMeshPreviewCaches.clear();
	try
	{
		if (!importDestination.empty())
		{
			model = std::make_shared<VMDLModel>(
				filepath.string().c_str(), 60.0f, importDestination.string().c_str());
			documentPath = importDestination;
			if (!ResourceManager::Instance().RefreshResources(documentPath))
				ErrorMessage("VMDL imported, but runtime cache refresh failed.");
			dirty = false;
		}
		else
		{
			documentPath = filepath;
			model = std::make_shared<VMDLModel>(filepath.string().c_str());
		}
		recentModelPath = documentPath;
		dirty = false;
		ikGizmoTarget = IkGizmoTarget::None;
		ikGizmoLeg = -1;
		showSelectionGizmo = true;
		previewStageSelected = false;
		selectedNode = model->GetNodes().empty() ? -1 : 0;
		selectedNodes.clear();
		if (selectedNode >= 0) selectedNodes.push_back(selectedNode);
		selectedMesh = -1;
		selectedMeshes.clear();
		selectedComponentType = AttachedComponentType::None;
		selectedComponentIndex = -1;
		selectedComponentIndices.clear();
		focusSelectedComponent = false;
		selectedMaterial = model->GetMaterials().empty() ? -1 : 0;
		selectedAnimation = model->GetAnimations().empty() ? -1 : 0;
		selectedMorph = model->GetVmdlExtensionData().morphs.empty() ? -1 : 0;
		for (int i = 0; i < Count(model->GetVmdlExtensionData().morphs); ++i)
		{
			if (!model->GetVmdlExtensionData().morphs[i].applyOnInitialize) continue;
			selectedMorph = i;
			break;
		}
		animationTime = 0.0f;
		animationPlaying = false;
		selectedKeyTrack = -1;
		selectedKeyIndex = -1;
		UpdateModelFraming();
		ResetAnimationControlPreview();
		model->ApplyInitialMorphs();
		ResolveMissingExternalMeshes();
	}
	catch (const std::exception& exception)
	{
		model.reset();
		externalMeshPreviewCaches.clear();
		selectedNode = -1;
		selectedNodes.clear();
		ErrorMessage(std::string("Load failed: ") + exception.what());
	}
}

void VmdlEditorScene::ErrorMessage(const std::string& message)
{
	MessageBoxW(Game::Graphics::Instance().GetWindowHandle(),
		Utf8ToWide(message).c_str(), L"VEER", MB_ICONERROR);
}
