#include "test_framework.h"
#include "core/bytes.h"
#include "core/rng.h"
#include "vehicle/voxformat.h"

using namespace vox;

namespace {

void chunkHeader(ByteWriter& w, const char* id, uint32_t content, uint32_t children) {
    for (int i = 0; i < 4; ++i) w.u8(static_cast<uint8_t>(id[i]));
    w.u32(content);
    w.u32(children);
}

// Minimal valid .vox: a 6x4x3 model, hull block (color 1) + wing slab (color 2).
std::vector<uint8_t> tinyVoxFile() {
    ByteWriter voxels;
    uint32_t count = 0;
    auto add = [&](int x, int y, int z, uint8_t color) {
        voxels.u8(static_cast<uint8_t>(x));
        voxels.u8(static_cast<uint8_t>(y));
        voxels.u8(static_cast<uint8_t>(z));
        voxels.u8(color);
        ++count;
    };
    for (int x = 0; x < 4; ++x)
        for (int y = 0; y < 4; ++y)
            for (int z = 0; z < 3; ++z) add(x, y, z, 1); // hull
    for (int x = 4; x < 6; ++x)
        for (int y = 0; y < 4; ++y) add(x, y, 1, 2);     // wing slab

    ByteWriter w;
    w.u32(0x20584F56); // "VOX "
    w.u32(150);
    const uint32_t sizeChunk = 12 + 12;
    const uint32_t xyziChunk = 12 + 4 + count * 4;
    chunkHeader(w, "MAIN", 0, sizeChunk + xyziChunk);
    chunkHeader(w, "SIZE", 12, 0);
    w.u32(6);
    w.u32(4);
    w.u32(3);
    chunkHeader(w, "XYZI", 4 + count * 4, 0);
    w.u32(count);
    w.data.insert(w.data.end(), voxels.data.begin(), voxels.data.end());
    return std::move(w.data);
}

const char* kSidecar = R"(
name TestCraft
locomotion jet
# color name type hp [armor]
part 1 hull hull 120 0.9
part 2 wing.right wing 40
)";

} // namespace

TEST(vox_parse_and_template_build) {
    const std::optional<VoxModel> model = parseVox(tinyVoxFile());
    CHECK(model.has_value());
    CHECK((model->dims == Int3{ 6, 4, 3 }));
    CHECK(model->voxels.size() == 4 * 4 * 3 + 2 * 4);

    const std::optional<VehicleTemplate> tmpl = templateFromVox(*model, kSidecar);
    CHECK(tmpl.has_value());
    CHECK(tmpl->name == "TestCraft");
    CHECK(tmpl->locomotion == LocomotionClass::Jet);
    CHECK(tmpl->parts.size() == 2);
    CHECK(tmpl->corePart == 0);
    CHECK(tmpl->partAt({ 1, 1, 1 }) == 0); // hull voxel
    CHECK(tmpl->partAt({ 5, 1, 1 }) == 1); // wing voxel
    CHECK(tmpl->partAt({ 5, 1, 2 }) < 0);  // empty space
    CHECK(!tmpl->adjacency[0].empty());    // wing touches hull

    // An imported template is a fully working vehicle.
    Vehicle v(*tmpl);
    Rng rng(1);
    const HitResult hit = v.applyHit({ 5, 1, 1 }, 40, DamageType::Kinetic, rng);
    CHECK(hit.partDestroyed); // wing pool exactly drained
    CHECK(!v.destroyed());
}

TEST(vox_import_rejects_bad_input) {
    const std::vector<uint8_t> good = tinyVoxFile();
    const VoxModel model = *parseVox(good);

    // Truncated file.
    std::vector<uint8_t> cut(good.begin(), good.begin() + good.size() / 2);
    CHECK(!parseVox(cut).has_value());
    // Wrong magic.
    std::vector<uint8_t> badMagic = good;
    badMagic[0] = 'X';
    CHECK(!parseVox(badMagic).has_value());

    // Sidecar violations: unmapped color, no hull, unknown keyword.
    CHECK(!templateFromVox(model, "name X\npart 1 hull hull 100\n").has_value()); // color 2 unmapped
    CHECK(!templateFromVox(model,
                           "name X\npart 1 a wing 50\npart 2 b wing 50\n")
               .has_value()); // no hull
    CHECK(!templateFromVox(model, "name X\nbogus 1\n").has_value());
}
