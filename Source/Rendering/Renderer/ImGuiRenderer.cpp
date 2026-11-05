#include "Application/SettingsAndDebug/ProfileScope.h"
// ImGuiRenderer.cpp
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <imgui_internal.h>
#include "Rendering/Renderer/ImGuiRenderer.h"
#include "Rendering/Renderer/ImGuiTheme.h"
#include "Resource/EmbeddedResources.h"
#include <filesystem>

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
		constexpr const char* prefix = "RecentStagePath=";
		if (strncmp(line, prefix, strlen(prefix)) == 0)
			static_cast<VstgEditorSettings*>(entry)->recentStagePath = line + strlen(prefix);
	};
	handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* value, ImGuiTextBuffer* output) {
		const auto* settings = static_cast<VstgEditorSettings*>(value->UserData);
		if (!settings->loaded) return;
		output->append("[VstgEditor][Settings]\n");
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
		int maximized = 0;
		if (sscanf_s(line, "Window=%d,%d,%d,%d,%d", &settings->windowX,
			&settings->windowY, &settings->windowWidth, &settings->windowHeight,
			&maximized) == 5)
		{
			settings->windowMaximized = maximized != 0;
			return;
		}
		if (sscanf_s(line, "PropertyPanelRatio=%f", &settings->propertyPanelRatio) == 1)
			return;
		if (sscanf_s(line, "ViewportPanelRatio=%f", &settings->viewportPanelRatio) == 1)
			return;
		if (sscanf_s(line, "BottomPanelRatio=%f", &settings->bottomPanelRatio) == 1)
			return;
		constexpr const char* prefix = "RecentModelPath=";
		if (strncmp(line, prefix, strlen(prefix)) == 0)
			settings->recentModelPath = line + strlen(prefix);
	};
	handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* value, ImGuiTextBuffer* output) {
		const auto* settings = static_cast<VmdlEditorLayoutSettings*>(value->UserData);
		if (!settings->loaded) return;
		output->append("[VmdlEditor][Layout]\n");
		output->appendf("Window=%d,%d,%d,%d,%d\n", settings->windowX, settings->windowY,
			settings->windowWidth, settings->windowHeight,
			settings->windowMaximized ? 1 : 0);
		output->appendf("PropertyPanelRatio=%.8f\n", settings->propertyPanelRatio);
		output->appendf("ViewportPanelRatio=%.8f\n", settings->viewportPanelRatio);
		output->appendf("BottomPanelRatio=%.8f\n", settings->bottomPanelRatio);
		output->appendf("RecentModelPath=%s\n\n", settings->recentModelPath.c_str());
	};
	ImGui::AddSettingsHandler(&handler);
}
}

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
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	// Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
														//io.ConfigViewportsNoAutoMerge = true;
														//io.ConfigViewportsNoTaskBarIcon = true;
														//io.ConfigViewportsNoDefaultParent = true;
														//io.ConfigDockingAlwaysTabBar = true;
														//io.ConfigDockingTransparentPayload = true;
#if 1
	io.ConfigFlags |=
		ImGuiConfigFlags_DpiEnableScaleFonts; // FIXME-DPI: THIS CURRENTLY DOESN'T WORK AS EXPECTED. DON'T USE IN USER APP!
	io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports; // FIXME-DPI
#endif

		// Bootstrap Dark style by Madam-Herta from ImThemes
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

		style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		style.Colors[ImGuiCol_TextDisabled] =
			ImVec4(0.5843137502670288f, 0.5960784554481506f, 0.615686297416687f, 1.0f);
		style.Colors[ImGuiCol_WindowBg] =
			ImVec4(0.062745101749897f, 0.06666667014360428f, 0.08627451211214066f, 1.0f);
		style.Colors[ImGuiCol_ChildBg] =
			ImVec4(0.04313725605607033f, 0.0470588244497776f, 0.05882352963089943f, 1.0f);
		style.Colors[ImGuiCol_PopupBg] =
			ImVec4(0.04313725605607033f, 0.0470588244497776f, 0.05882352963089943f, 1.0f);
		style.Colors[ImGuiCol_Border] =
			ImVec4(0.1098039224743843f, 0.1137254908680916f, 0.1333333402872086f, 1.0f);
		style.Colors[ImGuiCol_BorderShadow] =
			ImVec4(0.1098039224743843f, 0.1137254908680916f, 0.1333333402872086f, 1.0f);
		style.Colors[ImGuiCol_FrameBg] =
			ImVec4(0.062745101749897f, 0.06666667014360428f, 0.08627451211214066f, 1.0f);
		style.Colors[ImGuiCol_FrameBgHovered] =
			ImVec4(0.05882352963089943f, 0.529411792755127f, 0.9764705896377563f, 1.0f);
		style.Colors[ImGuiCol_FrameBgActive] =
			ImVec4(0.05882352963089943f, 0.529411792755127f, 0.9764705896377563f, 0.0f);
		style.Colors[ImGuiCol_TitleBg] =
			ImVec4(0.0470588244497776f, 0.05098039284348488f, 0.062745101749897f, 1.0f);
		style.Colors[ImGuiCol_TitleBgActive] =
			ImVec4(0.04313725605607033f, 0.0470588244497776f, 0.05882352963089943f, 1.0f);
		style.Colors[ImGuiCol_TitleBgCollapsed] =
			ImVec4(0.04313725605607033f, 0.0470588244497776f, 0.05882352963089943f, 1.0f);
		style.Colors[ImGuiCol_MenuBarBg] =
			ImVec4(0.04313725605607033f, 0.0470588244497776f, 0.05882352963089943f, 1.0f);
		style.Colors[ImGuiCol_ScrollbarBg] =
			ImVec4(0.04313725605607033f, 0.0470588244497776f, 0.05882352963089943f, 1.0f);
		style.Colors[ImGuiCol_ScrollbarGrab] =
			ImVec4(0.1098039224743843f, 0.1137254908680916f, 0.1333333402872086f, 1.0f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] =
			ImVec4(0.1450980454683304f, 0.1490196138620377f, 0.1843137294054031f, 1.0f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] =
			ImVec4(0.4862745106220245f, 0.4862745106220245f, 0.4862745106220245f, 1.0f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 1.0f, 1.0f, 0.2274678349494934f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(
			0.8196078538894653f, 0.8196078538894653f, 0.8196078538894653f, 0.3304721117019653f);
		style.Colors[ImGuiCol_Button] =
			ImVec4(0.2274509817361832f, 0.4431372582912445f, 0.7568627595901489f, 1.0f);
		style.Colors[ImGuiCol_ButtonHovered] =
			ImVec4(0.2078431397676468f, 0.4705882370471954f, 0.8509804010391235f, 1.0f);
		style.Colors[ImGuiCol_ButtonActive] =
			ImVec4(0.05882352963089943f, 0.529411792755127f, 0.9764705896377563f, 1.0f);
		style.Colors[ImGuiCol_Header] = ImVec4(
			0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.3100000023841858f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(
			0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.800000011920929f);
		style.Colors[ImGuiCol_HeaderActive] =
			ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 1.0f);
		style.Colors[ImGuiCol_Separator] = ImVec4(
			0.3882353007793427f, 0.3882353007793427f, 0.3882353007793427f, 0.6200000047683716f);
		style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(
			0.1372549086809158f, 0.4392156898975372f, 0.800000011920929f, 0.7799999713897705f);
		style.Colors[ImGuiCol_SeparatorActive] =
			ImVec4(0.1372549086809158f, 0.4392156898975372f, 0.800000011920929f, 1.0f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(
			0.3490196168422699f, 0.3490196168422699f, 0.3490196168422699f, 0.1700000017881393f);
		style.Colors[ImGuiCol_ResizeGripHovered] =
			ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 1.0f);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(
			0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.949999988079071f);
		style.Colors[ImGuiCol_Tab] = ImVec4(0.0f, 0.4745098054409027f, 1.0f, 0.9309999942779541f);
		style.Colors[ImGuiCol_TabHovered] = ImVec4(
			0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.800000011920929f);
		style.Colors[ImGuiCol_TabActive] =
			ImVec4(0.2078431397676468f, 0.2078431397676468f, 0.2078431397676468f, 1.0f);
		style.Colors[ImGuiCol_TabUnfocused] = ImVec4(
			0.9176470637321472f, 0.9254902005195618f, 0.9333333373069763f, 0.9861999750137329f);
		style.Colors[ImGuiCol_TabUnfocusedActive] =
			ImVec4(0.7411764860153198f, 0.8196078538894653f, 0.9137254953384399f, 1.0f);
		style.Colors[ImGuiCol_PlotLines] =
			ImVec4(0.3882353007793427f, 0.3882353007793427f, 0.3882353007793427f, 1.0f);
		style.Colors[ImGuiCol_PlotLinesHovered] =
			ImVec4(1.0f, 0.4274509847164154f, 0.3490196168422699f, 1.0f);
		style.Colors[ImGuiCol_PlotHistogram] =
			ImVec4(0.8980392217636108f, 0.6980392336845398f, 0.0f, 1.0f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.0f, 0.4470588266849518f, 0.0f, 1.0f);
		style.Colors[ImGuiCol_TableHeaderBg] =
			ImVec4(0.19f, 0.19f, 0.20f, 1.0f);
		style.Colors[ImGuiCol_TableBorderStrong] =
			ImVec4(0.5686274766921997f, 0.5686274766921997f, 0.6392157077789307f, 1.0f);
		style.Colors[ImGuiCol_TableBorderLight] =
			ImVec4(0.6784313917160034f, 0.6784313917160034f, 0.7372549176216125f, 1.0f);
		style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(
			0.2980392277240753f, 0.2980392277240753f, 0.2980392277240753f, 0.09000000357627869f);
		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(
			0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.3499999940395355f);
		style.Colors[ImGuiCol_DragDropTarget] = ImVec4(
			0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.949999988079071f);
		style.Colors[ImGuiCol_NavHighlight] = ImVec4(
			0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.800000011920929f);
		style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(
			0.6980392336845398f, 0.6980392336845398f, 0.6980392336845398f, 0.699999988079071f);
		style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(
			0.2000000029802322f, 0.2000000029802322f, 0.2000000029802322f, 0.2000000029802322f);
		style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(
			0.2000000029802322f, 0.2000000029802322f, 0.2000000029802322f, 0.3499999940395355f);

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// ImGuiスタイル設定
	{
		style.Alpha = 1.0f; // 全体を不透明にする
		style.WindowRounding = 8.0f;
		style.FrameRounding = 4.0f;

		// 背景色
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
		ImGuiTheme::ApplyYellowTheme(style);
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		style.Colors[ImGuiCol_ChildBg].w = 1.0f;
		style.Colors[ImGuiCol_PopupBg].w = 1.0f;
		style.Colors[ImGuiCol_FrameBg].w = 1.0f;
		style.Colors[ImGuiCol_FrameBgHovered].w = 1.0f;
		style.Colors[ImGuiCol_FrameBgActive].w = 1.0f;
		style.Colors[ImGuiCol_TitleBg].w = 1.0f;
		style.Colors[ImGuiCol_TitleBgActive].w = 1.0f;
		style.Colors[ImGuiCol_TitleBgCollapsed].w = 1.0f;
	}

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
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void ImGuiRenderer::NewFrame()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();

#if 0
	// Docking
	const ImGuiWindowFlags window_flags = ImGuiWindowFlags_None
		| ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoBringToFrontOnFocus
		| ImGuiWindowFlags_NoNavFocus
		| ImGuiWindowFlags_NoBackground
		;
	const ImGuiDockNodeFlags docspace_flags = ImGuiDockNodeFlags_None
		//| ImGuiDockNodeFlags_KeepAliveOnly
		| ImGuiDockNodeFlags_PassthruCentralNode
		;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	bool dock_open = true;
	if (ImGui::Begin("MainDockspace", &dock_open, window_flags))
	{
		ImGui::PopStyleVar(3);

		ImGuiID dockspaceId = ImGui::GetID("MyDockspace");
		ImGui::DockSpace(dockspaceId, ImVec2(0, 0), docspace_flags);
	}
	ImGui::End();
#endif
}

void ImGuiRenderer::Render(ID3D11DeviceContext* context)
{
	PROFILE_FUNCTION();
	auto& io = ImGui::GetIO();
	// Rendering
	ImGui::Render();

	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

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
