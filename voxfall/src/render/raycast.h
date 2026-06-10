#pragma once
#include "core/png.h"
#include "core/types.h"
#include "vehicle/vehicle.h"
#include "world/world.h"

namespace vox {

// Software raycaster: real images of worlds and vehicles with zero GPU.
// Preview/debug tooling (CI artifacts, headless screenshots) — not the game
// renderer, but it exercises the same data the renderer will draw.

struct PreviewCamera {
    Vec3 position;
    Vec3 lookAt;
    float fovDegrees = 60.0f;
};

Image renderWorld(const VoxelWorld& world, const PreviewCamera& cam, int width, int height);

// Turntable render of a vehicle's current damage state (parts as colors,
// damage as darkening, destroyed parts absent). yawDegrees spins the model.
Image renderVehicle(const VehicleTemplate& tmpl, const Vehicle& state, int width, int height,
                    float yawDegrees = 30.0f);

} // namespace vox
