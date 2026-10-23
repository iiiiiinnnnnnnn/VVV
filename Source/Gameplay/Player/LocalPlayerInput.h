#pragma once

#include "Application/Time/Timer.h"

#include <array>
#include <string>
#include <vector>

#include "Gameplay/Player/PlayerInput.h"

class LocalPlayerInput : public PlayerInput
{
public:
    LocalPlayerInput(Object* owner);
    InputContext Poll() override;
    void OnDrawGUI() override;
    void OnDisabled() override { ResetRuntime(); }
    const char* GetDebugName() const override { return ICON_FA_GAMEPAD " LocalPlayerInput"; }

private:
    enum class Action { Forward, Backward, Left, Right, Sprint, QuickShift, Crouch, Attack, AlignCamera, Count };
    enum class Device { Keyboard, Mouse, GamePad };
    enum class Axis { None, LeftX, LeftY, RightX, RightY };
    struct Binding
    {
        Device device = Device::Keyboard;
        int code = 'W';
        bool downOnly = false;
    };
    struct ButtonState
    {
        bool held = false;
        bool down = false;
    };
    struct Settings
    {
        std::array<std::vector<Binding>, static_cast<size_t>(Action::Count)> bindings;
        Axis moveXAxis = Axis::LeftX;
        Axis moveZAxis = Axis::LeftY;
        float moveDeadZone = 0.1f;
        float sprintStopThreshold = 0.1f;
        float dodgeDirectionThreshold = 0.35f;
        float dodgeBufferSeconds = 0.2f;
        bool normalizeKeyboardMovement = true;
        bool latchSprint = true;
        bool defaultForwardDodge = true;
    };

    static const char* ActionName(Action action);
    static std::string KeyName(int key);
    static Settings Defaults();
    float ReadAxis(Axis axis) const;
    ButtonState ReadButton(Action action) const;
    void ResetRuntime();
    bool Load();
    bool Save() const;

    Settings settings;
    std::string settingsPath;
    std::string status;
    InputContext previousInput;
    std::array<ButtonState, static_cast<size_t>(Action::Count)> rawStates{};
    std::array<bool, 256> keys{};
    std::array<bool, 256> previousKeys{};
    unsigned int bufferedDirections = 0;
    Game::Timer bufferTimer{0.0f};
    bool sprintLatched = false;
};
