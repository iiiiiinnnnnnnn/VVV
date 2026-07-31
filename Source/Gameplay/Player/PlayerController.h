// PlayerController.h

#pragma once

#include "Core/Object/Component.h"

struct InputContext
{
    float moveX = 0.0f;
    float moveZ = 0.0f;
    bool crouch = false;
    bool sprint = false;
    bool attackPressed = false;
    bool quickForwardPressed = false;
    bool quickBackwardPressed = false;
    bool quickLeftPressed = false;
    bool quickRightPressed = false;
    bool quickForwardStarted = false;
    bool quickBackwardStarted = false;
    bool quickLeftStarted = false;
    bool quickRightStarted = false;
    int attackType = 0;
};

class PlayerController : public Component
{
public:
    PlayerController(Object* owner) : Component(owner) {}
    virtual ~PlayerController() = default;

    virtual InputContext Poll() = 0;
};
