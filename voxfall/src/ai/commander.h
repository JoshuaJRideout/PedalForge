#pragma once
#include "ai/orders.h"
#include "sim/sim.h"

namespace vox {

// Strategic commander (DESIGN.md §11.1 "utility-AI commanders"): one per AI
// team. Expands the sector economy, produces an army, and throws it at the
// enemy. Deliberately simple utility rules for M0 — the contract that matters
// is that it plays the same game a human does, through the same Sim API.

class CommanderAi {
public:
    static constexpr int kTankCost = 80;
    static constexpr int kMaxArmy = 8;
    static constexpr int kMaxStations = 5;
    static constexpr int kEnergyReserve = 120;

    CommanderAi(uint8_t team, uint32_t hostEntity, Vec3 hostPosition);

    // Strategic pulse, call ~once per second. Issues builds/spawns/orders.
    void think(Sim& sim, AiController& ai);

    bool alive(const Sim& sim) const;
    uint8_t teamId() const { return team; }
    uint32_t host() const { return hostEntity; }

private:
    int armySize(const Sim& sim) const;
    uint32_t nearestEnemy(const Sim& sim, Vec3 from) const;

    uint8_t team;
    uint32_t hostEntity;
    Vec3 hostPosition;
    int stationsBuilt = 0;
    int spawnCounter = 0;
};

} // namespace vox
