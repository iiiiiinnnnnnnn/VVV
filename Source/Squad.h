// Squad.h

#pragma once

#include "Common.h"

class Commander;
class Soldier;

class Squad {
public:
    static constexpr int MAX_SOLDIERS = 8;

    void AddSoldier(std::shared_ptr<Soldier> soldier);
    void RemoveSoldier(Soldier* soldier);
    void Update(float elapsedTime);

    Commander* GetCommander() const { return commander; }
    const std::vector<std::shared_ptr<Soldier>>& GetSoldiers() const { return soldiers; }

private:
    Commander* commander = nullptr;
    std::vector<std::shared_ptr<Soldier>> soldiers;
};