#include "vehicle/vehicle.h"

namespace vox {

// Factory vehicle templates, authored in code for M0. These migrate to .vox
// files + parts-annotation sidecars once the asset pipeline exists (§4.1).

namespace {

VehicleTemplate makeWasp() {
    // "Wasp" fighter (DESIGN.md §4.2): 6 x 4 x 2 m at 0.25 m sub-voxels.
    // Axes: x = length (tail->nose), y = width (left->right), z = height.
    VehicleTemplate t;
    t.name = "Wasp";
    t.dims = { 24, 16, 8 };
    t.partIndex.assign(static_cast<size_t>(t.dims.x) * t.dims.y * t.dims.z, kEmptySubvoxel);

    const int hull = t.addPart("hull", PartType::Hull, 180, 0.9f);
    const int wingL = t.addPart("wing.left", PartType::Wing, 60);
    const int wingR = t.addPart("wing.right", PartType::Wing, 60);
    const int engine = t.addPart("engine", PartType::Engine, 90);
    const int cannon = t.addPart("weapon.cannon", PartType::Weapon, 40);
    const int sensor = t.addPart("sensor", PartType::Sensor, 25);

    t.fillBox({ 6, 6, 2 }, { 18, 10, 6 }, hull);
    t.fillBox({ 8, 0, 3 }, { 14, 6, 5 }, wingL);
    t.fillBox({ 8, 10, 3 }, { 14, 16, 5 }, wingR);
    t.fillBox({ 0, 6, 2 }, { 6, 10, 6 }, engine);
    t.fillBox({ 18, 7, 3 }, { 24, 9, 5 }, cannon);
    t.fillBox({ 10, 7, 6 }, { 14, 9, 8 }, sensor);

    t.finalize();
    return t;
}

VehicleTemplate makeBrick() {
    // "Brick" tank: 5 x 3 x 2.5 m at 0.25 m sub-voxels.
    VehicleTemplate t;
    t.name = "Brick";
    t.dims = { 20, 12, 10 };
    t.partIndex.assign(static_cast<size_t>(t.dims.x) * t.dims.y * t.dims.z, kEmptySubvoxel);

    const int hull = t.addPart("hull", PartType::Hull, 260, 0.7f);
    const int trackL = t.addPart("track.left", PartType::Track, 80);
    const int trackR = t.addPart("track.right", PartType::Track, 80);
    const int engine = t.addPart("engine", PartType::Engine, 100);
    const int turret = t.addPart("weapon.turret", PartType::Weapon, 70);

    t.fillBox({ 0, 0, 0 }, { 20, 3, 4 }, trackL);
    t.fillBox({ 0, 9, 0 }, { 20, 12, 4 }, trackR);
    t.fillBox({ 0, 3, 2 }, { 5, 9, 8 }, engine);
    t.fillBox({ 5, 3, 2 }, { 18, 9, 8 }, hull);
    t.fillBox({ 7, 4, 8 }, { 13, 8, 10 }, turret);  // turret block
    t.fillBox({ 13, 5, 8 }, { 20, 7, 9 }, turret);  // barrel

    t.finalize();
    return t;
}

} // namespace

const VehicleTemplate& VehicleTemplate::waspFighter() {
    static const VehicleTemplate t = makeWasp();
    return t;
}

const VehicleTemplate& VehicleTemplate::brickTank() {
    static const VehicleTemplate t = makeBrick();
    return t;
}

} // namespace vox
