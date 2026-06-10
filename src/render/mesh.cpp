#include "render/mesh.h"
#include <algorithm>

namespace vox {

namespace {

struct LatticeCell {
    int id = -1; // < 0 = empty
    uint8_t shade = 255;
};

// Greedy meshing over an abstract lattice (Mikola Lysenko's algorithm).
// cellAt must tolerate coordinates one step outside [0, dims) on every axis —
// that's how cross-chunk face culling works (the world answers for neighbors).
// Faces are only *emitted* for cells inside the lattice (no duplicate faces
// between adjacent chunks).
template <typename CellFn>
Mesh greedyMesh(Int3 dims, CellFn cellAt) {
    Mesh mesh;
    const int size[3] = { dims.x, dims.y, dims.z };

    for (int d = 0; d < 3; ++d) {
        const int u = (d + 1) % 3;
        const int v = (d + 2) % 3;
        std::vector<int64_t> mask(static_cast<size_t>(size[u]) * size[v]);

        auto cell = [&](int a, int b, int c) {
            int p[3];
            p[d] = a;
            p[u] = b;
            p[v] = c;
            return cellAt(Int3{ p[0], p[1], p[2] });
        };

        for (int slice = 0; slice <= size[d]; ++slice) {
            // Build the face mask between slice-1 and slice. Sign = normal
            // direction along d. Code packs id+shade so merging respects both.
            for (int j = 0; j < size[v]; ++j) {
                for (int i = 0; i < size[u]; ++i) {
                    const LatticeCell below = cell(slice - 1, i, j);
                    const LatticeCell above = cell(slice, i, j);
                    int64_t code = 0;
                    if (below.id >= 0 && above.id < 0 && slice >= 1) {
                        code = ((static_cast<int64_t>(below.id) << 8) | below.shade) + 1;
                    } else if (above.id >= 0 && below.id < 0 && slice < size[d]) {
                        code = -(((static_cast<int64_t>(above.id) << 8) | above.shade) + 1);
                    }
                    mask[static_cast<size_t>(j) * size[u] + i] = code;
                }
            }

            // Greedy rectangle merge.
            for (int j = 0; j < size[v]; ++j) {
                for (int i = 0; i < size[u];) {
                    const int64_t code = mask[static_cast<size_t>(j) * size[u] + i];
                    if (code == 0) {
                        ++i;
                        continue;
                    }
                    int w = 1;
                    while (i + w < size[u]
                           && mask[static_cast<size_t>(j) * size[u] + i + w] == code)
                        ++w;
                    int h = 1;
                    bool grow = true;
                    while (j + h < size[v] && grow) {
                        for (int k = 0; k < w; ++k)
                            if (mask[static_cast<size_t>(j + h) * size[u] + i + k] != code)
                                grow = false;
                        if (grow) ++h;
                    }
                    for (int jj = 0; jj < h; ++jj)
                        for (int ii = 0; ii < w; ++ii)
                            mask[static_cast<size_t>(j + jj) * size[u] + i + ii] = 0;

                    const bool positive = code > 0;
                    const int64_t magnitude = (positive ? code : -code) - 1;
                    const uint8_t shade = static_cast<uint8_t>(magnitude & 0xFF);
                    const uint8_t id = static_cast<uint8_t>((magnitude >> 8) & 0xFF);

                    Vec3 normal{};
                    (&normal.x)[d] = positive ? 1.0f : -1.0f;

                    auto corner = [&](int du, int dv) {
                        float p[3];
                        p[d] = static_cast<float>(slice);
                        p[u] = static_cast<float>(i + du);
                        p[v] = static_cast<float>(j + dv);
                        return Vec3{ p[0], p[1], p[2] };
                    };
                    const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
                    mesh.vertices.push_back({ corner(0, 0), normal, id, shade });
                    mesh.vertices.push_back({ corner(w, 0), normal, id, shade });
                    mesh.vertices.push_back({ corner(w, h), normal, id, shade });
                    mesh.vertices.push_back({ corner(0, h), normal, id, shade });
                    if (positive) {
                        const uint32_t order[6] = { 0, 2, 1, 0, 3, 2 };
                        for (uint32_t k : order) mesh.indices.push_back(base + k);
                    } else {
                        const uint32_t order[6] = { 0, 1, 2, 0, 2, 3 };
                        for (uint32_t k : order) mesh.indices.push_back(base + k);
                    }
                    i += w;
                }
            }
        }
    }
    return mesh;
}

} // namespace

Mesh meshChunk(const VoxelWorld& world, Int3 chunkCoord) {
    const Int3 origin{ chunkCoord.x * VoxelWorld::kChunkSize,
                       chunkCoord.y * VoxelWorld::kChunkSize,
                       chunkCoord.z * VoxelWorld::kChunkSize };
    const int n = VoxelWorld::kChunkSize;

    Mesh mesh = greedyMesh({ n, n, n }, [&](Int3 p) {
        // Out-of-chunk queries hit the world: neighbor chunks occlude faces.
        const Material m = world.at(origin + p);
        if (!materialInfo(m).solid) return LatticeCell{};
        return LatticeCell{ static_cast<int>(m), 255 };
    });
    for (MeshVertex& vtx : mesh.vertices) {
        vtx.position.x += static_cast<float>(origin.x);
        vtx.position.y += static_cast<float>(origin.y);
        vtx.position.z += static_cast<float>(origin.z);
    }
    return mesh;
}

Mesh meshVehicle(const VehicleTemplate& tmpl, const Vehicle& state) {
    Mesh mesh = greedyMesh(tmpl.dims, [&](Int3 p) {
        const int part = tmpl.partAt(p);
        if (part < 0 || !state.partAlive(part)) return LatticeCell{}; // detached = gone
        const float f = state.partHpFraction(part);
        const uint8_t shade = f >= 0.75f ? 255 : f >= 0.5f ? 200 : f >= 0.25f ? 150 : 100;
        return LatticeCell{ part, shade };
    });

    // Lattice (x=fwd, y=side, z=up) -> model space (x=fwd, y=up, z=side),
    // 0.25 m sub-voxels, anchor at bottom center. The axis swap mirrors
    // handedness, so triangle winding is reversed to stay CCW.
    constexpr float s = 0.25f;
    const float cx = static_cast<float>(tmpl.dims.x) * 0.5f;
    const float cy = static_cast<float>(tmpl.dims.y) * 0.5f;
    for (MeshVertex& vtx : mesh.vertices) {
        const Vec3 p = vtx.position;
        vtx.position = { (p.x - cx) * s, p.z * s, (p.y - cy) * s };
        const Vec3 n = vtx.normal;
        vtx.normal = { n.x, n.z, n.y };
    }
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
        std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);
    return mesh;
}

} // namespace vox
