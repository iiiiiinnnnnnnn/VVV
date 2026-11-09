#include <windows.h>
#include <Xinput.h>
#include <algorithm>
#include <cmath>
#include <InputKit/GamePad.h>

namespace InputKit
{
// 円形デッドゾーンを除去し、残った範囲を0～1へ詰める
static void NormalizeStick(SHORT rawX, SHORT rawY, SHORT deadZone, float& x, float& y)
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
static float NormalizeTrigger(BYTE value)
{
	if (value <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD) return 0.0f;
	return static_cast<float>(value - XINPUT_GAMEPAD_TRIGGER_THRESHOLD) /
		static_cast<float>(255 - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
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

	// 接続中の入力は毎フレーム取得し、未接続スロットの探索は1秒ごとに行う。
	XINPUT_STATE xinputState{};
	const auto now = GetTickCount64();
	if (connected || now >= nextConnectionCheck)
	{
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
		if (!connected) nextConnectionCheck = now + 1000;
	}
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

	// ボタン情報の更新
	{
		buttonState[1] = buttonState[0];	// スイッチ履歴
		buttonState[0] = newButtonState;

		buttonDown = ~buttonState[1] & newButtonState;	// 押した瞬間
		buttonUp = ~newButtonState & buttonState[1];	// 離した瞬間
	}
}

}
