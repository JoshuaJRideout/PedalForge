#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "core/rng.h"
#include "core/types.h"
#include "world/world.h" // DamageType

namespace vox {

// Vehicle sub-voxels are 0.25 m — 4x finer per axis than the world grid
// (DESIGN.md §4.1). Templates are authored grids segmented into named parts.

enum class PartType : uint8_t {
    Hull,      // core: vehicle dies when this dies
    Engine,
    Wing,      // also rotor / hoverpad
    Track,
    Weapon,
    Sensor,
    ShieldGen,
    Cargo,
    Power,
};

struct PartDef {
    std::string name;
    PartType type = PartType::Hull;
    int maxHp = 1;
    // Damage taken multiplier (1.0 = unarmored, 0.7 = takes 70%). Facing
    // multipliers come later with the hit-direction plumbing.
    float armorMul = 1.0f;
};

constexpr uint8_t kEmptySubvoxel = 0xFF;

struct VehicleTemplate {
    std::string name;
    Int3 dims;                       // sub-voxel grid dimensions
    std::vector<uint8_t> partIndex;  // per sub-voxel: kEmptySubvoxel or index into parts
    std::vector<PartDef> parts;
    int corePart = -1;

    int addPart(std::string name, PartType type, int maxHp, float armorMul = 1.0f);
    // [min, max) box fill of a part's sub-voxels.
    void fillBox(Int3 min, Int3 max, int part);
    // Compute part adjacency from grid contact. Call once after authoring.
    void finalize();

    size_t index(Int3 p) const {
        return static_cast<size_t>(p.z) * static_cast<size_t>(dims.x) * static_cast<size_t>(dims.y)
             + static_cast<size_t>(p.y) * static_cast<size_t>(dims.x)
             + static_cast<size_t>(p.x);
    }
    bool inBounds(Int3 p) const {
        return p.x >= 0 && p.y >= 0 && p.z >= 0 && p.x < dims.x && p.y < dims.y && p.z < dims.z;
    }
    int partAt(Int3 p) const {
        if (!inBounds(p)) return -1;
        const uint8_t v = partIndex[index(p)];
        return v == kEmptySubvoxel ? -1 : static_cast<int>(v);
    }
    size_t occupiedCount() const;

    std::vector<std::set<int>> adjacency; // per part: touching parts

    // Factory templates (DESIGN.md §4.2 "Wasp" fighter, plus a tank).
    static const VehicleTemplate& waspFighter();
    static const VehicleTemplate& brickTank();
};

enum class DropKind : uint8_t { AmmoCell, RepairKit, EnergyShard };

struct HitResult {
    int partHit = -1;            // part that absorbed the damage (-1 = miss)
    int damageApplied = 0;       // after armor
    int bleedDamage = 0;         // overkill transferred to a neighbor part
    int bleedPart = -1;
    bool partDestroyed = false;
    bool vehicleDestroyed = false;
    std::vector<DropKind> drops;
};

// A live vehicle instance: per-part HP state over a shared template.
class Vehicle {
public:
    explicit Vehicle(const VehicleTemplate& tmpl);

    const VehicleTemplate& tmpl() const { return *templ; }

    // Apply a hit landing on a template-local sub-voxel (DESIGN.md §5.2).
    // Rng is caller-provided so drop rolls replay deterministically.
    HitResult applyHit(Int3 subvoxel, int damage, DamageType type, Rng& rng);

    int partHp(int part) const { return parts[static_cast<size_t>(part)].hp; }
    bool partAlive(int part) const { return !parts[static_cast<size_t>(part)].destroyed; }
    bool destroyed() const { return dead; }

    // Functional status queries — locomotion/weapon models read these.
    bool hasAlivePartOfType(PartType type) const;
    int alivePartCountOfType(PartType type) const;

    // Damage-state fraction for cosmetic chip thresholds (75/50/25%).
    float partHpFraction(int part) const;

private:
    struct PartState {
        int hp = 0;
        bool destroyed = false;
    };

    // Apply damage to a specific (alive) part; handles destruction + drops.
    void damagePart(int part, int amount, Rng& rng, HitResult& out);
    int nearestAlivePart(int from) const; // BFS over template adjacency
    void rollDrops(const PartDef& def, bool isCore, Rng& rng, std::vector<DropKind>& out);

    const VehicleTemplate* templ;
    std::vector<PartState> parts;
    bool dead = false;
};

} // namespace vox
