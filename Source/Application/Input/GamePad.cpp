#include <windows.h>
#include <Xinput.h>
#include <algorithm>
#include <cmath>
#include "Application/Input/GamePad.h"

namespace
{
// 円形デッドゾーンを除去し、残った範囲を0～1へ詰める
void NormalizeStick(SHORT rawX, SHORT rawY, SHORT deadZone, float& x, float& y)
{
	const float sourceX = static_cast<float>(rawX);
	const float sourceY = static_cast<float>(rawY);
	const float magnitude = std::sqrt(sourceX * sourceX + sourceY * sourceY);
	if (magnitude <= static_cast<float>(deadZone))
	{
		x = 0.0f;
		y = 0.0f;
		return;
	}

	constexpr float maximumMagnitude = 32767.0f;
	const float normalizedMagnitude = std::clamp(
		(magnitude - static_cast<float>(deadZone)) /
		(maximumMagnitude - static_cast<float>(deadZone)), 0.0f, 1.0f);
	x = sourceX / magnitude * normalizedMagnitude;
	y = sourceY / magnitude * normalizedMagnitude;
}

// トリガーの遊びを除去し、残った範囲を0～1へ詰める
float NormalizeTrigger(BYTE value)
{
	if (value <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD) return 0.0f;
	return static_cast<float>(value - XINPUT_GAMEPAD_TRIGGER_THRESHOLD) /
		static_cast<float>(255 - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
}
}

// 更新
void GamePad::Update(bool acceptsInput)
{
	axisLx = axisLy = 0.0f;
	axisRx = axisRy = 0.0f;
	triggerL = triggerR = 0.0f;
	if (!acceptsInput)
	{
		buttonState[0] = buttonState[1] = 0;
		buttonDown = buttonUp = 0;
		return;
	}

	GamePadButton newButtonState = 0;

	// 指定スロットが未接続なら、接続中のXboxコントローラーを自動検出
	XINPUT_STATE xinputState{};
	DWORD inputResult = XInputGetState(slot, &xinputState);
	if (inputResult != ERROR_SUCCESS)
	{
		for (DWORD candidate = 0; candidate < XUSER_MAX_COUNT; ++candidate)
		{
			if (static_cast<int>(candidate) == slot) continue;
			XINPUT_STATE candidateState{};
			if (XInputGetState(candidate, &candidateState) != ERROR_SUCCESS) continue;
			slot = static_cast<int>(candidate);
			xinputState = candidateState;
			inputResult = ERROR_SUCCESS;
			break;
		}
	}
	connected = inputResult == ERROR_SUCCESS;
	if (connected)
	{
		const XINPUT_GAMEPAD& pad = xinputState.Gamepad;
		if (pad.wButtons & XINPUT_GAMEPAD_DPAD_UP) newButtonState |= BTN_UP;
		if (pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) newButtonState |= BTN_RIGHT;
		if (pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) newButtonState |= BTN_DOWN;
		if (pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) newButtonState |= BTN_LEFT;
		if (pad.wButtons & XINPUT_GAMEPAD_A) newButtonState |= BTN_A;
		if (pad.wButtons & XINPUT_GAMEPAD_B) newButtonState |= BTN_B;
		if (pad.wButtons & XINPUT_GAMEPAD_X) newButtonState |= BTN_X;
		if (pad.wButtons & XINPUT_GAMEPAD_Y) newButtonState |= BTN_Y;
		if (pad.wButtons & XINPUT_GAMEPAD_START) newButtonState |= BTN_START;
		if (pad.wButtons & XINPUT_GAMEPAD_BACK) newButtonState |= BTN_BACK;
		if (pad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) newButtonState |= BTN_LEFT_THUMB;
		if (pad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) newButtonState |= BTN_RIGHT_THUMB;
		if (pad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) newButtonState |= BTN_LEFT_SHOULDER;
		if (pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) newButtonState |= BTN_RIGHT_SHOULDER;

		triggerL = NormalizeTrigger(pad.bLeftTrigger);
		triggerR = NormalizeTrigger(pad.bRightTrigger);
		if (triggerL > 0.0f) newButtonState |= BTN_LEFT_TRIGGER;
		if (triggerR > 0.0f) newButtonState |= BTN_RIGHT_TRIGGER;
		NormalizeStick(pad.sThumbLX, pad.sThumbLY,
			XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, axisLx, axisLy);
		NormalizeStick(pad.sThumbRX, pad.sThumbRY,
			XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, axisRx, axisRy);
	}

	// キーボードでエミュレーション
	{
		float lx = 0.0f;
		float ly = 0.0f;
		float rx = 0.0f;
		float ry = 0.0f;
		if (GetAsyncKeyState('W') & 0x8000) ly = 1.0f;
		if (GetAsyncKeyState('A') & 0x8000) lx = -1.0f;
		if (GetAsyncKeyState('S') & 0x8000) ly = -1.0f;
		if (GetAsyncKeyState('D') & 0x8000) lx = 1.0f;
		if (GetAsyncKeyState('I') & 0x8000) ry = 1.0f;
		if (GetAsyncKeyState('J') & 0x8000) rx = -1.0f;
		if (GetAsyncKeyState('K') & 0x8000) ry = -1.0f;
		if (GetAsyncKeyState('L') & 0x8000) rx = 1.0f;
		if (GetAsyncKeyState('X') & 0x8000) newButtonState |= BTN_B;
		if (GetAsyncKeyState('C') & 0x8000) newButtonState |= BTN_X;
		if (GetAsyncKeyState('V') & 0x8000) newButtonState |= BTN_Y;
		if (GetAsyncKeyState(VK_UP) & 0x8000)	newButtonState |= BTN_UP;
		if (GetAsyncKeyState(VK_RIGHT) & 0x8000)	newButtonState |= BTN_RIGHT;
		if (GetAsyncKeyState(VK_DOWN) & 0x8000)	newButtonState |= BTN_DOWN;
		if (GetAsyncKeyState(VK_LEFT) & 0x8000)	newButtonState |= BTN_LEFT;

		// デバッグ用のF1～F12
		if (GetAsyncKeyState(VK_F1) & 0x8000) newButtonState |= BTN_F1;
		if (GetAsyncKeyState(VK_F2) & 0x8000) newButtonState |= BTN_F2;
		if (GetAsyncKeyState(VK_F3) & 0x8000) newButtonState |= BTN_F3;
		if (GetAsyncKeyState(VK_F4) & 0x8000) newButtonState |= BTN_F4;
		if (GetAsyncKeyState(VK_F5) & 0x8000) newButtonState |= BTN_F5;
		if (GetAsyncKeyState(VK_F6) & 0x8000) newButtonState |= BTN_F6;
		if (GetAsyncKeyState(VK_F7) & 0x8000) newButtonState |= BTN_F7;
		if (GetAsyncKeyState(VK_F8) & 0x8000) newButtonState |= BTN_F8;
		if (GetAsyncKeyState(VK_F9) & 0x8000) newButtonState |= BTN_F9;
		if (GetAsyncKeyState(VK_F10) & 0x8000) newButtonState |= BTN_F10;
		if (GetAsyncKeyState(VK_F11) & 0x8000) newButtonState |= BTN_F11;
		if (GetAsyncKeyState(VK_F12) & 0x8000) newButtonState |= BTN_F12;
		if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) newButtonState |= BTN_ESCAPE;

#if 1
		if (newButtonState & BTN_UP)    ly = 1.0f;
		if (newButtonState & BTN_RIGHT) lx = 1.0f;
		if (newButtonState & BTN_DOWN)  ly = -1.0f;
		if (newButtonState & BTN_LEFT)  lx = -1.0f;
#endif

		if (lx != 0.0f || ly != 0.0f)
		{
			float power = std::sqrt(lx * lx + ly * ly);
			axisLx = lx / power;
			axisLy = ly / power;
		}

		if (rx != 0.0f || ry != 0.0f)
		{
			float power = std::sqrt(rx * rx + ry * ry);
			axisRx = rx / power;
			axisRy = ry / power;
		}
	}

	// ボタン情報の更新
	{
		buttonState[1] = buttonState[0];	// スイッチ履歴
		buttonState[0] = newButtonState;

		buttonDown = ~buttonState[1] & newButtonState;	// 押した瞬間
		buttonUp = ~newButtonState & buttonState[1];	// 離した瞬間
	}
}
