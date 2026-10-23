#pragma once

#include "Gameplay/Player/Player.h"

class LocalPlayer : public Player
{
public:
    LocalPlayer();

protected:
    void OnInit() override;
};
