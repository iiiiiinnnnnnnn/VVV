// PlayerController

#pragma once
#include "Core/Foundation/Common.h"

struct InputContext
{
    float moveX = 0.0f;
    float moveZ = 0.0f;
    bool crouch = false;
    bool sprint = false;

    bool attackPressed = false;
    int attackType = 0;
};

class PlayerController
{
public:
    virtual ~PlayerController() = default;
    virtual InputContext Poll() = 0;
};