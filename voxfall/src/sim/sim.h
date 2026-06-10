#pragma once
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>
#include "core/rng.h"
#include "core/types.h"
#include "vehicle/locomotion.h"
#include "vehicle/vehicle.h"
#include "world/world.h"

namespace vox {

// Deterministic headless simulation: the authoritative game state on the
// server, replayed/replicated on clients (DESIGN.md §7.2). Everything that
// happens in a tick is captured as SimEvents — the netcode broadcast payload
// and (later) the replay log.

struct WeaponSpec {
    int damage = 25;
    DamageType type = DamageType::Kinetic;
    float range = 300.0f;
    float blastRadius = 0.0f; // >0: explodes on terrain impact
    int blastDamage = 0;
};

struct VehicleEntity {
    uint32_t id = 0;
    uint8_t team = 0;
    const VehicleTemplate* tmpl = nullptr;
    Vehicle state;
    BodyState body;
    ControlInput input;
    int ammo = 200;

    VehicleEntity(uint32_t entityId, uint8_t entityTeam, const VehicleTemplate& t)
        : id(entityId), team(entityTeam), tmpl(&t), state(t) {}
};

struct Pickup {
    uint32_t id = 0;
    DropKind kind = DropKind::EnergyShard;
    Vec3 position;
    uint64_t despawnTick = 0;
};

struct SimEvent {
    enum class Type : uint8_t {
        Blast,            // terrain destruction (blast field valid)
        PartDestroyed,    // entity + part valid
        VehicleDestroyed, // entity valid
        PickupSpawned,    // pickup/kind/position valid
        PickupCollected,  // pickup + entity valid
    };
    Type type = Type::Blast;
    uint32_t entity = 0;
    int part = -1;
    uint32_t pickup = 0;
    DropKind kind = DropKind::EnergyShard;
    Vec3 position;
    BlastEvent blast;
};

struct HitscanResult {
    bool hitVehicle = false;
    bool hitTerrain = false;
    uint32_t entity = 0;
    HitResult hit;       // when hitVehicle
    Vec3 impact;
};

class Sim {
public:
    static constexpr float kSubvoxelSize = 0.25f; // §4.1
    static constexpr uint64_t kPickupLifetimeTicks = 45 * 60; // §4.4: 45 s
    static constexpr float kPickupRadius = 3.0f;

    Sim(VoxelWorld world, uint64_t seed);

    uint32_t spawnVehicle(TemplateId id, uint8_t team, Vec3 position, float yaw);
    VehicleEntity* find(uint32_t id);
    const VehicleEntity* find(uint32_t id) const;
    const std::vector<VehicleEntity>& entities() const { return vehicles; }
    const std::vector<Pickup>& pickups() const { return drops; }

    void setInput(uint32_t id, const ControlInput& input);

    // Immediate hitscan from an entity's weapon along dir (§5.2 pipeline:
    // march the ray, test vehicle sub-voxel grids and world voxels, apply part
    // damage or terrain blast, spawn drops). Requires an alive weapon part and ammo.
    std::optional<HitscanResult> fire(uint32_t shooter, Vec3 dir, const WeaponSpec& spec = {});

    // Server-authoritative terrain event (also what clients apply on receipt).
    void applyBlast(const BlastEvent& e);

    // Advance one fixed tick: locomotion, pickup magnetism, despawns.
    void step();

    uint64_t tick() const { return tickCount; }
    VoxelWorld& world() { return voxels; }
    const VoxelWorld& world() const { return voxels; }
    int teamEnergy(uint8_t team) const { return energy[team & 3]; }

    // Events recorded since the last takeEvents() — the replication payload.
    std::vector<SimEvent> takeEvents() { return std::exchange(pending, {}); }

    // Order-sensitive digest of world + entities + pickups for sync tests/audits.
    uint64_t stateHash() const;

    // World-space point -> occupied sub-voxel of an entity (nullopt = no hit).
    std::optional<Int3> worldToSubvoxel(const VehicleEntity& e, Vec3 worldPoint) const;

private:
    void recordPartEvents(VehicleEntity& target, const HitResult& hit, Vec3 at);
    void collectPickups();

    VoxelWorld voxels;
    Rng rng;
    std::vector<VehicleEntity> vehicles;
    std::vector<Pickup> drops;
    std::vector<SimEvent> pending;
    uint64_t tickCount = 0;
    uint32_t nextEntityId = 1;
    uint32_t nextPickupId = 1;
    int energy[4] = { 0, 0, 0, 0 };
};

} // namespace vox
