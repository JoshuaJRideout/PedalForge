#include "test_framework.h"
#include "world/gen.h"

using namespace vox;

TEST(arena_all_biomes_validate_across_seeds) {
    for (const Biome biome : { Biome::Dunes, Biome::Canyons, Biome::Archipelago,
                               Biome::ShatteredCity }) {
        for (uint64_t seed : { 11ull, 222ull, 3333ull }) {
            VoxelWorld w({ 160, 80, 160 });
            ArenaParams p;
            p.seed = seed;
            p.biome = biome;
            p.teams = 2;
            const MapMeta meta = generateArena(w, p);
            CHECK(meta.spawns.size() == 2);
            CHECK(validateArena(w, meta)); // every seed ships tank-playable
        }
    }
}

TEST(arena_generation_is_deterministic) {
    ArenaParams p;
    p.seed = 99;
    p.biome = Biome::ShatteredCity;
    VoxelWorld a({ 128, 64, 128 });
    VoxelWorld b({ 128, 64, 128 });
    generateArena(a, p);
    generateArena(b, p);
    CHECK(a.contentHash() == b.contentHash());
}

TEST(city_biome_builds_concrete_ruins) {
    VoxelWorld w({ 160, 80, 160 });
    ArenaParams p;
    p.seed = 5;
    p.biome = Biome::ShatteredCity;
    generateArena(w, p);
    size_t concrete = 0;
    for (int z = 0; z < 160; ++z)
        for (int x = 0; x < 160; ++x)
            for (int y = 0; y < 80; ++y)
                if (w.at({ x, y, z }) == Material::Concrete) ++concrete;
    CHECK(concrete > 4000); // a real skyline, not a shed
}

TEST(archipelago_biome_is_mostly_water) {
    VoxelWorld w({ 160, 80, 160 });
    ArenaParams p;
    p.seed = 8;
    p.biome = Biome::Archipelago;
    generateArena(w, p);
    int wet = 0;
    for (int z = 0; z < 160; ++z)
        for (int x = 0; x < 160; ++x)
            if (w.heightAt(x, z) < w.seaLevel()) ++wet;
    CHECK(wet > 160 * 160 / 5); // at least 20% sea
}

TEST(validator_rejects_walled_map) {
    VoxelWorld w({ 64, 48, 64 });
    for (int x = 0; x < 64; ++x)
        for (int z = 0; z < 64; ++z)
            for (int y = 0; y < 10; ++y) w.set({ x, y, z }, Material::Rock);
    // Impassable hardrock curtain bisecting the map.
    for (int z = 0; z < 64; ++z)
        for (int y = 10; y < 30; ++y) w.set({ 32, y, z }, Material::Hardrock);
    MapMeta meta;
    meta.spawns = { { { 8, 10, 32 }, 0 }, { { 56, 10, 32 }, 1 } };
    CHECK(!validateArena(w, meta));
}
