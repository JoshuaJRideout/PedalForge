#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "core/types.h"
#include "world/material.h"

namespace vox {

// Damage types (DESIGN.md §5.1). Terrain multiplier is applied by the world;
// part-armor interaction is applied by Vehicle.
enum class DamageType : uint8_t { Kinetic, Explosive, Energy, Seismic };

inline float terrainDamageMul(DamageType t) {
    switch (t) {
        case DamageType::Kinetic:   return 0.25f;
        case DamageType::Explosive: return 1.0f;
        case DamageType::Energy:    return 0.0f;
        case DamageType::Seismic:   return 2.0f;
    }
    return 1.0f;
}

// Server-authoritative destruction event (DESIGN.md §7.2): clients apply these
// deterministically; chunk hashes audit the result.
struct BlastEvent {
    Vec3 center;
    float radius = 0.0f;
    int damage = 0;            // damage at center, falls off linearly to 0 at radius
    DamageType type = DamageType::Explosive;
};

struct BlastResult {
    std::vector<Int3> destroyed; // voxels removed this blast (for VFX/debris/mesh updates)
    std::vector<Int3> collapsed; // structural voxels that lost support and fell (§5.3)
};

// Fixed-size voxel world, 1 voxel = 1 m (DESIGN.md §3.1).
// M0 storage is a flat dense material array + sparse accumulated-damage map.
// Palette-compressed 32^3 chunks replace the flat array when meshing/netcode land;
// chunkHash() already exposes the 32^3 audit granularity that work will keep.
class VoxelWorld {
public:
    static constexpr int kChunkSize = 32;

    explicit VoxelWorld(Int3 sizeVoxels);

    // Deterministic generation from seed (DESIGN.md §3.3). Same seed + same size
    // must produce identical contentHash() on every client.
    void generate(uint64_t seed);

    Int3 size() const { return dims; }
    bool inBounds(Int3 p) const {
        return p.x >= 0 && p.y >= 0 && p.z >= 0 && p.x < dims.x && p.y < dims.y && p.z < dims.z;
    }

    Material at(Int3 p) const { return inBounds(p) ? voxels[index(p)] : Material::Air; }
    void set(Int3 p, Material m);

    // Column height: highest solid voxel y + 1 (0 if none).
    int heightAt(int x, int z) const;

    BlastResult applyBlast(const BlastEvent& e);

    int seaLevel() const { return seaLevelY; }
    void setSeaLevel(int y) { seaLevelY = y; } // map loading (§3.4)

    // FNV-1a over all voxel materials — whole-world audit / determinism tests.
    uint64_t contentHash() const;
    // Hash of one 32^3 chunk (chunk coordinates) — incremental sync audits.
    uint64_t chunkHash(Int3 chunkCoord) const;

private:
    // Structural integrity (§5.3): after voxels are removed, concrete/metal
    // clusters that no longer touch terrain collapse. Deterministic, bounded.
    void collapseOrphans(const std::vector<Int3>& removed, BlastResult& result);

    size_t index(Int3 p) const {
        return static_cast<size_t>(p.y) * static_cast<size_t>(dims.x) * static_cast<size_t>(dims.z)
             + static_cast<size_t>(p.z) * static_cast<size_t>(dims.x)
             + static_cast<size_t>(p.x);
    }

    Int3 dims;
    int seaLevelY = 0;
    std::vector<Material> voxels;
    std::unordered_map<uint64_t, int> damage; // packed index -> accumulated damage
};

} // namespace vox
