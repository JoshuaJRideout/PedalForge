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
    Cockpit,   // mechs (§4.7): destroyed while core lives -> powered-down husk
    Leg,       // walkers/mechs: per-leg mobility loss
    JumpJets,  // mechs: jump capability
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

// Movement model selector (locomotion.h implements each — DESIGN.md §4.5).
// Static = buildings: "vehicles without locomotion", same part/damage system.
enum class LocomotionClass : uint8_t { Tracked, Jet, Walker, Pilot, Static };

// Stable wire/content IDs for templates (network + forge packs need these).
enum class TemplateId : uint8_t {
    Wasp = 0, Brick, Talon, Pilot, PowerStation, HostStation,
    KesselFighter, KesselTank, KesselMech, KesselPilot,
    MirageFighter, MirageTank, MirageMech, MiragePilot,
    ChoirFighter, ChoirTank, ChoirMech, ChoirPilot,
    KesselPower, KesselHost, MiragePower, MirageHost, ChoirPower, ChoirHost,
    Count
};

// The four cultures (DESIGN.md §4.6), each with a real-world design language:
// Vanguard = NATO-modern, Kessler = heavy eastern-bloc industry, Mirage =
// faceted stealth drones, Choir = organic forward-swept bio-craft.
enum class Faction : uint8_t { Vanguard = 0, Kessler, Mirage, Choir, Count };
enum class UnitClass : uint8_t { Fighter = 0, Tank, Mech, PilotUnit, Power, Host, Count };

// Faction mechanics (DESIGN.md §4.6): Kessler pays more for tougher metal,
// Mirage swarms cheap fragile units, Choir self-repairs but builds dear.
struct FactionStats {
    int tankCost = 80;
    float repairCostMul = 1.0f; // host repair economy
    int regenPerSecond = 0;     // passive part HP regen (Choir)
};
const FactionStats& factionStats(Faction f);

struct VehicleTemplate;
const VehicleTemplate& factionTemplate(Faction f, UnitClass c);
inline Faction factionOfTeam(uint8_t team) { return static_cast<Faction>(team % 4); }

struct VehicleTemplate {
    std::string name;
    TemplateId id = TemplateId::Count;
    LocomotionClass locomotion = LocomotionClass::Tracked;
    // Sub-voxel edge length in meters (§4.1): 0.25 m standard; small craft
    // use 0.125 m for detail, huge units (Colossus) may go coarser.
    float voxelSize = 0.25f;
    Int3 dims;                       // sub-voxel grid dimensions
    std::vector<uint8_t> partIndex;  // per sub-voxel: kEmptySubvoxel or index into parts
    // Optional per-voxel paint: 0 = unpainted (renderers fall back to part
    // colors), else 1-based index into paletteRgb. The .vox import path can
    // fill this from the model palette; code-authored templates paint directly.
    std::vector<uint8_t> paint;
    uint8_t paletteRgb[256][3] = {};
    std::vector<PartDef> parts;
    int corePart = -1;

    int addPart(std::string name, PartType type, int maxHp, float armorMul = 1.0f);
    void setPaint(Int3 p, uint8_t color) {
        if (paint.empty()) paint.assign(partIndex.size(), 0);
        paint[index(p)] = color;
    }
    // [min, max) box fill of a part's sub-voxels.
    void fillBox(Int3 min, Int3 max, int part);
    // [min, max) box clear — sculpting tool for template authoring.
    void carveBox(Int3 min, Int3 max);
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

    // Factory templates (DESIGN.md §4.2 "Wasp" fighter, plus a tank, a mid
    // mech for Scrap Pilots, and the on-foot pilot — §4.7).
    static const VehicleTemplate& waspFighter();
    static const VehicleTemplate& brickTank();
    static const VehicleTemplate& talonMech();
    static const VehicleTemplate& pilot();
    static const VehicleTemplate& powerStation();
    static const VehicleTemplate& hostStation();
    static const VehicleTemplate& byId(TemplateId id);
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

    // Husk rule (§4.7): cockpit(s) destroyed while the core lives. The machine
    // is intact but pilotless — boardable by anyone, ignores control input.
    bool isHusk() const;
    // Alive, not a husk: accepts control input.
    bool operable() const { return !dead && !isHusk(); }

    // Functional status queries — locomotion/weapon models read these.
    bool hasAlivePartOfType(PartType type) const;
    int alivePartCountOfType(PartType type) const;

    // Damage-state fraction for cosmetic chip thresholds (75/50/25%).
    float partHpFraction(int part) const;

    // Field repair (§5.4): restores HP to a damaged-but-alive part. Destroyed
    // parts cannot be re-fabricated in the field. Returns HP actually restored.
    int repairPart(int part, int amount);
    // The alive part with the lowest HP fraction (-1 if none damaged) — repair
    // kits target this (§4.4).
    int lowestDamagedPart() const;

    // Direct part-state write for replication (client replicas mirror the
    // server's authoritative state; this skips drops/bleed logic on purpose).
    void replicatePartState(int part, int hp, bool destroyedFlag);

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
