// GameStartScene.cpp

#include "Gameplay/Scene/GameStartScene.h"

#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/TestPlayScene.h"
#include "Gameplay/Scene/VmdlEditorScene.h"
#include "Gameplay/Scene/VstgEditorScene.h"
#include "Rendering/Core/Graphics.h"

GameStartScene::GameStartScene(SceneMessage message)
	: Scene(message)
{
}

void GameStartScene::OnUpdate()
{
	if (!windowConfigured)
	{
		ConfigureWindow();
		if (!windowConfigured) return;
	}
	if (loadRequested) return;
#if !defined(_DEBUG)
	loadRequested = SceneManager::Instance().LoadScene<TestPlayScene>();
#endif
}

void GameStartScene::OnDrawGUI()
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
	constexpr ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings;
	if (!ImGui::Begin("Startup", nullptr, flags))
	{
		ImGui::End();
		return;
	}

#if defined(_DEBUG)
	if (ImGui::Button("Game", ImVec2(-FLT_MIN, 50)))
	{
		loadRequested = SceneManager::Instance().LoadScene<TestPlayScene>();
	}
	if (ImGui::Button("VMDL Editor", ImVec2(-FLT_MIN, 50)))
	{
		loadRequested = SceneManager::Instance().LoadScene<VmdlEditorScene>();
	}
	if (ImGui::Button("VSTG Editor", ImVec2(-FLT_MIN, 50)))
	{
		loadRequested = SceneManager::Instance().LoadScene<VstgEditorScene>();
	}
#endif
	ImGui::End();
}

void GameStartScene::ConfigureWindow()
{
	Game::Graphics& graphics = Game::Graphics::Instance();
	if (graphics.IsBorderlessFullscreen())
	{
		graphics.SetBorderlessFullscreen(false);
		return;
	}

	HWND window = graphics.GetWindowHandle();
	constexpr LONG_PTR style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	constexpr int clientWidth = 320;
	constexpr int clientHeight = 250;
	RECT rect{0, 0, clientWidth, clientHeight};
	AdjustWindowRect(&rect, static_cast<DWORD>(style), FALSE);
	MONITORINFO monitorInfo{};
	monitorInfo.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitorInfo);

	SetWindowLongPtr(window, GWL_STYLE, style);
	SetWindowPos(
		window,
		HWND_TOP,
		monitorInfo.rcWork.left,
		monitorInfo.rcWork.top,
		rect.right - rect.left,
		rect.bottom - rect.top,
		SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	graphics.SetWindowMovementLocked(true);
	windowConfigured = true;
}
