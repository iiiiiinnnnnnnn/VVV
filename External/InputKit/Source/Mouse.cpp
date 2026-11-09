#include <InputKit/Mouse.h>

namespace InputKit
{
static const int KeyMap[] =
	{
		VK_LBUTTON, // 左ボタン
		VK_MBUTTON, // 中ボタン
		VK_RBUTTON, // 右ボタン
};

// コンストラクタ
Mouse::Mouse(HWND hWnd)
	: hWnd(hWnd)
{
}

// 更新
void Mouse::Update(UINT viewportWidth, UINT viewportHeight)
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
	buttonState[1] = buttonState[0]; // スイッチ履歴
	buttonState[0] = newButtonState;

	buttonDown = ~buttonState[1] & newButtonState; // 押した瞬間
	buttonUp = ~newButtonState & buttonState[1];   // 離した瞬間

	// ロック差分は常に実際のクライアント座標で計算する。
	POINT cursor{};
	RECT bounds{};
	if (!hWnd || GetForegroundWindow() != hWnd || !GetClientRect(hWnd, &bounds) ||
		!GetCursorPos(&cursor) || !ScreenToClient(hWnd, &cursor))
	{
		axisX = axisY = 0;
		buttonState[0] = buttonState[1] = buttonDown = buttonUp = 0;
		motionInitialized = false;
		return;
	}
	const LONG width = bounds.right - bounds.left;
	const LONG height = bounds.bottom - bounds.top;
	if (width <= 0 || height <= 0)
	{
		axisX = axisY = 0;
		motionInitialized = false;
		return;
	}
	const float scaleX = viewportWidth ? static_cast<float>(viewportWidth) / width : 1.0f;
	const float scaleY = viewportHeight ? static_cast<float>(viewportHeight) / height : 1.0f;
	positionX[1] = positionX[0];
	positionY[1] = positionY[0];
	positionX[0] = static_cast<int>(cursor.x * scaleX);
	positionY[0] = static_cast<int>(cursor.y * scaleY);
	if (cursorLocked)
	{
		const POINT center{width / 2, height / 2};
		axisX = motionInitialized ? cursor.x - center.x : 0;
		axisY = motionInitialized ? cursor.y - center.y : 0;
		POINT screenCenter = center;
		if (ClientToScreen(hWnd, &screenCenter)) SetCursorPos(screenCenter.x, screenCenter.y);
		positionX[0] = positionX[1] = static_cast<int>(center.x * scaleX);
		positionY[0] = positionY[1] = static_cast<int>(center.y * scaleY);
	}
	else
	{
		axisX = motionInitialized ? positionX[0] - positionX[1] : 0;
		axisY = motionInitialized ? positionY[0] - positionY[1] : 0;
	}
	motionInitialized = true;
}

void Mouse::SetCursorLock(bool lock)
{
	if (cursorLocked == lock) return;
	cursorLocked = lock;
	motionInitialized = false;
}

void Mouse::SetCursorVisible(bool visible)
{
	CURSORINFO info{sizeof(CURSORINFO)};
	const bool systemVisible = ::GetCursorInfo(&info) && (info.flags & CURSOR_SHOWING) != 0;
	// ImGuiのWin32バックエンドなどがShowCursorを直接変更する場合があるため、
	// 内部フラグだけでなくOS上の実表示も要求状態へ戻す。
	if (cursorVisible != visible || systemVisible != visible)
		ForceCursorVisible(visible);
}

void Mouse::ForceCursorVisible(bool visible)
{
	CURSORINFO info{sizeof(CURSORINFO)};
	if (::GetCursorInfo(&info) && ((info.flags & CURSOR_SHOWING) != 0) == visible)
	{
		cursorVisible = visible;
		return;
	}
	if (visible)
	{
		while (::ShowCursor(TRUE) < 0)
		{
		}
	}
	else
	{
		while (::ShowCursor(FALSE) >= 0)
		{
		}
	}
	cursorVisible = visible;
}

} // namespace InputKit
