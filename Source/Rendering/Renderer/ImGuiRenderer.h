#pragma once
#include <wrl.h>

#include <Windows.h>
#include <d3d11.h>
#include <string>

struct VmdlEditorLayoutSettings
{
	bool loaded = false;
	int windowX = 0;
	int windowY = 0;
	int windowWidth = 0;
	int windowHeight = 0;
	bool windowMaximized = false;
	float propertyPanelRatio = -1.0f;
	float viewportPanelRatio = -1.0f;
	float bottomPanelRatio = -1.0f;
	std::string recentModelPath;
};

struct VstgEditorSettings
{
	bool loaded = false;
	std::string recentStagePath;
};

struct GameEditorLayoutSettings
{
	bool loaded = false;
	float leftWidth = 320.0f;
	float rightWidth = 440.0f;
	float panelHeight = 0.0f;
	int hierarchyTab = 0;
	int inspectorTab = 0;
	int sceneSettingsPage = 0;
};

class ImGuiRenderer
{
public:
	// 初期化
	static void Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* dc);
	
	// 終了化
	static void Finalize();

	// フレーム開始処理
	static void NewFrame();

	// 描画実行
	static void Render(ID3D11DeviceContext* context);

	// WIN32メッセージハンドラー
	static LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	static VmdlEditorLayoutSettings& GetVmdlEditorLayoutSettings();
	static VstgEditorSettings& GetVstgEditorSettings();
	static GameEditorLayoutSettings& GetGameEditorLayoutSettings();
	static void SaveSettings();

};
