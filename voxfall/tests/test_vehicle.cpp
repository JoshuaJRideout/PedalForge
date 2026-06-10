#include "test_framework.h"
#include "vehicle/vehicle.h"

using namespace vox;

namespace {
// Any sub-voxel inside the named part of a template (asserts it exists).
Int3 subvoxelOf(const VehicleTemplate& t, const char* partName) {
    int target = -1;
    for (size_t i = 0; i < t.parts.size(); ++i)
        if (t.parts[i].name == partName) target = static_cast<int>(i);
    for (int z = 0; z < t.dims.z; ++z)
        for (int y = 0; y < t.dims.y; ++y)
            for (int x = 0; x < t.dims.x; ++x)
                if (t.partAt({ x, y, z }) == target) return { x, y, z };
    return { -1, -1, -1 };
}

int partIndexOf(const VehicleTemplate& t, const char* partName) {
    for (size_t i = 0; i < t.parts.size(); ++i)
        if (t.parts[i].name == partName) return static_cast<int>(i);
    return -1;
}
} // namespace

TEST(templates_are_well_formed) {
    const VehicleTemplate& wasp = VehicleTemplate::waspFighter();
    CHECK(wasp.corePart >= 0);
    CHECK(wasp.occupiedCount() > 400);            // roughly fighter-sized (§4.1)
    CHECK(wasp.parts.size() == 6);
    // Every part must touch at least one other part — no floating parts.
    for (size_t i = 0; i < wasp.parts.size(); ++i) CHECK(!wasp.adjacency[i].empty());

    const VehicleTemplate& brick = VehicleTemplate::brickTank();
    CHECK(brick.corePart >= 0);
    for (size_t i = 0; i < brick.parts.size(); ++i) CHECK(!brick.adjacency[i].empty());
}

TEST(part_group_hp_drains_and_detaches) {
    Vehicle v(VehicleTemplate::waspFighter());
    Rng rng(1);
    const VehicleTemplate& t = v.tmpl();
    const Int3 wingVoxel = subvoxelOf(t, "wing.left");
    const int wing = partIndexOf(t, "wing.left");
    CHECK(wing >= 0);

    // Wing pool: 60 HP, unarmored. Two 25-damage hits leave it alive at 10.
    HitResult h1 = v.applyHit(wingVoxel, 25, DamageType::Kinetic, rng);
    CHECK(h1.partHit == wing);
    CHECK(h1.damageApplied == 25);
    CHECK(!h1.partDestroyed);
    v.applyHit(wingVoxel, 25, DamageType::Kinetic, rng);
    CHECK(v.partHp(wing) == 10);
    CHECK(v.partHpFraction(wing) < 0.25f); // cosmetic chip threshold reached

    // Third hit destroys the part; vehicle survives; wing count drops.
    CHECK(v.alivePartCountOfType(PartType::Wing) == 2);
    HitResult h3 = v.applyHit(wingVoxel, 25, DamageType::Kinetic, rng);
    CHECK(h3.partDestroyed);
    CHECK(!h3.vehicleDestroyed);
    CHECK(!v.partAlive(wing));
    CHECK(v.alivePartCountOfType(PartType::Wing) == 1);
    CHECK(!v.destroyed());
}

TEST(overkill_bleeds_into_neighbor) {
    Vehicle v(VehicleTemplate::waspFighter());
    Rng rng(2);
    const VehicleTemplate& t = v.tmpl();
    const Int3 wingVoxel = subvoxelOf(t, "wing.left");

    // 160 damage on a 60 HP wing: 100 overkill, 50 bleeds into the hull.
    HitResult h = v.applyHit(wingVoxel, 160, DamageType::Kinetic, rng);
    CHECK(h.partDestroyed);
    CHECK(h.bleedPart == t.corePart);
    CHECK(h.bleedDamage == 50);
    CHECK(v.partHp(t.corePart) == t.parts[static_cast<size_t>(t.corePart)].maxHp - 50);
}

TEST(hits_on_detached_part_strike_behind_it) {
    Vehicle v(VehicleTemplate::waspFighter());
    Rng rng(3);
    const VehicleTemplate& t = v.tmpl();
    const Int3 wingVoxel = subvoxelOf(t, "wing.left");

    v.applyHit(wingVoxel, 60, DamageType::Kinetic, rng); // exactly destroys wing
    HitResult h = v.applyHit(wingVoxel, 10, DamageType::Kinetic, rng);
    CHECK(h.partHit == t.corePart); // redirected to the airframe behind the stub
}

TEST(core_destruction_kills_vehicle) {
    Vehicle v(VehicleTemplate::brickTank());
    Rng rng(4);
    const VehicleTemplate& t = v.tmpl();
    const Int3 hullVoxel = subvoxelOf(t, "hull");

    // Brick hull: 260 HP at 0.7 armor -> 1000 raw is overkill regardless.
    HitResult h = v.applyHit(hullVoxel, 1000, DamageType::Explosive, rng);
    CHECK(h.partDestroyed);
    CHECK(h.vehicleDestroyed);
    CHECK(v.destroyed());
    // Whole-vehicle kill always yields at least the bonus energy shard.
    bool hasShard = false;
    for (DropKind d : h.drops) hasShard |= (d == DropKind::EnergyShard);
    CHECK(hasShard);
    // Dead vehicles ignore further hits.
    HitResult after = v.applyHit(hullVoxel, 100, DamageType::Explosive, rng);
    CHECK(after.partHit == -1);
}

TEST(armor_and_energy_interaction) {
    Rng rng(5);
    const VehicleTemplate& t = VehicleTemplate::brickTank();
    const Int3 hullVoxel = subvoxelOf(t, "hull");
    const int hull = t.corePart;
    const int maxHp = t.parts[static_cast<size_t>(hull)].maxHp;

    // Kinetic vs 0.7 armor: 100 -> 70.
    Vehicle a(t);
    a.applyHit(hullVoxel, 100, DamageType::Kinetic, rng);
    CHECK(a.partHp(hull) == maxHp - 70);

    // Energy ignores half the armor: mul (0.7+1)/2 = 0.85 -> 85.
    Vehicle b(t);
    b.applyHit(hullVoxel, 100, DamageType::Energy, rng);
    CHECK(b.partHp(hull) == maxHp - 85);
}

TEST(drop_rolls_are_deterministic_per_seed) {
    const VehicleTemplate& t = VehicleTemplate::waspFighter();
    const Int3 cannonVoxel = subvoxelOf(t, "weapon.cannon");

    auto runDrops = [&](uint64_t seed) {
        Vehicle v(t);
        Rng rng(seed);
        return v.applyHit(cannonVoxel, 500, DamageType::Kinetic, rng).drops;
    };
    CHECK(runDrops(77) == runDrops(77)); // same seed, same loot

    // Weapon parts drop ammo 60% of the time — over many seeds, both outcomes occur.
    int ammoDrops = 0;
    for (uint64_t s = 0; s < 100; ++s) {
        for (DropKind d : runDrops(s))
            if (d == DropKind::AmmoCell) ++ammoDrops;
    }
    CHECK(ammoDrops > 35);
    CHECK(ammoDrops < 85);
}

TEST(functional_status_queries) {
    Vehicle v(VehicleTemplate::waspFighter());
    Rng rng(6);
    CHECK(v.hasAlivePartOfType(PartType::Engine));
    const Int3 engineVoxel = subvoxelOf(v.tmpl(), "engine");
    // Exactly the engine pool: destroys it with no overkill bleed.
    v.applyHit(engineVoxel, 90, DamageType::Kinetic, rng);
    CHECK(!v.hasAlivePartOfType(PartType::Engine)); // gliding now (§4.2)
    CHECK(!v.destroyed());

    // Massive overkill on a fresh fighter's engine bleeds through and can
    // take the hull with it — mobility-kill vs overkill-kill is a real choice.
    Vehicle v2(VehicleTemplate::waspFighter());
    const HitResult h = v2.applyHit(engineVoxel, 500, DamageType::Kinetic, rng);
    CHECK(h.partDestroyed);
    CHECK(h.bleedDamage > 0);
}
