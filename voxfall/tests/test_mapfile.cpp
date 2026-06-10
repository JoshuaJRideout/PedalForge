#include <cstdio>
#include "test_framework.h"
#include "world/mapfile.h"

using namespace vox;

TEST(map_roundtrip_preserves_world_and_meta) {
    VoxelWorld original({ 96, 64, 96 });
    original.generate(2026);

    MapMeta meta;
    meta.name = "Glass Dunes Skirmish";
    meta.spawns = { { { 10, 30, 10 }, 0 }, { { 86, 30, 86 }, 1 } };

    const std::vector<uint8_t> bytes = encodeMap(original, meta);
    const std::optional<LoadedMap> loaded = decodeMap(bytes);
    CHECK(loaded.has_value());
    CHECK(loaded->world.contentHash() == original.contentHash());
    CHECK(loaded->world.seaLevel() == original.seaLevel());
    CHECK(loaded->world.size() == original.size());
    CHECK(loaded->meta.name == meta.name);
    CHECK(loaded->meta.spawns == meta.spawns);
}

TEST(map_edits_survive_roundtrip) {
    // The hybrid "baked seed" workflow (§3.4): generate, hand-edit, save.
    VoxelWorld world({ 64, 32, 64 });
    world.generate(5);
    for (int x = 20; x < 28; ++x)
        for (int y = 10; y < 18; ++y) world.set({ x, y, 32 }, Material::Metal); // hand-built wall

    const std::optional<LoadedMap> loaded = decodeMap(encodeMap(world, { "edited", {} }));
    CHECK(loaded.has_value());
    CHECK(loaded->world.at({ 24, 14, 32 }) == Material::Metal);
    CHECK(loaded->world.contentHash() == world.contentHash());
}

TEST(map_rle_compresses_terrain) {
    VoxelWorld world({ 96, 64, 96 });
    world.generate(7);
    const size_t rawBytes = 96ull * 64 * 96;
    const std::vector<uint8_t> bytes = encodeMap(world, { "", {} });
    CHECK(bytes.size() < rawBytes / 2); // RLE must beat raw materially
}

TEST(map_decode_rejects_malformed_input) {
    VoxelWorld world({ 32, 16, 32 });
    world.generate(1);
    std::vector<uint8_t> bytes = encodeMap(world, { "ok", {} });

    // Bad magic.
    std::vector<uint8_t> badMagic = bytes;
    badMagic[0] ^= 0xFF;
    CHECK(!decodeMap(badMagic).has_value());

    // Unsupported future version.
    std::vector<uint8_t> badVersion = bytes;
    badVersion[4] = 99;
    CHECK(!decodeMap(badVersion).has_value());

    // Truncated voxel data.
    std::vector<uint8_t> truncated(bytes.begin(), bytes.begin() + bytes.size() / 2);
    CHECK(!decodeMap(truncated).has_value());

    // Empty.
    CHECK(!decodeMap({}).has_value());
}

TEST(map_file_save_and_load) {
    VoxelWorld world({ 48, 32, 48 });
    world.generate(11);
    MapMeta meta;
    meta.name = "disk test";
    meta.spawns = { { { 5, 20, 5 }, 0 } };

    // CWD-relative: portable across Linux/macOS/Windows CI runners.
    const std::string path = "voxfall_test_map.vxm";
    CHECK(saveMapFile(path, world, meta));
    const std::optional<LoadedMap> loaded = loadMapFile(path);
    CHECK(loaded.has_value());
    if (loaded) {
        CHECK(loaded->world.contentHash() == world.contentHash());
        CHECK(loaded->meta.name == "disk test");
    }
    std::remove(path.c_str());

    CHECK(!loadMapFile("voxfall_does_not_exist.vxm").has_value());
}
