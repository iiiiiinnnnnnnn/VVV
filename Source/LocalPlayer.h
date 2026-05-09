// LocalPlayer.h

#pragma once

#include "PlayerController.h"

class LocalPlayer : public PlayerController {
public:
    float GetMoveX() override;
    float GetMoveZ() override;
    bool GetJump() override;
    bool GetAttack() override;
};
