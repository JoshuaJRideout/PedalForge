#include "test_framework.h"
#include "render/mesh.h"

using namespace vox;

TEST(mesh_single_voxel_is_a_cube) {
    VoxelWorld w({ 32, 32, 32 });
    w.set({ 10, 10, 10 }, Material::Rock);
    const Mesh m = meshChunk(w, { 0, 0, 0 });
    CHECK(m.quadCount() == 6);
    CHECK(m.vertices.size() == 24);
    CHECK(m.indices.size() == 36);
}

TEST(mesh_greedy_merges_coplanar_faces) {
    // A solid 8x8x8 cube must still be exactly 6 quads, not 384.
    VoxelWorld w({ 32, 32, 32 });
    for (int x = 4; x < 12; ++x)
        for (int y = 4; y < 12; ++y)
            for (int z = 4; z < 12; ++z) w.set({ x, y, z }, Material::Concrete);
    const Mesh m = meshChunk(w, { 0, 0, 0 });
    CHECK(m.quadCount() == 6);

    // Two materials side by side may not merge across the seam.
    VoxelWorld w2({ 32, 32, 32 });
    w2.set({ 5, 5, 5 }, Material::Rock);
    w2.set({ 6, 5, 5 }, Material::Metal);
    const Mesh m2 = meshChunk(w2, { 0, 0, 0 });
    CHECK(m2.quadCount() == 10); // 2 cubes sharing one hidden face pair, no merges
}

TEST(mesh_culls_faces_against_neighbor_chunks) {
    // World solid across two x-chunks: chunk 0's +x boundary must be culled by
    // chunk 1's voxels, and no duplicate seam faces emitted.
    VoxelWorld w({ 64, 32, 32 });
    for (int x = 0; x < 64; ++x)
        for (int y = 0; y < 32; ++y)
            for (int z = 0; z < 32; ++z) w.set({ x, y, z }, Material::Rock);
    const Mesh m = meshChunk(w, { 0, 0, 0 });
    // Faces: -x (world edge), top, bottom, -z, +z. NOT +x (neighbor chunk).
    CHECK(m.quadCount() == 5);
    for (const MeshVertex& v : m.vertices) CHECK(!(v.normal.x > 0.5f));
}

TEST(mesh_vehicle_loses_destroyed_parts) {
    const VehicleTemplate& tmpl = VehicleTemplate::waspFighter();
    Vehicle intact(tmpl);
    const Mesh full = meshVehicle(tmpl, intact);
    CHECK(full.vertices.size() > 0);

    // Shoot off the left wing: the re-mesh must shrink.
    Vehicle damaged(tmpl);
    Rng rng(1);
    Int3 wingVoxel{ -1, -1, -1 };
    for (int z = 0; z < tmpl.dims.z && wingVoxel.x < 0; ++z)
        for (int y = 0; y < tmpl.dims.y && wingVoxel.x < 0; ++y)
            for (int x = 0; x < tmpl.dims.x && wingVoxel.x < 0; ++x)
                if (tmpl.partAt({ x, y, z }) >= 0
                    && tmpl.parts[static_cast<size_t>(tmpl.partAt({ x, y, z }))].name == "wing.left")
                    wingVoxel = { x, y, z };
    damaged.applyHit(wingVoxel, 60, DamageType::Kinetic, rng);
    const Mesh clipped = meshVehicle(tmpl, damaged);
    CHECK(clipped.vertices.size() < full.vertices.size());

    // Damaged-but-alive parts darken (shade < 255 appears after hull damage).
    damaged.applyHit({ 24, 16, 8 }, 120, DamageType::Kinetic, rng); // hull to ~33%
    const Mesh charred = meshVehicle(tmpl, damaged);
    bool darkened = false;
    for (const MeshVertex& v : charred.vertices) darkened |= (v.shade < 255);
    CHECK(darkened);
}

TEST(mesh_vehicle_is_in_model_space) {
    const VehicleTemplate& tmpl = VehicleTemplate::brickTank();
    Vehicle v(tmpl);
    const Mesh m = meshVehicle(tmpl, v);
    // Brick is 20x12x10 sub-voxels = 5 x 3 x 2.5 m, anchored bottom-center:
    // x in [-2.5, 2.5], y in [0, 2.5], z in [-1.5, 1.5].
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (const MeshVertex& vtx : m.vertices) {
        minX = std::min(minX, vtx.position.x);
        maxX = std::max(maxX, vtx.position.x);
        minY = std::min(minY, vtx.position.y);
        maxY = std::max(maxY, vtx.position.y);
    }
    CHECK(minX == -2.5f);
    CHECK(maxX == 2.5f);
    CHECK(minY == 0.0f);
    CHECK(maxY == 2.5f);
}
