#include "Mouse.h"

static const int KeyMap[] =
{
	VK_LBUTTON,		// 左ボタン
	VK_MBUTTON,		// 中ボタン
	VK_RBUTTON,		// 右ボタン
};

// コンストラクタ
Mouse::Mouse(HWND hWnd)
	: hWnd(hWnd)
{
	RECT rc;
	GetClientRect(hWnd, &rc);
	screenWidth = rc.right - rc.left;
	screenHeight = rc.bottom - rc.top;
}

// 更新
void Mouse::Update()
{
	// スイッチ情報
	MouseButton newButtonState = 0;

	for (int i = 0; i < ARRAYSIZE(KeyMap); ++i)
	{
		if (::GetAsyncKeyState(KeyMap[i]) & 0x8000)
		{
			newButtonState |= (1 << i);
		}
	}

	// ホイール
	wheel[1] = wheel[0];
	wheel[0] = 0;

	// ボタン情報更新
	buttonState[1] = buttonState[0];	// スイッチ履歴
	buttonState[0] = newButtonState;

	buttonDown = ~buttonState[1] & newButtonState;	// 押した瞬間
	buttonUp = ~newButtonState & buttonState[1];	// 離した瞬間

	// カーソル位置の取得
	POINT cursor;
	::GetCursorPos(&cursor);
	::ScreenToClient(hWnd, &cursor);

	// 画面のサイズを取得する。
	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT screenW = rc.right - rc.left;
	UINT screenH = rc.bottom - rc.top;
	UINT viewportW = screenWidth;
	UINT viewportH = screenHeight;

	// 画面補正
	positionX[1] = positionX[0];
	positionY[1] = positionY[0];
	positionX[0] = (LONG)(cursor.x / static_cast<float>(viewportW) * static_cast<float>(screenW));
	positionY[0] = (LONG)(cursor.y / static_cast<float>(viewportH) * static_cast<float>(screenH));

	// カーソルロック処理
	if (cursorLocked)
	{
		RECT rc;
		GetClientRect(hWnd, &rc);
		POINT center = { (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };

		// 中央からの差分をaxisに保存
		axisX = positionX[0] - center.x;
		axisY = positionY[0] - center.y;

		// カーソルを中央に戻す
		POINT screenCenter = center;
		ClientToScreen(hWnd, &screenCenter);
		SetCursorPos(screenCenter.x, screenCenter.y);

		// 次フレーム用に両方中央にリセット
		positionX[0] = center.x;
		positionX[1] = center.x;
		positionY[0] = center.y;
		positionY[1] = center.y;
	}
	else
	{
		axisX = positionX[0] - positionX[1];
		axisY = positionY[0] - positionY[1];
	}
}

void Mouse::SetCursorLock(bool lock)
{
	cursorLocked = lock;
}

void Mouse::SetCursorVisible(bool visible)
{
	if (cursorVisible != visible)
	{
		ShowCursor(visible);
		cursorVisible = visible;
	}
}
