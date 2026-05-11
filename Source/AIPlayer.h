// AIPlayer.h

#pragma once

#include "PlayerController.h"

class AIPlayer : public PlayerController {
public:
    float GetMoveX() override;
    float GetMoveZ() override;
    bool GetJump() override;
    bool GetShoot() override;
    bool GetReload() override;
};
