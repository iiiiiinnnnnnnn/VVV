#pragma once

#include "Application/Time/Timer.h"

#include <array>
#include <InputKit/InputKit.h>
#include <string>
#include <vector>

#include "Gameplay/Player/PlayerInput.h"

class LocalPlayerInput : public PlayerInput
{
public:
    enum class Control { None, Pause, Move, Camera, Attack, Sprint, Dodge, Crouch, AlignCamera };
    bool IsControlHeld(Control control) const;


    LocalPlayerInput(Object* owner);
    InputContext Poll() override;
    bool PollAttackButton();

    void OnDisabled() override { ResetRuntime(); }
    const char* GetDebugName() const override { return ICON_FA_GAMEPAD " LocalPlayerInput"; }

private:
    enum class Action { Forward, Backward, Left, Right, Sprint, QuickShift, Crouch, Attack, AlignCamera, Count };
    using ButtonState = InputKit::ButtonState;
    static const char* ActionName(Action action);
    void ResetRuntime();

    InputContext previousInput;
    std::array<ButtonState, static_cast<size_t>(Action::Count)> rawStates{};
    unsigned int bufferedDirections = 0;
    Game::Timer bufferTimer{0.0f};
    bool sprintLatched = false;
};
