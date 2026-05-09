// PlayerController.h

#pragma once

#include "Common.h"

class PlayerController {
public:
    virtual float GetMoveX() = 0;
    virtual float GetMoveZ() = 0;
    virtual bool GetJump() = 0;
    virtual bool GetAttack() = 0;
};
