#include "LocalPlayer.h"
#include "Input.h"

InputContext LocalPlayer::Poll()
{
    if (!Game::Input::IsFocusedWindow()) return {};

    auto& pad = Game::Input::Instance().GetGamePad();
    auto& mouse = Game::Input::Instance().GetMouse();

    InputContext ctx;
    ctx.moveX = pad.GetAxisLX();
    ctx.moveZ = pad.GetAxisLY();
    ctx.jump = pad.GetButton() & GamePad::BTN_A;
    ctx.ready = (mouse.GetButton() & Mouse::BTN_RIGHT)
        | (pad.GetButton() & GamePad::BTN_RIGHT_SHOULDER);
    ctx.shoot = (mouse.GetButton() & Mouse::BTN_LEFT)
        | (pad.GetButton() & GamePad::BTN_LEFT_SHOULDER);
    ctx.sprint = pad.GetButton() & GamePad::BTN_LEFT_THUMB; // スティック押し込み
    return ctx;
}