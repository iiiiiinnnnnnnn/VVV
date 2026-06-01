#pragma once
#include "PlayerController.h"

class EnemyAIController : public PlayerController
{
public:
    InputContext Poll() override { return {}; }
};