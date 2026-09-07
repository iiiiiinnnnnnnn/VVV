#include "Gameplay/Player/LocalPlayerController.h"

#include <windows.h>

#include "Application/Input/Input.h"
#include "Application/Time/GameTime.h"

InputContext LocalPlayerController::Poll()
{
    constexpr float quickStepBufferDuration = 0.2f;
	// プレイ中のカーソル解放はScene側で管理するため、OSウィンドウのフォーカスだけを見る
    if (!Game::Input::IsFocusedWindow(true))
    {
        quickStepKeyHeld = false;
        quickStepDirectionMask = 0;
        quickStepBufferTimer = 0.0f;
		sprintLatched = false;
        return {};
    }

    auto& pad = Game::Input::Instance().GetGamePad();
    auto& mouse = Game::Input::Instance().GetMouse();

    InputContext context;
    const int quickStepKeys[4] = {
        'W',
        'S',
        'A',
        'D'
    };
    bool* quickStepPressed[4] = {
        &context.quickForwardPressed,
        &context.quickBackwardPressed,
        &context.quickLeftPressed,
        &context.quickRightPressed
    };
    bool* quickStepStarted[4] = {
        &context.quickForwardStarted,
        &context.quickBackwardStarted,
        &context.quickLeftStarted,
        &context.quickRightStarted
    };

    quickStepBufferTimer =
        std::max(
            quickStepBufferTimer -
            Game::Time::unscaledDeltaTime,
            0.0f);
    if (quickStepBufferTimer <= 0.0f)
        quickStepDirectionMask = 0;

    for (int index = 0; index < 4; ++index)
    {
        *quickStepPressed[index] =
            (quickStepDirectionMask &
             (1u << index)) != 0;
    }

    unsigned int heldDirectionMask = 0;
    for (int index = 0; index < 4; ++index)
    {
        if (GetAsyncKeyState(
            quickStepKeys[index]) &
            0x8000)
        {
            heldDirectionMask |=
                1u << index;
        }
    }
	constexpr float gamePadDodgeDirectionThreshold = 0.35f;
	if (pad.GetAxisLY() > gamePadDodgeDirectionThreshold) heldDirectionMask |= 1u << 0;
	if (pad.GetAxisLY() < -gamePadDodgeDirectionThreshold) heldDirectionMask |= 1u << 1;
	if (pad.GetAxisLX() < -gamePadDodgeDirectionThreshold) heldDirectionMask |= 1u << 2;
	if (pad.GetAxisLX() > gamePadDodgeDirectionThreshold) heldDirectionMask |= 1u << 3;

    const bool keyboardDodgeDown =
        (GetAsyncKeyState(VK_SPACE) &
         0x8000) != 0;
    const bool quickStepStartedNow =
        (keyboardDodgeDown && !quickStepKeyHeld) ||
		(pad.GetButtonDown() & GamePad::BTN_LEFT_TRIGGER);
    quickStepKeyHeld =
        keyboardDodgeDown;

    if (quickStepStartedNow)
    {
        if (heldDirectionMask == 0)
        {
            context.quickDefaultForwardStarted = true;
        }
        for (int index = 0; index < 4; ++index)
        {
            if ((heldDirectionMask &
                (1u << index)) == 0)
            {
                continue;
            }

            *quickStepPressed[index] = true;
            *quickStepStarted[index] = true;
        }

        quickStepDirectionMask =
            heldDirectionMask;
        quickStepBufferTimer =
            quickStepBufferDuration;
    }

    context.moveX = pad.GetAxisLX();
    context.moveZ = pad.GetAxisLY();

    if (fabsf(context.moveX) < 0.1f && fabsf(context.moveZ) < 0.1f)
    {
        if (GetAsyncKeyState('W') & 0x8000) context.moveZ = 1.0f;
        if (GetAsyncKeyState('S') & 0x8000) context.moveZ = -1.0f;
        if (GetAsyncKeyState('A') & 0x8000) context.moveX = -1.0f;
        if (GetAsyncKeyState('D') & 0x8000) context.moveX = 1.0f;

        const float length = sqrtf(
            context.moveX * context.moveX + context.moveZ * context.moveZ);
        if (length > 1.0f)
        {
            context.moveX /= length;
            context.moveZ /= length;
        }
    }

	const float movePower =
		context.moveX * context.moveX + context.moveZ * context.moveZ;
	if (movePower <= 0.01f)
	{
		sprintLatched = false;
	}
	else if ((pad.GetButtonDown() & GamePad::BTN_LEFT_THUMB) ||
		(GetAsyncKeyState(VK_LSHIFT) & 0x8000))
	{
		sprintLatched = true;
	}
	context.sprint = sprintLatched;
    context.crouch =
		(pad.GetButton() & GamePad::BTN_RIGHT_THUMB) ||
		(GetAsyncKeyState(VK_LCONTROL) & 0x8000);
    context.attackPressed =
		(pad.GetButtonDown() &
			(GamePad::BTN_A | GamePad::BTN_X | GamePad::BTN_RIGHT_TRIGGER)) ||
        (mouse.GetButtonDown() & Mouse::BTN_LEFT);

    return context;
}
