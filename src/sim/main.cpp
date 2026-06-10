// voxfall_sim — headless demo of the M0 core: deterministic worldgen,
// terrain destruction, and vehicle part damage with drops.
//
// Usage: voxfall_sim [seed]

#include <cstdio>
#include <cstdlib>
#include "vehicle/locomotion.h"
#include "vehicle/vehicle.h"
#include "world/world.h"

using namespace vox;

namespace {

const char* dropName(DropKind d) {
    switch (d) {
        case DropKind::AmmoCell:    return "ammo cell";
        case DropKind::RepairKit:   return "repair kit";
        case DropKind::EnergyShard: return "energy shard";
    }
    return "?";
}

void printHeightMap(const VoxelWorld& w) {
    // ASCII top-down: sample every few columns, shade by height band.
    static const char shades[] = " .:-=+*#%@";
    const Int3 size = w.size();
    const int stepX = std::max(1, size.x / 72);
    const int stepZ = std::max(1, size.z / 36);
    for (int z = 0; z < size.z; z += stepZ) {
        for (int x = 0; x < size.x; x += stepX) {
            const int h = w.heightAt(x, z);
            if (h <= w.seaLevel()) {
                std::printf("~");
            } else {
                const int band = std::min(9, h * 10 / size.y);
                std::printf("%c", shades[band]);
            }
        }
        std::printf("\n");
    }
}

void printPartStatus(const Vehicle& v) {
    const VehicleTemplate& t = v.tmpl();
    for (size_t i = 0; i < t.parts.size(); ++i) {
        const int part = static_cast<int>(i);
        std::printf("    %-14s %3d/%3d HP %s\n", t.parts[i].name.c_str(), v.partHp(part),
                    t.parts[i].maxHp, v.partAlive(part) ? "" : "[DESTROYED]");
    }
}

} // namespace

int main(int argc, char** argv) {
    const uint64_t seed = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1337ull;

    // --- World ---
    std::printf("=== Voxfall M0 core demo ===\n\n");
    std::printf("Generating 192x96x192 world, seed %llu...\n", static_cast<unsigned long long>(seed));
    VoxelWorld world({ 192, 96, 192 });
    world.generate(seed);
    std::printf("World hash: %016llx (sea level y=%d)\n\n",
                static_cast<unsigned long long>(world.contentHash()), world.seaLevel());
    printHeightMap(world);

    // Determinism spot-check: regenerate and compare.
    VoxelWorld world2({ 192, 96, 192 });
    world2.generate(seed);
    std::printf("\nDeterminism check (regenerate same seed): %s\n",
                world.contentHash() == world2.contentHash() ? "MATCH" : "MISMATCH!");

    // --- Terrain destruction ---
    const int cx = 96, cz = 96;
    const int ground = world.heightAt(cx, cz);
    const BlastEvent shellHit{ { static_cast<float>(cx), static_cast<float>(ground) - 1.0f,
                                 static_cast<float>(cz) },
                               4.0f, 300, DamageType::Explosive };
    const BlastResult crater = world.applyBlast(shellHit);
    std::printf("\nArtillery shell at (%d, %d, %d): crater of %zu voxels, new height %d\n", cx,
                ground - 1, cz, crater.destroyed.size(), world.heightAt(cx, cz));

    // --- Vehicle part damage ---
    std::printf("\n--- Wasp fighter under fire (sub-voxel part damage) ---\n");
    Vehicle wasp(VehicleTemplate::waspFighter());
    Rng combatRng(seed ^ 0xC0817ull);
    std::printf("Template: %zu occupied sub-voxels, %zu parts\n",
                wasp.tmpl().occupiedCount(), wasp.tmpl().parts.size());
    printPartStatus(wasp);

    // Walk 30mm cannon fire across the left wing until something gives.
    const Int3 wingAim{ 20, 4, 8 };
    int shots = 0;
    while (!wasp.destroyed() && shots < 20) {
        ++shots;
        const HitResult hit = wasp.applyHit(wingAim, 22, DamageType::Kinetic, combatRng);
        if (hit.partHit < 0) break;
        const std::string& partName = wasp.tmpl().parts[static_cast<size_t>(hit.partHit)].name;
        std::printf("  shot %2d -> %-14s %3d dmg", shots, partName.c_str(), hit.damageApplied);
        if (hit.bleedPart >= 0)
            std::printf("  (+%d bleed into %s)", hit.bleedDamage,
                        wasp.tmpl().parts[static_cast<size_t>(hit.bleedPart)].name.c_str());
        if (hit.partDestroyed) std::printf("  ** PART DESTROYED **");
        for (DropKind d : hit.drops) std::printf("  [drop: %s]", dropName(d));
        std::printf("\n");
        if (hit.partDestroyed) break;
    }
    std::printf("After wing volley (engine alive: %s, wings left: %d):\n",
                wasp.hasAlivePartOfType(PartType::Engine) ? "yes" : "no",
                wasp.alivePartCountOfType(PartType::Wing));
    printPartStatus(wasp);

    // Finish it with a missile into the hull.
    std::printf("\nMissile into the hull:\n");
    const HitResult kill = wasp.applyHit({ 24, 16, 8 }, 400, DamageType::Explosive, combatRng);
    std::printf("  %d dmg -> %s%s\n", kill.damageApplied,
                kill.partHit >= 0 ? wasp.tmpl().parts[static_cast<size_t>(kill.partHit)].name.c_str()
                                  : "miss",
                kill.vehicleDestroyed ? "  ** VEHICLE DESTROYED **" : "");
    for (DropKind d : kill.drops) std::printf("  [drop: %s]\n", dropName(d));

    // --- Locomotion: damage changes handling ---
    std::printf("\n--- Locomotion on generated terrain ---\n");

    // Tank drives east across the real terrain for 8 s.
    Vehicle tank(VehicleTemplate::brickTank());
    BodyState tankBody;
    tankBody.position = { 20.0f, static_cast<float>(world.heightAt(20, 96)), 96.0f };
    tankBody.grounded = true;
    ControlInput drive;
    drive.throttle = 1.0f;
    int blockedTicks = 0;
    for (int t = 0; t < 8 * 60; ++t)
        if (stepTracked(tankBody, drive, tank, world).blocked) ++blockedTicks;
    std::printf("Brick tank, 8 s east: x %.1f -> %.1f (y %.1f, %.1f s blocked by slopes)\n",
                20.0f, tankBody.position.x, tankBody.position.y,
                static_cast<float>(blockedTicks) / 60.0f);

    // Jet loses its engine mid-flight and glides down.
    Vehicle glider(VehicleTemplate::waspFighter());
    BodyState jetBody;
    jetBody.position = { 20.0f, 80.0f, 60.0f };
    jetBody.speed = 60.0f;
    ControlInput fly;
    fly.throttle = 1.0f;
    for (int t = 0; t < 120; ++t) stepJet(jetBody, fly, glider, world);
    Rng locoRng(seed ^ 0x10C0ull);
    glider.applyHit({ 4, 16, 8 }, 90, DamageType::Kinetic, locoRng); // engine out
    std::printf("Wasp engine destroyed at altitude %.1f, airspeed %.1f. Gliding:\n",
                jetBody.position.y, jetBody.speed);
    for (int s = 1; s <= 4; ++s) {
        for (int t = 0; t < 60; ++t) stepJet(jetBody, fly, glider, world);
        std::printf("  +%d s: altitude %.1f, airspeed %.1f\n", s, jetBody.position.y, jetBody.speed);
    }

    // Mech jumps; then loses its cockpit and becomes a stealable husk.
    Vehicle mech(VehicleTemplate::talonMech());
    BodyState mechBody;
    mechBody.position = { 100.0f, static_cast<float>(world.heightAt(100, 96)), 96.0f };
    mechBody.grounded = true;
    ControlInput stride;
    stride.throttle = 1.0f;
    stride.jump = true;
    stepWalker(mechBody, stride, mech, world);
    stride.jump = false;
    float peak = mechBody.position.y;
    while (!mechBody.grounded) {
        stepWalker(mechBody, stride, mech, world);
        peak = std::max(peak, mechBody.position.y);
    }
    std::printf("Talon mech jump: peak +%.1f m, landed at y %.1f\n",
                peak - static_cast<float>(world.heightAt(100, 96)), mechBody.position.y);
    mech.applyHit({ 20, 10, 50 }, 50, DamageType::Kinetic, locoRng); // cockpit out
    const float beforeX = mechBody.position.x;
    for (int t = 0; t < 60; ++t) stepWalker(mechBody, stride, mech, world);
    std::printf("Cockpit destroyed -> husk: %s, input ignored (moved %.2f m). Steal it.\n",
                mech.isHusk() ? "yes" : "no", mechBody.position.x - beforeX);

    std::printf("\nDone.\n");
    return 0;
}
