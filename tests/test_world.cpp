#include "test_framework.h"
#include "world/world.h"

using namespace vox;

TEST(worldgen_is_deterministic_per_seed) {
    VoxelWorld a({ 96, 64, 96 });
    VoxelWorld b({ 96, 64, 96 });
    a.generate(12345);
    b.generate(12345);
    CHECK(a.contentHash() == b.contentHash());

    VoxelWorld c({ 96, 64, 96 });
    c.generate(54321);
    CHECK(a.contentHash() != c.contentHash());
}

TEST(worldgen_produces_layered_terrain) {
    VoxelWorld w({ 64, 64, 64 });
    w.generate(7);
    // Bedrock floor everywhere.
    CHECK(w.at({ 10, 0, 10 }) == Material::Bedrock);
    CHECK(w.at({ 50, 1, 50 }) == Material::Bedrock);
    // Some column has solid ground above bedrock.
    int maxHeight = 0;
    for (int x = 0; x < 64; ++x)
        for (int z = 0; z < 64; ++z) maxHeight = std::max(maxHeight, w.heightAt(x, z));
    CHECK(maxHeight > 4);
    CHECK(maxHeight < 64);
}

TEST(blast_removes_soft_voxels_not_bedrock) {
    VoxelWorld w({ 32, 32, 32 });
    // Hand-build a column: bedrock floor, rock, soil cap.
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            w.set({ x, 0, z }, Material::Bedrock);
            for (int y = 1; y < 6; ++y) w.set({ x, y, z }, Material::Rock);
            for (int y = 6; y < 9; ++y) w.set({ x, y, z }, Material::Soil);
        }
    }
    BlastEvent blast{ { 16.0f, 8.0f, 16.0f }, 4.0f, 200, DamageType::Explosive };
    const BlastResult r = w.applyBlast(blast);
    CHECK(!r.destroyed.empty());
    CHECK(w.at({ 16, 8, 16 }) == Material::Air);   // crater center gone
    CHECK(w.at({ 16, 0, 16 }) == Material::Bedrock); // bedrock immune

    // Energy weapons must not damage terrain at all.
    VoxelWorld w2({ 16, 16, 16 });
    w2.set({ 8, 8, 8 }, Material::Soil);
    const BlastResult r2 = w2.applyBlast({ { 8.5f, 8.5f, 8.5f }, 3.0f, 1000, DamageType::Energy });
    CHECK(r2.destroyed.empty());
    CHECK(w2.at({ 8, 8, 8 }) == Material::Soil);
}

TEST(blast_damage_accumulates_across_hits) {
    VoxelWorld w({ 16, 16, 16 });
    w.set({ 8, 8, 8 }, Material::Hardrock); // 400 hp
    const BlastEvent half{ { 8.5f, 8.5f, 8.5f }, 2.0f, 250, DamageType::Explosive };
    CHECK(w.applyBlast(half).destroyed.empty()); // 250 * falloff < 400
    CHECK(w.at({ 8, 8, 8 }) == Material::Hardrock);
    w.applyBlast(half);
    w.applyBlast(half);
    CHECK(w.at({ 8, 8, 8 }) == Material::Air); // accumulated past 400
}

TEST(blast_below_sea_level_floods_with_water) {
    VoxelWorld w({ 64, 64, 64 });
    w.generate(99);
    const int sea = w.seaLevel();
    CHECK(sea > 2);
    // Find a solid voxel below sea level and blast it.
    bool tested = false;
    for (int x = 2; x < 62 && !tested; ++x) {
        for (int z = 2; z < 62 && !tested; ++z) {
            const Int3 p{ x, sea - 1, z };
            if (w.at(p) == Material::Soil || w.at(p) == Material::Rock) {
                w.applyBlast({ { x + 0.5f, sea - 0.5f, z + 0.5f }, 1.5f, 5000, DamageType::Seismic });
                CHECK(w.at(p) == Material::Water);
                tested = true;
            }
        }
    }
    CHECK(tested);
}

TEST(chunk_hash_changes_only_in_damaged_chunk) {
    VoxelWorld w({ 64, 64, 64 });
    w.generate(42);
    // Blast a known voxel deep inside chunk-x/z (1,1) and inside one y-chunk.
    const int h = w.heightAt(48, 48);
    const Int3 target{ 48, h - 2, 48 };
    const Int3 hitChunk{ target.x / VoxelWorld::kChunkSize,
                         target.y / VoxelWorld::kChunkSize,
                         target.z / VoxelWorld::kChunkSize };
    const uint64_t farBefore = w.chunkHash({ 0, 0, 0 });
    const uint64_t hitBefore = w.chunkHash(hitChunk);
    const BlastResult r = w.applyBlast({ { static_cast<float>(target.x) + 0.5f,
                                           static_cast<float>(target.y) + 0.5f,
                                           static_cast<float>(target.z) + 0.5f },
                                         1.2f, 5000, DamageType::Seismic });
    CHECK(!r.destroyed.empty());
    CHECK(w.chunkHash(hitChunk) != hitBefore);   // damaged chunk hash moved
    CHECK(w.chunkHash({ 0, 0, 0 }) == farBefore); // far chunk untouched
}
