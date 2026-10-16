#pragma once

#include <unordered_map>

#include "Core/Object/Component.h"

enum class PlayerAction
{
    Crouch,
    Sprint,
    Attack,
    AlignCamera,
    QuickForward,
    QuickBackward,
    QuickLeft,
    QuickRight,
    QuickDefaultForward
};

struct InputContext
{
    float moveX = 0.0f;
    float moveZ = 0.0f;
    int attackType = 0;

    bool Held(PlayerAction action) const { return Get(action).held; }
    bool Down(PlayerAction action) const { return Get(action).down; }
    bool Release(PlayerAction action) const { return Get(action).released; }

    void Set(PlayerAction action, bool held, bool down)
    {
        actions[action] = { held, down, false };
    }

    void UpdateReleases(const InputContext& previous)
    {
        for (auto& [action, state] : actions)
        {
            state.released = !state.held && previous.Held(action);
        }
    }

private:
    struct ActionState
    {
        bool held = false;
        bool down = false;
        bool released = false;
    };

    ActionState Get(PlayerAction action) const
    {
        const auto found = actions.find(action);
        return found != actions.end() ? found->second : ActionState{};
    }

    std::unordered_map<PlayerAction, ActionState> actions;
};

class PlayerInput : public Component
{
public:
    PlayerInput(Object* owner) : Component(owner) {}
    virtual ~PlayerInput() = default;

    virtual InputContext Poll() = 0;
};
