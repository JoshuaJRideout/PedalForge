#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "core/types.h"
#include "vehicle/vehicle.h"

namespace vox {

// MagicaVoxel .vox import (DESIGN.md §4.1): vehicles are authored as .vox
// models plus a parts-annotation sidecar. This is the forge-pack asset path —
// the same loader factory content and mods use.

struct VoxModel {
    Int3 dims;                                      // x, y, z (z = up, like our templates)
    std::vector<std::pair<Int3, uint8_t>> voxels;   // position, palette color index (1-255)
    uint32_t palette[256] = {};                     // RGBA, palette[i-1] for color i
};

// Parses the first model in a .vox file. Bounds-checked; nullopt on malformed.
std::optional<VoxModel> parseVox(const std::vector<uint8_t>& bytes);

// Builds a runtime VehicleTemplate from a model + sidecar annotation.
//
// Sidecar format (line-based, '#' comments):
//   name Wasp
//   locomotion jet            # tracked | jet | walker | pilot | static
//   part <color> <name> <type> <hp> [armorMul]
//     e.g.: part 1 hull hull 180 0.9
//   types: hull engine wing track weapon sensor shieldgen cargo power
//          cockpit leg jumpjets
//
// Every color used by the model must be mapped to a part; exactly one part
// must be of type hull (the core). nullopt on any violation.
std::optional<VehicleTemplate> templateFromVox(const VoxModel& model, const std::string& sidecar);

} // namespace vox
