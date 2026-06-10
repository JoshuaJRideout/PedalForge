#pragma once
#include <cstdint>

namespace vox {

// World voxel materials (DESIGN.md §3.2). HP < 0 means indestructible.
enum class Material : uint8_t {
    Air = 0,
    Soil,
    Rock,
    Hardrock,
    Concrete,
    Metal,
    Water,
    Crystal,
    Bedrock,
    Wood,
    Foliage,
    Count
};

struct MaterialInfo {
    const char* name;
    int hp;          // damage to destroy one voxel; <0 = indestructible
    bool solid;      // blocks movement / takes hits
    bool diggable;   // tunneling units can excavate
};

inline const MaterialInfo& materialInfo(Material m) {
    static constexpr MaterialInfo table[] = {
        { "air",      0,   false, false },
        { "soil",     20,  true,  true  },
        { "rock",     80,  true,  true  },
        { "hardrock", 400, true,  false },
        { "concrete", 60,  true,  false },
        { "metal",    120, true,  false },
        { "water",    0,   false, false },
        { "crystal",  50,  true,  false },
        { "bedrock",  -1,  true,  false },
        { "wood",     40,  true,  false },
        { "foliage",  15,  true,  false },
    };
    static_assert(sizeof(table) / sizeof(table[0]) == static_cast<size_t>(Material::Count));
    return table[static_cast<size_t>(m)];
}

} // namespace vox
