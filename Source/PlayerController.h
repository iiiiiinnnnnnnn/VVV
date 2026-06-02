#pragma once
#include "Common.h"

struct InputContext
{
    float moveX = 0.0f;
    float moveZ = 0.0f;
    bool jump = false;
    bool crouch = false;
    bool ready = false;
    bool shoot = false;
    bool reload = false;
    bool sprint = false;
};

class PlayerController
{
public:
    virtual ~PlayerController() = default;
    virtual InputContext Poll() = 0;
};