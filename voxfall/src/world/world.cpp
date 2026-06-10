#include "world/world.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_set>
#include "core/noise.h"
#include "core/rng.h"

namespace vox {

VoxelWorld::VoxelWorld(Int3 sizeVoxels) : dims(sizeVoxels) {
    voxels.assign(static_cast<size_t>(dims.x) * dims.y * dims.z, Material::Air);
}

void VoxelWorld::set(Int3 p, Material m) {
    if (inBounds(p)) voxels[index(p)] = m;
}

void VoxelWorld::generate(uint64_t seed) {
    std::fill(voxels.begin(), voxels.end(), Material::Air);
    damage.clear();

    // Heightfield base (§3.3.2): fBm over a domain scaled so feature size is
    // independent of map size.
    const float freq = 1.0f / 48.0f;
    const int baseHeight = dims.y / 3;
    const int amplitude = dims.y / 3;
    seaLevelY = baseHeight - 2;

    for (int z = 0; z < dims.z; ++z) {
        for (int x = 0; x < dims.x; ++x) {
            const float n = fbm2(seed, static_cast<float>(x) * freq, static_cast<float>(z) * freq, 4);
            const int h = std::clamp(baseHeight + static_cast<int>(n * static_cast<float>(amplitude)),
                                     3, dims.y - 1);
            for (int y = 0; y < h; ++y) {
                Material m;
                if (y < 2) {
                    m = Material::Bedrock;
                } else if (y < h - 3) {
                    m = Material::Rock;
                } else {
                    m = Material::Soil;
                }
                voxels[index({ x, y, z })] = m;
            }
            // Flood air below sea level with water.
            for (int y = h; y < seaLevelY; ++y) {
                voxels[index({ x, y, z })] = Material::Water;
            }
        }
    }

    // Hardrock ribs: a second low-frequency noise band turns deep rock into
    // near-permanent canyon skeleton (§3.2, §8 "the map can't be fully flattened").
    const float ribFreq = 1.0f / 96.0f;
    for (int z = 0; z < dims.z; ++z) {
        for (int x = 0; x < dims.x; ++x) {
            const float rib = fbm2(seed ^ 0xB07E5ull, static_cast<float>(x) * ribFreq,
                                   static_cast<float>(z) * ribFreq, 3);
            if (rib < 0.38f) continue;
            const int top = heightAt(x, z) - 6;
            for (int y = 2; y < top; ++y) {
                const size_t i = index({ x, y, z });
                if (voxels[i] == Material::Rock) voxels[i] = Material::Hardrock;
            }
        }
    }

    // Crystal veins: deterministic coordinate-hash scatter inside rock (§3.3.6).
    for (int z = 0; z < dims.z; ++z) {
        for (int x = 0; x < dims.x; ++x) {
            for (int y = 2; y < dims.y; ++y) {
                const size_t i = index({ x, y, z });
                if (voxels[i] != Material::Rock) continue;
                if ((hashCoords(seed ^ 0xC57A1ull, x / 3, y / 3, z / 3) & 0x3FF) == 0) {
                    voxels[i] = Material::Crystal;
                }
            }
        }
    }
}

int VoxelWorld::heightAt(int x, int z) const {
    if (x < 0 || z < 0 || x >= dims.x || z >= dims.z) return 0;
    for (int y = dims.y - 1; y >= 0; --y) {
        if (materialInfo(voxels[index({ x, y, z })]).solid) return y + 1;
    }
    return 0;
}

BlastResult VoxelWorld::applyBlast(const BlastEvent& e) {
    BlastResult result;
    const float mul = terrainDamageMul(e.type);
    if (mul <= 0.0f || e.radius <= 0.0f || e.damage <= 0) return result;

    const int r = static_cast<int>(std::ceil(e.radius));
    const Int3 c{ static_cast<int>(std::floor(e.center.x)),
                  static_cast<int>(std::floor(e.center.y)),
                  static_cast<int>(std::floor(e.center.z)) };

    for (int dy = -r; dy <= r; ++dy) {
        for (int dz = -r; dz <= r; ++dz) {
            for (int dx = -r; dx <= r; ++dx) {
                const Int3 p = c + Int3{ dx, dy, dz };
                if (!inBounds(p)) continue;
                const Material m = voxels[index(p)];
                const MaterialInfo& info = materialInfo(m);
                if (!info.solid || info.hp < 0) continue;

                const Vec3 voxelCenter{ static_cast<float>(p.x) + 0.5f,
                                        static_cast<float>(p.y) + 0.5f,
                                        static_cast<float>(p.z) + 0.5f };
                const float dist = distance(voxelCenter, e.center);
                if (dist > e.radius) continue;

                // Linear falloff from full damage at center to 0 at radius.
                const float falloff = 1.0f - dist / e.radius;
                const int applied = static_cast<int>(static_cast<float>(e.damage) * falloff * mul);
                if (applied <= 0) continue;

                const uint64_t key = static_cast<uint64_t>(index(p));
                const int total = (damage[key] += applied);
                if (total >= info.hp) {
                    voxels[index(p)] = p.y < seaLevelY ? Material::Water : Material::Air;
                    damage.erase(key);
                    result.destroyed.push_back(p);
                }
            }
        }
    }
    collapseOrphans(result.destroyed, result);
    return result;
}

namespace {
bool isStructural(Material m) { return m == Material::Concrete || m == Material::Metal; }
} // namespace

void VoxelWorld::collapseOrphans(const std::vector<Int3>& removed, BlastResult& result) {
    if (removed.empty()) return;

    // Search bound: clusters larger than this are treated as supported (cost
    // cap; a whole intact skyscraper never collapses from one shell anyway).
    constexpr size_t kMaxCluster = 8192;
    const Int3 steps[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 },
                            { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

    std::unordered_set<uint64_t> visited;
    for (const Int3& gone : removed) {
        for (const Int3& s : steps) {
            const Int3 start = gone + s;
            if (!inBounds(start) || !isStructural(at(start))) continue;
            if (visited.count(static_cast<uint64_t>(index(start)))) continue;

            // Flood the structural cluster; supported if any voxel touches a
            // non-structural solid (terrain) or the world floor.
            std::vector<Int3> cluster;
            std::queue<Int3> frontier;
            frontier.push(start);
            visited.insert(static_cast<uint64_t>(index(start)));
            bool supported = false;
            while (!frontier.empty() && cluster.size() <= kMaxCluster) {
                const Int3 p = frontier.front();
                frontier.pop();
                cluster.push_back(p);
                if (p.y == 0) supported = true;
                for (const Int3& d : steps) {
                    const Int3 n = p + d;
                    if (!inBounds(n)) continue;
                    const Material m = at(n);
                    if (isStructural(m)) {
                        const uint64_t key = static_cast<uint64_t>(index(n));
                        if (visited.insert(key).second) frontier.push(n);
                    } else if (materialInfo(m).solid) {
                        supported = true; // resting on/against terrain
                    }
                }
            }
            if (supported || cluster.size() > kMaxCluster) continue;

            for (const Int3& p : cluster) {
                voxels[index(p)] = p.y < seaLevelY ? Material::Water : Material::Air;
                damage.erase(static_cast<uint64_t>(index(p)));
                result.collapsed.push_back(p);
            }
        }
    }
}

uint64_t VoxelWorld::contentHash() const {
    uint64_t h = 0xCBF29CE484222325ull;
    for (Material m : voxels) {
        h ^= static_cast<uint64_t>(m);
        h *= 0x100000001B3ull;
    }
    return h;
}

uint64_t VoxelWorld::chunkHash(Int3 chunkCoord) const {
    uint64_t h = 0xCBF29CE484222325ull;
    const Int3 base{ chunkCoord.x * kChunkSize, chunkCoord.y * kChunkSize, chunkCoord.z * kChunkSize };
    for (int y = 0; y < kChunkSize; ++y) {
        for (int z = 0; z < kChunkSize; ++z) {
            for (int x = 0; x < kChunkSize; ++x) {
                const Int3 p = base + Int3{ x, y, z };
                h ^= static_cast<uint64_t>(inBounds(p) ? voxels[index(p)] : Material::Air);
                h *= 0x100000001B3ull;
            }
        }
    }
    return h;
}

} // namespace vox
