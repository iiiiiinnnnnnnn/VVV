// VmdlEditorScene.cpp
#include "Gameplay/Scene/VmdlEditorScene.h"

#include "Application/SettingsAndDebug/PhysicsLayerManager.h"
#include "Application/Tools/Dialog.h"
#include "Animation/Animator.h"
#include "Animation/HumanoidFootIK.h"
#include "Animation/MultiLegFootIK.h"
#include "Animation/SpringBone.h"
#include "Audio/SoundSystem.h"
#include "Core/Foundation/Json.h"
#include "Core/Object/Object.h"
#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Camera/CameraMoveSpeedOverlay.h"
#include "Physics/Collider/MeshCollider.h"
#include "Physics/RigidBody/Rigidbody.h"
#include "Rendering/Component/TrailRenderComponent.h"
#include "Rendering/Component/VMDLParticleEmitterComponent.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Renderer/ImGuiTheme.h"
#include "Resource/VMDLModel.h"

#include "SceneManager.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imguizmo/ImGuizmo.h>

#include <algorithm>
#include <format>
#include <cfloat>
#include <cmath>
#include <fstream>
#include <string_view>
#include "GameStartScene.h"
#include "Application/Time/GameTime.h"
#include "Resource/ResourceManager.h"
#include "Resource/MeshCache.h"
#include "Rendering/Renderer/ImGuiRenderer.h"

constexpr UINT PreviewWidth = 1024;
constexpr UINT PreviewHeight = 1024;
constexpr int PreviewGridSubdivisions = 20;
constexpr float PreviewGridScale = 0.5f;
constexpr float PreviewMinCameraDistance = 0.2f;
constexpr float PreviewMaxCameraDistance = 100000.0f;

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

std::string MakeVmshFileLabel(const std::string& source)
{
	std::string result;
	result.reserve(std::min<size_t>(source.size(), 48));
	for (unsigned char c : source)
	{
		if (result.size() >= 48) break;
		if (std::isalnum(c) || c == '_' || c == '-') result.push_back(static_cast<char>(c));
		else if (std::isspace(c) && !result.empty() && result.back() != '_') result.push_back('_');
	}
	return result.empty() ? "mesh" : result;
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

json ParticleEmitterToJson(const VMDLModel::VmdlParticleEmitter& v)
{
	return {
		{"version", 2}, {"rendererType", v.rendererType}, {"parent", v.parentEmitterIndex},
		{"name", v.name}, {"texture", v.texturePath},
		{"sheet", {v.columns, v.rows, v.frame}}, {"animated", v.animated},
		{"animationSpeed", v.animationSpeed}, {"capacity", v.capacity},
		{"offset", {v.offset.x, v.offset.y, v.offset.z}},
		{"spawnExtents", {v.spawnExtents.x, v.spawnExtents.y, v.spawnExtents.z}},
		{"velocityMin", {v.velocityMin.x, v.velocityMin.y, v.velocityMin.z}},
		{"velocityMax", {v.velocityMax.x, v.velocityMax.y, v.velocityMax.z}},
		{"acceleration", {v.acceleration.x, v.acceleration.y, v.acceleration.z}},
		{"emissionRate", v.emissionRate}, {"burstCount", v.burstCount},
		{"lifetime", {v.lifetimeMin, v.lifetimeMax}},
		{"sizeMin", {v.sizeMin.x, v.sizeMin.y}}, {"sizeMax", {v.sizeMax.x, v.sizeMax.y}},
		{"color", {v.color.x, v.color.y, v.color.z, v.color.w}},
		{"fade", {v.fadeInDuration, v.fadeOutDuration}},
		{"localVelocity", v.localVelocity}, {"additive", v.additive},
		{"ribbon", {{"root", {v.ribbonRootOffset.x, v.ribbonRootOffset.y, v.ribbonRootOffset.z}},
			{"tip", {v.ribbonTipOffset.x, v.ribbonTipOffset.y, v.ribbonTipOffset.z}},
			{"lifetime", v.ribbonLifetime}, {"maxPoints", v.ribbonMaxPoints},
			{"tipRatio", v.ribbonTipRatio}, {"sampleInterval", v.ribbonSampleInterval},
			{"endColor", {v.ribbonEndColor.x, v.ribbonEndColor.y,
				v.ribbonEndColor.z, v.ribbonEndColor.w}}}}
	};
}

bool ParticleEmitterFromJson(const json& j, VMDLModel::VmdlParticleEmitter& v)
{
	try
	{
		auto vec2 = [](const json& a, Vector2 fallback) {
			return a.is_array() && a.size() >= 2
				? Vector2(a[0].get<float>(), a[1].get<float>()) : fallback;
		};
		auto vec3 = [](const json& a, Vector3 fallback) {
			return a.is_array() && a.size() >= 3
				? Vector3(a[0].get<float>(), a[1].get<float>(), a[2].get<float>()) : fallback;
		};
		v.name = j.value("name", v.name);
		v.rendererType = std::clamp(j.value("rendererType", v.rendererType), 0, 1);
		v.parentEmitterIndex = j.value("parent", v.parentEmitterIndex);
		v.texturePath = j.value("texture", v.texturePath);
		if (const auto it = j.find("sheet"); it != j.end() && it->is_array() && it->size() >= 3)
		{
			v.columns = (*it)[0].get<int>(); v.rows = (*it)[1].get<int>();
			v.frame = (*it)[2].get<int>();
		}
		v.animated = j.value("animated", v.animated);
		v.animationSpeed = j.value("animationSpeed", v.animationSpeed);
		v.capacity = j.value("capacity", v.capacity);
		if (j.contains("offset")) v.offset = vec3(j["offset"], v.offset);
		if (j.contains("spawnExtents")) v.spawnExtents = vec3(j["spawnExtents"], v.spawnExtents);
		if (j.contains("velocityMin")) v.velocityMin = vec3(j["velocityMin"], v.velocityMin);
		if (j.contains("velocityMax")) v.velocityMax = vec3(j["velocityMax"], v.velocityMax);
		if (j.contains("acceleration")) v.acceleration = vec3(j["acceleration"], v.acceleration);
		v.emissionRate = j.value("emissionRate", v.emissionRate);
		v.burstCount = j.value("burstCount", v.burstCount);
		if (const auto it = j.find("lifetime"); it != j.end() && it->is_array() && it->size() >= 2)
		{
			v.lifetimeMin = (*it)[0].get<float>(); v.lifetimeMax = (*it)[1].get<float>();
		}
		if (j.contains("sizeMin")) v.sizeMin = vec2(j["sizeMin"], v.sizeMin);
		if (j.contains("sizeMax")) v.sizeMax = vec2(j["sizeMax"], v.sizeMax);
		if (const auto it = j.find("color"); it != j.end() && it->is_array() && it->size() >= 4)
			v.color = Color((*it)[0].get<float>(), (*it)[1].get<float>(),
				(*it)[2].get<float>(), (*it)[3].get<float>());
		if (const auto it = j.find("fade"); it != j.end() && it->is_array() && it->size() >= 2)
		{
			v.fadeInDuration = (*it)[0].get<float>(); v.fadeOutDuration = (*it)[1].get<float>();
		}
		v.localVelocity = j.value("localVelocity", v.localVelocity);
		v.additive = j.value("additive", v.additive);
		if (const auto ribbon = j.find("ribbon"); ribbon != j.end() && ribbon->is_object())
		{
			if (ribbon->contains("root")) v.ribbonRootOffset = vec3((*ribbon)["root"], v.ribbonRootOffset);
			if (ribbon->contains("tip")) v.ribbonTipOffset = vec3((*ribbon)["tip"], v.ribbonTipOffset);
			v.ribbonLifetime = ribbon->value("lifetime", v.ribbonLifetime);
			v.ribbonMaxPoints = ribbon->value("maxPoints", v.ribbonMaxPoints);
			v.ribbonTipRatio = ribbon->value("tipRatio", v.ribbonTipRatio);
			v.ribbonSampleInterval = ribbon->value("sampleInterval", v.ribbonSampleInterval);
			if (const auto c = ribbon->find("endColor"); c != ribbon->end() && c->is_array() && c->size() >= 4)
				v.ribbonEndColor = Color((*c)[0].get<float>(), (*c)[1].get<float>(),
					(*c)[2].get<float>(), (*c)[3].get<float>());
		}
		v.columns = std::clamp(v.columns, 1, 32); v.rows = std::clamp(v.rows, 1, 32);
		v.capacity = std::clamp(v.capacity, 1, 8192);
		return true;
	}
	catch (...) { return false; }
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

}

VmdlEditorScene::VmdlEditorScene() : VmdlEditorScene(std::filesystem::path{}) {}

VmdlEditorScene::VmdlEditorScene(std::filesystem::path filepath)
{
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(false);
	graphics.SetWindowMovementLocked(false);

	previewSceneTarget = std::make_unique<RenderTarget>(
		graphics.GetDevice(), PreviewWidth, PreviewHeight, DXGI_FORMAT_R16G16B16A16_FLOAT);

	previewTarget = std::make_unique<RenderTarget>(
		graphics.GetDevice(), PreviewWidth, PreviewHeight, DXGI_FORMAT_R8G8B8A8_UNORM);

	cameraOwner = std::make_unique<Object>("VMDL Editor Camera");

	editorCamera = cameraOwner->AddComponent<Camera>();

	editorLightDirection = {0.6f, -0.7f, 0.0f};
	LoadLayoutSettings();
	try
	{
		const auto source = ResourceManager::FindSourceResourceRoot();
		if (!source.empty()) editorSoundTracks.Load(source / "Sound" / "tracks.ini");
	}
	catch (const std::exception&)
	{}
	if (filepath.empty() && !recentModelPath.empty()) filepath = recentModelPath;
	if (!filepath.empty() && filepath.is_relative() && !ResourceManager::FindSourceResourceRoot().empty())
	{
		const std::filesystem::path relativePath =
			filepath.lexically_relative(std::filesystem::path("Resources"));
		const std::filesystem::path sourcePath =
			ResourceManager::FindSourceResourceRoot() / relativePath;
		if (std::filesystem::exists(sourcePath)) filepath = sourcePath;
	}
	if (!filepath.empty()) LoadModel(filepath);
}

VmdlEditorScene::~VmdlEditorScene()
{
	SoundSystem::Instance().SetListenerOverride(nullptr);
	SaveLayoutSettings();
	Game::Graphics& graphics = Game::Graphics::Instance();
	if (GetCapture() == graphics.GetWindowHandle()) ReleaseCapture();
	graphics.SetBorderlessFullscreen(false);
	graphics.SetWindowMovementLocked(false);
}

std::string VmdlEditorScene::SoundTrackLabel(int track) const
{
	const auto* entry = editorSoundTracks.Find(track);
	if (!entry) return (const char*)u8"未登録";
	const std::string normalized =
		std::filesystem::path(entry->path).lexically_normal().generic_string();
	constexpr std::string_view prefix = "Resources/Sound/";
	const std::string display = normalized.starts_with(prefix)
		? normalized.substr(prefix.size()) : normalized;
	return "SoundTrack::" + SoundTrackRegistry::ConstantName(entry->path) + "  -  " + display;
}

bool VmdlEditorScene::DrawSoundTrackSelector(const char* label, int& track)
{
	const std::string preview = SoundTrackLabel(track);
	bool changed = false;
	if (ImGui::BeginCombo(label, preview.c_str()))
	{
		for (const auto& [candidate, entry] : editorSoundTracks.GetEntries())
		{
			const std::string item = SoundTrackLabel(candidate);
			if (ImGui::Selectable(item.c_str(), track == candidate))
			{
				track = candidate;
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

	// 旧専用JSONがあれば初回だけ読み込み、シーン破棄時にEditor.iniへ移行する
	const std::filesystem::path settingsPath =
		std::filesystem::current_path() / "VmdlEditorLayout.json";
	const std::filesystem::path legacyPath =
		std::filesystem::current_path() / "Resources" / "VmdlEditorLayout.json";
	std::ifstream stream(std::filesystem::exists(settingsPath) ? settingsPath : legacyPath);
	if (!stream) return;

	try
	{
		json root;
		stream >> root;
		const std::string recentPathUtf8 = root.value("recentModelPath", std::string{});
		recentModelPath = std::filesystem::path(std::u8string(
			reinterpret_cast<const char8_t*>(recentPathUtf8.data()), recentPathUtf8.size()));
		if (const auto window = root.find("window"); window != root.end() && window->is_object())
		{
			savedWindowX = window->value("x", savedWindowX);
			savedWindowY = window->value("y", savedWindowY);
			savedWindowWidth = window->value("width", savedWindowWidth);
			savedWindowHeight = window->value("height", savedWindowHeight);
			savedWindowMaximized = window->value("maximized", savedWindowMaximized);
			savedWindowPlacementValid = savedWindowWidth >= 640 && savedWindowHeight >= 480;
		}

		const float propertyRatio = root.value("propertyPanelRatio", -1.0f);
		const float viewportRatio = root.value("viewportPanelRatio", -1.0f);
		const float bottomRatio = root.value("bottomPanelRatio", -1.0f);
		if (propertyRatio < 0.1f || propertyRatio > 0.6f || viewportRatio < 0.2f ||
			viewportRatio > 0.6f || propertyRatio + viewportRatio > 0.85f ||
			bottomRatio < 0.1f || bottomRatio > 0.7f)
			return;

		loadedPropertyPanelRatio = propertyRatio;
		loadedViewportPanelRatio = viewportRatio;
		loadedBottomPanelRatio = bottomRatio;
	}
	catch (const json::exception&)
	{}
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
	SetWindowTextW(window, L"VMDL Editor");

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
			const int workLeft = static_cast<int>(work.left);
			const int workTop = static_cast<int>(work.top);
			const int workRight = static_cast<int>(work.right);
			const int workBottom = static_cast<int>(work.bottom);
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
		selectedAnimation < static_cast<int>(model->GetAnimations().size()))
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

	// 画面全体を覆うエディタウィンドウ
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	constexpr ImGuiWindowFlags flags =
		ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;

	if (!ImGui::Begin((const char*)u8"VMDLエディタ", nullptr, flags))
	{
		ImGui::End();
		style = gameStyle;
		return;
	}

	// メニューは背後のプロパティや3Dビューが透けると読みにくいため、
	// VMDL Editor内だけ完全に不透明なポップアップとして描画する。
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.055f, 0.055f, 0.06f, 1.0f));
	DrawMenuBar();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	// 保存値と現在の画面サイズからパネル幅を決定
	const float totalHeight = ImGui::GetContentRegionAvail().y;
	const float splitterHeight = 6.0f;
	const float totalWidth = ImGui::GetContentRegionAvail().x;
	const float columnSplitterWidth = 6.0f;
	const float availableColumnWidth = totalWidth - columnSplitterWidth * 2.0f;
	const float previewAspect =
		static_cast<float>(PreviewWidth) / static_cast<float>(PreviewHeight);
	const bool canInitializeLayout =
		!restoreWindowPending && !layoutWindowMetricsPending &&
		availableColumnWidth >= 660.0f &&
		totalHeight >= 360.0f;
	if (!layoutInitialized)
	{
		bottomPanelHeight = 430.0f;
		if (canInitializeLayout && loadedBottomPanelRatio > 0.0f)
			bottomPanelHeight = totalHeight * loadedBottomPanelRatio;
		bottomPanelHeight = std::clamp(
			bottomPanelHeight, 140.0f, std::max(140.0f, totalHeight - 220.0f));

		const float upperHeight =
			std::max(220.0f, totalHeight - bottomPanelHeight - splitterHeight);
		const float defaultViewportWidth =
			std::min(upperHeight * previewAspect, std::max(220.0f, totalWidth - 440.0f));
		viewportPanelWidth = defaultViewportWidth;
		propertyPanelWidth =
			std::max(220.0f, (availableColumnWidth - viewportPanelWidth) * 0.5f);
		if (canInitializeLayout && loadedPropertyPanelRatio > 0.0f)
		{
			propertyPanelWidth = availableColumnWidth * loadedPropertyPanelRatio;
			viewportPanelWidth = availableColumnWidth * loadedViewportPanelRatio;
		}
		layoutInitialized = canInitializeLayout;
	}

	bottomPanelHeight =
		std::clamp(bottomPanelHeight, 140.0f, std::max(140.0f, totalHeight - 220.0f));
	const float upperHeight = std::max(220.0f, totalHeight - bottomPanelHeight - splitterHeight);
	const float minimumViewportWidth = std::clamp(totalWidth * 0.25f, 300.0f, 480.0f);
	const float maximumStoredViewportWidth =
		std::max(minimumViewportWidth, availableColumnWidth - 440.0f);
	viewportPanelWidth =
		std::clamp(viewportPanelWidth, minimumViewportWidth, maximumStoredViewportWidth);
	propertyPanelWidth = std::clamp(propertyPanelWidth, 220.0f,
		std::max(220.0f, availableColumnWidth - viewportPanelWidth - 220.0f));
	if (layoutInitialized)
	{
		layoutColumnWidth = availableColumnWidth;
		layoutTotalHeight = totalHeight;
	}
	// SetWindowPosはImGui::NewFrameより後に実行されるため、このフレームの
	// DisplaySizeはまだランチャーの値。次フレームから保存値を適用する
	if (!restoreWindowPending && layoutWindowMetricsPending)
		layoutWindowMetricsPending = false;

	// 上段のプロパティ、3Dビュー、階層
	ImGui::BeginChild("Property", ImVec2(propertyPanelWidth, upperHeight), true);
	DrawProperty();
	ImGui::EndChild();

	ImGui::SameLine(0.0f, 0.0f);
	ImGui::InvisibleButton("##PropertySplitter", ImVec2(columnSplitterWidth, upperHeight));
	if (ImGui::IsItemActive())
	{
		const float maximumPropertyWidth =
			std::max(220.0f, availableColumnWidth - viewportPanelWidth - 220.0f);
		const float nextWidth = std::clamp(
			propertyPanelWidth + io.MouseDelta.x, 220.0f, maximumPropertyWidth);
		propertyPanelWidth = nextWidth;
	}
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

	ImGui::SameLine(0.0f, 0.0f);
	ImGui::BeginChild(
		"3D View", ImVec2(viewportPanelWidth, upperHeight), true, ImGuiWindowFlags_NoScrollbar);
	DrawViewport();
	ImGui::EndChild();

	ImGui::SameLine(0.0f, 0.0f);
	ImGui::InvisibleButton("##ViewportSplitter", ImVec2(columnSplitterWidth, upperHeight));
	if (ImGui::IsItemActive())
	{
		const float nextWidth = std::clamp(viewportPanelWidth + io.MouseDelta.x,
			minimumViewportWidth, maximumStoredViewportWidth);
		viewportPanelWidth = nextWidth;
	}
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

	ImGui::SameLine(0.0f, 0.0f);
	ImGui::BeginChild("Hierarchy", ImVec2(0.0f, upperHeight), true);
	DrawHierarchy();
	ImGui::EndChild();

	// 上段と下段の高さ調整
	ImGui::Button("##BottomSplitter", ImVec2(-1.0f, splitterHeight));
	if (ImGui::IsItemActive())
	{
		const float nextHeight =
			std::clamp(bottomPanelHeight - io.MouseDelta.y, 140.0f, totalHeight - 220.0f);
		bottomPanelHeight = nextHeight;
	}
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	// 下段の編集タブ
	ImGui::BeginChild("Editor Bottom", ImVec2(0.0f, 0.0f), true);
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
		if (ImGui::BeginTabItem((const char*)u8"マテリアル"))
		{
			const ImVec2 tabMin = ImGui::GetItemRectMin();
			const ImVec2 tabMax = ImGui::GetItemRectMax();
			ImGui::GetWindowDrawList()->AddRect(
				tabMin, tabMax, ImGuiTheme::SelectedOutline, 2.0f, 0, 2.0f);
			ImGui::GetWindowDrawList()->AddRectFilled(
				tabMin, ImVec2(tabMin.x + 4.0f, tabMax.y), ImGuiTheme::SelectedAccent);
			DrawMaterialEditor();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::PopStyleColor(10);
	ImGui::EndChild();

	if (animationRecording)
	{
		const ImVec2 position = ImGui::GetWindowPos();
		const ImVec2 size = ImGui::GetWindowSize();

		constexpr float thickness = 4.0f;
		constexpr float inset = thickness * 0.5f;

		ImGui::GetForegroundDrawList()->AddRect(ImVec2(position.x + inset, position.y + inset),
			ImVec2(position.x + size.x - inset, position.y + size.y - inset),
			IM_COL32(255, 0, 0, 255), 0.0f, ImDrawFlags_None, thickness);
	}

	ImGui::End();
	if (showPhysicsLayerWindow) PhysicsLayerManager::Instance().DrawGUI(&showPhysicsLayerWindow);
	if (showFootIkPreviewWindow) DrawFootIkPreviewWindow();

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
	if (selectedAnimation >= 0 && selectedAnimation < static_cast<int>(model->GetAnimations().size()) &&
		animationTime >= model->GetAnimations()[selectedAnimation].secondsLength - 0.0001f)
	{
		animationTime = 0.0f;
		ApplyAnimationPreview();
	}
	unifiedPreviewActive = true;
	animationPlaying = true;
	animationSoundPreviewStarting = true;
	particlePreviewBurstPending = true;
	springPreviewSignature.clear();
	StartPresentationPreview();
	if (model->GetVmdlIKSettings().type != 0)
	{
		showFootIkTestStage = true;
		if (!footIkTestStageModel) LoadFootIkTestStage();
	}
}

void VmdlEditorScene::StopUnifiedPreview(bool rewind)
{
	animationPlaying = false;
	particlePreviewBurstPending = false;
	if (!rewind) return;

	unifiedPreviewActive = false;
	animationTime = 0.0f;
	ApplyAnimationPreview();
	ResetAnimationControlPreview();
	animationSoundPreviewStarting = true;
	cameraShakePreviewTimer = cameraShakePreviewDuration = cameraShakePreviewIntensity = 0.0f;
	radialBlurPreviewTimer = radialBlurPreviewDuration = radialBlurPreviewPower = 0.0f;
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
	cameraShakePreviewTimer = cameraShakePreviewDuration = cameraShakePreviewIntensity = 0.0f;
	radialBlurPreviewTimer = radialBlurPreviewDuration = radialBlurPreviewPower = 0.0f;
	if (!model || !editorCamera) return;
	if (selectedAnimation >= 0 && selectedAnimation < static_cast<int>(model->GetAnimations().size()))
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
		const Vector3 origin = node >= 0 && node < static_cast<int>(model->GetNodes().size())
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
		cameraShakePreviewTimer = std::max(cameraShakePreviewTimer, value.duration);
		cameraShakePreviewIntensity = std::max(cameraShakePreviewIntensity, value.intensity * strength);
	}
	for (const auto& value : model->GetVmdlPresentationData().radialBlurs)
	{
		const float strength = strengthAt(value.nodeIndex, value.range, value.distanceAttenuation);
		if (value.power * strength <= radialBlurPreviewPower) continue;
		radialBlurPreviewDuration = value.duration;
		radialBlurPreviewTimer = value.duration;
		radialBlurPreviewPower = value.power * strength;
		radialBlurPreviewAttackRate = value.attackRate;
	}
}

void VmdlEditorScene::PlayPresentationPreviewEvents(
	int animationIndex, float beginTime, float endTime)
{
	if (!model || !editorCamera || animationIndex < 0 ||
		animationIndex >= static_cast<int>(model->GetAnimations().size()) || endTime < beginTime)
		return;
	const std::string& animationName = model->GetAnimations()[animationIndex].name;
	const auto& presentation = model->GetVmdlPresentationData();
	const Vector3 listener = editorCamera->GetEye();
	const auto strengthAt = [this, &listener](int node, float range, bool attenuate) {
		const Vector3 origin = node >= 0 && node < static_cast<int>(model->GetNodes().size())
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
				key.componentIndex >= static_cast<int>(presentation.cameraShakes.size())) continue;
			const auto& value = presentation.cameraShakes[key.componentIndex];
			const float strength = strengthAt(value.nodeIndex, value.range, value.distanceAttenuation);
			cameraShakePreviewDuration = cameraShakePreviewTimer = value.duration;
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
				key.componentIndex >= static_cast<int>(presentation.radialBlurs.size())) continue;
			const auto& value = presentation.radialBlurs[key.componentIndex];
			const float strength = strengthAt(value.nodeIndex, value.range, value.distanceAttenuation);
			radialBlurPreviewDuration = radialBlurPreviewTimer = value.duration;
			radialBlurPreviewPower = value.power * strength;
			radialBlurPreviewAttackRate = value.attackRate;
		}
	}
}

void VmdlEditorScene::RenderPreview()
{
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

	const float zoomLerp = 1.0f - std::exp(-12.0f * Game::Time::unscaledDeltaTime);
	cameraDistance = std::lerp(cameraDistance, targetCameraDistance, zoomLerp);
	const Vector3 focus = Vector3(0.0f, 1.0f, 0.0f) + cameraFocusOffset;
	const float horizontalDistance = cameraDistance * std::cos(cameraPitch);
	const Vector3 eye = focus + Vector3(std::sin(cameraYaw) * horizontalDistance,
									std::sin(cameraPitch) * cameraDistance,
									-std::cos(cameraYaw) * horizontalDistance);
	editorCamera->SetLookAt(eye, focus, Vector3::Up);
	if (cameraShakePreviewTimer > 0.0f)
	{
		cameraShakePreviewTimer = std::max(0.0f,
			cameraShakePreviewTimer - Game::Time::unscaledDeltaTime);
		const float fade = cameraShakePreviewDuration > 0.0f
			? cameraShakePreviewTimer / cameraShakePreviewDuration : 0.0f;
		// VMDLプレビューはモデル全体を収めるためゲームカメラより遠くなることがある。
		// ワールド単位の強度をそのまま使うと画面上でサブピクセルになるので、
		// 基準距離5mに対する表示距離で補正し、見た目の揺れ幅を維持する。
		const float previewDistanceScale = std::max(cameraDistance / 5.0f, 1.0f);
		const float elapsed = std::max(cameraShakePreviewDuration - cameraShakePreviewTimer, 0.0f);
		const float horizontal = std::sin(elapsed * 71.0f) * cameraShakePreviewIntensity;
		const float vertical = std::sin(elapsed * 97.0f + 1.3f) * cameraShakePreviewIntensity;
		const Vector3 offset = (editorCamera->GetRight() * horizontal +
			editorCamera->GetUp() * vertical) * previewDistanceScale * fade;
		editorCamera->SetLookAt(eye + offset, focus + offset, Vector3::Up);
	}
	SoundSystem::Instance().SetListenerOverride(editorCamera);
	const float nearClip = std::max(0.01f, cameraDistance * 0.0001f);
	const float farClip = std::max(1000.0f, cameraDistance * 2.0f);
	editorCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(45.0f), 1.0f, nearClip, farClip);

	Game::Graphics& graphics = Game::Graphics::Instance();
	ID3D11DeviceContext* dc = graphics.GetDeviceContext();
	previewSceneTarget->Clear(dc, 0, 0, 0, 1.0f);
	previewSceneTarget->Activate(dc);

	RenderContext rc{};
	rc.deviceContext = dc;
	rc.renderState = graphics.GetRenderState();
	rc.camera = editorCamera;
	editorLights.GetDirectionalLight().transform.SetAngle(editorLightDirection);
	editorLightDirection.y += Game::Time::unscaledDeltaTime * 50.0f;
	if (editorLightDirection.y > 360.0f) editorLightDirection.y -= 360.0f;
	rc.lightManager = &editorLights;
	rc.iblData.diffuseIrradianceEnvironmentMap = graphics.GetIBLDiffuseIEM();
	rc.iblData.specularPremappingRadianceEnvironmentMap = graphics.GetIBLSpecularPMREM();
	rc.iblData.ggxLookUpTableMap = graphics.GetIBLGGXLUT();
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
		graphics.GetModelRenderer()->Render(rc);
	}

	if (showDebugOverlays && showGrid)
	{
		graphics.GetPrimitiveRenderer()->DrawGrid(PreviewGridSubdivisions, PreviewGridScale);
		graphics.GetPrimitiveRenderer()->Render(dc, editorCamera->GetView(),
			editorCamera->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	}

	if (model)
	{
		model->UpdateTransform(Matrix::Identity);
		std::vector<VMDLModel::NodePose> animationPose;
		const bool useFootIk = animationPlaying && showFootIkTestStage && footIkTestStageModel;
		if (useFootIk || animationPlaying) model->GetNodePoses(animationPose);
		const bool footIkApplied = useFootIk && ApplyFootIkPreview();
		const bool springApplied = UpdateSpringPreview();

		if (showMesh)
		{
			UpdateExternalMeshPreview();
			rc.renderSettings.wireframe = false;
			VMatRenderParams params;
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
			graphics.GetModelRenderer()->Draw(ModelShaderId::VMat, model, &params);
			for (const auto& [groupIndex, cache] : externalMeshPreviewCaches)
				graphics.GetModelRenderer()->DrawMeshCache(
					ModelShaderId::VMat, cache, model, &params);
			graphics.GetModelRenderer()->Render(rc);
		}
		UpdateTrailPreview(rc);
		UpdateParticlePreview(rc);
		dc->OMSetDepthStencilState(
			rc.renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

		bool hasShapes = false;
		if (showDebugOverlays && showBones)
		{
			const auto& nodes = model->GetNodes();
			const Matrix renderScaleTransform = model->GetRenderScaleTransform();
			for (int nodeIndex = 0; nodeIndex < static_cast<int>(nodes.size()); ++nodeIndex)
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
			if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model->GetNodes().size()))
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
			for (int i = 0; i < static_cast<int>(ikSettings.legs.size()); ++i)
			{
				const auto& leg = ikSettings.legs[i];
				const int rootIndex = model->GetNodeIndex(leg.root.c_str());
				const int midIndex = model->GetNodeIndex(leg.mid.c_str());
				const int tipIndex = model->GetNodeIndex(leg.tip.c_str());
				if (rootIndex < 0 || midIndex < 0 || tipIndex < 0) continue;

				Vector3 polePosition;
				if (i < static_cast<int>(poles.size()) && poles[i].custom)
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
		if (showDebugOverlays && showRigidBody)
		{
			for (const auto& value : data.rigidBodies)
			{
				Matrix transform = nodeOffsetTransform(
					value.nodeIndex, value.offsetPosition, value.offsetRotation);
				Vector3 scale;
				Vector3 position;
				Quaternion rotation;
				transform.Decompose(scale, rotation, position);
				graphics.GetShapeRenderer()->DrawBox(position, rotation.ToEuler(),
					Vector3(0.2f, 0.2f, 0.2f), Color(1.0f, 0.75f, 0.1f, 0.7f));
				hasShapes = true;
			}
		}
		if (showDebugOverlays && showCollider)
		{
			for (int i = 0; i < static_cast<int>(data.colliders.size()); ++i)
			{
				if (i < static_cast<int>(previewColliderActive.size()) &&
					previewColliderActive[i] == 0)
					continue;
				const auto& value = data.colliders[i];
				Matrix transform = model->GetScaledAttachmentTransform(
					nodeOffsetTransform(value.nodeIndex, value.center, value.rotation));
				const Vector3 scaledSize = model->GetScaledAttachmentVector(value.size);
				Vector3 transformScale;
				Vector3 position;
				Quaternion rotation;
				transform.Decompose(transformScale, rotation, position);
				const Matrix pose =
					Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(position);
				if (value.shape == 1)
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
		// 3Dサウンドの減衰開始範囲と再生限界範囲を表示
		if (showDebugOverlays && showSoundRange)
		{
			const auto& soundSources = model->GetVmdlSoundData().sources;
			const auto& nodes = model->GetNodes();
			for (const auto& source : soundSources)
			{
				if (!source.spatial || source.nodeIndex < 0 ||
					source.nodeIndex >= static_cast<int>(nodes.size()))
					continue;

				const Vector3 position = matrixPosition(model->GetScaledAttachmentTransform(
					nodes[source.nodeIndex].worldTransform));
				graphics.GetShapeRenderer()->DrawSphere(position,
					std::max(source.maxDistance, source.minDistance + 0.01f),
					Color(0.95f, 0.20f, 0.75f, 0.7f));
				graphics.GetShapeRenderer()->DrawSphere(position,
					std::max(0.01f, source.minDistance),
					Color(0.25f, 0.85f, 1.0f, 0.9f));
				hasShapes = true;
			}
		}
		if (showDebugOverlays && showSoundRange)
		{
			const auto& nodes = model->GetNodes();
			const auto& presentation = model->GetVmdlPresentationData();
			for (const auto& value : presentation.cameraShakes)
			{
				if (value.nodeIndex < 0 || value.nodeIndex >= static_cast<int>(nodes.size())) continue;
				graphics.GetShapeRenderer()->DrawSphere(nodes[value.nodeIndex].worldTransform.Translation(),
					value.range, Color(1.0f, 0.65f, 0.1f, 0.7f));
				hasShapes = true;
			}
			for (const auto& value : presentation.radialBlurs)
			{
				if (value.nodeIndex < 0 || value.nodeIndex >= static_cast<int>(nodes.size())) continue;
				graphics.GetShapeRenderer()->DrawSphere(nodes[value.nodeIndex].worldTransform.Translation(),
					value.range, Color(0.7f, 0.25f, 1.0f, 0.7f));
				hasShapes = true;
			}
		}
		if (showDebugOverlays && showSpringCollider)
		{
			for (const auto& value : data.springColliders)
			{
				graphics.GetShapeRenderer()->DrawSphere(
					matrixPosition(nodeOffsetTransform(value.nodeIndex, value.offsetPosition)),
					value.radius, Color(1.0f, 0.2f, 0.9f, 0.7f));
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
				const Vector3 start =
					matrixPosition(nodeOffsetTransform(value.nodeIndex, Vector3::Zero));
				const Vector3 end =
					matrixPosition(nodeOffsetTransform(value.nodeIndex, value.offsetPosition));
				graphics.GetPrimitiveRenderer()->DrawLine(
					start, end, Color(0.3f, 1.0f, 0.3f, 1.0f), Color(0.1f, 0.5f, 0.1f, 1.0f));
			}
			graphics.GetPrimitiveRenderer()->Render(dc, editorCamera->GetView(),
				editorCamera->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		}
		if (showDebugOverlays && showTrail)
		{
			const auto& trails = model->GetVmdlTrailData().trails;
			for (int i = 0; i < static_cast<int>(trails.size()); ++i)
			{
				if (i < static_cast<int>(previewTrailActive.size()) && previewTrailActive[i] == 0)
					continue;
				const auto& value = trails[i];
				const Vector3 root = matrixPosition(model->GetScaledAttachmentTransform(
					nodeOffsetTransform(value.nodeIndex, value.rootOffset, value.offsetAngle)));
				const Vector3 tip = matrixPosition(model->GetScaledAttachmentTransform(
					nodeOffsetTransform(value.nodeIndex, value.tipOffset, value.offsetAngle)));
				graphics.GetPrimitiveRenderer()->DrawLine(root, tip, value.color, value.color);
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

	previewSceneTarget->Deactivate(dc);

	previewTarget->Clear(dc);
	previewTarget->Activate(dc);
	postProcess.ClearRuntimeEffects();
	if (radialBlurPreviewTimer > 0.0f)
	{
		radialBlurPreviewTimer = std::max(0.0f,
			radialBlurPreviewTimer - Game::Time::unscaledDeltaTime);
		const float elapsed = radialBlurPreviewDuration - radialBlurPreviewTimer;
		const float attackDuration = radialBlurPreviewDuration * radialBlurPreviewAttackRate;
		const float intensity = elapsed < attackDuration
			? elapsed / std::max(attackDuration, 0.001f)
			: radialBlurPreviewTimer / std::max(radialBlurPreviewDuration - attackDuration, 0.001f);
		postProcess.AddRuntimeRadialBlur(radialBlurPreviewPower * std::clamp(intensity, 0.0f, 1.0f));
	}
	postProcess.ToneMapping(rc, previewSceneTarget->GetSRV());
	previewTarget->Deactivate(dc);
}

void VmdlEditorScene::DrawMenuBar()
{
	auto& io = ImGui::GetIO();
	if (!ImGui::BeginMenuBar()) return;

	// ファイル操作
	if (ImGui::BeginMenu((const char*)u8"ファイル"))
	{
		if (ImGui::MenuItem((const char*)u8"VMDLを開く...", "Ctrl+O")) OpenVmdl();
		if (ImGui::MenuItem((const char*)u8"VMDLを保存", "Ctrl+S", false, model != nullptr))
			SaveVmdl();
		if (ImGui::MenuItem(
				(const char*)u8"名前を付けて保存...", "Ctrl+Shift+S", false, model != nullptr))
			SaveVmdlAs();
		ImGui::Separator();
		if (ImGui::MenuItem((const char*)u8"GLBをインポート...")) ImportGlb();
		if (ImGui::MenuItem(
				(const char*)u8"GLBキャッシュを置換...", nullptr, false, model != nullptr))
			ReplaceGlbCache();
		if (ImGui::MenuItem(
				(const char*)u8"アニメーションGLBを追加...", nullptr, false, model != nullptr))
		{
			AppendAnimationGlb();
		}
		ImGui::Separator();
		if (ImGui::MenuItem((const char*)u8"終了") && OnRequestExit()) exiting = true;
		ImGui::EndMenu();
	}

	// プレビュー表示
	if (ImGui::BeginMenu((const char*)u8"表示"))
	{
		if (ImGui::MenuItem("PBR", nullptr, previewShadingMode == PreviewShadingMode::Pbr))
			previewShadingMode = PreviewShadingMode::Pbr;
		if (ImGui::MenuItem("Unlit", nullptr, previewShadingMode == PreviewShadingMode::Unlit))
			previewShadingMode = PreviewShadingMode::Unlit;
		if (ImGui::MenuItem(
				(const char*)u8"ソリッド", nullptr, previewShadingMode == PreviewShadingMode::Solid))
			previewShadingMode = PreviewShadingMode::Solid;
		ImGui::Separator();
		ImGui::MenuItem((const char*)u8"デバッグ表示", "F3", &showDebugOverlays);
		ImGui::Separator();
		ImGui::MenuItem((const char*)u8"メッシュ", "M", &showMesh);
		ImGui::MenuItem((const char*)u8"ボーン", "B", &showBones);
		ImGui::MenuItem((const char*)u8"IKポール", "I", &showIkPole);
		ImGui::MenuItem((const char*)u8"リジッドボディ", "R", &showRigidBody);
		ImGui::MenuItem((const char*)u8"コライダー", "C", &showCollider);
		ImGui::MenuItem((const char*)u8"サウンド・演出範囲", nullptr, &showSoundRange);
		ImGui::MenuItem((const char*)u8"スプリング", "S", &showSpring);
		ImGui::MenuItem((const char*)u8"スプリングコライダー", "Shift+C", &showSpringCollider);
		ImGui::MenuItem((const char*)u8"トレイル", "T", &showTrail);
		ImGui::MenuItem((const char*)u8"パーティクル", "P", &showParticle);
		ImGui::MenuItem((const char*)u8"グリッド", "G", &showGrid);
		ImGui::EndMenu();
	}

	// 補助ウィンドウ
	if (ImGui::BeginMenu((const char*)u8"ウィンドウ"))
	{
		if (ImGui::MenuItem((const char*)u8"物理レイヤー")) showPhysicsLayerWindow = true;
		ImGui::EndMenu();
	}

	// プレビュー用ツール
	if (ImGui::BeginMenu((const char*)u8"ツール"))
	{
		if (ImGui::MenuItem((const char*)u8"スケール設定...", nullptr, false, model != nullptr))
		{
			setScaleValue = model->GetModelScale();
			showSetScaleWindow = true;
		}
		if (ImGui::MenuItem((const char*)u8"Foot IKテストステージ設定..."))
		{
			showFootIkPreviewWindow = true;
			showFootIkTestStage = true;
			LoadFootIkTestStage();
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
	ImGui::EndMenuBar();

	if (exiting && SceneManager::Instance().LoadScene<GameStartScene>()) exiting = false;
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
	for (int groupIndex = 0; groupIndex < static_cast<int>(groups.size()); ++groupIndex)
	{
		const auto& group = groups[groupIndex];
		bool active = false;
		for (int meshIndex : group.meshIndices)
			if (meshIndex >= 0 && meshIndex < static_cast<int>(meshes.size()) &&
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
				loaded = externalMeshPreviewCaches.emplace(groupIndex,
					std::make_shared<MeshCache>(resolved, *model)).first;
			}
			catch (const std::exception& exception)
			{
				OutputDebugStringA(("VMSH preview load failed: " + group.path + " (" +
					exception.what() + ")\n").c_str());
				continue;
			}
		}
		auto& cacheMeshes = loaded->second->GetMeshes();
		for (auto& cacheMesh : cacheMeshes) cacheMesh.isDraw = false;
		for (size_t bindingSlot = 0; bindingSlot < group.meshIndices.size(); ++bindingSlot)
		{
			const int cacheSlot = bindingSlot < group.cacheMeshIndices.size()
				? group.cacheMeshIndices[bindingSlot]
				: static_cast<int>(bindingSlot);
			const int meshIndex = group.meshIndices[bindingSlot];
			if (cacheSlot < 0 || cacheSlot >= static_cast<int>(cacheMeshes.size())) continue;
			cacheMeshes[cacheSlot].isDraw = meshIndex >= 0 &&
				meshIndex < static_cast<int>(meshes.size()) && meshes[meshIndex].isDraw;
		}
	}
}

void VmdlEditorScene::DrawFootIkPreviewWindow()
{
	if (!ImGui::Begin((const char*)u8"Foot IKプレビュー", &showFootIkPreviewWindow,
			ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Checkbox((const char*)u8"テストステージを表示", &showFootIkTestStage))
	{
		if (showFootIkTestStage) LoadFootIkTestStage();
		UpdateFootIkTestStage();
	}
	ImGui::TextDisabled((const char*)u8"Foot IKは上部の再生ボタンと連動します");
	ImGui::Checkbox((const char*)u8"IKターゲットとレイを表示", &showFootIkDebug);

	bool stageChanged =
		ImGui::DragFloat3((const char*)u8"ステージ位置", &footIkTestStagePosition.x, 0.1f);
	stageChanged |=
		ImGui::DragFloat3((const char*)u8"ステージ回転", &footIkTestStageRotation.x, 1.0f);
	stageChanged |= ImGui::DragFloat(
		(const char*)u8"ステージサイズ", &footIkTestStageSize, 1.0f, 0.01f, 100000.0f);
	if (stageChanged) UpdateFootIkTestStage();

	ImGui::Separator();
	if (!model)
	{
		ImGui::TextDisabled((const char*)u8"VMDLを開くとIK割合を確認できます");
	}
	else if (model->GetVmdlIKSettings().type == 0)
	{
		ImGui::TextDisabled((const char*)u8"IK設定がありません");
	}
	else if (selectedAnimation < 0 ||
			 selectedAnimation >= static_cast<int>(model->GetAnimations().size()))
	{
		ImGui::TextDisabled((const char*)u8"アニメーションが選択されていません");
	}
	else
	{
		const auto& legs = model->GetVmdlIKSettings().legs;
		for (int footIndex = 0; footIndex < static_cast<int>(legs.size()); ++footIndex)
		{
			const char* name =
				legs[footIndex].name.empty() ? (const char*)u8"脚" : legs[footIndex].name.c_str();
			ImGui::Text((const char*)u8"%s %d  IK割合 %.2f", name, footIndex + 1,
				model->EvaluateFootIKWeight(selectedAnimation, animationTime, footIndex));
		}
	}
	ImGui::End();
}

void VmdlEditorScene::LoadFootIkTestStage()
{
	if (footIkTestStageModel) return;

	footIkTestStageModel = ResourceManager::Instance().LoadModel("Resources/Model/teststage.vmdl");
	if (!footIkTestStageModel)
	{
		ErrorMessage("Resources/Model/teststage.vmdl could not be loaded.");
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
	for (int animationIndex = 0; animationIndex < static_cast<int>(model->GetAnimations().size());
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
		selectedAnimation >= static_cast<int>(model->GetAnimations().size()))
		return false;

	const auto& settings = model->GetVmdlIKSettings();
	std::string signature = std::to_string(settings.type) + "|" + settings.centerNode + "|";
	const auto& solverSettings = model->GetVmdlMultiLegIKSettings();
	signature += std::to_string(solverSettings.bodyHeightOffset) + "|" +
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
	if (selectedAnimation >= static_cast<int>(layer.states.size())) return false;
	layer.currentStateIndex = selectedAnimation;
	layer.currentTime = animationTime;
	model->UpdateTransform(Matrix::Identity);
	footIkPreviewOwner->LateUpdate();
	return true;
}

void VmdlEditorScene::DrawHierarchy()
{
	ImGui::TextUnformatted((const char*)u8"階層");
	ImGui::Separator();

	// ノードとコンポーネントの検索
	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::InputTextWithHint("##HierarchySearch",
			(const char*)u8"ノード・メッシュ・コンポーネントを検索", &hierarchySearch))
		hierarchySearchUpper = ToUpperString(hierarchySearch);
	ImGui::Separator();
	if (!model)
	{
		ImGui::TextDisabled((const char*)u8"モデルが読み込まれていません");
		return;
	}

	// 検索条件に一致したノードツリー
	const auto& nodes = model->GetNodes();
	bool found = false;
	for (int index = 0; index < static_cast<int>(nodes.size()); ++index)
	{
		if (nodes[index].parentIndex >= 0 || !NodeMatchesHierarchySearch(index)) continue;
		DrawNodeTree(index);
		found = true;
	}
	if (!found) ImGui::TextDisabled((const char*)u8"一致する項目がありません");
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
	if (!model || nodeIndex < 0 || nodeIndex >= static_cast<int>(model->GetNodes().size())) return;

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
	if (!model || meshIndex < 0 || meshIndex >= static_cast<int>(model->GetMeshes().size())) return;

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
	for (int nodeIndex : selectedNodes)
	{
		if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model->GetNodes().size())) continue;

		int componentIndex = -1;
		switch (type)
		{
		case AttachedComponentType::RigidBody:
		{
			const std::string name =
				MakeUniqueAttachedComponentName(type, "RIGIDBODY");
			componentIndex = static_cast<int>(data.rigidBodies.size());
			auto& value = data.rigidBodies.emplace_back();
			value.name = name;
			value.nodeIndex = nodeIndex;
			break;
		}
		case AttachedComponentType::Collider:
		{
			const std::string name = MakeUniqueAttachedComponentName(type, "COLLIDER");
			componentIndex = static_cast<int>(data.colliders.size());
			auto& value = data.colliders.emplace_back();
			value.name = name;
			value.nodeIndex = nodeIndex;
			break;
		}
		case AttachedComponentType::Spring:
		{
			const std::string name = MakeUniqueAttachedComponentName(type, "SPRING");
			componentIndex = static_cast<int>(data.springs.size());
			auto& value = data.springs.emplace_back();
			value.name = name;
			value.nodeIndex = nodeIndex;
			break;
		}
		case AttachedComponentType::SpringCollider:
		{
			const std::string name =
				MakeUniqueAttachedComponentName(type, "SPRING COLLIDER");
			componentIndex = static_cast<int>(data.springColliders.size());
			auto& value = data.springColliders.emplace_back();
			value.name = name;
			value.nodeIndex = nodeIndex;
			break;
		}
		case AttachedComponentType::Trail:
		{
			const std::string name = MakeUniqueAttachedComponentName(type, "TRAIL");
			auto& trails = model->GetVmdlTrailData().trails;
			componentIndex = static_cast<int>(trails.size());
			auto& value = trails.emplace_back();
			value.name = name;
			value.nodeIndex = nodeIndex;
			break;
		}
		case AttachedComponentType::Particle:
		{
			const std::string name = MakeUniqueAttachedComponentName(type, "PARTICLE");
			auto& emitters = model->GetVmdlParticleData().emitters;
			componentIndex = static_cast<int>(emitters.size());
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
			componentIndex = static_cast<int>(sources.size());
			auto& value = sources.emplace_back();
			value.name = name;
			value.nodeIndex = nodeIndex;
			break;
		}
		case AttachedComponentType::CameraShake:
		{
			auto& values = model->GetVmdlPresentationData().cameraShakes;
			componentIndex = static_cast<int>(values.size());
			auto& value = values.emplace_back();
			value.name = MakeUniqueAttachedComponentName(type, "CAMERA SHAKE");
			value.nodeIndex = nodeIndex;
			break;
		}
		case AttachedComponentType::RadialBlur:
		{
			auto& values = model->GetVmdlPresentationData().radialBlurs;
			componentIndex = static_cast<int>(values.size());
			auto& value = values.emplace_back();
			value.name = MakeUniqueAttachedComponentName(type, "RADIAL BLUR");
			value.nodeIndex = nodeIndex;
			break;
		}
		default:
			break;
		}
		if (nodeIndex == selectedNode) primaryComponentIndex = componentIndex;
	}

	selectedMesh = -1;
	selectedMeshes.clear();
	selectedComponentType = type;
	selectedComponentIndex = primaryComponentIndex;
	focusSelectedComponent = primaryComponentIndex >= 0;
	MarkDirty();
}

void VmdlEditorScene::DrawHierarchyComponent(int nodeIndex, AttachedComponentType type,
	int componentIndex, const char* label, const std::string& name)
{
	ImGui::PushID(static_cast<int>(type));
	ImGui::PushID(componentIndex);
	const std::string displayName = name.empty() ? label : std::string(label) + " : " + name;
	const bool selected = selectedNode == nodeIndex && selectedMesh < 0 &&
		selectedComponentType == type && selectedComponentIndex == componentIndex;
	if (ImGui::Selectable(displayName.c_str(), selected))
	{
		SelectNode(nodeIndex, false);
		selectedComponentType = type;
		selectedComponentIndex = componentIndex;
		focusSelectedComponent = true;
		selectedKeyTrack = -1;
		selectedKeyIndex = -1;
	}
	ImGui::PopID();
	ImGui::PopID();
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
		std::any_of(model->GetVmdlPresentationData().cameraShakes.begin(),
			model->GetVmdlPresentationData().cameraShakes.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }) ||
		std::any_of(model->GetVmdlPresentationData().radialBlurs.begin(),
			model->GetVmdlPresentationData().radialBlurs.end(),
			[nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; });

	const bool selected = IsNodeSelected(nodeIndex) && selectedMesh < 0;
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
	for (int meshIndex = 0; meshIndex < static_cast<int>(meshes.size()); ++meshIndex)
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
				if (ImGui::MenuItem((const char*)u8"VMSHへ分離...", nullptr, false,
						selectedMorph >= 0))
					SeparateMeshToCache(meshIndex);
				if (selectedMorph < 0)
					ImGui::TextDisabled((const char*)u8"分離には適用先モーフの選択が必要です");
			}
			ImGui::EndPopup();
		}
	}
	for (int i = 0; i < static_cast<int>(extension.rigidBodies.size()); ++i)
	{
		const auto& value = extension.rigidBodies[i];
		if (value.nodeIndex != nodeIndex ||
			(!showAllContents && !ComponentMatchesHierarchySearch(
				(const char*)u8"リジッドボディ RIGID BODY", value.name)))
			continue;
		DrawHierarchyComponent(
			nodeIndex, AttachedComponentType::RigidBody, i, (const char*)u8"リジッドボディ", value.name);
	}
	for (int i = 0; i < static_cast<int>(extension.colliders.size()); ++i)
	{
		const auto& value = extension.colliders[i];
		if (value.nodeIndex != nodeIndex ||
			(!showAllContents && !ComponentMatchesHierarchySearch(
				(const char*)u8"コライダー COLLIDER", value.name)))
			continue;
		DrawHierarchyComponent(
			nodeIndex, AttachedComponentType::Collider, i, (const char*)u8"コライダー", value.name);
	}
	for (int i = 0; i < static_cast<int>(extension.springs.size()); ++i)
	{
		const auto& value = extension.springs[i];
		if (value.nodeIndex != nodeIndex ||
			(!showAllContents && !ComponentMatchesHierarchySearch(
				(const char*)u8"スプリング SPRING", value.name)))
			continue;
		DrawHierarchyComponent(
			nodeIndex, AttachedComponentType::Spring, i, (const char*)u8"スプリング", value.name);
	}
	for (int i = 0; i < static_cast<int>(extension.springColliders.size()); ++i)
	{
		const auto& value = extension.springColliders[i];
		if (value.nodeIndex != nodeIndex ||
			(!showAllContents && !ComponentMatchesHierarchySearch(
				(const char*)u8"スプリングコライダー SPRING COLLIDER", value.name)))
			continue;
		DrawHierarchyComponent(nodeIndex, AttachedComponentType::SpringCollider, i,
			(const char*)u8"スプリングコライダー", value.name);
	}
	const auto& trailComponents = model->GetVmdlTrailData().trails;
	for (int i = 0; i < static_cast<int>(trailComponents.size()); ++i)
	{
		const auto& value = trailComponents[i];
		if (value.nodeIndex != nodeIndex ||
			(!showAllContents && !ComponentMatchesHierarchySearch(
				(const char*)u8"トレイル TRAIL", value.name)))
			continue;
		DrawHierarchyComponent(
			nodeIndex, AttachedComponentType::Trail, i, (const char*)u8"トレイル", value.name);
	}
	for (int i = 0; i < static_cast<int>(particleEmitters.size()); ++i)
	{
		const auto& value = particleEmitters[i];
		if (value.nodeIndex != nodeIndex ||
			(!showAllContents && !ComponentMatchesHierarchySearch(
				(const char*)u8"パーティクル PARTICLE", value.name)))
			continue;
		DrawHierarchyComponent(nodeIndex, AttachedComponentType::Particle, i,
			(const char*)u8"パーティクル", value.name);
	}
	for (int i = 0; i < static_cast<int>(soundSources.size()); ++i)
	{
		const auto& value = soundSources[i];
		if (value.nodeIndex != nodeIndex ||
			(!showAllContents && !ComponentMatchesHierarchySearch(
				(const char*)u8"サウンドソース SOUND SOURCE", value.name)))
			continue;
		DrawHierarchyComponent(nodeIndex, AttachedComponentType::SoundSource, i,
			(const char*)u8"サウンドソース", value.name);
	}
	for (int i = 0; i < static_cast<int>(cameraShakes.size()); ++i)
	{
		const auto& value = cameraShakes[i];
		if (value.nodeIndex != nodeIndex) continue;
		DrawHierarchyComponent(nodeIndex, AttachedComponentType::CameraShake, i,
			(const char*)u8"カメラシェイク", value.name);
	}
	for (int i = 0; i < static_cast<int>(radialBlurs.size()); ++i)
	{
		const auto& value = radialBlurs[i];
		if (value.nodeIndex != nodeIndex) continue;
		DrawHierarchyComponent(nodeIndex, AttachedComponentType::RadialBlur, i,
			(const char*)u8"ラジアルブラー", value.name);
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
		if (selectedNodes.size() > 1)
			ImGui::TextDisabled((const char*)u8"選択中: %zuノード", selectedNodes.size());
		else
			ImGui::TextDisabled(
				(const char*)u8"ノード: %s", MakeNodeLabel(nodeIndex, node.name).c_str());
		ImGui::Separator();
		if (ImGui::BeginMenu((const char*)u8"追加"))
		{
			if (ImGui::BeginMenu((const char*)u8"カメラ"))
			{
				if (ImGui::MenuItem((const char*)u8"カメラシェイク"))
					AddAttachedComponentToSelectedNodes(AttachedComponentType::CameraShake);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu((const char*)u8"ポストエフェクト"))
			{
				if (ImGui::MenuItem((const char*)u8"ラジアルブラー"))
					AddAttachedComponentToSelectedNodes(AttachedComponentType::RadialBlur);
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem((const char*)u8"リジッドボディ"))
				AddAttachedComponentToSelectedNodes(AttachedComponentType::RigidBody);
			if (ImGui::MenuItem((const char*)u8"コライダー"))
				AddAttachedComponentToSelectedNodes(AttachedComponentType::Collider);
			if (ImGui::MenuItem((const char*)u8"スプリング"))
				AddAttachedComponentToSelectedNodes(AttachedComponentType::Spring);
			if (ImGui::MenuItem((const char*)u8"スプリングコライダー"))
				AddAttachedComponentToSelectedNodes(AttachedComponentType::SpringCollider);
			if (ImGui::MenuItem((const char*)u8"トレイル"))
				AddAttachedComponentToSelectedNodes(AttachedComponentType::Trail);
			if (ImGui::MenuItem((const char*)u8"パーティクル"))
				AddAttachedComponentToSelectedNodes(AttachedComponentType::Particle);
			if (ImGui::MenuItem((const char*)u8"サウンドソース"))
				AddAttachedComponentToSelectedNodes(AttachedComponentType::SoundSource);
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();
}

void VmdlEditorScene::DrawViewport()
{
	auto& io = ImGui::GetIO();
	ImGui::TextUnformatted((const char*)u8"3Dビュー");
	const float transportButtonWidth = 42.0f;
	const float transportWidth = transportButtonWidth * 2.0f + ImGui::GetStyle().ItemSpacing.x;
	ImGui::SameLine();
	ImGui::SetCursorPosX(std::max(
		ImGui::GetCursorPosX(), (ImGui::GetWindowWidth() - transportWidth) * 0.5f));
	if (ImGui::Button(animationPlaying ? ICON_FA_PAUSE "##UnifiedPlay" : ICON_FA_PLAY "##UnifiedPlay",
			ImVec2(transportButtonWidth, 0.0f)))
	{
		if (animationPlaying) StopUnifiedPreview(false); else StartUnifiedPreview();
	}
	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_STOP "##UnifiedStop", ImVec2(transportButtonWidth, 0.0f)))
		StopUnifiedPreview(true);
	ImGui::SameLine();
	if (ImGui::SmallButton((const char*)u8"移動")) gizmoOperation = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (ImGui::SmallButton((const char*)u8"回転")) gizmoOperation = ImGuizmo::ROTATE;
	ImGui::SameLine();
	if (ImGui::SmallButton((const char*)u8"拡大縮小")) gizmoOperation = ImGuizmo::SCALE;
	ImGui::Separator();

	ImVec2 available = ImGui::GetContentRegionAvail();
	const float side = std::min(available.x, available.y);
	const ImVec2 imageSize(side, side);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available.x - side) * 0.5f);
	ImGui::Image(previewTarget->GetSRV(), imageSize);
	const ImVec2 imageMin = ImGui::GetItemRectMin();
	const ImVec2 imageMax = ImGui::GetItemRectMax();
	if (viewportRotationDragging)
	{
		const std::string speedText = std::format("移動速度 {:.1f}  ホイールで調整", previewCameraMoveSpeed);
		ImGui::GetWindowDrawList()->AddText(
			ImVec2(imageMin.x + 10.0f, imageMin.y + 10.0f), IM_COL32(255, 255, 255, 220), speedText.c_str());
	}
	const bool imageHovered = ImGui::IsItemHovered();
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
			else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) showRigidBody = !showRigidBody;
			else if (ImGui::IsKeyPressed(ImGuiKey_C, false)) showCollider = !showCollider;
			else if (ImGui::IsKeyPressed(ImGuiKey_S, false)) showSpring = !showSpring;
			else if (ImGui::IsKeyPressed(ImGuiKey_T, false)) showTrail = !showTrail;
			else if (ImGui::IsKeyPressed(ImGuiKey_G, false)) showGrid = !showGrid;
		}
	}

	if (model && selectedNode >= 0 && selectedNode < static_cast<int>(model->GetNodes().size()))
	{
		Matrix view = editorCamera->GetView();
		Matrix projection = editorCamera->GetProjection();
		const Matrix renderScaleTransform = model->GetRenderScaleTransform();
		Matrix world = model->GetNodes()[selectedNode].worldTransform * renderScaleTransform;
		ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
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
	}
	if (imageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		viewportRotationDragging = true;
		SetCapture(Game::Graphics::Instance().GetWindowHandle());
	}
	if (viewportRotationDragging)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			cameraReturningToFront = false;
			cameraYaw -= io.MouseDelta.x * 0.01f;
			cameraPitch = std::clamp(cameraPitch + io.MouseDelta.y * 0.01f, -1.45f, 1.45f);
			if (io.MouseWheel != 0.0f)
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
	if (!imageHovered || ImGuizmo::IsUsing() || ImGuizmo::IsOver()) return;

	if (model && showDebugOverlays && showBones && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
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
		for (int nodeIndex = 0; nodeIndex < static_cast<int>(nodes.size()); ++nodeIndex)
		{
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
	}
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
	{
		float panScale = cameraDistance * 0.0015f;
		if (io.KeyShift) panScale *= 10.0f;
		cameraFocusOffset -= editorCamera->GetRight() * io.MouseDelta.x * panScale;
		cameraFocusOffset += editorCamera->GetUp() * io.MouseDelta.y * panScale;
	}
	if (io.MouseWheel != 0.0f && !viewportRotationDragging)
	{
		if (io.KeyShift) targetCameraDistance *= std::pow(2.0f, -io.MouseWheel);
		else targetCameraDistance -= io.MouseWheel * 0.4f;
		targetCameraDistance =
			std::clamp(targetCameraDistance, PreviewMinCameraDistance, PreviewMaxCameraDistance);
	}
}

void VmdlEditorScene::DrawProperty()
{
	ImGui::TextUnformatted((const char*)u8"プロパティ");
	ImGui::Separator();
	if (!model || selectedNode < 0 || selectedNode >= static_cast<int>(model->GetNodes().size()))
	{
		ImGui::TextDisabled((const char*)u8"ノードまたはメッシュを選択してください");
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
	if (selectedMesh >= 0 && selectedMesh < static_cast<int>(model->GetMeshes().size()))
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
	}
	if (previewShadingMode == PreviewShadingMode::Solid)
		ImGui::ColorEdit4((const char*)u8"ソリッド色", &solidColor.x);

	// 選択ノードに付いているコンポーネント
	ImGui::SeparatorText((const char*)u8"付加設定");
	ImGui::TextDisabled((const char*)u8"階層でノードを右クリックすると付加設定を追加できます");
	DrawAttachedData(selectedNode);
}

void VmdlEditorScene::DrawAttachedData(int nodeIndex)
{
	auto& data = model->GetVmdlExtensionData();
	const bool openSelectedComponent = focusSelectedComponent;
	focusSelectedComponent = false;

	// リジッドボディ
	int deleteRigidBody = -1;
	for (int i = 0; i < static_cast<int>(data.rigidBodies.size()); ++i)
	{
		auto& value = data.rigidBodies[i];
		if (value.nodeIndex != nodeIndex) continue;
		ImGui::PushID(1000 + i);
		const bool selected = selectedComponentType == AttachedComponentType::RigidBody &&
			selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)u8"リジッドボディ",
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |=
				ImGui::DragFloat3((const char*)u8"位置オフセット", &value.offsetPosition.x, 0.01f);
			changed |=
				ImGui::DragFloat3((const char*)u8"回転オフセット", &value.offsetRotation.x, 0.01f);
			changed |= ImGui::DragFloat((const char*)u8"質量", &value.mass, 0.05f, 0.0f);
			changed |= ImGui::Checkbox((const char*)u8"キネマティック", &value.kinematic);
			if (changed) MarkDirty();
			if (ImGui::Button((const char*)u8"削除")) deleteRigidBody = i;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteRigidBody >= 0)
	{
		data.rigidBodies.erase(data.rigidBodies.begin() + deleteRigidBody);
		if (selectedComponentType == AttachedComponentType::RigidBody)
		{
			if (selectedComponentIndex == deleteRigidBody)
			{
				selectedComponentType = AttachedComponentType::None;
				selectedComponentIndex = -1;
			}
			else if (selectedComponentIndex > deleteRigidBody)
				--selectedComponentIndex;
		}
		MarkDirty();
	}

	// コライダー
	int deleteCollider = -1;
	for (int i = 0; i < static_cast<int>(data.colliders.size()); ++i)
	{
		auto& value = data.colliders[i];
		if (value.nodeIndex != nodeIndex) continue;
		ImGui::PushID(2000 + i);
		const bool selected = selectedComponentType == AttachedComponentType::Collider &&
			selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)u8"コライダー",
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			const char* shapes[] = {
				(const char*)u8"ボックス", (const char*)u8"球", (const char*)u8"カプセル"};
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
			if (ImGui::Combo((const char*)u8"形状", &value.shape, shapes, IM_ARRAYSIZE(shapes)))
			{
				if (value.shape == 1) value.size.y = value.size.z = value.size.x;
				changed = true;
			}
			Vector3 scaledCenter = model->GetScaledAttachmentVector(value.center);
			Vector3 scaledSize = model->GetScaledAttachmentVector(value.size);
			if (ImGui::DragFloat3((const char*)u8"位置オフセット", &scaledCenter.x, 0.01f))
			{
				value.center = model->GetUnscaledAttachmentVector(scaledCenter);
				changed = true;
			}
			changed |= ImGui::DragFloat3((const char*)u8"回転オフセット", &value.rotation.x, 0.01f);
			if (value.shape == 1)
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
			if (changed) MarkDirty();
			if (ImGui::Checkbox((const char*)u8"初期状態で有効", &initialActive))
			{
				model->SetColliderInitialActive(i, initialActive);
				if (previewColliderActive.size() <= static_cast<size_t>(i))
					previewColliderActive.resize(i + 1, 1);
				previewColliderActive[i] = initialActive ? 1 : 0;
				MarkDirty();
			}

			if (ImGui::Button((const char*)u8"削除")) deleteCollider = i;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteCollider >= 0)
	{
		data.colliders.erase(data.colliders.begin() + deleteCollider);
		if (selectedComponentType == AttachedComponentType::Collider)
		{
			if (selectedComponentIndex == deleteCollider)
			{
				selectedComponentType = AttachedComponentType::None;
				selectedComponentIndex = -1;
			}
			else if (selectedComponentIndex > deleteCollider)
				--selectedComponentIndex;
		}
		auto& control = model->GetVmdlAnimationControlData();
		if (deleteCollider < static_cast<int>(control.colliderInitialActive.size()))
			control.colliderInitialActive.erase(
				control.colliderInitialActive.begin() + deleteCollider);
		std::erase_if(control.colliderTracks,
			[deleteCollider](const auto& track) { return track.colliderIndex == deleteCollider; });
		for (auto& track : control.colliderTracks)
		{
			if (track.colliderIndex > deleteCollider) --track.colliderIndex;
		}
		if (deleteCollider < static_cast<int>(previewColliderActive.size()))
			previewColliderActive.erase(previewColliderActive.begin() + deleteCollider);
		selectedColliderEventTarget = data.colliders.empty()
										  ? 0
										  : std::min(selectedColliderEventTarget,
												static_cast<int>(data.colliders.size()) - 1);
		if (timelineEventContextKind == 0) timelineEventContextKind = -1;
		MarkDirty();
	}

	// スプリング
	int deleteSpring = -1;
	for (int i = 0; i < static_cast<int>(data.springs.size()); ++i)
	{
		auto& value = data.springs[i];
		if (value.nodeIndex != nodeIndex) continue;
		ImGui::PushID(3000 + i);
		const bool selected = selectedComponentType == AttachedComponentType::Spring &&
			selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)u8"スプリング",
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |=
				ImGui::DragFloat3((const char*)u8"位置オフセット", &value.offsetPosition.x, 0.01f);
			changed |=
				ImGui::DragFloat3((const char*)u8"回転オフセット", &value.offsetRotation.x, 0.01f);
			changed |= ImGui::DragFloat((const char*)u8"硬さ", &value.stiffness, 0.01f, 0.0f, 1.0f);
			changed |= ImGui::DragFloat((const char*)u8"抵抗", &value.drag, 0.01f, 0.0f, 1.0f);
			if (changed) MarkDirty();

			if (ImGui::Button((const char*)u8"削除")) deleteSpring = i;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteSpring >= 0)
	{
		data.springs.erase(data.springs.begin() + deleteSpring);
		if (selectedComponentType == AttachedComponentType::Spring)
		{
			if (selectedComponentIndex == deleteSpring)
			{
				selectedComponentType = AttachedComponentType::None;
				selectedComponentIndex = -1;
			}
			else if (selectedComponentIndex > deleteSpring)
				--selectedComponentIndex;
		}
		MarkDirty();
	}

	// スプリングコライダー
	int deleteSpringCollider = -1;
	for (int i = 0; i < static_cast<int>(data.springColliders.size()); ++i)
	{
		auto& value = data.springColliders[i];
		if (value.nodeIndex != nodeIndex) continue;
		ImGui::PushID(4000 + i);
		const bool selected = selectedComponentType == AttachedComponentType::SpringCollider &&
			selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)u8"スプリングコライダー",
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |=
				ImGui::DragFloat3((const char*)u8"位置オフセット", &value.offsetPosition.x, 0.01f);
			changed |= ImGui::DragFloat((const char*)u8"半径", &value.radius, 0.01f, 0.001f);
			if (changed) MarkDirty();

			if (ImGui::Button((const char*)u8"削除")) deleteSpringCollider = i;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteSpringCollider >= 0)
	{
		data.springColliders.erase(data.springColliders.begin() + deleteSpringCollider);
		if (selectedComponentType == AttachedComponentType::SpringCollider)
		{
			if (selectedComponentIndex == deleteSpringCollider)
			{
				selectedComponentType = AttachedComponentType::None;
				selectedComponentIndex = -1;
			}
			else if (selectedComponentIndex > deleteSpringCollider)
				--selectedComponentIndex;
		}
		MarkDirty();
	}

	// トレイル
	auto& trailData = model->GetVmdlTrailData();
	int deleteTrail = -1;
	for (int i = 0; i < static_cast<int>(trailData.trails.size()); ++i)
	{
		auto& value = trailData.trails[i];
		if (value.nodeIndex != nodeIndex) continue;
		ImGui::PushID(5000 + i);
		const bool selected = selectedComponentType == AttachedComponentType::Trail &&
			selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)u8"トレイル",
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool initialActive = model->GetTrailInitialActive(i);
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
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
				if (previewTrailActive.size() <= static_cast<size_t>(i))
					previewTrailActive.resize(i + 1, 1);
				previewTrailActive[i] = initialActive ? 1 : 0;
				MarkDirty();
			}
			if (ImGui::Button((const char*)u8"削除")) deleteTrail = i;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteTrail >= 0)
	{
		trailData.trails.erase(trailData.trails.begin() + deleteTrail);
		if (selectedComponentType == AttachedComponentType::Trail)
		{
			if (selectedComponentIndex == deleteTrail)
			{
				selectedComponentType = AttachedComponentType::None;
				selectedComponentIndex = -1;
			}
			else if (selectedComponentIndex > deleteTrail)
				--selectedComponentIndex;
		}
		if (deleteTrail < static_cast<int>(trailData.initialActive.size()))
			trailData.initialActive.erase(trailData.initialActive.begin() + deleteTrail);
		std::erase_if(trailData.tracks,
			[deleteTrail](const auto& track) { return track.trailIndex == deleteTrail; });
		for (auto& track : trailData.tracks)
		{
			if (track.trailIndex > deleteTrail) --track.trailIndex;
		}
		if (deleteTrail < static_cast<int>(previewTrailActive.size()))
			previewTrailActive.erase(previewTrailActive.begin() + deleteTrail);
		selectedTrailEventTarget =
			trailData.trails.empty()
				? 0
				: std::min(selectedTrailEventTarget, static_cast<int>(trailData.trails.size()) - 1);
		if (timelineEventContextKind == 2) timelineEventContextKind = -1;
		MarkDirty();
	}

	// パーティクルエミッタ
	auto& particleData = model->GetVmdlParticleData();
	int deleteParticle = -1;
	for (int i = 0; i < static_cast<int>(particleData.emitters.size()); ++i)
	{
		auto& value = particleData.emitters[i];
		if (value.nodeIndex != nodeIndex) continue;
		ImGui::PushID(5500 + i);
		const bool selected = selectedComponentType == AttachedComponentType::Particle &&
			selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)u8"パーティクル",
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			const char* rendererNames[] = {"Sprite", "Ribbon"};
			changed |= ImGui::Combo((const char*)u8"描画タイプ", &value.rendererType,
				rendererNames, static_cast<int>(std::size(rendererNames)));

			ImGui::SeparatorText((const char*)u8"プリセット");
			if (ImGui::Button((const char*)u8"走行風"))
			{
				value.rendererType = 0;
				value.texturePath = "Resources/Image/fog_particle.png";
				value.columns = value.rows = 1; value.frame = 0; value.animated = false;
				value.emissionRate = 20.0f; value.burstCount = 0;
				value.spawnExtents = Vector3(0.35f, 0.15f, 0.2f);
				value.velocityMin = Vector3(-0.15f, 0.1f, -2.5f);
				value.velocityMax = Vector3(0.15f, 0.45f, -1.5f);
				value.acceleration = Vector3::Zero; value.lifetimeMin = 0.18f; value.lifetimeMax = 0.35f;
				value.sizeMin = Vector2(0.08f, 0.3f); value.sizeMax = Vector2(0.18f, 0.7f);
				value.color = Color(0.7f, 0.9f, 1.0f, 1.0f); value.fadeOutDuration = 0.12f;
				value.localVelocity = true; value.additive = false; changed = true;
			}
			ImGui::SameLine();
			if (ImGui::Button((const char*)u8"剣の火花"))
			{
				value.rendererType = 0;
				value.texturePath = "Resources/Image/particle256x256.png";
				value.columns = value.rows = 4; value.frame = 7; value.animated = false;
				value.emissionRate = 0.0f; value.burstCount = 24;
				value.spawnExtents = Vector3(0.05f, 0.05f, 0.05f);
				value.velocityMin = Vector3(-2.0f, -0.4f, -2.0f);
				value.velocityMax = Vector3(2.0f, 2.5f, 2.0f);
				value.acceleration = Vector3(0.0f, -5.0f, 0.0f);
				value.lifetimeMin = 0.12f; value.lifetimeMax = 0.35f;
				value.sizeMin = Vector2(0.04f, 0.04f); value.sizeMax = Vector2(0.12f, 0.12f);
				value.color = Color(1.0f, 0.65f, 0.15f, 1.0f); value.fadeOutDuration = 0.15f;
				value.localVelocity = true; value.additive = true; changed = true;
			}
			ImGui::SameLine();
			if (ImGui::Button((const char*)u8"オーラ"))
			{
				value.rendererType = 0;
				value.texturePath = "Resources/Image/particle256x256.png";
				value.columns = value.rows = 4; value.frame = 10; value.animated = true;
				value.emissionRate = 28.0f; value.burstCount = 8;
				value.spawnExtents = Vector3(0.45f, 0.8f, 0.45f);
				value.velocityMin = Vector3(-0.1f, 0.4f, -0.1f);
				value.velocityMax = Vector3(0.1f, 1.2f, 0.1f);
				value.acceleration = Vector3::Zero; value.lifetimeMin = 0.35f; value.lifetimeMax = 0.7f;
				value.sizeMin = Vector2(0.12f, 0.12f); value.sizeMax = Vector2(0.35f, 0.35f);
				value.color = Color(0.25f, 0.65f, 1.0f, 0.75f); value.fadeOutDuration = 0.25f;
				value.localVelocity = true; value.additive = true; changed = true;
			}
			if (ImGui::Button((const char*)u8"剣リボン"))
			{
				value.rendererType = 1;
				value.emissionRate = 0.0f; value.burstCount = 0;
				value.ribbonRootOffset = Vector3::Zero;
				value.ribbonTipOffset = Vector3(-1.0f, 0.0f, 0.0f);
				value.ribbonLifetime = 0.16f; value.ribbonMaxPoints = 36;
				value.ribbonTipRatio = 0.8f; value.ribbonSampleInterval = 0.008f;
				value.color = Color(0.75f, 0.95f, 1.4f, 1.0f);
				value.ribbonEndColor = Color(0.08f, 0.25f, 1.0f, 0.0f);
				value.additive = true; changed = true;
			}

			if (value.rendererType == 1)
			{
				ImGui::SeparatorText((const char*)u8"Ribbon形状");
				ImGui::TextDisabled((const char*)u8"剣ボーン基準の根元と先端を、過去フレームへ帯状に接続します");
				changed |= ImGui::DragFloat3((const char*)u8"リボン根元", &value.ribbonRootOffset.x, 0.01f);
				changed |= ImGui::DragFloat3((const char*)u8"リボン先端", &value.ribbonTipOffset.x, 0.01f);
				changed |= ImGui::DragFloat((const char*)u8"残存時間", &value.ribbonLifetime,
					0.005f, 0.01f, 5.0f, "%.3f sec");
				changed |= ImGui::DragInt((const char*)u8"最大サンプル数", &value.ribbonMaxPoints, 1, 2, 1024);
				changed |= ImGui::DragFloat((const char*)u8"先細り", &value.ribbonTipRatio, 0.01f, 0.0f, 4.0f);
				changed |= ImGui::DragFloat((const char*)u8"サンプル間隔", &value.ribbonSampleInterval,
					0.001f, 0.001f, 0.1f, "%.3f sec");
				changed |= ImGui::ColorEdit4((const char*)u8"終端色", &value.ribbonEndColor.x,
					ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
			}

			ImGui::SeparatorText((const char*)u8"描画素材");
			changed |= ImGui::InputText((const char*)u8"テクスチャ", &value.texturePath);
			ImGui::SameLine();
			if (ImGui::Button((const char*)u8"参照..."))
			{
				std::string path = value.texturePath;
				if (Dialog::OpenFileName(path,
					"Image (*.png;*.dds;*.tga)\0*.png;*.dds;*.tga\0", "Particle Texture") == DialogResult::OK)
				{
					value.texturePath = PortableResourcePath(path);
					changed = true;
				}
			}
			changed |= ImGui::DragInt((const char*)u8"横コマ", &value.columns, 1, 1, 32);
			changed |= ImGui::DragInt((const char*)u8"縦コマ", &value.rows, 1, 1, 32);
			changed |= ImGui::DragInt((const char*)u8"開始コマ", &value.frame, 1, 0,
				std::max(0, value.columns * value.rows - 1));
			changed |= ImGui::Checkbox((const char*)u8"スプライトアニメ", &value.animated);
			if (value.animated)
				changed |= ImGui::DragFloat((const char*)u8"アニメ速度", &value.animationSpeed, 0.5f, 0.0f, 120.0f);
			changed |= ImGui::Checkbox((const char*)u8"加算合成", &value.additive);

			ImGui::SeparatorText((const char*)u8"発生");
			changed |= ImGui::DragFloat3((const char*)u8"位置オフセット", &value.offset.x, 0.01f);
			changed |= ImGui::DragFloat3((const char*)u8"発生範囲", &value.spawnExtents.x, 0.01f, 0.0f, 100.0f);
			changed |= ImGui::DragFloat((const char*)u8"毎秒発生数", &value.emissionRate, 0.5f, 0.0f, 2000.0f);
			changed |= ImGui::DragInt((const char*)u8"開始バースト数", &value.burstCount, 1, 0, 8192);
			changed |= ImGui::DragInt((const char*)u8"最大保持数", &value.capacity, 1, 1, 8192);
			bool initialActive = model->GetParticleInitialActive(i);
			if (ImGui::Checkbox((const char*)u8"初期状態で発生", &initialActive))
			{
				model->SetParticleInitialActive(i, initialActive); changed = true;
			}

			ImGui::SeparatorText((const char*)u8"動き");
			changed |= ImGui::DragFloat3((const char*)u8"初速 最小", &value.velocityMin.x, 0.05f);
			changed |= ImGui::DragFloat3((const char*)u8"初速 最大", &value.velocityMax.x, 0.05f);
			changed |= ImGui::DragFloat3((const char*)u8"加速度／重力", &value.acceleration.x, 0.05f);
			changed |= ImGui::Checkbox((const char*)u8"ボーンの向きを使用", &value.localVelocity);
			changed |= ImGui::DragFloatRange2((const char*)u8"寿命", &value.lifetimeMin,
				&value.lifetimeMax, 0.01f, 0.01f, 30.0f, "%.2f", "%.2f");

			ImGui::SeparatorText((const char*)u8"見た目");
			changed |= ImGui::DragFloat2((const char*)u8"サイズ 最小", &value.sizeMin.x, 0.01f, 0.001f, 100.0f);
			changed |= ImGui::DragFloat2((const char*)u8"サイズ 最大", &value.sizeMax.x, 0.01f, 0.001f, 100.0f);
			changed |= ImGui::ColorEdit4((const char*)u8"色", &value.color.x,
				ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
			changed |= ImGui::SliderFloat((const char*)u8"不透明度", &value.color.w, 0.0f, 1.0f);
			changed |= ImGui::DragFloat((const char*)u8"フェードイン", &value.fadeInDuration, 0.01f, 0.0f, 30.0f);
			changed |= ImGui::DragFloat((const char*)u8"フェードアウト", &value.fadeOutDuration, 0.01f, 0.0f, 30.0f);

			if (ImGui::Button((const char*)u8"VFX書き出し...")) ExportParticlePrefab(i);
			ImGui::SameLine();
			if (ImGui::Button((const char*)u8"VFX読込...")) { ImportParticlePrefab(i); changed = true; }
			if (ImGui::Button((const char*)u8"削除")) deleteParticle = i;

			value.columns = std::clamp(value.columns, 1, 32);
			value.rows = std::clamp(value.rows, 1, 32);
			value.capacity = std::clamp(value.capacity, 1, 8192);
			value.ribbonLifetime = std::clamp(value.ribbonLifetime, 0.01f, 5.0f);
			value.ribbonMaxPoints = std::clamp(value.ribbonMaxPoints, 2, 1024);
			value.ribbonTipRatio = std::clamp(value.ribbonTipRatio, 0.0f, 4.0f);
			value.ribbonSampleInterval = std::clamp(
				value.ribbonSampleInterval, 0.001f, 1.0f);
			value.spawnExtents.x = std::max(0.0f, value.spawnExtents.x);
			value.spawnExtents.y = std::max(0.0f, value.spawnExtents.y);
			value.spawnExtents.z = std::max(0.0f, value.spawnExtents.z);
			if (changed) { MarkDirty(); RebuildParticlePreview(); }
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteParticle >= 0)
	{
		particleData.emitters.erase(particleData.emitters.begin() + deleteParticle);
		if (deleteParticle < static_cast<int>(particleData.initialActive.size()))
			particleData.initialActive.erase(particleData.initialActive.begin() + deleteParticle);
		std::erase_if(particleData.tracks,
			[deleteParticle](const auto& track) { return track.emitterIndex == deleteParticle; });
		for (auto& track : particleData.tracks)
			if (track.emitterIndex > deleteParticle) --track.emitterIndex;
		if (selectedComponentType == AttachedComponentType::Particle)
		{
			if (selectedComponentIndex == deleteParticle) { selectedComponentType = AttachedComponentType::None; selectedComponentIndex = -1; }
			else if (selectedComponentIndex > deleteParticle) --selectedComponentIndex;
		}
		selectedParticleEventTarget = particleData.emitters.empty() ? 0 :
			std::min(selectedParticleEventTarget, static_cast<int>(particleData.emitters.size()) - 1);
		MarkDirty(); RebuildParticlePreview();
	}

	// サウンドソース
	auto& soundData = model->GetVmdlSoundData();
	int deleteSoundSource = -1;
	for (int i = 0; i < static_cast<int>(soundData.sources.size()); ++i)
	{
		auto& value = soundData.sources[i];
		if (value.nodeIndex != nodeIndex) continue;
		ImGui::PushID(6000 + i);
		const bool selected = selectedComponentType == AttachedComponentType::SoundSource &&
			selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)u8"サウンドソース",
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			if (DrawSoundTrackSelector((const char*)u8"サウンド", value.track)) changed = true;
			bool randomVariant = value.variant < 0;
			if (ImGui::Checkbox((const char*)u8"音源番号をランダム選択", &randomVariant))
			{
				value.variant = randomVariant ? -1 : 0;
				changed = true;
			}
			if (!randomVariant)
				changed |= ImGui::DragInt(
					(const char*)u8"音源番号", &value.variant, 1.0f, 0, 10000);
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
			value.track = std::clamp(value.track, 0, SoundTrackRegistry::MaximumTrack);
			value.pitchMin = std::clamp(value.pitchMin, 0.125f, 8.0f);
			value.pitchMax = std::clamp(value.pitchMax, value.pitchMin, 8.0f);
			value.minDistance = std::max(0.01f, value.minDistance);
			value.maxDistance = std::max(value.minDistance + 0.01f, value.maxDistance);
			value.lowPassHz = std::clamp(value.lowPassHz, 20.0f, 20000.0f);
			value.farLowPassHz = std::clamp(value.farLowPassHz, 20.0f, 20000.0f);
			value.reverbMix = std::clamp(value.reverbMix, 0.0f, 1.0f);
			if (changed) MarkDirty();

			if (ImGui::Button((const char*)u8"削除")) deleteSoundSource = i;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteSoundSource >= 0)
	{
		soundData.sources.erase(soundData.sources.begin() + deleteSoundSource);
		for (auto& track : soundData.tracks)
		{
			std::erase_if(track.keys, [deleteSoundSource](const auto& key)
				{ return key.sourceIndex == deleteSoundSource; });
			for (auto& key : track.keys)
				if (key.sourceIndex > deleteSoundSource) --key.sourceIndex;
		}
		std::erase_if(soundData.tracks,
			[](const auto& track) { return track.keys.empty(); });
		if (selectedComponentType == AttachedComponentType::SoundSource)
		{
			if (selectedComponentIndex == deleteSoundSource)
			{
				selectedComponentType = AttachedComponentType::None;
				selectedComponentIndex = -1;
			}
			else if (selectedComponentIndex > deleteSoundSource)
				--selectedComponentIndex;
		}
		MarkDirty();
	}

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
	for (int i = 0; i < static_cast<int>(presentation.cameraShakes.size()); ++i)
	{
		auto& value = presentation.cameraShakes[i];
		if (value.nodeIndex != nodeIndex) continue;
		ImGui::PushID(7000 + i);
		const bool selected = selectedComponentType == AttachedComponentType::CameraShake && selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)u8"カメラシェイク", selected ? ImGuiTreeNodeFlags_Selected : 0))
		{
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= ImGui::DragFloat((const char*)u8"効果範囲", &value.range, 0.1f, 0.01f, 10000.0f);
			const bool oldMode = value.distanceAttenuation; drawDistanceMode(value.distanceAttenuation);
			changed |= oldMode != value.distanceAttenuation;
			changed |= ImGui::DragFloat((const char*)u8"継続時間", &value.duration, 0.05f, 0.01f, 30.0f);
			changed |= ImGui::DragFloat((const char*)u8"揺れの強さ", &value.intensity, 0.005f, 0.0f, 10.0f);
			value.range = std::max(0.01f, value.range); value.duration = std::max(0.01f, value.duration);
			value.intensity = std::max(0.0f, value.intensity);
			if (changed) MarkDirty();
			if (ImGui::Button((const char*)u8"削除")) deleteCameraShake = i;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteCameraShake >= 0)
	{
		presentation.cameraShakes.erase(presentation.cameraShakes.begin() + deleteCameraShake);
		for (auto& track : presentation.cameraShakeTracks)
		{
			std::erase_if(track.keys, [deleteCameraShake](const auto& key) {
				return key.componentIndex == deleteCameraShake;
			});
			for (auto& key : track.keys) if (key.componentIndex > deleteCameraShake) --key.componentIndex;
		}
		std::erase_if(presentation.cameraShakeTracks,
			[](const auto& track) { return track.keys.empty(); });
		if (selectedComponentType == AttachedComponentType::CameraShake) { selectedComponentType = AttachedComponentType::None; selectedComponentIndex = -1; }
		MarkDirty();
	}

	int deleteRadialBlur = -1;
	for (int i = 0; i < static_cast<int>(presentation.radialBlurs.size()); ++i)
	{
		auto& value = presentation.radialBlurs[i];
		if (value.nodeIndex != nodeIndex) continue;
		ImGui::PushID(8000 + i);
		const bool selected = selectedComponentType == AttachedComponentType::RadialBlur && selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx((const char*)u8"ラジアルブラー", selected ? ImGuiTreeNodeFlags_Selected : 0))
		{
			bool changed = ImGui::InputText((const char*)u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= ImGui::DragFloat((const char*)u8"効果範囲", &value.range, 0.1f, 0.01f, 10000.0f);
			const bool oldMode = value.distanceAttenuation; drawDistanceMode(value.distanceAttenuation);
			changed |= oldMode != value.distanceAttenuation;
			changed |= ImGui::DragFloat((const char*)u8"継続時間", &value.duration, 0.05f, 0.01f, 30.0f);
			changed |= ImGui::DragFloat((const char*)u8"ブラー強度", &value.power, 0.05f, 0.0f, 20.0f);
			changed |= ImGui::SliderFloat((const char*)u8"立ち上がり割合", &value.attackRate, 0.01f, 0.95f);
			value.range = std::max(0.01f, value.range); value.duration = std::max(0.01f, value.duration);
			value.power = std::max(0.0f, value.power);
			if (changed) MarkDirty();
			if (ImGui::Button((const char*)u8"削除")) deleteRadialBlur = i;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteRadialBlur >= 0)
	{
		presentation.radialBlurs.erase(presentation.radialBlurs.begin() + deleteRadialBlur);
		for (auto& track : presentation.radialBlurTracks)
		{
			std::erase_if(track.keys, [deleteRadialBlur](const auto& key) {
				return key.componentIndex == deleteRadialBlur;
			});
			for (auto& key : track.keys) if (key.componentIndex > deleteRadialBlur) --key.componentIndex;
		}
		std::erase_if(presentation.radialBlurTracks,
			[](const auto& track) { return track.keys.empty(); });
		if (selectedComponentType == AttachedComponentType::RadialBlur) { selectedComponentType = AttachedComponentType::None; selectedComponentIndex = -1; }
		MarkDirty();
	}
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
		selectedAnimation >= 0 && selectedAnimation < static_cast<int>(animations.size())
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
		for (int i = 0; i < static_cast<int>(animations.size()); ++i)
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
		selectedAnimation >= 0 && selectedAnimation < static_cast<int>(animations.size());
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
					std::erase_if(
						editorData.footWeightTracks, [&footWeightPrefix](const auto& track) {
							return track.animationName.starts_with(footWeightPrefix);
						});
					auto& controlData = model->GetVmdlAnimationControlData();
					std::erase_if(controlData.colliderTracks, [&deletedName](const auto& track) {
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
						: std::min(selectedAnimation, static_cast<int>(animations.size()) - 1);
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
	if (selectedAnimation < 0 || selectedAnimation >= static_cast<int>(animations.size())) return;

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
	if (ImGui::Button(ICON_FA_STOP "##Stop", ImVec2(38.0f, 0.0f)))
	{
		StopUnifiedPreview(true);
	}
	ImGui::SameLine();
	ImGui::Checkbox((const char*)u8"ループ", &animationLoop);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(110.0f);
	ImGui::DragFloat((const char*)u8"速度", &playbackSpeed, 0.05f, 0.05f, 4.0f, "x %.2f");
	ImGui::SameLine();
	ImGui::Text("%.3f / %.3f sec", animationTime, animation.secondsLength);

	// 選択ノードのキーとカーブ
	if (selectedNode >= 0 && selectedNode < static_cast<int>(animation.nodeAnims.size()))
	{
		DrawAnimationCurves();
	}
}

void VmdlEditorScene::DrawAnimationCurves()
{
	auto& io = ImGui::GetIO();
	if (!model || selectedAnimation < 0 || selectedNode < 0) return;
	auto& animation = model->GetAnimations()[selectedAnimation];
	if (selectedNode >= static_cast<int>(animation.nodeAnims.size())) return;
	auto& keys = animation.nodeAnims[selectedNode];
	const float length = std::max(0.001f, animation.secondsLength);
	const auto& ikSettings = model->GetVmdlIKSettings();
	const int footWeightCount = ikSettings.type == 0
									? 0
									: static_cast<int>(std::min(ikSettings.legs.size(),
										  VMDLModel::VmdlIKSettings::MaxLegCount));
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
	addRows((const char*)u8"位置", 0, positionTrackExpanded, 3);
	addRows((const char*)u8"回転", 1, rotationTrackExpanded, 4);
	addRows((const char*)u8"スケール", 2, scaleTrackExpanded, 3);
	const auto hasSelectedKey = [&]() {
		if (selectedKeyTrack == 0)
			return selectedKeyIndex >= 0 &&
				   selectedKeyIndex < static_cast<int>(keys.positionKeyframes.size());
		if (selectedKeyTrack == 1)
			return selectedKeyIndex >= 0 &&
				   selectedKeyIndex < static_cast<int>(keys.rotationKeyframes.size());
		if (selectedKeyTrack == 2)
			return selectedKeyIndex >= 0 &&
				   selectedKeyIndex < static_cast<int>(keys.scaleKeyframes.size());
		return false;
	};
	const auto deleteSelectedKey = [&]() {
		if (!hasSelectedKey()) return;
		if (selectedKeyTrack == 0)
			keys.positionKeyframes.erase(keys.positionKeyframes.begin() + selectedKeyIndex);
		else if (selectedKeyTrack == 1)
			keys.rotationKeyframes.erase(keys.rotationKeyframes.begin() + selectedKeyIndex);
		else keys.scaleKeyframes.erase(keys.scaleKeyframes.begin() + selectedKeyIndex);
		selectedKeyTrack = -1;
		selectedKeyIndex = -1;
		draggingAnimationKey = false;
		draggingNumericTrack = -1;
		draggingNumericComponent = -1;
		MarkDirty();
		ApplyAnimationPreview();
	};
	struct KeyHit
	{
		ImVec2 position;
		int track;
		int index;
	};
	std::vector<KeyHit> hitKeys;

	ImGui::SeparatorText((const char*)u8"ドープシート");
	ImGui::BeginDisabled(!hasSelectedKey());
	if (ImGui::SmallButton((const char*)u8"選択キーを削除")) deleteSelectedKey();
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (hasSelectedKey() && !io.WantTextInput &&
		ImGui::IsKeyPressed(ImGuiKey_Delete, false))
		deleteSelectedKey();
	const float rulerHeight = 26.0f;
	const float footWeightHeight = 20.0f;
	const float rowHeight = 20.0f;
	const float labelWidth = 250.0f;
	const float footWeightsHeight = footWeightHeight * static_cast<float>(footWeightCount);
	const float rowsTopOffset = rulerHeight + footWeightsHeight;
	const int colliderRowCount = static_cast<int>(model->GetVmdlExtensionData().colliders.size());
	const int trailRowCount = static_cast<int>(model->GetVmdlTrailData().trails.size());
	const int particleRowCount = static_cast<int>(model->GetVmdlParticleData().emitters.size());
	const int morphRowCount = static_cast<int>(model->GetVmdlExtensionData().morphs.size());
	const int soundRowCount = static_cast<int>(model->GetVmdlSoundData().sources.size());
	const int cameraShakeRowCount =
		static_cast<int>(model->GetVmdlPresentationData().cameraShakes.size());
	const int radialBlurRowCount =
		static_cast<int>(model->GetVmdlPresentationData().radialBlurs.size());
	const int eventRowCount = colliderRowCount + trailRowCount + particleRowCount + morphRowCount +
		soundRowCount + cameraShakeRowCount + radialBlurRowCount;
	const float eventRowsTopOffset = rowsTopOffset + rowHeight * static_cast<float>(rows.size());
	const float sheetHeight = eventRowsTopOffset + rowHeight * static_cast<float>(eventRowCount);
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
		const float t = scaled - static_cast<float>(index);
		ImVec4 color(std::lerp(stops[index].x, stops[index + 1].x, t),
			std::lerp(stops[index].y, stops[index + 1].y, t),
			std::lerp(stops[index].z, stops[index + 1].z, t), 1.0f);
		return ImGui::ColorConvertFloat4ToU32(color);
	};
	const float weightTop = sheetMin.y + rulerHeight;
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
			const float sampleWidth = timeWidth / static_cast<float>(track->weights.size());
			for (int i = 0; i < static_cast<int>(track->weights.size()); ++i)
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
				selectedKeyIndex < static_cast<int>(keys.positionKeyframes.size()))
				return (&keys.positionKeyframes[selectedKeyIndex].value.x)[component];
			return (&model->GetNodes()[selectedNode].position.x)[component];
		}
		if (track == 1)
		{
			if (selectedKeyTrack == track && selectedKeyIndex >= 0 &&
				selectedKeyIndex < static_cast<int>(keys.rotationKeyframes.size()))
				return (&keys.rotationKeyframes[selectedKeyIndex].value.x)[component];
			return (&model->GetNodes()[selectedNode].rotation.x)[component];
		}
		if (selectedKeyTrack == track && selectedKeyIndex >= 0 &&
			selectedKeyIndex < static_cast<int>(keys.scaleKeyframes.size()))
			return (&keys.scaleKeyframes[selectedKeyIndex].value.x)[component];
		return (&model->GetNodes()[selectedNode].scale.x)[component];
	};
	for (int row = 0; row < static_cast<int>(rows.size()); ++row)
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
			for (int keyIndex = 0; keyIndex < static_cast<int>(trackKeys.size()); ++keyIndex)
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
		else
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
	}

	const float playheadX = timeToX(animationTime);
	drawList->AddLine(ImVec2(playheadX, sheetMin.y), ImVec2(playheadX, sheetMax.y),
		IM_COL32(90, 190, 255, 255), 2.0f);
	const ImVec2 playheadTriangle[] = {ImVec2(playheadX - 5.0f, sheetMin.y),
		ImVec2(playheadX + 5.0f, sheetMin.y), ImVec2(playheadX, sheetMin.y + 7.0f)};
	drawList->AddConvexPolyFilled(playheadTriangle, 3, IM_COL32(90, 190, 255, 255));

	if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		const ImVec2 mouse = io.MousePos;
		if (mouse.y >= sheetMin.y + eventRowsTopOffset && mouse.y < sheetMax.y)
		{
			const int eventRow = std::clamp(
				static_cast<int>((mouse.y - sheetMin.y - eventRowsTopOffset) / rowHeight), 0,
				eventRowCount - 1);
			if (eventRow < colliderRowCount)
			{
				timelineEventContextKind = 0;
				timelineEventContextTarget = eventRow;
			}
			else if (eventRow < colliderRowCount + trailRowCount)
			{
				timelineEventContextKind = 2;
				timelineEventContextTarget = eventRow - colliderRowCount;
			}
			else if (eventRow < colliderRowCount + trailRowCount + particleRowCount)
			{
				timelineEventContextKind = 4;
				timelineEventContextTarget = eventRow - colliderRowCount - trailRowCount;
			}
			else if (eventRow < colliderRowCount + trailRowCount + particleRowCount + morphRowCount)
			{
				timelineEventContextKind = 1;
				timelineEventContextTarget =
					eventRow - colliderRowCount - trailRowCount - particleRowCount;
			}
			else if (eventRow < colliderRowCount + trailRowCount + particleRowCount + morphRowCount + soundRowCount)
			{
				timelineEventContextKind = 3;
				timelineEventContextTarget =
					eventRow - colliderRowCount - trailRowCount - particleRowCount - morphRowCount;
			}
			else if (eventRow < colliderRowCount + trailRowCount + particleRowCount + morphRowCount +
				soundRowCount + cameraShakeRowCount)
			{
				timelineEventContextKind = 5;
				timelineEventContextTarget = eventRow - colliderRowCount - trailRowCount -
					particleRowCount - morphRowCount - soundRowCount;
			}
			else
			{
				timelineEventContextKind = 6;
				timelineEventContextTarget = eventRow - colliderRowCount - trailRowCount -
					particleRowCount - morphRowCount - soundRowCount - cameraShakeRowCount;
			}
			timelineEventContextTime = xToTime(mouse.x);
			timelineEventContextKey = -1;
			float closest = 9.0f;
			if (timelineEventContextKind == 0)
			{
				for (const auto& track : controlData.colliderTracks)
				{
					if (track.animationName != animation.name ||
						track.colliderIndex != timelineEventContextTarget)
						continue;
					for (int i = 0; i < static_cast<int>(track.keys.size()); ++i)
					{
						const float distance = std::abs(mouse.x - timeToX(track.keys[i].seconds));
						if (distance >= closest) continue;
						closest = distance;
						timelineEventContextKey = i;
					}
					break;
				}
			}
			else if (timelineEventContextKind == 1)
			{
				for (const auto& track : controlData.morphTracks)
				{
					if (track.animationName != animation.name) continue;
					for (int i = 0; i < static_cast<int>(track.keys.size()); ++i)
					{
						if (track.keys[i].morphIndex != timelineEventContextTarget) continue;
						const float distance = std::abs(mouse.x - timeToX(track.keys[i].seconds));
						if (distance >= closest) continue;
						closest = distance;
						timelineEventContextKey = i;
					}
					break;
				}
			}
			else if (timelineEventContextKind == 2)
			{
				for (const auto& track : model->GetVmdlTrailData().tracks)
				{
					if (track.animationName != animation.name ||
						track.trailIndex != timelineEventContextTarget)
						continue;
					for (int i = 0; i < static_cast<int>(track.keys.size()); ++i)
					{
						const float distance = std::abs(mouse.x - timeToX(track.keys[i].seconds));
						if (distance >= closest) continue;
						closest = distance;
						timelineEventContextKey = i;
					}
					break;
				}
			}
			else if (timelineEventContextKind == 3)
			{
				for (const auto& track : soundData.tracks)
				{
					if (track.animationName != animation.name) continue;
					for (int i = 0; i < static_cast<int>(track.keys.size()); ++i)
					{
						if (track.keys[i].sourceIndex != timelineEventContextTarget) continue;
						const float distance = std::abs(mouse.x - timeToX(track.keys[i].seconds));
						if (distance >= closest) continue;
						closest = distance;
						timelineEventContextKey = i;
					}
					break;
				}
			}
			else if (timelineEventContextKind == 4)
			{
				for (const auto& track : particleData.tracks)
				{
					if (track.animationName != animation.name ||
						track.emitterIndex != timelineEventContextTarget) continue;
					for (int i = 0; i < static_cast<int>(track.keys.size()); ++i)
					{
						const float distance = std::abs(mouse.x - timeToX(track.keys[i].seconds));
						if (distance < closest) { closest = distance; timelineEventContextKey = i; }
					}
					break;
				}
			}
			else if (timelineEventContextKind == 5 || timelineEventContextKind == 6)
			{
				const auto& tracks = timelineEventContextKind == 5
					? presentation.cameraShakeTracks : presentation.radialBlurTracks;
				for (const auto& track : tracks)
				{
					if (track.animationName != animation.name) continue;
					for (int i = 0; i < static_cast<int>(track.keys.size()); ++i)
					{
						if (track.keys[i].componentIndex != timelineEventContextTarget) continue;
						const float distance = std::abs(mouse.x - timeToX(track.keys[i].seconds));
						if (distance < closest) { closest = distance; timelineEventContextKey = i; }
					}
					break;
				}
			}
			ImGui::OpenPopup((const char*)u8"タイムラインイベントキー");
		}
	}
	if (ImGui::BeginPopup((const char*)u8"タイムラインイベントキー"))
	{
		const auto byTime = [](const auto& left, const auto& right) {
			return left.seconds < right.seconds;
		};
		if (timelineEventContextKind == 0 && timelineEventContextTarget >= 0 &&
			timelineEventContextTarget < colliderRowCount)
		{
			ImGui::Text((const char*)u8"コライダー: %s",
				colliders[timelineEventContextTarget].name.c_str());
			VMDLModel::VmdlColliderAnimationTrack* track = nullptr;
			for (auto& candidate : controlData.colliderTracks)
			{
				if (candidate.animationName == animation.name &&
					candidate.colliderIndex == timelineEventContextTarget)
				{
					track = &candidate;
					break;
				}
			}
			if (track && timelineEventContextKey >= 0 &&
				timelineEventContextKey < static_cast<int>(track->keys.size()))
			{
				auto& key = track->keys[timelineEventContextKey];
				bool changed = ImGui::DragFloat(
					(const char*)u8"時間", &key.seconds, 0.01f, 0.0f, length, "%.3f sec");
				changed |= ImGui::Checkbox((const char*)u8"有効", &key.value);
				if (changed)
				{
					const float editedSeconds = key.seconds;
					std::sort(track->keys.begin(), track->keys.end(), byTime);
					for (int i = 0; i < static_cast<int>(track->keys.size()); ++i)
					{
						if (std::abs(track->keys[i].seconds - editedSeconds) < 0.0001f)
							timelineEventContextKey = i;
					}
					MarkDirty();
					ApplyAnimationPreview();
				}
				if (ImGui::Button((const char*)u8"キーを削除"))
				{
					track->keys.erase(track->keys.begin() + timelineEventContextKey);
					MarkDirty();
					ApplyAnimationPreview();
					ImGui::CloseCurrentPopup();
				}
			}
			else
			{
				if (ImGui::MenuItem((const char*)u8"ONキーを追加"))
				{
					auto& newTrack = model->GetOrCreateColliderAnimationTrack(
						animation.name, timelineEventContextTarget);
					newTrack.keys.push_back({timelineEventContextTime, true});
					std::sort(newTrack.keys.begin(), newTrack.keys.end(), byTime);
					MarkDirty();
					ApplyAnimationPreview();
				}
				if (ImGui::MenuItem((const char*)u8"OFFキーを追加"))
				{
					auto& newTrack = model->GetOrCreateColliderAnimationTrack(
						animation.name, timelineEventContextTarget);
					newTrack.keys.push_back({timelineEventContextTime, false});
					std::sort(newTrack.keys.begin(), newTrack.keys.end(), byTime);
					MarkDirty();
					ApplyAnimationPreview();
				}
			}
		}
		else if (timelineEventContextKind == 1 && timelineEventContextTarget >= 0 &&
				 timelineEventContextTarget < morphRowCount)
		{
			ImGui::Text(
				(const char*)u8"モーフ: %s", morphs[timelineEventContextTarget].name.c_str());
			VMDLModel::VmdlMorphAnimationTrack* track = nullptr;
			for (auto& candidate : controlData.morphTracks)
			{
				if (candidate.animationName == animation.name)
				{
					track = &candidate;
					break;
				}
			}
			if (track && timelineEventContextKey >= 0 &&
				timelineEventContextKey < static_cast<int>(track->keys.size()))
			{
				auto& key = track->keys[timelineEventContextKey];
				if (ImGui::DragFloat(
						(const char*)u8"時間", &key.seconds, 0.01f, 0.0f, length, "%.3f sec"))
				{
					const float editedSeconds = key.seconds;
					std::sort(track->keys.begin(), track->keys.end(), byTime);
					for (int i = 0; i < static_cast<int>(track->keys.size()); ++i)
					{
						if (std::abs(track->keys[i].seconds - editedSeconds) < 0.0001f &&
							track->keys[i].morphIndex == timelineEventContextTarget)
							timelineEventContextKey = i;
					}
					MarkDirty();
					ApplyAnimationPreview();
				}
				if (ImGui::Button((const char*)u8"キーを削除"))
				{
					track->keys.erase(track->keys.begin() + timelineEventContextKey);
					MarkDirty();
					ApplyAnimationPreview();
					ImGui::CloseCurrentPopup();
				}
			}
			else if (ImGui::MenuItem((const char*)u8"モーフキーを追加"))
			{
				auto& newTrack = model->GetOrCreateMorphAnimationTrack(animation.name);
				newTrack.keys.push_back({timelineEventContextTime, timelineEventContextTarget});
				std::sort(newTrack.keys.begin(), newTrack.keys.end(), byTime);
				MarkDirty();
				ApplyAnimationPreview();
			}
		}
		else if (timelineEventContextKind == 2 && timelineEventContextTarget >= 0 &&
				 timelineEventContextTarget < trailRowCount)
		{
			ImGui::Text(
				(const char*)u8"トレイル: %s", trails[timelineEventContextTarget].name.c_str());
			VMDLModel::VmdlTrailAnimationTrack* track = nullptr;
			for (auto& candidate : model->GetVmdlTrailData().tracks)
			{
				if (candidate.animationName == animation.name &&
					candidate.trailIndex == timelineEventContextTarget)
				{
					track = &candidate;
					break;
				}
			}
			if (track && timelineEventContextKey >= 0 &&
				timelineEventContextKey < static_cast<int>(track->keys.size()))
			{
				auto& key = track->keys[timelineEventContextKey];
				bool changed = ImGui::DragFloat(
					(const char*)u8"時間", &key.seconds, 0.01f, 0.0f, length, "%.3f sec");
				changed |= ImGui::Checkbox((const char*)u8"有効", &key.value);
				if (changed)
				{
					const float editedSeconds = key.seconds;
					std::sort(track->keys.begin(), track->keys.end(), byTime);
					for (int i = 0; i < static_cast<int>(track->keys.size()); ++i)
					{
						if (std::abs(track->keys[i].seconds - editedSeconds) < 0.0001f)
							timelineEventContextKey = i;
					}
					MarkDirty();
					ApplyAnimationPreview();
				}
				if (ImGui::Button((const char*)u8"キーを削除"))
				{
					track->keys.erase(track->keys.begin() + timelineEventContextKey);
					MarkDirty();
					ApplyAnimationPreview();
					ImGui::CloseCurrentPopup();
				}
			}
			else
			{
				if (ImGui::MenuItem((const char*)u8"ONキーを追加"))
				{
					auto& newTrack = model->GetOrCreateTrailAnimationTrack(
						animation.name, timelineEventContextTarget);
					newTrack.keys.push_back({timelineEventContextTime, true});
					std::sort(newTrack.keys.begin(), newTrack.keys.end(), byTime);
					MarkDirty();
					ApplyAnimationPreview();
				}
				if (ImGui::MenuItem((const char*)u8"OFFキーを追加"))
				{
					auto& newTrack = model->GetOrCreateTrailAnimationTrack(
						animation.name, timelineEventContextTarget);
					newTrack.keys.push_back({timelineEventContextTime, false});
					std::sort(newTrack.keys.begin(), newTrack.keys.end(), byTime);
					MarkDirty();
					ApplyAnimationPreview();
				}
			}
		}
		else if (timelineEventContextKind == 3 && timelineEventContextTarget >= 0 &&
				 timelineEventContextTarget < soundRowCount)
		{
			auto& source = soundData.sources[timelineEventContextTarget];
			ImGui::Text((const char*)u8"サウンドソース: %s", source.name.c_str());
			ImGui::TextUnformatted(SoundTrackLabel(source.track).c_str());
			VMDLModel::VmdlSoundAnimationTrack* track = nullptr;
			for (auto& candidate : soundData.tracks)
			{
				if (candidate.animationName == animation.name)
				{
					track = &candidate;
					break;
				}
			}
			const bool hasKey = track && timelineEventContextKey >= 0 &&
				timelineEventContextKey < static_cast<int>(track->keys.size()) &&
				track->keys[timelineEventContextKey].sourceIndex == timelineEventContextTarget;
			if (hasKey)
			{
				auto& key = track->keys[timelineEventContextKey];
				int frame = static_cast<int>(std::round(key.seconds * 60.0f));
				bool changed = false;
				if (ImGui::DragInt((const char*)u8"フレーム", &frame, 1.0f, 0,
						static_cast<int>(std::ceil(length * 60.0f))))
				{
					key.seconds = static_cast<float>(frame) / 60.0f;
					changed = true;
				}
				if (changed) MarkDirty();
				if (ImGui::Button((const char*)u8"試聴"))
				{
					SoundSystem::PlayOptions options;
					options.volume = source.volume;
					options.pitch = source.pitchMax > source.pitchMin
						? Random::Range(source.pitchMin, source.pitchMax) : source.pitchMin;
					SoundSystem::Instance().PlayTrack(source.track, source.variant, options);
				}
				ImGui::SameLine();
				if (ImGui::Button((const char*)u8"キーを削除"))
				{
					track->keys.erase(track->keys.begin() + timelineEventContextKey);
					std::erase_if(soundData.tracks,
						[](const auto& candidate) { return candidate.keys.empty(); });
					MarkDirty();
					ImGui::CloseCurrentPopup();
				}
			}
			else
			{
				if (ImGui::Button((const char*)u8"Playキーを追加"))
				{
					if (!track)
					{
						track = &soundData.tracks.emplace_back();
						track->animationName = animation.name;
					}
					track->keys.push_back({timelineEventContextTime, timelineEventContextTarget,
						source.track, source.variant, 1.0f, source.pitchMin, source.pitchMax});
					std::sort(track->keys.begin(), track->keys.end(), byTime);
					MarkDirty();
					ImGui::CloseCurrentPopup();
				}
			}
		}
		else if (timelineEventContextKind == 4 && timelineEventContextTarget >= 0 &&
			timelineEventContextTarget < particleRowCount)
		{
			ImGui::Text((const char*)u8"パーティクル: %s",
				particles[timelineEventContextTarget].name.c_str());
			VMDLModel::VmdlParticleAnimationTrack* track = nullptr;
			for (auto& candidate : particleData.tracks)
				if (candidate.animationName == animation.name &&
					candidate.emitterIndex == timelineEventContextTarget) { track = &candidate; break; }
			if (track && timelineEventContextKey >= 0 &&
				timelineEventContextKey < static_cast<int>(track->keys.size()))
			{
				auto& key = track->keys[timelineEventContextKey];
				bool changed = ImGui::DragFloat(
					(const char*)u8"時間", &key.seconds, 0.01f, 0.0f, length, "%.3f sec");
				changed |= ImGui::Checkbox((const char*)u8"発生", &key.value);
				if (changed) { std::sort(track->keys.begin(), track->keys.end(), byTime); MarkDirty(); }
				if (ImGui::Button((const char*)u8"キーを削除"))
				{
					track->keys.erase(track->keys.begin() + timelineEventContextKey);
					MarkDirty(); ImGui::CloseCurrentPopup();
				}
			}
			else
			{
				if (ImGui::MenuItem((const char*)u8"ONキーを追加"))
				{
					auto& value = model->GetOrCreateParticleAnimationTrack(
						animation.name, timelineEventContextTarget);
					value.keys.push_back({timelineEventContextTime, true});
					std::sort(value.keys.begin(), value.keys.end(), byTime); MarkDirty();
				}
				if (ImGui::MenuItem((const char*)u8"OFFキーを追加"))
				{
					auto& value = model->GetOrCreateParticleAnimationTrack(
						animation.name, timelineEventContextTarget);
					value.keys.push_back({timelineEventContextTime, false});
					std::sort(value.keys.begin(), value.keys.end(), byTime); MarkDirty();
				}
			}
		}
		else if ((timelineEventContextKind == 5 || timelineEventContextKind == 6) &&
			timelineEventContextTarget >= 0)
		{
			auto& tracks = timelineEventContextKind == 5
				? presentation.cameraShakeTracks : presentation.radialBlurTracks;
			const int componentCount = timelineEventContextKind == 5
				? cameraShakeRowCount : radialBlurRowCount;
			if (timelineEventContextTarget < componentCount)
			{
				const std::string& name = timelineEventContextKind == 5
					? presentation.cameraShakes[timelineEventContextTarget].name
					: presentation.radialBlurs[timelineEventContextTarget].name;
				ImGui::Text("%s: %s", timelineEventContextKind == 5
					? (const char*)u8"カメラシェイク" : (const char*)u8"ラジアルブラー", name.c_str());
				VMDLModel::VmdlPresentationAnimationTrack* track = nullptr;
				for (auto& candidate : tracks)
					if (candidate.animationName == animation.name) { track = &candidate; break; }
				const bool hasKey = track && timelineEventContextKey >= 0 &&
					timelineEventContextKey < static_cast<int>(track->keys.size()) &&
					track->keys[timelineEventContextKey].componentIndex == timelineEventContextTarget;
				if (hasKey)
				{
					auto& key = track->keys[timelineEventContextKey];
					if (ImGui::DragFloat((const char*)u8"時間", &key.seconds, 0.01f,
						0.0f, length, "%.3f sec"))
					{
						std::sort(track->keys.begin(), track->keys.end(), byTime); MarkDirty();
					}
					if (ImGui::Button((const char*)u8"キーを削除"))
					{
						track->keys.erase(track->keys.begin() + timelineEventContextKey);
						std::erase_if(tracks, [](const auto& value) { return value.keys.empty(); });
						MarkDirty(); ImGui::CloseCurrentPopup();
					}
				}
				else if (ImGui::MenuItem((const char*)u8"再生キーを追加"))
				{
					if (!track)
					{
						track = &tracks.emplace_back();
						track->animationName = animation.name;
					}
					track->keys.push_back({timelineEventContextTime, timelineEventContextTarget});
					std::sort(track->keys.begin(), track->keys.end(), byTime);
					MarkDirty(); ImGui::CloseCurrentPopup();
				}
			}
		}
		ImGui::EndPopup();
	}

	if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		const ImVec2 mouse = io.MousePos;
		const bool clickedNoneButton = mouse.x >= sheetMin.x + labelWidth - 42.0f &&
									   mouse.x <= sheetMin.x + labelWidth - 24.0f;
		const bool clickedApplyButton =
			mouse.x >= sheetMin.x + labelWidth - 22.0f && mouse.x <= sheetMin.x + labelWidth - 4.0f;
		if ((clickedNoneButton || clickedApplyButton) && mouse.y >= weightTop &&
			mouse.y < weightBottom)
		{
			const int footIndex = std::clamp(
				static_cast<int>((mouse.y - weightTop) / footWeightHeight), 0, footWeightCount - 1);
			auto& track = model->GetOrCreateFootWeightTrack(animation.name, footIndex);
			const int sampleCount =
				std::max(2, static_cast<int>(std::ceil(length * track.sampleRate)) + 1);
			track.weights.assign(sampleCount, clickedNoneButton ? 0.0f : 1.0f);
			MarkDirty();
			ApplyAnimationPreview();
			return;
		}
		if (mouse.x >= timeLeft && mouse.y >= weightTop && mouse.y < weightBottom)
		{
			paintingFootWeight = true;
			paintingFootWeightIndex = std::clamp(
				static_cast<int>((mouse.y - weightTop) / footWeightHeight), 0, footWeightCount - 1);
			paintingFootWeightLastSample = -1;
		}
		else if (mouse.x < sheetMin.x + labelWidth && mouse.y >= sheetMin.y + rowsTopOffset &&
				 mouse.y < sheetMin.y + eventRowsTopOffset)
		{
			const int row =
				std::clamp(static_cast<int>((mouse.y - sheetMin.y - rowsTopOffset) / rowHeight), 0,
					static_cast<int>(rows.size()) - 1);
			if (!rows[row].child)
			{
				if (rows[row].track == 0) positionTrackExpanded = !positionTrackExpanded;
				else if (rows[row].track == 1) rotationTrackExpanded = !rotationTrackExpanded;
				else scaleTrackExpanded = !scaleTrackExpanded;
			}
			else if (mouse.x >= sheetMin.x + 92.0f)
			{
				draggingNumericTrack = rows[row].track;
				draggingNumericComponent = rows[row].component;
			}
			return;
		}
		if (paintingFootWeight) return;
		int hitTrack = -1;
		int hitIndex = -1;
		float closest = 9.0f;
		for (const KeyHit& hit : hitKeys)
		{
			const float distance =
				std::abs(mouse.x - hit.position.x) + std::abs(mouse.y - hit.position.y);
			if (distance >= closest) continue;
			closest = distance;
			hitTrack = hit.track;
			hitIndex = hit.index;
		}
		if (hitTrack >= 0)
		{
			selectedKeyTrack = hitTrack;
			selectedKeyIndex = hitIndex;
			draggingAnimationKey = true;
		}
		else if (mouse.x >= timeLeft)
		{
			animationPlaying = false;
			scrubbingAnimationTime = true;
			animationTime = xToTime(mouse.x);
			ApplyAnimationPreview();
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
	if (scrubbingAnimationTime && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		animationPlaying = false;
		animationTime = xToTime(io.MousePos.x);
		ApplyAnimationPreview();
	}
	if (scrubbingAnimationTime && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		scrubbingAnimationTime = false;
	if (draggingNumericTrack >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		const float delta = io.MouseDelta.x * 0.01f;
		VMDLModel::Node& node = model->GetNodes()[selectedNode];
		if (draggingNumericTrack == 0)
		{
			if (selectedKeyTrack == 0 && selectedKeyIndex >= 0 &&
				selectedKeyIndex < static_cast<int>(keys.positionKeyframes.size()))
				(&keys.positionKeyframes[selectedKeyIndex].value.x)[draggingNumericComponent] +=
					delta;
			else (&node.position.x)[draggingNumericComponent] += delta;
		}
		else if (draggingNumericTrack == 1)
		{
			if (selectedKeyTrack == 1 && selectedKeyIndex >= 0 &&
				selectedKeyIndex < static_cast<int>(keys.rotationKeyframes.size()))
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
				selectedKeyIndex < static_cast<int>(keys.scaleKeyframes.size()))
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
	if (draggingAnimationKey && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		const float seconds = xToTime(io.MousePos.x);
		if (selectedKeyTrack == 0 &&
			selectedKeyIndex < static_cast<int>(keys.positionKeyframes.size()))
			keys.positionKeyframes[selectedKeyIndex].seconds = seconds;
		else if (selectedKeyTrack == 1 &&
				 selectedKeyIndex < static_cast<int>(keys.rotationKeyframes.size()))
			keys.rotationKeyframes[selectedKeyIndex].seconds = seconds;
		else if (selectedKeyTrack == 2 &&
				 selectedKeyIndex < static_cast<int>(keys.scaleKeyframes.size()))
			keys.scaleKeyframes[selectedKeyIndex].seconds = seconds;
		animationTime = seconds;
		animationPlaying = false;
		MarkDirty();
		ApplyAnimationPreview();
	}
	if (draggingAnimationKey && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		const float selectedTime = animationTime;
		const auto byTime = [](const auto& left, const auto& right) {
			return left.seconds < right.seconds;
		};
		if (selectedKeyTrack == 0)
			std::sort(keys.positionKeyframes.begin(), keys.positionKeyframes.end(), byTime);
		else if (selectedKeyTrack == 1)
			std::sort(keys.rotationKeyframes.begin(), keys.rotationKeyframes.end(), byTime);
		else if (selectedKeyTrack == 2)
			std::sort(keys.scaleKeyframes.begin(), keys.scaleKeyframes.end(), byTime);
		const auto findSelected = [&](const auto& trackKeys) {
			int closestIndex = -1;
			float closestDistance = FLT_MAX;
			for (int i = 0; i < static_cast<int>(trackKeys.size()); ++i)
			{
				const float distance = std::abs(trackKeys[i].seconds - selectedTime);
				if (distance >= closestDistance) continue;
				closestDistance = distance;
				closestIndex = i;
			}
			return closestIndex;
		};
		if (selectedKeyTrack == 0) selectedKeyIndex = findSelected(keys.positionKeyframes);
		else if (selectedKeyTrack == 1) selectedKeyIndex = findSelected(keys.rotationKeyframes);
		else if (selectedKeyTrack == 2) selectedKeyIndex = findSelected(keys.scaleKeyframes);
		draggingAnimationKey = false;
	}
}

void VmdlEditorScene::ApplyAnimationPreview()
{
	if (!model || selectedAnimation < 0 ||
		selectedAnimation >= static_cast<int>(model->GetAnimations().size()))
		return;
	const auto& animation = model->GetAnimations()[selectedAnimation];
	auto& nodes = model->GetNodes();
	const size_t count = std::min(nodes.size(), animation.nodeAnims.size());
	for (size_t i = 0; i < count; ++i)
	{
		VMDLModel::NodePose pose{nodes[i].position, nodes[i].rotation, nodes[i].scale};
		model->ComputeAnimation(selectedAnimation, static_cast<int>(i), animationTime, pose);
		nodes[i].position = pose.position;
		nodes[i].rotation = pose.rotation;
		nodes[i].scale = pose.scale;
	}
	if (previewColliderActive.size() != model->GetVmdlExtensionData().colliders.size())
		previewColliderActive.resize(model->GetVmdlExtensionData().colliders.size(), 1);
	for (int i = 0; i < static_cast<int>(previewColliderActive.size()); ++i)
		previewColliderActive[i] =
			model->EvaluateColliderActive(selectedAnimation, animationTime, i) ? 1 : 0;
	previewTrailActive.resize(model->GetVmdlTrailData().trails.size(), 1);
	for (int i = 0; i < static_cast<int>(previewTrailActive.size()); ++i)
		previewTrailActive[i] =
			model->EvaluateTrailActive(selectedAnimation, animationTime, i) ? 1 : 0;
	model->ApplyMorphAnimation(selectedAnimation, animationTime);
}

void VmdlEditorScene::PlayAnimationSoundPreview(
	int animationIndex, float beginTime, float endTime)
{
	if (!model || animationIndex < 0 ||
		animationIndex >= static_cast<int>(model->GetAnimations().size()) || endTime < beginTime)
		return;

	const auto& animation = model->GetAnimations()[animationIndex];
	const auto& soundData = model->GetVmdlSoundData();
	for (const auto& track : soundData.tracks)
	{
		if (track.animationName != animation.name) continue;
		for (const auto& key : track.keys)
		{
			if (key.seconds <= beginTime || key.seconds > endTime + 0.0001f ||
				key.sourceIndex < 0 || key.sourceIndex >= static_cast<int>(soundData.sources.size()))
				continue;

			const auto& source = soundData.sources[key.sourceIndex];
			SoundSystem::PlayOptions options;
			options.volume = source.volume;
			options.pitch = source.pitchMax > source.pitchMin
				? Random::Range(source.pitchMin, source.pitchMax) : source.pitchMin;
			if (source.spatial && source.nodeIndex >= 0 &&
				source.nodeIndex < static_cast<int>(model->GetNodes().size()))
			{
				SoundSystem::SpatialOptions spatial;
				static_cast<SoundSystem::PlayOptions&>(spatial) = options;
				spatial.minDistance = source.minDistance;
				spatial.maxDistance = source.maxDistance;
				spatial.lowPassHz = source.lowPassHz;
				spatial.farLowPassHz = source.farLowPassHz;
				spatial.reverbMix = source.reverbMix;
				SoundSystem::Instance().PlayTrack3DAt(source.track,
					model->GetNodes()[source.nodeIndex].worldTransform.Translation(),
					source.variant, spatial);
			}
			else SoundSystem::Instance().PlayTrack(source.track, source.variant, options);
		}
	}
}

void VmdlEditorScene::ResetAnimationControlPreview()
{
	if (!model) return;
	previewColliderActive.resize(model->GetVmdlExtensionData().colliders.size(), 1);
	for (int i = 0; i < static_cast<int>(previewColliderActive.size()); ++i)
		previewColliderActive[i] = model->GetColliderInitialActive(i) ? 1 : 0;
	previewTrailActive.resize(model->GetVmdlTrailData().trails.size(), 1);
	for (int i = 0; i < static_cast<int>(previewTrailActive.size()); ++i)
		previewTrailActive[i] = model->GetTrailInitialActive(i) ? 1 : 0;
	previewParticleActive.resize(model->GetVmdlParticleData().emitters.size(), 0);
	for (int i = 0; i < static_cast<int>(previewParticleActive.size()); ++i)
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
		collider.start = value.offsetPosition;
		collider.end = value.offsetPosition;
		collider.radius = value.radius;
		collider.nodeIndex = value.nodeIndex;
		springColliders.push_back(collider);
	}

	springPreviewOwner = std::make_unique<Object>("VMDL Spring Preview");
	for (const auto& value : data.springs)
	{
		if (value.nodeIndex < 0 || value.nodeIndex >= static_cast<int>(model->GetNodes().size()))
			continue;
		SpringBone* spring = springPreviewOwner->AddComponent<SpringBone>(
			0, model.get(), value.nodeIndex, springColliders, value.stiffness, value.drag);
		spring->SetName(value.name);
		spring->SetUseUnscaledTime(true);
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
		signature += std::format("{}|{},{},{}|{},{},{}|{}|{}|", value.nodeIndex,
			value.offsetPosition.x, value.offsetPosition.y, value.offsetPosition.z,
			value.offsetRotation.x, value.offsetRotation.y, value.offsetRotation.z,
			value.stiffness, value.drag);
	}
	for (const auto& value : data.springColliders)
	{
		signature += std::format("{}|{},{},{}|{}|", value.nodeIndex, value.offsetPosition.x,
			value.offsetPosition.y, value.offsetPosition.z, value.radius);
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
	for (int i = 0; i < static_cast<int>(trails.size()); ++i)
	{
		const auto& value = trails[i];
		if (value.nodeIndex < 0 || value.nodeIndex >= static_cast<int>(model->GetNodes().size()))
			continue;
		TrailRenderComponent* trail = trailPreviewOwner->AddComponent<TrailRenderComponent>(
			model.get(), value.nodeIndex, value.rootOffset, value.tipOffset, value.color,
			value.tipRatio, std::max(0.01f, value.lifeTime), std::max(2, value.maxPoints),
			value.offsetAngle);
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
			"{}|{},{},{}|{},{},{}|{},{},{},{}|{}|{}|{}|{},{},{}|", value.nodeIndex,
			value.rootOffset.x, value.rootOffset.y, value.rootOffset.z, value.tipOffset.x,
			value.tipOffset.y, value.tipOffset.z, value.color.x, value.color.y, value.color.z,
			value.color.w, value.tipRatio, value.lifeTime, value.maxPoints, value.offsetAngle.x,
			value.offsetAngle.y, value.offsetAngle.z);
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

	for (int i = 0; i < static_cast<int>(trailPreviewComponents.size()); ++i)
	{
		TrailRenderComponent* trail = trailPreviewComponents[i];
		if (!trail) continue;
		const bool eventActive =
			i < static_cast<int>(previewTrailActive.size())
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
	particlePreviewAnimation = -1;
	particlePreviewAnimationTime = 0.0f;
	if (!model) return;
	particlePreviewOwner = std::make_unique<Object>("VMDL Particle Preview");
	const auto& data = model->GetVmdlParticleData();
	particlePreviewComponents.assign(data.emitters.size(), nullptr);
	for (int i = 0; i < static_cast<int>(data.emitters.size()); ++i)
	{
		const auto& value = data.emitters[i];
		if (value.nodeIndex < 0 || value.nodeIndex >= static_cast<int>(model->GetNodes().size()))
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
	for (int i = 0; i < static_cast<int>(particlePreviewComponents.size()); ++i)
	{
		if (!particlePreviewComponents[i]) continue;
		const bool eventActive = selectedAnimation >= 0
			? model->EvaluateParticleActive(selectedAnimation, animationTime, i)
			: model->GetParticleInitialActive(i);
		const bool active = animationPlaying && eventActive;
		previewParticleActive[i] = active ? 1 : 0;
		particlePreviewComponents[i]->SetEmitting(active);
		if (particlePreviewBurstPending) particlePreviewComponents[i]->Burst();
	}
	particlePreviewBurstPending = false;
	particlePreviewOwner->LateUpdate();
	for (VMDLParticleEmitterComponent* emitter : particlePreviewComponents)
		if (emitter) emitter->RenderParticles(rc);
	particlePreviewAnimation = selectedAnimation;
	particlePreviewAnimationTime = animationTime;
}

void VmdlEditorScene::ExportParticlePrefab(int emitterIndex)
{
	if (!model || emitterIndex < 0 ||
		emitterIndex >= static_cast<int>(model->GetVmdlParticleData().emitters.size())) return;
	const auto root = ResourceManager::FindSourceResourceRoot();
	std::filesystem::path proposed = root.empty() ? std::filesystem::path("particle.vfx")
		: root / "Effect" / (model->GetVmdlParticleData().emitters[emitterIndex].name + ".vfx");
	std::error_code ec;
	std::filesystem::create_directories(proposed.parent_path(), ec);
	std::string path = proposed.string();
	if (Dialog::SaveFileName(path, "VFX Asset (*.vfx)\0*.vfx\0",
		"Export VFX Asset", "vfx") != DialogResult::OK) return;
	std::ofstream output(path, std::ios::trunc);
	if (!output) { ErrorMessage("Failed to open the particle prefab for writing."); return; }
	json asset = {
		{"format", "VVV_VFX"},
		{"version", 1},
		{"emitter", ParticleEmitterToJson(model->GetVmdlParticleData().emitters[emitterIndex])}
	};
	output << asset.dump(2);
	if (!output.good()) ErrorMessage("Failed to write the particle prefab.");
}

void VmdlEditorScene::ImportParticlePrefab(int emitterIndex)
{
	if (!model || emitterIndex < 0 ||
		emitterIndex >= static_cast<int>(model->GetVmdlParticleData().emitters.size())) return;
	const auto root = ResourceManager::FindSourceResourceRoot();
	const std::string initial = root.empty() ? std::string{} : (root / "Effect").string();
	std::string path;
	if (Dialog::OpenFileName(path, "VFX Asset (*.vfx;*.vfxp)\0*.vfx;*.vfxp\0",
		"Import VFX Asset", initial.empty() ? nullptr : initial.c_str()) != DialogResult::OK) return;
	std::ifstream input(path);
	if (!input) { ErrorMessage("Failed to open the particle prefab."); return; }
	try
	{
		json data; input >> data;
		const json* emitterData = &data;
		if (data.is_object() && data.value("format", std::string{}) == "VVV_VFX")
		{
			const auto found = data.find("emitter");
			if (found == data.end() || !found->is_object())
			{
				ErrorMessage("Invalid VFX asset: emitter is missing."); return;
			}
			emitterData = &*found;
		}
		auto& emitter = model->GetVmdlParticleData().emitters[emitterIndex];
		const int nodeIndex = emitter.nodeIndex;
		if (!ParticleEmitterFromJson(*emitterData, emitter))
		{
			ErrorMessage("Invalid particle prefab."); return;
		}
		emitter.nodeIndex = nodeIndex;
		MarkDirty();
		RebuildParticlePreview();
	}
	catch (const std::exception& e) { ErrorMessage(std::string("Particle prefab load failed: ") + e.what()); }
}

void VmdlEditorScene::DrawAnimationEventEditor()
{
	if (!model || selectedAnimation < 0 ||
		selectedAnimation >= static_cast<int>(model->GetAnimations().size()))
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
				std::clamp(selectedColliderEventTarget, 0, static_cast<int>(colliders.size()) - 1);
			if (ImGui::BeginCombo((const char*)u8"コライダー",
					colliders[selectedColliderEventTarget].name.c_str()))
			{
				for (int i = 0; i < static_cast<int>(colliders.size()); ++i)
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
				for (int i = 0; i < static_cast<int>(selectedTrack->keys.size()); ++i)
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
				static_cast<int>(particleData.emitters.size()) - 1);
			if (ImGui::BeginCombo((const char*)u8"エミッタ",
					particleData.emitters[selectedParticleEventTarget].name.c_str()))
			{
				for (int i = 0; i < static_cast<int>(particleData.emitters.size()); ++i)
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
				for (int i = 0; i < static_cast<int>(selectedTrack->keys.size()); ++i)
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
				std::clamp(selectedMorphEventTarget, 0, static_cast<int>(morphs.size()) - 1);
			if (ImGui::BeginCombo(
					(const char*)u8"モーフ", morphs[selectedMorphEventTarget].name.c_str()))
			{
				for (int i = 0; i < static_cast<int>(morphs.size()); ++i)
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
				for (int i = 0; i < static_cast<int>(selectedTrack->keys.size()); ++i)
				{
					auto& key = selectedTrack->keys[i];
					ImGui::PushID(i);
					ImGui::SetNextItemWidth(150.0f);
					if (ImGui::DragFloat("##Time", &key.seconds, 0.01f, 0.0f,
							animation.secondsLength, "%.3f sec"))
						changed = true;
					ImGui::SameLine();
					const char* keyMorphName =
						key.morphIndex >= 0 && key.morphIndex < static_cast<int>(morphs.size())
							? morphs[key.morphIndex].name.c_str()
							: "(missing)";
					ImGui::SetNextItemWidth(220.0f);
					if (ImGui::BeginCombo("##Morph", keyMorphName))
					{
						for (int morphIndex = 0; morphIndex < static_cast<int>(morphs.size());
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
			for (int nodeIndex = 0; nodeIndex < static_cast<int>(nodes.size()); ++nodeIndex)
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
	for (int i = 0; i < static_cast<int>(settings.legs.size()); ++i)
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
				changed |= ImGui::DragFloat3(
					(const char*)u8"ポール位置 (Pole Position)", &pole.position.x, 0.01f);
				ImGui::TextDisabled((const char*)u8"モデルのローカル座標");
			}

			auto& ray = raySettings[i];
			changed |= ImGui::Checkbox((const char*)u8"カスタムレイ (Custom Ray)", &ray.custom);
			if (ray.custom)
			{
				changed |= ImGui::DragFloat3(
					(const char*)u8"開始位置 (Start Offset)", &ray.startOffset.x, 0.01f);
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
		selectedMorph >= 0 && selectedMorph < static_cast<int>(morphs.size());

	// モーフの追加と操作
	if (ImGui::Button((const char*)u8"現在のモーフを登録"))
	{
		auto& morph = morphs.emplace_back();
		morph.name = MakeUniqueMorphName("MORPH " + std::to_string(morphs.size()));
		morph.meshVisibility.reserve(model->GetMeshes().size());
		for (const VMDLModel::Mesh& mesh : model->GetMeshes())
			morph.meshVisibility.push_back(mesh.isDraw ? 1 : 0);
		selectedMorph = static_cast<int>(morphs.size()) - 1;
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
		selectedMorph = static_cast<int>(morphs.size()) - 1;
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
		else selectedMorph = std::min(selectedMorph, static_cast<int>(morphs.size()) - 1);
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
		for (int i = 0; i < static_cast<int>(morphs.size()); ++i)
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
				for (size_t meshIndex = 0;
					meshIndex < morphs[i].meshVisibility.size() &&
					meshIndex < model->GetMeshes().size(); ++meshIndex)
				{
					if (morphs[i].meshVisibility[meshIndex] == 1 &&
						!model->IsExternalMesh(static_cast<int>(meshIndex)))
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
		if (selectedMorph >= 0 && selectedMorph < static_cast<int>(morphs.size()))
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
			for (int i = 0; i < static_cast<int>(model->GetMeshes().size()); ++i)
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
	selectedMaterial = std::clamp(selectedMaterial, 0, static_cast<int>(materials.size()) - 1);

	constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable |
										   ImGuiTableFlags_BordersInnerV |
										   ImGuiTableFlags_SizingStretchProp;
	if (!ImGui::BeginTable("Material Editor Layout", 2, tableFlags, ImVec2(0.0f, 0.0f))) return;
	ImGui::TableSetupColumn("Material List Column", ImGuiTableColumnFlags_WidthStretch, 0.28f);
	ImGui::TableSetupColumn("Material Property Column", ImGuiTableColumnFlags_WidthStretch, 0.72f);
	ImGui::TableNextRow();

	// 左側のマテリアル一覧
	ImGui::TableSetColumnIndex(0);
	ImGui::BeginChild("Material List", ImVec2(0.0f, 0.0f), true);
	for (int i = 0; i < static_cast<int>(materials.size()); ++i)
	{
		ImGui::PushID(i);
		if (ImGui::Selectable(materials[i].name.c_str(), selectedMaterial == i))
			selectedMaterial = i;
		const ImVec2 itemMin = ImGui::GetItemRectMin();
		const ImVec2 itemMax = ImGui::GetItemRectMax();
		if (selectedMaterial == i)
		{
			ImGui::GetWindowDrawList()->AddRect(
				itemMin, itemMax, ImGuiTheme::SelectedOutline, 2.0f, 0, 2.0f);
			ImGui::GetWindowDrawList()->AddRectFilled(
				itemMin, ImVec2(itemMin.x + 4.0f, itemMax.y), ImGuiTheme::SelectedAccent);
		}
		ImGui::PopID();
	}
	ImGui::EndChild();

	// 右側のマテリアル設定
	ImGui::TableSetColumnIndex(1);
	ImGui::BeginChild("Material Property", ImVec2(0.0f, 0.0f), true);
	auto& material = materials[selectedMaterial];
	ImGui::Text((const char*)u8"マテリアル: %s", material.name.c_str());
	ImGui::SameLine();
	if (ImGui::Button((const char*)u8"GLB設定に初期化") &&
		model->ResetMaterialToGLB(static_cast<size_t>(selectedMaterial)))
	{
		MarkDirty();
	}

	// PBRパラメーター
	ImGui::SeparatorText("PBR");
	bool changed = ImGui::ColorEdit4((const char*)u8"基本色", &material.baseColor.x);
	changed |= ImGui::ColorEdit4((const char*)u8"発光色", &material.emissiveColor.x,
		ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
	changed |= ImGui::SliderFloat((const char*)u8"メタリック", &material.metalness, 0.0f, 1.0f);
	changed |= ImGui::SliderFloat((const char*)u8"粗さ", &material.roughness, 0.0001f, 1.0f);
	changed |= ImGui::SliderFloat((const char*)u8"オクルージョン", &material.occlusion, 0.0f, 1.0f);
	changed |= ImGui::SliderFloat(
		(const char*)u8"オクルージョン強度", &material.occlusionStrength, 0.0f, 1.0f);
	changed |= ImGui::SliderFloat((const char*)u8"影の強度", &material.shadowStrength, 0.0f, 1.0f);

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
		ImGui::TableSetupColumn((const char*)u8"種類", ImGuiTableColumnFlags_WidthFixed, 150.0f);
		ImGui::TableSetupColumn((const char*)u8"参照元", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn((const char*)u8"操作", ImGuiTableColumnFlags_WidthFixed, 300.0f);
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
	if (changed) MarkDirty();
	ImGui::EndChild();
	ImGui::EndTable();
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
	if (dirty)
	{
		int result = MessageBoxW(Game::Graphics::Instance().GetWindowHandle(),
			L"\u7D42\u4E86\u3059\u308B\u524D\u306B\u4FDD\u5B58\u3057\u307E\u3059\u304B\uFF1F",
			L"VMDL Editor", MB_YESNOCANCEL | MB_ICONQUESTION);
		if (result == IDYES)
		{
			SaveVmdl();
			if (dirty) return false;
		}
		else if (result == IDCANCEL)
		{
			return false; // Cancel exit
		}
	}
	return true;
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
		const int firstAppendedIndex = static_cast<int>(model->GetAnimations().size());
		model->AppendAnimations(filepath.c_str());
		if (firstAppendedIndex >= static_cast<int>(model->GetAnimations().size()))
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
		selectedNode = model->GetNodes().empty() ? -1 : 0;
		selectedNodes.clear();
		if (selectedNode >= 0) selectedNodes.push_back(selectedNode);
		selectedMesh = -1;
		selectedMeshes.clear();
		selectedComponentType = AttachedComponentType::None;
		selectedComponentIndex = -1;
		focusSelectedComponent = false;
		selectedMaterial = model->GetMaterials().empty() ? -1 : 0;
		selectedAnimation = model->GetAnimations().empty() ? -1 : 0;
		selectedMorph = model->GetVmdlExtensionData().morphs.empty() ? -1 : 0;
		for (int i = 0; i < static_cast<int>(model->GetVmdlExtensionData().morphs.size()); ++i)
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
		meshIndex >= static_cast<int>(model->GetMeshes().size())) return {};
	const auto& mesh = model->GetMeshes()[meshIndex];
	std::string meshLabel;
	if (mesh.materialIndex >= 0 &&
		mesh.materialIndex < static_cast<int>(model->GetMaterials().size()))
		meshLabel = model->GetMaterials()[mesh.materialIndex].name;
	if (meshLabel.empty() && mesh.nodeIndex >= 0 &&
		mesh.nodeIndex < static_cast<int>(model->GetNodes().size()))
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
		meshIndex >= static_cast<int>(model->GetMeshes().size())) return;
	const std::vector<int> meshIndices = {meshIndex};
	if (selectedMorph < 0 ||
		selectedMorph >= static_cast<int>(model->GetVmdlExtensionData().morphs.size()))
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
			L"VMDL Editor", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
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
		morphIndex >= static_cast<int>(model->GetVmdlExtensionData().morphs.size())) return;
	const auto& morph = model->GetVmdlExtensionData().morphs[morphIndex];
	std::vector<int> meshIndices;
	for (size_t meshIndex = 0;
		meshIndex < morph.meshVisibility.size() && meshIndex < model->GetMeshes().size();
		++meshIndex)
	{
		// 「+ 表示」のメッシュが、このモーフを適用したときに必要となる実体
		if (morph.meshVisibility[meshIndex] == 1 &&
			!model->IsExternalMesh(static_cast<int>(meshIndex)))
			meshIndices.push_back(static_cast<int>(meshIndex));
	}
	if (meshIndices.empty()) return;

	const std::wstring message =
		L"モーフ「" + Utf8ToWide(morph.name) + L"」で表示する" +
		std::to_wstring(meshIndices.size()) +
		L"個のメッシュを、それぞれ個別のVMSHへ分離します。\n"
		L"既存のVMSHファイルは上書きしません。\n\n続行しますか？";
	if (MessageBoxW(Game::Graphics::Instance().GetWindowHandle(), message.c_str(),
			L"VMDL Editor", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
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
			L"VMDL Editor", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
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
				L"VMDL Editor", MB_OK | MB_ICONWARNING);
			return;
		}
		documentPath = ResourceManager::ResolveSourcePath(documentPath);
		if (!model->SaveVmdl(documentPath) ||
			!ResourceManager::Instance().RefreshResources(documentPath))
		{
			MessageBoxW(Game::Graphics::Instance().GetWindowHandle(),
				L"VMDLの保存に失敗したため、復旧用のVMSHは削除しませんでした。",
				L"VMDL Editor", MB_OK | MB_ICONWARNING);
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
				L"VMDL Editor", MB_OK | MB_ICONWARNING);
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
	const auto filepath = ResourceManager::ResolveSourcePath(requestedPath);
	const auto importDestination = ResourceManager::ResolveSourcePath(requestedDestination);
	if (!std::filesystem::is_regular_file(filepath))
	{
		ErrorMessage("Model file not found. Open an existing file from Resources/Model.");
		return;
	}
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
		selectedNode = model->GetNodes().empty() ? -1 : 0;
		selectedNodes.clear();
		if (selectedNode >= 0) selectedNodes.push_back(selectedNode);
		selectedMesh = -1;
		selectedMeshes.clear();
		selectedComponentType = AttachedComponentType::None;
		selectedComponentIndex = -1;
		focusSelectedComponent = false;
		selectedMaterial = model->GetMaterials().empty() ? -1 : 0;
		selectedAnimation = model->GetAnimations().empty() ? -1 : 0;
		selectedMorph = model->GetVmdlExtensionData().morphs.empty() ? -1 : 0;
		for (int i = 0; i < static_cast<int>(model->GetVmdlExtensionData().morphs.size()); ++i)
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
		std::wstring(message.begin(), message.end()).c_str(), L"VMDL Editor", MB_ICONERROR);
}
