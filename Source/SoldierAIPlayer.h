// SoldierAIPlayer.h

#pragma once

#include "AIPlayer.h"
#include "Squad.h"

class SoldierAIPlayer : public AIPlayer {
public:
    enum class State {
        Follow,   // 指揮官についていく
        Scout,    // 索敵
        Combat,   // 戦闘
    };

private:
    void UpdateFollow();
    void UpdateScout();
    void UpdateCombat();

    State state = State::Follow;
    Squad* squad = nullptr;
};
