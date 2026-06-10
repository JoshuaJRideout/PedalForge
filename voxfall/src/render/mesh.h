#pragma once
#include <cstdint>
#include <vector>
#include "core/types.h"
#include "vehicle/vehicle.h"
#include "world/world.h"

namespace vox {

// CPU-side meshing (DESIGN.md §11.1): greedy-merged quads for world chunks,
// per-vehicle re-mesh on damage. Pure data in/out — the GPU upload lives in
// the renderer binary; everything here is unit-testable headless.

struct MeshVertex {
    Vec3 position;
    Vec3 normal;
    uint8_t material = 0; // world: Material; vehicles: part index (palette lookup)
    uint8_t shade = 255;  // damage charring etc. (cosmetic, §4.2)
};

struct Mesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices; // triangles, CCW

    size_t quadCount() const { return indices.size() / 6; }
};

// Greedy-mesh one 32^3 chunk of the world (visible faces only, faces merged
// into maximal rectangles per material). Chunk coordinates as in chunkHash().
Mesh meshChunk(const VoxelWorld& world, Int3 chunkCoord);

// Mesh a vehicle's current damage state in template-local space (0.25 m sub-
// voxels, anchor = bottom center): destroyed parts' sub-voxels are omitted —
// the wing is visibly gone — and damaged parts darken via 'shade'.
Mesh meshVehicle(const VehicleTemplate& tmpl, const Vehicle& state);

} // namespace vox
