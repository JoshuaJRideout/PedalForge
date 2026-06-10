#pragma once
#include <cmath>
#include <cstdint>
#include <map>
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

    // Possession (§4.7): unified pilot model. Vehicles spawn piloted (strategy
    // modes flavor this as remote-link); eject leaves them inert until boarded.
    bool hasPilot = true;

    VehicleEntity(uint32_t entityId, uint8_t entityTeam, const VehicleTemplate& t)
        : id(entityId), team(entityTeam), tmpl(&t), state(t) {}
};

struct Pickup {
    uint32_t id = 0;
    DropKind kind = DropKind::EnergyShard;
    Vec3 position;
    uint64_t despawnTick = 0;
};

// In-flight ordnance (§5.1): ballistic point integrated per tick with
// sub-steps; impacts resolve through the same hit pipeline as hitscan.
struct Projectile {
    uint32_t id = 0;
    uint32_t shooter = 0;
    Vec3 position;
    Vec3 velocity;
    WeaponSpec spec;
    float gravityFactor = 1.0f; // 0 = missile/rocket, 1 = lobbed shell
    uint64_t expireTick = 0;
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

    // Launch a projectile from an entity's weapon (consumes ammo). Returns
    // projectile id (0 = failed).
    uint32_t launch(uint32_t shooter, Vec3 dir, float speed, const WeaponSpec& spec,
                    float gravityFactor = 1.0f);
    const std::vector<Projectile>& projectiles() const { return ordnance; }

    // Server-authoritative terrain event (also what clients apply on receipt).
    // Also damages vehicles in the radius (entry-point sampling, falloff) —
    // so death craters chain into tightly packed formations.
    void applyBlast(const BlastEvent& e);

    // Eject the pilot from a vehicle (§4.7): spawns an on-foot pilot beside it
    // and leaves the vehicle inert. Returns the pilot's entity id (0 = failed).
    uint32_t eject(uint32_t vehicleId);
    // Board a pilotless vehicle within reach (intact cockpit or none required).
    // The pilot entity is consumed. Boarding ignores teams: stealing is a
    // feature (§7.1 Scrap Pilots). Invalidates entity pointers.
    bool board(uint32_t pilotId, uint32_t vehicleId);
    static constexpr float kBoardRange = 4.0f;

    // Advance one fixed tick: locomotion, pickup magnetism, despawns.
    void step();

    uint64_t tick() const { return tickCount; }
    VoxelWorld& world() { return voxels; }
    const VoxelWorld& world() const { return voxels; }
    int teamEnergy(uint8_t team) const { return energy[team & 3]; }
    void addEnergy(uint8_t team, int amount) { energy[team & 3] += amount; }

    // --- Sector economy (DESIGN.md §2.2) ---
    static constexpr int kSectorSize = 16;          // world-voxel columns
    static constexpr int kPowerStationCost = 100;
    static constexpr int kSectorIncome = 5;         // per owned sector...
    static constexpr uint64_t kIncomeIntervalTicks = 60; // ...per second

    Int3 sectorOf(Vec3 worldPos) const {
        return { static_cast<int>(std::floor(worldPos.x)) / kSectorSize, 0,
                 static_cast<int>(std::floor(worldPos.z)) / kSectorSize };
    }
    // -1 = neutral.
    int sectorOwner(Int3 sector) const;
    // Builds a power station (energy permitting) and claims its sector.
    // Fails on insufficient energy or an already-claimed sector. Returns
    // the station's entity id (0 = failed). Destroying the station's core
    // flips the sector back to neutral.
    uint32_t buildPowerStation(uint8_t team, Vec3 position);

    // Events recorded since the last takeEvents() — the replication payload.
    std::vector<SimEvent> takeEvents() { return std::exchange(pending, {}); }

    // Order-sensitive digest of world + entities + pickups for sync tests/audits.
    uint64_t stateHash() const;

    // World-space point -> occupied sub-voxel of an entity (nullopt = no hit).
    std::optional<Int3> worldToSubvoxel(const VehicleEntity& e, Vec3 worldPoint) const;

private:
    void recordPartEvents(VehicleEntity& target, const HitResult& hit, Vec3 at);
    void collectPickups();
    void stepProjectiles();
    // First vehicle hit at point p (excluding one entity); -1 part = none.
    VehicleEntity* vehicleAt(Vec3 p, uint32_t exclude, Int3& subvoxelOut);

    VoxelWorld voxels;
    Rng rng;
    std::vector<VehicleEntity> vehicles;
    std::vector<Pickup> drops;
    std::vector<Projectile> ordnance;
    uint32_t nextProjectileId = 1;
    std::vector<SimEvent> pending;
    uint64_t tickCount = 0;
    uint32_t nextEntityId = 1;
    uint32_t nextPickupId = 1;
    int energy[4] = { 0, 0, 0, 0 };
    std::map<std::pair<int, int>, uint8_t> sectors;   // (sx, sz) -> owner team
    std::map<uint32_t, std::pair<int, int>> stations; // station entity -> sector
};

} // namespace vox
