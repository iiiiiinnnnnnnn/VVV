// LocalPlayerController.cpp

#include "Gameplay/Player/LocalPlayerController.h"

#include <windows.h>

#include "Application/Input/Input.h"
#include "Application/Time/GameTime.h"

InputContext LocalPlayerController::Poll()
{
    constexpr float quickStepBufferDuration = 0.2f;
    if (!Game::Input::IsFocusedWindow())
    {
        quickStepKeyHeld = false;
        quickStepDirectionMask = 0;
        quickStepBufferTimer = 0.0f;
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

    const bool quickStepChordDown =
        (GetAsyncKeyState(VK_SPACE) &
         0x8000) != 0 &&
        heldDirectionMask != 0;
    const bool quickStepStartedNow =
        quickStepChordDown &&
        !quickStepKeyHeld;
    quickStepKeyHeld =
        quickStepChordDown;

    if (quickStepStartedNow)
    {
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

    context.sprint =
        (pad.GetButton() & GamePad::BTN_LEFT_THUMB) ||
        (GetAsyncKeyState(VK_LSHIFT) & 0x8000);
    context.crouch = GetAsyncKeyState(VK_LCONTROL) & 0x8000;
    context.attackPressed =
        (pad.GetButtonDown() & GamePad::BTN_A) ||
        (mouse.GetButtonDown() & Mouse::BTN_LEFT);

    return context;
}
