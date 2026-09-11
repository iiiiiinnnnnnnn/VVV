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
	static void SaveSettings();

};
