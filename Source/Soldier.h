// Soldier.h

#pragma once

#include "AIPlayer.h"
#include "Squad.h"
#include "Character.h"

class Soldier : public Character {
public:
    Soldier(Squad* squad);

    Squad* GetSquad() const { return squad; }
    void SetSquad(Squad* s) { squad = s; }

    void SetTarget(Actor* target) { this->target = target; }

private:
    Squad* squad = nullptr;
    Actor* target = nullptr;
    std::unique_ptr<AIPlayer> controller;
};
