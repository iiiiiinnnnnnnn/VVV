// Input.cpp

#include "Input.h"
#include "imgui.h"

// 初期化
void Input::Initialize(HWND hWnd)
{
	gamePad = std::make_unique<GamePad>();
	mouse = std::make_unique<Mouse>(hWnd);
}

// 更新処理
void Input::Update()
{
	gamePad->Update();
	mouse->Update();
}

bool Input::IsFocusedWindow()
{
	// デバッグウインドウ操作中は処理しない
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
	{
		return false;
	}

	if (ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard)
	{
		return false;
	}

	// ウィンドウが最前面でない場合は処理しない
	HWND hWnd = GetActiveWindow();
	if (hWnd == nullptr)
	{
		return false;
	}

	return true;
}
