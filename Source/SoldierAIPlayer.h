// SoldierAIPlayer.h

#pragma once

#include "AIPlayer.h"
#include "Squad.h"

class SoldierAIPlayer : public AIPlayer {
public:
    enum class State {
        Follow,   // wŠöŠ¯‚É‚Â‚¢‚Ä‚¢‚­
        Scout,    // õ“G
        Combat,   // í“¬
    };

private:
    void UpdateFollow();
    void UpdateScout();
    void UpdateCombat();

    State state = State::Follow;
    Squad* squad = nullptr;
};
