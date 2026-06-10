#include "ai/commander.h"
#include <cmath>

namespace vox {

CommanderAi::CommanderAi(uint8_t teamId, uint32_t host, Vec3 hostPos)
    : team(teamId), hostEntity(host), hostPosition(hostPos) {}

bool CommanderAi::alive(const Sim& sim) const {
    const VehicleEntity* host = sim.find(hostEntity);
    return host && !host->state.destroyed();
}

int CommanderAi::armySize(const Sim& sim) const {
    int count = 0;
    for (const VehicleEntity& e : sim.entities())
        if (e.team == team && !e.state.destroyed()
            && e.tmpl->locomotion != LocomotionClass::Static
            && e.tmpl->locomotion != LocomotionClass::Pilot)
            ++count;
    return count;
}

uint32_t CommanderAi::nearestEnemy(const Sim& sim, Vec3 from) const {
    uint32_t best = 0;
    float bestDist = 1e30f;
    for (const VehicleEntity& e : sim.entities()) {
        if (e.team == team || e.state.destroyed()) continue;
        if (e.tmpl->locomotion == LocomotionClass::Pilot) continue; // squishing pilots is not strategy
        const float d = distance(from, e.body.position);
        if (d < bestDist) {
            bestDist = d;
            best = e.id;
        }
    }
    return best;
}

void CommanderAi::think(Sim& sim, AiController& ai) {
    if (!alive(sim)) return;

    // Economy: expand the sector ring while energy allows (§2.3 step 2).
    if (stationsBuilt < kMaxStations
        && sim.teamEnergy(team) >= Sim::kPowerStationCost + kEnergyReserve) {
        // Deterministic spiral of candidate sectors around the host.
        const Int3 home = sim.sectorOf(hostPosition);
        const int ring[8][2] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 },
                                 { 1, 1 }, { -1, 1 }, { -1, -1 }, { 1, -1 } };
        for (int radius = 0; radius < 3 && stationsBuilt < kMaxStations; ++radius) {
            for (const auto& d : ring) {
                const Int3 sector{ home.x + d[0] * (radius + 1), 0, home.z + d[1] * (radius + 1) };
                if (sector.x < 0 || sector.z < 0) continue;
                const Vec3 center{ static_cast<float>(sector.x * Sim::kSectorSize + 8), 0.0f,
                                   static_cast<float>(sector.z * Sim::kSectorSize + 8) };
                if (sim.sectorOwner(sector) >= 0) continue;
                if (sim.buildPowerStation(team, center) != 0) {
                    ++stationsBuilt;
                    break;
                }
            }
        }
    }

    // Production: keep the army topped up (§2.3 step 1).
    if (sim.teamEnergy(team) >= kTankCost && armySize(sim) < kMaxArmy) {
        sim.addEnergy(team, -kTankCost);
        const float angle = 0.7853981f * static_cast<float>(spawnCounter++ % 8);
        const float radius = 12.0f;
        Vec3 pos{ hostPosition.x + std::cos(angle) * radius, 0.0f,
                  hostPosition.z + std::sin(angle) * radius };
        pos.y = static_cast<float>(
            sim.world().heightAt(static_cast<int>(pos.x), static_cast<int>(pos.z)));
        sim.spawnVehicle(factionTemplate(factionOfTeam(team), UnitClass::Tank).id, team, pos,
                         angle);
    }

    // Orders: idle units attack the nearest enemy (§2.3 step 3).
    for (const VehicleEntity& e : sim.entities()) {
        if (e.team != team || e.state.destroyed()) continue;
        if (e.tmpl->locomotion == LocomotionClass::Static) continue;
        if (e.tmpl->locomotion == LocomotionClass::Pilot) continue;
        const Order* current = ai.orderOf(e.id);
        bool needsOrder = !current || current->type == OrderType::Hold;
        if (current && current->type == OrderType::Attack) {
            const VehicleEntity* target = sim.find(current->targetEntity);
            needsOrder = !target || target->state.destroyed();
        }
        if (needsOrder) {
            const uint32_t enemy = nearestEnemy(sim, e.body.position);
            if (enemy != 0) {
                Order attack;
                attack.type = OrderType::Attack;
                attack.targetEntity = enemy;
                ai.setOrder(e.id, attack);
            }
        }
    }
}

} // namespace vox
