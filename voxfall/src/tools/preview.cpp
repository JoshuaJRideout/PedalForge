// voxfall_preview — headless screenshots: renders a generated world (with
// some battle damage) and vehicle damage states to PNG via software raycast.
//
// Usage: voxfall_preview [seed] [outdir]

#include <cstdio>
#include <cstdlib>
#include <string>
#include "render/raycast.h"
#include "world/gen.h"
#include "world/world.h"

using namespace vox;

namespace {
void renderBiome(Biome biome, const char* name, uint64_t seed, const std::string& outDir) {
    VoxelWorld w({ 224, 96, 224 });
    ArenaParams p;
    p.seed = seed;
    p.biome = biome;
    const MapMeta meta = generateArena(w, p);
    PreviewCamera cam;
    const float ground = static_cast<float>(w.heightAt(112, 112));
    cam.position = { 24.0f, ground + 50.0f, 24.0f };
    cam.lookAt = { 112.0f, ground, 112.0f };
    const std::string path = outDir + "/biome_" + name + ".png";
    writePng(path, renderWorld(w, cam, 960, 540));
    std::printf("wrote %s (valid: %s)\n", path.c_str(),
                validateArena(w, meta) ? "yes" : "NO");
}
} // namespace

int main(int argc, char** argv) {
    const uint64_t seed = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1337ull;
    const std::string outDir = argc > 2 ? argv[2] : ".";

    std::printf("Generating 256x96x256 world, seed %llu...\n",
                static_cast<unsigned long long>(seed));
    VoxelWorld world({ 256, 96, 256 });
    world.generate(seed);

    // Some battle scars so the preview shows destruction.
    Rng rng(seed);
    for (int i = 0; i < 24; ++i) {
        const int x = 40 + static_cast<int>(rng.range(176));
        const int z = 40 + static_cast<int>(rng.range(176));
        const int y = world.heightAt(x, z) - 1;
        world.applyBlast({ { static_cast<float>(x), static_cast<float>(y),
                             static_cast<float>(z) },
                           3.0f + rng.unit() * 4.0f, 400, DamageType::Explosive });
    }

    PreviewCamera cam;
    const int cx = 128, cz = 128;
    const float ground = static_cast<float>(world.heightAt(cx, cz));
    cam.position = { 30.0f, ground + 55.0f, 30.0f };
    cam.lookAt = { static_cast<float>(cx), ground, static_cast<float>(cz) };

    const std::string worldPath = outDir + "/world_preview.png";
    writePng(worldPath, renderWorld(world, cam, 960, 540));
    std::printf("wrote %s\n", worldPath.c_str());

    // Vehicle damage states: intact, wing shot off, heavily damaged.
    const VehicleTemplate& tmpl = VehicleTemplate::waspFighter();
    Vehicle intact(tmpl);
    Vehicle winged(tmpl);
    Vehicle battered(tmpl);
    Rng combat(7);
    winged.applyHit({ 10, 2, 4 }, 60, DamageType::Kinetic, combat);   // left wing off
    battered.applyHit({ 10, 2, 4 }, 60, DamageType::Kinetic, combat); // wing off
    battered.applyHit({ 2, 8, 4 }, 90, DamageType::Kinetic, combat);  // engine out
    battered.applyHit({ 12, 8, 4 }, 120, DamageType::Kinetic, combat);// hull mauled

    writePng(outDir + "/wasp_intact.png", renderVehicle(tmpl, intact, 480, 360));
    writePng(outDir + "/wasp_wing_destroyed.png", renderVehicle(tmpl, winged, 480, 360));
    writePng(outDir + "/wasp_battered.png", renderVehicle(tmpl, battered, 480, 360));
    std::printf("wrote wasp_intact.png, wasp_wing_destroyed.png, wasp_battered.png\n");

    // The full roster, intact.
    const struct { TemplateId id; const char* file; } roster[] = {
        { TemplateId::Brick, "vehicle_brick_tank.png" },
        { TemplateId::Talon, "vehicle_talon_mech.png" },
        { TemplateId::Pilot, "vehicle_pilot.png" },
        { TemplateId::PowerStation, "vehicle_power_station.png" },
        { TemplateId::HostStation, "vehicle_host_station.png" },
    };
    for (const auto& entry : roster) {
        const VehicleTemplate& rosterTmpl = VehicleTemplate::byId(entry.id);
        Vehicle fresh(rosterTmpl);
        writePng(outDir + "/" + entry.file, renderVehicle(rosterTmpl, fresh, 480, 360));
        std::printf("wrote %s\n", entry.file);
    }
    // A mobility-killed Talon: one leg shot away.
    {
        const VehicleTemplate& talon = VehicleTemplate::talonMech();
        Vehicle legless(talon);
        Rng mechRng(3);
        legless.applyHit({ 8, 1, 6 }, 500, DamageType::Kinetic, mechRng); // left leg
        writePng(outDir + "/vehicle_talon_leg_destroyed.png",
                 renderVehicle(talon, legless, 480, 360));
        std::printf("wrote vehicle_talon_leg_destroyed.png\n");
    }

    renderBiome(Biome::ShatteredCity, "city", seed, outDir);
    renderBiome(Biome::Canyons, "canyons", seed, outDir);
    renderBiome(Biome::Archipelago, "archipelago", seed, outDir);
    return 0;
}
