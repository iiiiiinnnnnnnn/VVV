#pragma once
#include "Gameplay/Player/PlayerController.h"

class LocalPlayer : public PlayerController
{
public:
    InputContext Poll() override;
};