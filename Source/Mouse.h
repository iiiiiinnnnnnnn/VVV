#pragma once

#include <windows.h>

using MouseButton = unsigned int;

// マウス
class Mouse
{
public:
	static const MouseButton BTN_LEFT = (1 << 0);
	static const MouseButton BTN_MIDDLE = (1 << 1);
	static const MouseButton BTN_RIGHT = (1 << 2);

public:
	Mouse(HWND hWnd);
	~Mouse() {}

	// 更新
	void Update();

	// ボタン入力状態の取得
	MouseButton GetButton() const { return buttonState[0]; }

	// ボタン押下状態の取得
	MouseButton GetButtonDown() const { return buttonDown; }

	// ボタン押上状態の取得
	MouseButton GetButtonUp() const { return buttonUp; }

	// ホイール値の設定
	void SetWheel(int wheel) { this->wheel[0] += wheel; }

	// ホイール値の取得
	int GetWheel() const { return wheel[1]; }

	// マウスカーソルX座標取得
	int GetPositionX() const { return positionX[0]; }

	// マウスカーソルY座標取得
	int GetPositionY() const { return positionY[0]; }

	// 前回のマウスカーソルX座標取得
	int GetOldPositionX() const { return positionX[1]; }

	// 前回のマウスカーソルY座標取得
	int GetOldPositionY() const { return positionY[1]; }

	// スクリーン幅設定
	void SetScreenWidth(int width) { screenWidth = width; }

	// スクリーン高さ設定
	void SetScreenHeight(int height) { screenHeight = height; }

	// スクリーン幅取得
	int GetScreenWidth() const { return screenWidth; }

	// スクリーン高さ取得
	int GetScreenHeight() const { return screenHeight; }

	// 移動量取得
	int GetAxisX() const { return axisX; }
	int GetAxisY() const { return axisY; }

	// カーソルロック設定
	void SetCursorLock(bool lock);

	// カーソル可視設定
	void SetCursorVisible(bool visible);

private:
	MouseButton		buttonState[2] = { 0 };
	MouseButton		buttonDown = 0;
	MouseButton		buttonUp = 0;
	int				positionX[2];
	int				positionY[2];
	int				wheel[2];
	int				screenWidth = 0;
	int				screenHeight = 0;
	int				axisX = 0;
	int				axisY = 0;
	bool			cursorLocked = false;
	bool			cursorVisible = true;
	HWND			hWnd = nullptr;
};
