#include "Gameplay/Player/LocalPlayerInput.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <utility>

#include "Application/Input/Input.h"
#include "Application/Time/GameTime.h"
#include "Core/Foundation/Json.h"

const char* LocalPlayerInput::ActionName(Action action)
{
    static const char* names[] = { "Move Forward", "Move Backward", "Move Left", "Move Right",
        "Sprint", "QuickShift", "Crouch", "Attack", "Align Camera" };
    return names[static_cast<size_t>(action)];
}

std::string LocalPlayerInput::KeyName(int key)
{
    char name[128]{};
    UINT scan = MapVirtualKeyA(key, MAPVK_VK_TO_VSC);
    if (key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN ||
        key == VK_INSERT || key == VK_DELETE || key == VK_HOME || key == VK_END ||
        key == VK_PRIOR || key == VK_NEXT || key == VK_RCONTROL || key == VK_RMENU)
        scan |= 0x100;
    if (GetKeyNameTextA(static_cast<LONG>(scan << 16), name, sizeof(name)))
        return std::string(name) + " (" + std::to_string(key) + ")";
    return "VK " + std::to_string(key);
}

LocalPlayerInput::Settings LocalPlayerInput::Defaults()
{
    Settings result;
    const auto bind = [&result](Action action, Device device, int code, bool downOnly = false) {
        result.bindings[static_cast<size_t>(action)].push_back({ device, code, downOnly });
    };
    bind(Action::Forward, Device::Keyboard, 'W');
    bind(Action::Backward, Device::Keyboard, 'S');
    bind(Action::Left, Device::Keyboard, 'A');
    bind(Action::Right, Device::Keyboard, 'D');
    bind(Action::Sprint, Device::Keyboard, VK_LSHIFT);
    bind(Action::Sprint, Device::GamePad, GamePad::BTN_LEFT_THUMB, true);
    bind(Action::QuickShift, Device::Keyboard, VK_SPACE);
    bind(Action::QuickShift, Device::GamePad, GamePad::BTN_LEFT_TRIGGER);
    bind(Action::Crouch, Device::Keyboard, VK_CONTROL);
    bind(Action::Crouch, Device::GamePad, GamePad::BTN_RIGHT_THUMB);
    bind(Action::Attack, Device::Mouse, Mouse::BTN_LEFT);
    bind(Action::Attack, Device::GamePad, GamePad::BTN_A);
    bind(Action::Attack, Device::GamePad, GamePad::BTN_X);
    bind(Action::Attack, Device::GamePad, GamePad::BTN_RIGHT_TRIGGER);
    bind(Action::AlignCamera, Device::Mouse, Mouse::BTN_RIGHT);
    return result;
}

LocalPlayerInput::LocalPlayerInput(Object* owner) : PlayerInput(owner), settings(Defaults())
{
    auto directory = std::filesystem::current_path();
    settingsPath = (directory / "Resources/Input/Player.playerinput").string();
    while (!directory.empty())
    {
        if (std::filesystem::exists(directory / "VEER.sln"))
        {
            settingsPath = (directory / "Resources/Input/Player.playerinput").string();
            break;
        }
        if (directory == directory.root_path()) break;
        directory = directory.parent_path();
    }
    status = Load() ? "Loaded." : "Using defaults (settings missing or invalid).";
}

void LocalPlayerInput::ResetRuntime()
{
    previousInput = {};
    rawStates = {};
    keys.fill(false);
    previousKeys.fill(false);
    bufferedDirections = 0;
    bufferTimer.Stop();
    sprintLatched = false;
}

float LocalPlayerInput::ReadAxis(Axis axis) const
{
    const auto& pad = Game::Input::Instance().GetGamePad();
    switch (axis)
    {
    case Axis::LeftX: return pad.GetAxisLX();
    case Axis::LeftY: return pad.GetAxisLY();
    case Axis::RightX: return pad.GetAxisRX();
    case Axis::RightY: return pad.GetAxisRY();
    default: return 0.0f;
    }
}

LocalPlayerInput::ButtonState LocalPlayerInput::ReadButton(Action action) const
{
    auto& input = Game::Input::Instance();
    ButtonState result;
    for (const Binding& binding : settings.bindings[static_cast<size_t>(action)])
    {
        bool held = false;
        bool down = false;
        switch (binding.device)
        {
        case Device::Keyboard:
            held = keys[binding.code];
            down = held && !previousKeys[binding.code];
            break;
        case Device::Mouse:
            held = (input.GetMouse().GetButton() & binding.code) != 0;
            down = (input.GetMouse().GetButtonDown() & binding.code) != 0;
            break;
        case Device::GamePad:
            held = (input.GetGamePad().GetButton() & binding.code) != 0;
            down = (input.GetGamePad().GetButtonDown() & binding.code) != 0;
            break;
        }
        result.held |= binding.downOnly ? down : held;
        result.down |= down;
    }
    return result;
}

InputContext LocalPlayerInput::Poll()
{
    const auto& io = ImGui::GetIO();
    if (!IsActive() || Game::Input::Instance().IsGameplayInputSuppressed() ||
        !Game::Input::IsFocusedWindow(true) || io.WantCaptureMouse || io.WantCaptureKeyboard || io.WantTextInput)
    {
        ResetRuntime();
        return {};
    }
    for (int key = 0; key < static_cast<int>(keys.size()); ++key)
        keys[key] = (GetAsyncKeyState(key) & 0x8000) != 0;

    for (int i = 0; i < static_cast<int>(Action::Count); ++i)
        rawStates[i] = ReadButton(static_cast<Action>(i));

    const bool forward = rawStates[static_cast<size_t>(Action::Forward)].held;
    const bool backward = rawStates[static_cast<size_t>(Action::Backward)].held;
    const bool left = rawStates[static_cast<size_t>(Action::Left)].held;
    const bool right = rawStates[static_cast<size_t>(Action::Right)].held;
    const float stickX = ReadAxis(settings.moveXAxis);
    const float stickZ = ReadAxis(settings.moveZAxis);
    InputContext context;
    context.moveX = stickX;
    context.moveZ = stickZ;
    if (fabsf(stickX) < settings.moveDeadZone && fabsf(stickZ) < settings.moveDeadZone)
    {
        if (forward) context.moveZ = 1.0f;
        if (backward) context.moveZ = -1.0f;
        if (left) context.moveX = -1.0f;
        if (right) context.moveX = 1.0f;
        const float length = sqrtf(context.moveX * context.moveX + context.moveZ * context.moveZ);
        if (settings.normalizeKeyboardMovement && length > 1.0f)
        {
            context.moveX /= length;
            context.moveZ /= length;
        }
    }

    const float movePower = context.moveX * context.moveX + context.moveZ * context.moveZ;
    if (movePower <= settings.sprintStopThreshold * settings.sprintStopThreshold)
        sprintLatched = false;
    else if (!settings.latchSprint || rawStates[static_cast<size_t>(Action::Sprint)].held)
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
        if (forward || stickZ > settings.dodgeDirectionThreshold) started |= 1u << 0;
        if (backward || stickZ < -settings.dodgeDirectionThreshold) started |= 1u << 1;
        if (left || stickX < -settings.dodgeDirectionThreshold) started |= 1u << 2;
        if (right || stickX > settings.dodgeDirectionThreshold) started |= 1u << 3;
        pressed |= started;
        bufferedDirections = started;
        bufferTimer.Reset(settings.dodgeBufferSeconds);
    }
    const PlayerAction directions[] = { PlayerAction::QuickForward, PlayerAction::QuickBackward,
        PlayerAction::QuickLeft, PlayerAction::QuickRight };
    for (int i = 0; i < 4; ++i)
        context.Set(directions[i], (pressed & (1u << i)) != 0, (started & (1u << i)) != 0);
    const bool defaultDodge = settings.defaultForwardDodge && dodgeDown && started == 0;
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
    previousKeys = keys;
    return context;
}

bool LocalPlayerInput::Load()
{
    try
    {
        std::ifstream stream(settingsPath);
        if (!stream) return false;
        json root;
        stream >> root;
        if (root.at("version").get<int>() != 1) return false;
        Settings loaded = Defaults();
        const json& movement = root.at("movement");
        loaded.moveXAxis = static_cast<Axis>(movement.at("xAxis").get<int>());
        loaded.moveZAxis = static_cast<Axis>(movement.at("zAxis").get<int>());
        loaded.moveDeadZone = movement.at("deadZone").get<float>();
        loaded.sprintStopThreshold = movement.at("sprintStopThreshold").get<float>();
        loaded.normalizeKeyboardMovement = movement.at("normalizeKeyboard").get<bool>();
        loaded.latchSprint = root.at("latchSprint").get<bool>();
        loaded.dodgeDirectionThreshold = root.at("dodge").at("directionThreshold").get<float>();
        loaded.dodgeBufferSeconds = root.at("dodge").at("bufferSeconds").get<float>();
        loaded.defaultForwardDodge = root.at("dodge").at("defaultForward").get<bool>();
        const auto validUnit = [](float value) { return std::isfinite(value) && value >= 0.0f && value <= 1.0f; };
        if (static_cast<int>(loaded.moveXAxis) < 0 || static_cast<int>(loaded.moveXAxis) > 4 ||
            static_cast<int>(loaded.moveZAxis) < 0 || static_cast<int>(loaded.moveZAxis) > 4 ||
            !validUnit(loaded.moveDeadZone) || loaded.moveDeadZone == 0.0f ||
            !validUnit(loaded.sprintStopThreshold) || !validUnit(loaded.dodgeDirectionThreshold) ||
            !std::isfinite(loaded.dodgeBufferSeconds) || loaded.dodgeBufferSeconds < 0.0f ||
            loaded.dodgeBufferSeconds > 5.0f) return false;
        for (int i = 0; i < static_cast<int>(Action::Count); ++i)
        {
            const json& entries = root.at("actions").at(ActionName(static_cast<Action>(i)));
            if (!entries.is_array()) return false;
            auto& bindings = loaded.bindings[i];
            bindings.clear();
            for (const auto& entry : entries)
            {
                const std::string device = entry.at("device").get<std::string>();
                Binding binding;
                if (device == "Keyboard") binding.device = Device::Keyboard;
                else if (device == "Mouse") binding.device = Device::Mouse;
                else if (device == "GamePad") binding.device = Device::GamePad;
                else return false;
                binding.code = entry.at("code").get<int>();
                binding.downOnly = entry.value("downOnly", false);
                if (binding.device == Device::Keyboard && (binding.code < 1 || binding.code > 254)) return false;
                if (binding.device == Device::Mouse && binding.code != 1 && binding.code != 2 && binding.code != 4) return false;
                if (binding.device == Device::GamePad && (binding.code <= 0 || binding.code > (1 << 15) ||
                    (binding.code & (binding.code - 1)) != 0)) return false;
                bindings.push_back(binding);
            }
        }
        settings = std::move(loaded);
        ResetRuntime();
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool LocalPlayerInput::Save() const
{
    try
    {
        json root;
        root["version"] = 1;
        root["movement"] = { {"xAxis", static_cast<int>(settings.moveXAxis)},
            {"zAxis", static_cast<int>(settings.moveZAxis)}, {"deadZone", settings.moveDeadZone},
            {"sprintStopThreshold", settings.sprintStopThreshold},
            {"normalizeKeyboard", settings.normalizeKeyboardMovement} };
        root["latchSprint"] = settings.latchSprint;
        root["dodge"] = { {"directionThreshold", settings.dodgeDirectionThreshold},
            {"bufferSeconds", settings.dodgeBufferSeconds}, {"defaultForward", settings.defaultForwardDodge} };
        const char* devices[] = { "Keyboard", "Mouse", "GamePad" };
        for (int i = 0; i < static_cast<int>(Action::Count); ++i)
        {
            json entries = json::array();
            for (const Binding& binding : settings.bindings[i])
                entries.push_back({ {"device", devices[static_cast<int>(binding.device)]},
                    {"code", binding.code}, {"downOnly", binding.downOnly} });
            root["actions"][ActionName(static_cast<Action>(i))] = std::move(entries);
        }
        const auto path = std::filesystem::path(settingsPath);
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
        std::ofstream stream(path);
        if (!stream) return false;
        stream << root.dump(4) << '\n';
        stream.close();
        return !stream.fail();
    }
    catch (const std::exception&)
    {
        return false;
    }
}

void LocalPlayerInput::OnDrawGUI()
{
    ImGui::TextWrapped("%s", settingsPath.c_str());
    bool changed = false;
    if (ImGui::Button("Save")) status = Save() ? "Saved." : "Save failed.";
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
    {
        changed = Load();
        status = changed ? "Loaded." : "Load failed. Current settings preserved.";
    }
    ImGui::SameLine();
    if (ImGui::Button("Restore Defaults"))
    {
        settings = Defaults();
        changed = true;
        status = "Defaults restored. Save to keep changes.";
    }
    ImGui::TextWrapped("%s", status.c_str());

    if (ImGui::CollapsingHeader("Movement / Sprint / Dodge", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const char* axes[] = { "None", "Left Stick X", "Left Stick Y", "Right Stick X", "Right Stick Y" };
        int xAxis = static_cast<int>(settings.moveXAxis);
        int zAxis = static_cast<int>(settings.moveZAxis);
        if (ImGui::Combo("Move X Axis", &xAxis, axes, 5)) { settings.moveXAxis = static_cast<Axis>(xAxis); changed = true; }
        if (ImGui::Combo("Move Z Axis", &zAxis, axes, 5)) { settings.moveZAxis = static_cast<Axis>(zAxis); changed = true; }
        changed |= ImGui::SliderFloat("Stick Dead Zone", &settings.moveDeadZone, 0.01f, 1.0f);
        ImGui::TextWrapped("Stick has priority. Direction buttons are used inside the dead zone; Backward / Right win opposing inputs.");
        changed |= ImGui::Checkbox("Normalize Button Movement", &settings.normalizeKeyboardMovement);
        changed |= ImGui::Checkbox("Keep Sprint Until Stopped", &settings.latchSprint);
        changed |= ImGui::SliderFloat("Sprint Stop Threshold", &settings.sprintStopThreshold, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Dodge Direction Threshold", &settings.dodgeDirectionThreshold, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Dodge Buffer Seconds", &settings.dodgeBufferSeconds, 0.0f, 5.0f, "%.2f");
        changed |= ImGui::Checkbox("Dodge Forward Without Direction", &settings.defaultForwardDodge);
        ImGui::TextWrapped("Dodge combines direction buttons and the selected stick axes. Its Held state lasts for the buffer duration.");
        ImGui::TextWrapped("Bindings are combined with OR. Pulse on Down makes Held active for one frame; Down always reports a press edge.");
    }

    const char* devices[] = { "Keyboard", "Mouse", "GamePad" };
    const char* mouseNames[] = { "Left", "Middle", "Right" };
    const char* padNames[] = { "D-Pad Up", "D-Pad Right", "D-Pad Down", "D-Pad Left",
        "A", "B", "X", "Y", "Start", "Back", "Left Thumb", "Right Thumb",
        "Left Shoulder", "Right Shoulder", "Left Trigger", "Right Trigger" };
    for (int i = 0; i < static_cast<int>(Action::Count); ++i)
    {
        ImGui::PushID(i);
        if (ImGui::TreeNodeEx(ActionName(static_cast<Action>(i)), ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto& bindings = settings.bindings[i];
            int removeIndex = -1;
            for (int j = 0; j < static_cast<int>(bindings.size()); ++j)
            {
                ImGui::PushID(j);
                Binding& binding = bindings[j];
                int device = static_cast<int>(binding.device);
                ImGui::SetNextItemWidth(110.0f);
                if (ImGui::Combo("##Device", &device, devices, 3))
                {
                    binding.device = static_cast<Device>(device);
                    binding.code = device == 0 ? 'W' : 1;
                    changed = true;
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(170.0f);
                if (binding.device == Device::Keyboard)
                {
                    if (ImGui::BeginCombo("##Key", KeyName(binding.code).c_str()))
                    {
                        for (int key = 1; key < 255; ++key)
                        {
                            ImGui::PushID(key);
                            if (ImGui::Selectable(KeyName(key).c_str(), key == binding.code))
                            {
                                binding.code = key;
                                changed = true;
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndCombo();
                    }
                }
                else
                {
                    int index = 0;
                    while ((1 << index) != binding.code && index < 15) ++index;
                    const bool mouse = binding.device == Device::Mouse;
                    if (ImGui::Combo("##Button", &index, mouse ? mouseNames : padNames, mouse ? 3 : 16))
                    {
                        binding.code = 1 << index;
                        changed = true;
                    }
                }
                changed |= ImGui::Checkbox("Pulse on Down", &binding.downOnly);
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove")) removeIndex = j;
                ImGui::PopID();
            }
            if (removeIndex >= 0)
            {
                bindings.erase(bindings.begin() + removeIndex);
                changed = true;
            }
            if (ImGui::SmallButton("Add Binding")) { bindings.push_back({}); changed = true; }
            const ButtonState live = rawStates[i];
            ImGui::Text("Raw Held: %s / Down: %s", live.held ? "yes" : "no", live.down ? "yes" : "no");
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (ImGui::CollapsingHeader("Output State"))
    {
        ImGui::Text("Move: %.3f, %.3f", previousInput.moveX, previousInput.moveZ);
        const char* names[] = { "Crouch", "Sprint", "Attack", "AlignCamera", "QuickForward",
            "QuickBackward", "QuickLeft", "QuickRight", "QuickDefaultForward" };
        for (int i = 0; i < 9; ++i)
        {
            const auto action = static_cast<PlayerAction>(i);
            ImGui::Text("%s: Held %d / Down %d / Release %d", names[i],
                previousInput.Held(action), previousInput.Down(action), previousInput.Release(action));
        }
    }
    if (changed)
    {
        ResetRuntime();
        Game::Input::Instance().SuppressGameplayInput();
    }
}
