#include "Gameplay/Player/LocalPlayerInput.h"

#include <algorithm>
#include <cmath>

#include <InputKit/InputKit.h>
#include "Application/Time/GameTime.h"

const char* LocalPlayerInput::ActionName(Action action)
{
    static const char* names[] = { "Move Forward", "Move Backward", "Move Left", "Move Right",
        "Sprint", "QuickShift", "Crouch", "Attack", "Align Camera" };
    return names[static_cast<size_t>(action)];
}

LocalPlayerInput::LocalPlayerInput(Object* owner) : PlayerInput(owner)
{
}

void LocalPlayerInput::ResetRuntime()
{
    previousInput = {};
    rawStates = {};
    bufferedDirections = 0;
    bufferTimer.Stop();
    sprintLatched = false;
}

bool LocalPlayerInput::IsControlHeld(Control control) const
{
    if (control == Control::None) return false;
    auto& input = InputKit::InputSystem::Instance();
    if (!IsActive() || !input.CanReadActions()) return false;
    switch (control)
    {
    case Control::Pause: return input.IsPressed("PlayerInput/Pause");
    case Control::Move:
    case Control::Camera:
    {
        const auto value = input.ReadValue<InputKit::Vector2>(control == Control::Move ? "PlayerInput/Move" : "PlayerInput/Camera");
        return std::abs(value.x) > 0.0001f || std::abs(value.y) > 0.0001f;
    }
    case Control::Attack: return input.IsPressed("PlayerInput/Attack", true);
    case Control::Sprint: return input.IsPressed("PlayerInput/Sprint", true);
    case Control::Dodge: return input.IsPressed("PlayerInput/QuickShift", true);
    case Control::Crouch: return input.IsPressed("PlayerInput/Crouch", true);
    case Control::AlignCamera: return input.IsPressed("PlayerInput/Align Camera", true);
    default: return false;
    }
}

bool LocalPlayerInput::PollAttackButton()
{
    if (!IsActive() || !InputKit::InputSystem::Instance().CanReadActions())
    {
        ResetRuntime();
        return false;
    }
	return InputKit::InputSystem::Instance().IsDown("PlayerInput/Attack");
}

InputContext LocalPlayerInput::Poll()
{
    if (!IsActive() || !InputKit::InputSystem::Instance().CanReadActions())
    {
        ResetRuntime();
        return {};
    }

    const auto& input = InputKit::InputSystem::Instance();
    for (int i = 0; i < static_cast<int>(Action::Count); ++i)
    {
        const std::string action = std::string("PlayerInput/") + ActionName(static_cast<Action>(i));
        rawStates[i].held = input.IsPressed(action);
        rawStates[i].down = input.IsDown(action);
        rawStates[i].up = input.IsUp(action);
    }

    InputContext context;
    const auto move = InputKit::InputSystem::Instance().ReadValue<InputKit::Vector2>("PlayerInput/Move");
    context.moveX = move.x;
    context.moveZ = move.y;

    const float movePower = context.moveX * context.moveX + context.moveZ * context.moveZ;
    if (movePower <= 0.1f * 0.1f)
        sprintLatched = false;
    else if (rawStates[static_cast<size_t>(Action::Sprint)].held)
        sprintLatched = rawStates[static_cast<size_t>(Action::Sprint)].held;
    context.Set(PlayerAction::Sprint, sprintLatched, sprintLatched && !previousInput.Held(PlayerAction::Sprint));

    // 回避方向は移動の優先順位とは独立して、両方のデバイスを受け付ける。
    bufferTimer.Update(Game::Time::unscaledDeltaTime);
    if (bufferTimer.GetRemaining() <= 0.0f) bufferedDirections = 0;
    unsigned int pressed = bufferedDirections;
    unsigned int started = 0;
    const bool dodgeDown = rawStates[static_cast<size_t>(Action::QuickShift)].down;
    if (dodgeDown)
    {
		auto& input = InputKit::InputSystem::Instance();
		if (input.IsPressed("PlayerInput/DodgeForward")) started |= 1u << 0;
		if (input.IsPressed("PlayerInput/DodgeBackward")) started |= 1u << 1;
		if (input.IsPressed("PlayerInput/DodgeLeft")) started |= 1u << 2;
		if (input.IsPressed("PlayerInput/DodgeRight")) started |= 1u << 3;
        pressed |= started;
        bufferedDirections = started;
        bufferTimer.Reset(0.2f);
    }
    const PlayerAction directions[] = { PlayerAction::QuickForward, PlayerAction::QuickBackward,
        PlayerAction::QuickLeft, PlayerAction::QuickRight };
    for (int i = 0; i < 4; ++i)
        context.Set(directions[i], (pressed & (1u << i)) != 0, (started & (1u << i)) != 0);
    const bool defaultDodge = dodgeDown && started == 0;
    context.Set(PlayerAction::QuickDefaultForward, defaultDodge, defaultDodge);

    const Action buttons[] = { Action::Crouch, Action::Attack, Action::AlignCamera };
    const PlayerAction actions[] = { PlayerAction::Crouch, PlayerAction::Attack, PlayerAction::AlignCamera };
    for (int i = 0; i < 3; ++i)
    {
        const ButtonState state = rawStates[static_cast<size_t>(buttons[i])];
        context.Set(actions[i], state.held, state.down);
    }
    context.UpdateReleases(previousInput);
    previousInput = context;
    return context;
}
