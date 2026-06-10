#pragma once
#include <memory>
#include <vector>
#include "ai/commander.h"
#include "ai/orders.h"
#include "sim/sim.h"
#include "world/mapfile.h"

namespace vox {

// Annihilation match (DESIGN.md §2.4): one Host Station per team; lose it and
// you are out — and your remaining forces detonate (Urban Assault rule). Last
// team standing wins. Human players replace commander seats by simply driving
// entities through the same Sim API; the match doesn't care who is steering.

class Match {
public:
    Match(VoxelWorld world, const MapMeta& spawns, uint64_t seed, int startingEnergy = 300);

    void tick();           // one sim tick (commanders pulse once per second)
    int winner() const { return winningTeam; } // -1 = still fighting
    bool finished() const { return winningTeam >= 0; }

    Sim& sim() { return simulation; }
    AiController& ai() { return controller; }
    uint32_t hostOf(int team) const;

private:
    void checkElimination();

    Sim simulation;
    AiController controller;
    std::vector<CommanderAi> commanders;
    std::vector<bool> eliminated;
    int winningTeam = -1;
};

} // namespace vox
