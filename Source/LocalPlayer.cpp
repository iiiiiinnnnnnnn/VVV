// LocalPlayer.cpp

#include "LocalPlayer.h"
#include "Input.h"
#include <windows.h>

InputContext LocalPlayer::Poll()
{
    if (!Game::Input::IsFocusedWindow()) return {};

    auto& pad   = Game::Input::Instance().GetGamePad();
    auto& mouse = Game::Input::Instance().GetMouse();

    InputContext ctx;

    // ---- ゲームパッド入力 ----
    ctx.moveX = pad.GetAxisLX();
    ctx.moveZ = pad.GetAxisLY();

    // ---- キーボード（WASD）入力 ----
    // ゲームパッドの軸入力がほぼゼロのときだけキーボードで上書き
    if (fabsf(ctx.moveX) < 0.1f && fabsf(ctx.moveZ) < 0.1f)
    {
        if (GetAsyncKeyState('W') & 0x8000) ctx.moveZ =  1.0f;
        if (GetAsyncKeyState('S') & 0x8000) ctx.moveZ = -1.0f;
        if (GetAsyncKeyState('A') & 0x8000) ctx.moveX = -1.0f;
        if (GetAsyncKeyState('D') & 0x8000) ctx.moveX =  1.0f;

        // 斜め入力を正規化（√2 倍にならないよう）
        float len = sqrtf(ctx.moveX * ctx.moveX + ctx.moveZ * ctx.moveZ);
        if (len > 1.0f)
        {
            ctx.moveX /= len;
            ctx.moveZ /= len;
        }
    }

    // ---- ボタン入力 ----
    ctx.sprint  = (pad.GetButton() & GamePad::BTN_LEFT_THUMB)
        | (GetAsyncKeyState(VK_LSHIFT) & 0x8000 ? 1 : 0);

    ctx.crouch  = (GetAsyncKeyState(VK_LCONTROL) & 0x8000 ? 1 : 0);

    ctx.attackPressed = (pad.GetButtonDown() & GamePad::BTN_A)
		| (mouse.GetButtonDown() & Mouse::BTN_LEFT);

    return ctx;
}
