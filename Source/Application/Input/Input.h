#pragma once

#include <algorithm>
#include <memory>
#include "Application/Input/GamePad.h"
#include "Application/Input/Mouse.h"

namespace Game
{
	class Input
	{
	private:
		Input() = default;
		~Input() = default;

	public:
		// インスタンス取得
		static Input& Instance()
		{
			static Input instance;
			return instance;
		}

		// 初期化
		void Initialize(HWND hWnd);

		// 更新処理
		void Update();

		// ゲームパッド取得
		GamePad& GetGamePad() { return *gamePad; }

		// マウス取得
		Mouse& GetMouse() { return *mouse; }

		// UI確定に使った入力をゲームプレイへ渡さない。
		void SuppressGameplayInput(int frames = 2)
		{
			gameplayInputSuppressionFrames = std::max(gameplayInputSuppressionFrames, frames);
		}
		bool IsGameplayInputSuppressed() const { return gameplayInputSuppressionFrames > 0; }

		// フォーカスが当たっているか
		static bool IsFocusedWindow(bool dontCheckImgui = false);

	private:
		HWND						hWnd = nullptr;
		std::unique_ptr<GamePad>	gamePad;
		std::unique_ptr<Mouse>		mouse;
		int gameplayInputSuppressionFrames = 0;
	};
}
