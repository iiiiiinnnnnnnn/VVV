#pragma once
#include "Gameplay/Player/PlayerController.h"

class EnemyAIController : public PlayerController
{
public:
    InputContext Poll() override { return {}; }
};