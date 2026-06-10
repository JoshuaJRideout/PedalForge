#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "world/world.h"

namespace vox {

// .vxm static map format (DESIGN.md §3.4): the bridge between procedural and
// hand-authored worlds, and the future map editor's output format. RLE over
// the canonical voxel order; metadata carries name and team spawn points.
// Version bumps are append-only — old loaders reject newer files cleanly.

struct MapSpawn {
    Int3 position;
    uint8_t team = 0;
    bool operator==(const MapSpawn&) const = default;
};

struct MapMeta {
    std::string name;
    std::vector<MapSpawn> spawns;
};

struct LoadedMap {
    VoxelWorld world;
    MapMeta meta;
};

// Serialize a pristine world (accumulated blast damage is transient and not saved).
std::vector<uint8_t> encodeMap(const VoxelWorld& world, const MapMeta& meta);

// Returns nullopt on malformed/truncated/unsupported input.
std::optional<LoadedMap> decodeMap(const std::vector<uint8_t>& bytes);

bool saveMapFile(const std::string& path, const VoxelWorld& world, const MapMeta& meta);
std::optional<LoadedMap> loadMapFile(const std::string& path);

} // namespace vox
