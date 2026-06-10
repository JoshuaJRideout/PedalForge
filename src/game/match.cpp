#include "game/match.h"

namespace vox {

Match::Match(VoxelWorld world, const MapMeta& spawns, uint64_t seed, int startingEnergy)
    : simulation(std::move(world), seed) {
    // Attack weapon for AI armies: tank shells, not pea-shooters.
    controller.weapon.damage = 60;
    controller.weapon.type = DamageType::Explosive;
    controller.weapon.blastRadius = 1.5f;
    controller.weapon.blastDamage = 60;

    for (const MapSpawn& s : spawns.spawns) {
        Vec3 pos{ static_cast<float>(s.position.x), static_cast<float>(s.position.y),
                  static_cast<float>(s.position.z) };
        const uint32_t host = simulation.spawnVehicle(TemplateId::HostStation, s.team, pos, 0.0f);
        simulation.addEnergy(s.team, startingEnergy);
        commanders.emplace_back(s.team, host, pos);
        eliminated.push_back(false);
    }
}

uint32_t Match::hostOf(int team) const {
    for (const CommanderAi& c : commanders)
        if (c.teamId() == team) return c.host();
    return 0;
}

void Match::tick() {
    if (finished()) return;
    if (simulation.tick() % 60 == 0)
        for (CommanderAi& c : commanders) c.think(simulation, controller);
    controller.tick(simulation);
    simulation.step();
    if (simulation.tick() % 60 == 0) checkElimination();
}

void Match::checkElimination() {
    int aliveCount = 0;
    int lastAlive = -1;
    for (size_t i = 0; i < commanders.size(); ++i) {
        if (eliminated[i]) continue;
        if (!commanders[i].alive(simulation)) {
            eliminated[i] = true;
            // Host down: the team's remaining forces detonate (§2.1).
            Rng boom(simulation.tick());
            for (const VehicleEntity& e : simulation.entities()) {
                if (e.team != commanders[i].teamId() || e.state.destroyed()) continue;
                VehicleEntity* doomed = simulation.find(e.id);
                const Int3 core{ doomed->tmpl->dims.x / 2, doomed->tmpl->dims.y / 2,
                                 doomed->tmpl->dims.z / 2 };
                doomed->state.applyHit(core, 100000, DamageType::Explosive, boom);
            }
            continue;
        }
        ++aliveCount;
        lastAlive = commanders[i].teamId();
    }
    if (aliveCount <= 1) winningTeam = lastAlive;
}

} // namespace vox
