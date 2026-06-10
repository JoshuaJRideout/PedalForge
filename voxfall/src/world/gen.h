#pragma once
#include "world/mapfile.h"
#include "world/world.h"

namespace vox {

// Arena generation (DESIGN.md §3.3, §8): biome dressing + spawn placement +
// route guarantee + the validation bot that gates ranked seed pools (§8.4).

enum class Biome : uint8_t {
    Dunes,         // open, soft, long sightlines
    Canyons,       // ridged hardrock verticality
    Archipelago,   // raised sea, island clusters
    ShatteredCity, // ruin grammar: hollow, enterable, collapsible buildings
};

struct ArenaParams {
    uint64_t seed = 1;
    Biome biome = Biome::Dunes;
    int teams = 2; // spawn zones, placed evenly on a ring
};

// Generates the world in place and returns spawn metadata. A ground corridor
// between consecutive spawns is carved/ramped so tank-only play is never
// softlocked (§3.3.5).
MapMeta generateArena(VoxelWorld& world, const ArenaParams& params);

// Validation bot (§3.3.7): tank-walkability BFS — every spawn must reach
// every other spawn climbing <= 1 voxel per cell and fording <= 2 deep water.
bool validateArena(const VoxelWorld& world, const MapMeta& meta);

} // namespace vox
