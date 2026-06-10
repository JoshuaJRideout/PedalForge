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
    t.id = TemplateId::Wasp;
    t.locomotion = LocomotionClass::Jet;
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
    t.id = TemplateId::Brick;
    t.locomotion = LocomotionClass::Tracked;
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

VehicleTemplate makeTalon() {
    // "Talon" mid mech (DESIGN.md §4.7): 4 x 3 x 8 m at 0.25 m sub-voxels.
    // Cockpit is a part but NOT the core: destroying it husks the mech.
    VehicleTemplate t;
    t.name = "Talon";
    t.id = TemplateId::Talon;
    t.locomotion = LocomotionClass::Walker;
    t.dims = { 16, 12, 32 };
    t.partIndex.assign(static_cast<size_t>(t.dims.x) * t.dims.y * t.dims.z, kEmptySubvoxel);

    const int torso = t.addPart("torso", PartType::Hull, 220, 0.8f);
    const int cockpit = t.addPart("cockpit", PartType::Cockpit, 50);
    const int legL = t.addPart("leg.left", PartType::Leg, 90);
    const int legR = t.addPart("leg.right", PartType::Leg, 90);
    const int armL = t.addPart("arm.left", PartType::Weapon, 60);
    const int armR = t.addPart("arm.right", PartType::Weapon, 60);
    const int jets = t.addPart("jumpjets", PartType::JumpJets, 40);
    const int sensor = t.addPart("sensor", PartType::Sensor, 25);

    t.fillBox({ 4, 0, 0 }, { 12, 4, 14 }, legL);
    t.fillBox({ 4, 8, 0 }, { 12, 12, 14 }, legR);
    t.fillBox({ 2, 4, 14 }, { 14, 8, 24 }, torso);
    t.fillBox({ 8, 4, 24 }, { 14, 8, 28 }, cockpit);
    t.fillBox({ 4, 2, 16 }, { 14, 4, 22 }, armL);
    t.fillBox({ 4, 8, 16 }, { 14, 10, 22 }, armR);
    t.fillBox({ 0, 4, 16 }, { 2, 8, 22 }, jets);
    t.fillBox({ 8, 5, 28 }, { 12, 7, 30 }, sensor);

    t.finalize();
    return t;
}

VehicleTemplate makePilot() {
    // On-foot pilot (DESIGN.md §4.7): 0.5 x 0.5 x 2 m. One fragile part.
    VehicleTemplate t;
    t.name = "Pilot";
    t.id = TemplateId::Pilot;
    t.locomotion = LocomotionClass::Pilot;
    t.dims = { 2, 2, 8 };
    t.partIndex.assign(static_cast<size_t>(t.dims.x) * t.dims.y * t.dims.z, kEmptySubvoxel);
    const int body = t.addPart("body", PartType::Hull, 40);
    t.fillBox({ 0, 0, 0 }, { 2, 2, 8 }, body);
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

const VehicleTemplate& VehicleTemplate::talonMech() {
    static const VehicleTemplate t = makeTalon();
    return t;
}

const VehicleTemplate& VehicleTemplate::pilot() {
    static const VehicleTemplate t = makePilot();
    return t;
}

const VehicleTemplate& VehicleTemplate::byId(TemplateId id) {
    switch (id) {
        case TemplateId::Wasp:  return waspFighter();
        case TemplateId::Brick: return brickTank();
        case TemplateId::Talon: return talonMech();
        case TemplateId::Pilot: return pilot();
        case TemplateId::Count: break;
    }
    return brickTank();
}

} // namespace vox
