// LocalPlayerController.cpp

#include "Gameplay/Player/LocalPlayerController.h"

#include <windows.h>

#include "Application/Input/Input.h"

InputContext LocalPlayerController::Poll()
{
    if (!Game::Input::IsFocusedWindow()) return {};

    auto& pad = Game::Input::Instance().GetGamePad();
    auto& mouse = Game::Input::Instance().GetMouse();

    InputContext context;
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
