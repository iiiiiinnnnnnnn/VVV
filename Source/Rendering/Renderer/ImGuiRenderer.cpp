#include <ImGuiToolKit/ImGuiToolKit.h>
#include <ProfileKit/ProfileKit.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <imgui_internal.h>
#include "Rendering/Renderer/ImGuiRenderer.h"
#include "Resource/EmbeddedResources.h"
#include <filesystem>
#include <array>
#include <cmath>
#include <vector>

// 再起動時に元のUI読み込み
namespace
{
VmdlEditorLayoutSettings vmdlEditorLayoutSettings;
VstgEditorSettings vstgEditorSettings;
GameEditorLayoutSettings gameEditorLayoutSettings;

void RegisterGameEditorSettingsHandler()
{
	ImGuiSettingsHandler handler;
	handler.TypeName = "GameEditor";
	handler.TypeHash = ImHashStr(handler.TypeName);
	handler.UserData = &gameEditorLayoutSettings;
	handler.ReadInitFn = [](ImGuiContext*, ImGuiSettingsHandler* value) {
		*static_cast<GameEditorLayoutSettings*>(value->UserData) = {};
	};
	handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler* value, const char* name) -> void* {
		if (strcmp(name, "Layout") != 0) return nullptr;
		auto* settings = static_cast<GameEditorLayoutSettings*>(value->UserData);
		settings->loaded = true;
		return settings;
	};
	handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
		auto* settings = static_cast<GameEditorLayoutSettings*>(entry);
		if (sscanf_s(line, "LeftWidth=%f", &settings->leftWidth) == 1) return;
		if (sscanf_s(line, "RightWidth=%f", &settings->rightWidth) == 1) return;
		if (sscanf_s(line, "PanelHeight=%f", &settings->panelHeight) == 1) return;
		if (sscanf_s(line, "HierarchyTab=%d", &settings->hierarchyTab) == 1) return;
		if (sscanf_s(line, "InspectorTab=%d", &settings->inspectorTab) == 1) return;
		sscanf_s(line, "SceneSettingsPage=%d", &settings->sceneSettingsPage);
	};
	handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* value, ImGuiTextBuffer* output) {
		const auto* settings = static_cast<GameEditorLayoutSettings*>(value->UserData);
		if (!settings->loaded) return;
		output->append("[GameEditor][Layout]\n");
		output->appendf("LeftWidth=%.3f\nRightWidth=%.3f\nPanelHeight=%.3f\n",
			settings->leftWidth, settings->rightWidth, settings->panelHeight);
		output->appendf("HierarchyTab=%d\nInspectorTab=%d\nSceneSettingsPage=%d\n\n",
			settings->hierarchyTab, settings->inspectorTab, settings->sceneSettingsPage);
	};
	ImGui::AddSettingsHandler(&handler);
}

void RegisterVstgEditorSettingsHandler()
{
	ImGuiSettingsHandler handler;
	handler.TypeName = "VstgEditor";
	handler.TypeHash = ImHashStr(handler.TypeName);
	handler.UserData = &vstgEditorSettings;
	handler.ReadInitFn = [](ImGuiContext*, ImGuiSettingsHandler* value) {
		*static_cast<VstgEditorSettings*>(value->UserData) = {};
	};
	handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler* value, const char* name) -> void* {
		if (strcmp(name, "Settings") != 0) return nullptr;
		auto* settings = static_cast<VstgEditorSettings*>(value->UserData);
		settings->loaded = true;
		return settings;
	};
	handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
		auto* settings = static_cast<VstgEditorSettings*>(entry);
		if (sscanf_s(line, "InspectorWidth=%f", &settings->inspectorWidth) == 1) return;
		if (sscanf_s(line, "HierarchyWidth=%f", &settings->hierarchyWidth) == 1) return;
		constexpr const char* prefix = "RecentStagePath=";
		if (strncmp(line, prefix, strlen(prefix)) == 0)
			static_cast<VstgEditorSettings*>(entry)->recentStagePath = line + strlen(prefix);
	};
	handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* value, ImGuiTextBuffer* output) {
		const auto* settings = static_cast<VstgEditorSettings*>(value->UserData);
		if (!settings->loaded) return;
		output->append("[VstgEditor][Settings]\n");
		output->appendf("InspectorWidth=%.3f\nHierarchyWidth=%.3f\n", settings->inspectorWidth, settings->hierarchyWidth);
		output->appendf("RecentStagePath=%s\n\n", settings->recentStagePath.c_str());
	};
	ImGui::AddSettingsHandler(&handler);
}

void RegisterVmdlEditorSettingsHandler()
{
	ImGuiSettingsHandler handler;
	handler.TypeName = "VmdlEditor";
	handler.TypeHash = ImHashStr(handler.TypeName);
	handler.UserData = &vmdlEditorLayoutSettings;
	handler.ReadInitFn = [](ImGuiContext*, ImGuiSettingsHandler* value) {
		static_cast<VmdlEditorLayoutSettings*>(value->UserData)->loaded = false;
	};
	handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler* value, const char* name) -> void* {
		if (strcmp(name, "Layout") != 0) return nullptr;
		auto* settings = static_cast<VmdlEditorLayoutSettings*>(value->UserData);
		settings->loaded = true;
		return settings;
	};
	handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
		auto* settings = static_cast<VmdlEditorLayoutSettings*>(entry);
		if (sscanf_s(line, "InspectorWidth=%f", &settings->inspectorWidth) == 1) return;
		if (sscanf_s(line, "HierarchyWidth=%f", &settings->hierarchyWidth) == 1) return;
		int maximized = 0;
		if (sscanf_s(line, "Window=%d,%d,%d,%d,%d", &settings->windowX,
				&settings->windowY, &settings->windowWidth, &settings->windowHeight,
				&maximized) == 5)
		{
			settings->windowMaximized = maximized != 0;
			return;
		}
		constexpr const char* prefix = "RecentModelPath=";
		if (strncmp(line, prefix, strlen(prefix)) == 0)
			settings->recentModelPath = line + strlen(prefix);
	};
	handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* value, ImGuiTextBuffer* output) {
		const auto* settings = static_cast<VmdlEditorLayoutSettings*>(value->UserData);
		if (!settings->loaded) return;
		output->append("[VmdlEditor][Layout]\n");
		output->appendf("InspectorWidth=%.3f\nHierarchyWidth=%.3f\n", settings->inspectorWidth, settings->hierarchyWidth);
		output->appendf("Window=%d,%d,%d,%d,%d\n", settings->windowX, settings->windowY,
			settings->windowWidth, settings->windowHeight,
			settings->windowMaximized ? 1 : 0);
		output->appendf("RecentModelPath=%s\n\n", settings->recentModelPath.c_str());
	};
	ImGui::AddSettingsHandler(&handler);
}
} // namespace

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void ImGuiRenderer::Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* dc)
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	auto& io = ImGui::GetIO();
	// シングルクリックでテキスト入力にフォーカスしないようにする
	io.ConfigDragClickToInputText = false;

	// Editor.iniはユーザー設定なのでリソースキャッシュの外へ置く
	static std::string editorIniPath;
	const std::filesystem::path destination =
		std::filesystem::current_path() / "Editor.ini";
	editorIniPath = destination.string();
	io.IniFilename = editorIniPath.c_str();
	RegisterVmdlEditorSettingsHandler();
	RegisterVstgEditorSettingsHandler();
	RegisterGameEditorSettingsHandler();
	// 別スレッドで生成されるエディタシーンより先に設定を確定させる
	ImGui::LoadIniSettingsFromDisk(io.IniFilename);

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
														//io.ConfigViewportsNoAutoMerge = true;
														//io.ConfigViewportsNoTaskBarIcon = true;
														//io.ConfigViewportsNoDefaultParent = true;
														//io.ConfigDockingAlwaysTabBar = true;
														//io.ConfigDockingTransparentPayload = true;
	io.ConfigFlags |=
		ImGuiConfigFlags_DpiEnableScaleFonts; // FIXME-DPI: THIS CURRENTLY DOESN'T WORK AS EXPECTED. DON'T USE IN USER APP!
	io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports; // FIXME-DPI

	// 余白やサイズを設定し、配色は Theme.h に集約する
	ImGuiStyle& style = ImGui::GetStyle();

	style.Alpha = 1.0f;
	style.DisabledAlpha = 0.5f;
	style.WindowPadding = ImVec2(11.69999980926514f, 6.0f);
	style.WindowRounding = 3.299999952316284f;
	style.WindowBorderSize = 0.0f;
	style.WindowMinSize = ImVec2(20.0f, 20.0f);
	style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
	style.WindowMenuButtonPosition = ImGuiDir_Left;
	style.ChildRounding = 0.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupRounding = 0.0f;
	style.PopupBorderSize = 1.0f;
	style.FramePadding = ImVec2(20.0f, 9.899999618530273f);
	style.FrameRounding = 0.0f;
	style.FrameBorderSize = 0.0f;
	style.ItemSpacing = ImVec2(8.0f, 4.0f);
	style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
	style.CellPadding = ImVec2(4.0f, 2.0f);
	style.IndentSpacing = 21.0f;
	style.ColumnsMinSpacing = 6.0f;
	style.ScrollbarSize = 14.0f;
	style.ScrollbarRounding = 9.0f;
	style.GrabMinSize = 10.0f;
	style.GrabRounding = 0.0f;
	style.TabRounding = 4.0f;
	style.TabBorderSize = 0.0f;
	style.ColorButtonPosition = ImGuiDir_Right;
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	ImGuiToolKit::Theme::ApplyYellowTheme(style);

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(device, dc);

	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
	// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
	// - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
	// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
	// - Read 'docs/FONTS.md' for more instructions and details.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	//io.Fonts->AddFontDefault();
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
	ImFont* font = io.Fonts->AddFontFromFileTTF(
		"Resources/Font/LINESeedJP.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	if (!font) io.Fonts->AddFontDefault();

	// Font Awesomeをマージ
	static const ImWchar icon_ranges[] = {0xf000, 0xf8ff, 0};
	ImFontConfig config;
	config.MergeMode = true;
	config.PixelSnapH = true;
	config.GlyphMinAdvanceX = 16.0f;
	config.FontDataOwnedByAtlas = false;
	const HMODULE module = GetModuleHandleW(nullptr);
	const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_FONT_AWESOME),
		MAKEINTRESOURCEW(10)); // RT_RCDATA
	if (resource)
	{
		const HGLOBAL loaded = LoadResource(module, resource);
		void* data = loaded ? LockResource(loaded) : nullptr;
		const DWORD size = SizeofResource(module, resource);
		if (data && size)
			io.Fonts->AddFontFromMemoryTTF(data, static_cast<int>(size), 18.0f, &config, icon_ranges);
	}
}

void ImGuiRenderer::Finalize()
{
	ImGuiToolKit::ClearAlerts();
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void ImGuiRenderer::NewFrame()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();
}

// 再集合成をLinearの16Floatにしたので描画時にSRGから変換する関数
static ImU32 SRGBCONV(ImU32 color)
{
	static const auto linear = []
	{
		std::array<ImU32, 256> values{};
		for (ImU32 channel = 0; channel < values.size(); ++channel)
		{
			const float srgb = static_cast<float>(channel) / 255.0f;
			const float value = srgb <= 0.04045f ? srgb / 12.92f : std::pow((srgb + 0.055f) / 1.055f, 2.4f);
			values[channel] = static_cast<ImU32>(value * 255.0f + 0.5f);
		}
		return values;
	}();

	// アルファは被覆率なので変換しない。RGB の並びは ImGui のパック設定に従う
	return (color & IM_COL32_A_MASK) |
		(linear[(color >> IM_COL32_R_SHIFT) & 0xff] << IM_COL32_R_SHIFT) |
		(linear[(color >> IM_COL32_G_SHIFT) & 0xff] << IM_COL32_G_SHIFT) |
		(linear[(color >> IM_COL32_B_SHIFT) & 0xff] << IM_COL32_B_SHIFT);
}

void ImGuiRenderer::Render(ID3D11DeviceContext* context)
{
	PROFILE_FUNCTION();
	auto& io = ImGui::GetIO();
	// 貯めたAlertを出す
	ImGuiToolKit::DrawAlerts();
	// Rendering
	ImGui::Render();

	ImDrawData* drawData = ImGui::GetDrawData();
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
	context->OMGetRenderTargets(1, target.GetAddressOf(), nullptr);
	bool linearOutput = false;
	if (target)
	{
		D3D11_RENDER_TARGET_VIEW_DESC desc{};
		target->GetDesc(&desc);
		linearOutput = desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT;
	}

	// スタイルだけでなく PushStyleColor や独自描画も含め、今回描く頂点色を変換する
	static std::vector<ImU32> originalColors;
	originalColors.clear();
	if (linearOutput)
	{
		originalColors.reserve(drawData->TotalVtxCount);
		for (ImDrawList* list : drawData->CmdLists)
			for (ImDrawVert& vertex : list->VtxBuffer)
			{
				originalColors.push_back(vertex.col);
				vertex.col = SRGBCONV(vertex.col);
			}
	}
	ImGui_ImplDX11_RenderDrawData(drawData);

	// 元データを残し、次のフレームや SDR の別ウィンドウで二重に変換しない
	if (linearOutput)
	{
		size_t index = 0;
		for (ImDrawList* list : drawData->CmdLists)
			for (ImDrawVert& vertex : list->VtxBuffer) vertex.col = originalColors[index++];
	}

	// Update and Render additional Platform Windows
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

LRESULT ImGuiRenderer::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}

VmdlEditorLayoutSettings& ImGuiRenderer::GetVmdlEditorLayoutSettings()
{
	return vmdlEditorLayoutSettings;
}

VstgEditorSettings& ImGuiRenderer::GetVstgEditorSettings()
{
	return vstgEditorSettings;
}

GameEditorLayoutSettings& ImGuiRenderer::GetGameEditorLayoutSettings()
{
	return gameEditorLayoutSettings;
}

void ImGuiRenderer::SaveSettings()
{
	if (!ImGui::GetCurrentContext()) return;
	ImGui::MarkIniSettingsDirty();
	if (const char* filename = ImGui::GetIO().IniFilename)
		ImGui::SaveIniSettingsToDisk(filename);
}
