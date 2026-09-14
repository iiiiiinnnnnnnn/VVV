#include "Application/Input/Input.h"
#include "imgui.h"

namespace Game
{
	// 初期化
	void Input::Initialize(HWND hWnd)
	{
		this->hWnd = hWnd;
		gamePad = std::make_unique<GamePad>();
		mouse = std::make_unique<Mouse>(hWnd);
	}

	// 更新処理
	void Input::Update()
	{
		if (gameplayInputSuppressionFrames > 0) --gameplayInputSuppressionFrames;
		const bool acceptsInput = GetForegroundWindow() == hWnd;
		gamePad->Update(acceptsInput);
		mouse->Update();
	}

	bool Input::IsFocusedWindow(bool dontCheckImgui)
	{
		auto& io = ImGui::GetIO();
		if (!dontCheckImgui)
		{
			// デバッグウインドウ操作中は処理しない
			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
			{
				return false;
			}

			if (io.WantCaptureMouse || io.WantCaptureKeyboard)
			{
				return false;
			}
		}

		// ゲームのメインウィンドウが最前面でない場合は処理しない
		const HWND hWnd = Input::Instance().hWnd;
		if (hWnd == nullptr || GetForegroundWindow() != hWnd)
		{
			return false;
		}

		return true;
	}
}
