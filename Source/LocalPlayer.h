#pragma once
#include "PlayerController.h"

class LocalPlayer : public PlayerController
{
public:
    InputContext Poll() override;
};