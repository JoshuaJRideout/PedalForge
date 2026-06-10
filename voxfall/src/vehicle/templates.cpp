#include "vehicle/vehicle.h"
#include <algorithm>

namespace vox {

// Factory vehicle templates, authored in code for M0. These migrate to .vox
// files + parts-annotation sidecars once authored art exists (§4.1) — the
// importer (voxformat.h) is ready whenever the art is.
//
// Template axes: x = forward (nose at +x), y = side (0 = left), z = up.
// NOTE: several tests/demos reference specific sub-voxels by coordinate
// (e.g. Wasp {12,8,4} = hull). Resculpt freely, but keep those points
// inside the same parts.

namespace {

VehicleTemplate makeWasp() {
    // "Wasp" fighter: 6 x 4 x 2 m at 0.125 m voxels (48x32x16) — concept-art
    // density. Tapered fuselage, 12-step delta wings, tip fences, dorsal fin,
    // framed canopy, twin exhausts, cone nose cannon.
    VehicleTemplate t;
    t.name = "Wasp";
    t.id = TemplateId::Wasp;
    t.locomotion = LocomotionClass::Jet;
    t.voxelSize = 0.125f;
    t.dims = { 48, 32, 16 };
    t.partIndex.assign(static_cast<size_t>(t.dims.x) * t.dims.y * t.dims.z, kEmptySubvoxel);

    const int hull = t.addPart("hull", PartType::Hull, 180, 0.9f);
    const int wingL = t.addPart("wing.left", PartType::Wing, 60);
    const int wingR = t.addPart("wing.right", PartType::Wing, 60);
    const int engine = t.addPart("engine", PartType::Engine, 90);
    const int cannon = t.addPart("weapon.cannon", PartType::Weapon, 40);
    const int sensor = t.addPart("sensor", PartType::Sensor, 25);

    // Fuselage (keeps {24,16,8} hull), shoulder chamfers, dorsal fin.
    t.fillBox({ 8, 12, 4 }, { 38, 20, 12 }, hull);
    t.carveBox({ 8, 12, 10 }, { 14, 14, 12 });
    t.carveBox({ 8, 18, 10 }, { 14, 20, 12 });
    t.carveBox({ 8, 12, 4 }, { 12, 13, 6 });
    t.carveBox({ 8, 19, 4 }, { 12, 20, 6 });
    t.fillBox({ 8, 14, 12 }, { 15, 18, 13 }, hull); // fin root
    t.fillBox({ 8, 15, 13 }, { 13, 17, 15 }, hull);
    t.fillBox({ 8, 15, 15 }, { 11, 17, 16 }, hull); // fin tip

    // Delta wings: swept leading edge, thin profile (keep {20,4,8} in left).
    for (int step = 0; step < 12; ++step) {
        const int x = 28 - step;
        const int reach = std::max(0, 11 - step);
        t.fillBox({ x, reach, 6 }, { x + 1, 12, 9 }, wingL);
        t.fillBox({ x, 20, 6 }, { x + 1, 32 - reach, 9 }, wingR);
    }
    t.fillBox({ 17, 2, 7 }, { 25, 12, 8 }, wingL);  // trailing fill
    t.fillBox({ 17, 20, 7 }, { 25, 30, 8 }, wingR);
    t.fillBox({ 16, 0, 6 }, { 22, 2, 12 }, wingL);  // tip fences
    t.fillBox({ 16, 30, 6 }, { 22, 32, 12 }, wingR);

    // Engine: rear block, twin carved exhausts, vent slits (keeps {4,16,8}).
    t.fillBox({ 0, 12, 4 }, { 8, 20, 12 }, engine);
    t.carveBox({ 0, 13, 5 }, { 2, 15, 9 });
    t.carveBox({ 0, 17, 5 }, { 2, 19, 9 });
    t.carveBox({ 2, 12, 11 }, { 4, 13, 12 });
    t.carveBox({ 2, 19, 11 }, { 4, 20, 12 });

    // Nose cannon: stepped cone.
    t.fillBox({ 38, 12, 4 }, { 41, 20, 12 }, cannon);
    t.fillBox({ 41, 13, 5 }, { 44, 19, 11 }, cannon);
    t.fillBox({ 44, 14, 6 }, { 46, 18, 10 }, cannon);
    t.fillBox({ 46, 15, 7 }, { 48, 17, 9 }, cannon);

    // Canopy (sensor): raked, with a frame step.
    t.fillBox({ 22, 13, 12 }, { 33, 19, 13 }, sensor);
    t.fillBox({ 23, 14, 13 }, { 31, 18, 14 }, sensor);
    t.fillBox({ 25, 14, 14 }, { 29, 18, 15 }, sensor);

    t.finalize();
    return t;
}

VehicleTemplate makeBrick() {
    // "Brick" tank: 5 x 3 x 2.5 m at 0.125 m voxels (40x24x20). Sloped
    // glacis, skirted hull, domed turret with cupola ring, barrel + muzzle
    // brake, road-wheel tracks. Mesh-bounds test expects full extents.
    VehicleTemplate t;
    t.name = "Brick";
    t.id = TemplateId::Brick;
    t.locomotion = LocomotionClass::Tracked;
    t.voxelSize = 0.125f;
    t.dims = { 40, 24, 20 };
    t.partIndex.assign(static_cast<size_t>(t.dims.x) * t.dims.y * t.dims.z, kEmptySubvoxel);

    const int hull = t.addPart("hull", PartType::Hull, 260, 0.7f);
    const int trackL = t.addPart("track.left", PartType::Track, 80);
    const int trackR = t.addPart("track.right", PartType::Track, 80);
    const int engine = t.addPart("engine", PartType::Engine, 100);
    const int turret = t.addPart("weapon.turret", PartType::Weapon, 70);

    // Tracks: rounded ends, road wheels, return-roller notches.
    t.fillBox({ 0, 0, 0 }, { 40, 6, 8 }, trackL);
    t.fillBox({ 0, 18, 0 }, { 40, 24, 8 }, trackR);
    for (int side = 0; side < 2; ++side) {
        const int y0 = side == 0 ? 0 : 18;
        t.carveBox({ 0, y0, 6 }, { 4, y0 + 6, 8 });
        t.carveBox({ 0, y0, 4 }, { 2, y0 + 6, 6 });
        t.carveBox({ 36, y0, 6 }, { 40, y0 + 6, 8 });
        t.carveBox({ 38, y0, 4 }, { 40, y0 + 6, 6 });
        for (int x = 4; x < 36; x += 6) {
            t.carveBox({ x, y0, 0 }, { x + 2, y0 + 6, 2 }); // road wheels
            t.carveBox({ x + 1, y0, 7 }, { x + 2, y0 + 6, 8 }); // rollers
        }
    }

    // Engine: rear deck, exhaust grille slots, stack.
    t.fillBox({ 0, 6, 4 }, { 10, 18, 14 }, engine);
    t.carveBox({ 0, 8, 10 }, { 2, 11, 14 });
    t.carveBox({ 0, 13, 10 }, { 2, 16, 14 });
    t.fillBox({ 2, 7, 14 }, { 5, 9, 16 }, engine); // exhaust stack

    // Hull with stepped glacis and side steps.
    t.fillBox({ 10, 6, 4 }, { 32, 18, 16 }, hull);
    t.fillBox({ 32, 6, 4 }, { 36, 18, 12 }, hull);
    t.fillBox({ 36, 8, 4 }, { 40, 16, 10 }, hull); // prow
    t.fillBox({ 32, 8, 12 }, { 34, 16, 14 }, hull); // glacis step
    t.carveBox({ 10, 6, 14 }, { 12, 8, 16 });       // deck chamfers
    t.carveBox({ 10, 16, 14 }, { 12, 18, 16 });

    // Turret: dome (keeps {16,10,16}), corner rounding, barrel, muzzle brake.
    t.fillBox({ 12, 8, 16 }, { 26, 16, 20 }, turret);
    t.carveBox({ 12, 8, 18 }, { 14, 10, 20 });
    t.carveBox({ 12, 14, 18 }, { 14, 16, 20 });
    t.carveBox({ 24, 8, 18 }, { 26, 10, 20 });
    t.carveBox({ 24, 14, 18 }, { 26, 16, 20 });
    t.fillBox({ 26, 11, 16 }, { 36, 13, 18 }, turret); // barrel
    t.fillBox({ 36, 10, 15 }, { 39, 14, 19 }, turret); // muzzle brake
    t.carveBox({ 37, 11, 16 }, { 38, 13, 18 });        // brake vent

    t.finalize();
    return t;
}

VehicleTemplate makeTalon() {
    // "Talon" mid mech: 4 x 3 x 8 m at 0.125 m voxels (32x24x64). Articulated
    // legs, waisted torso, shoulder-padded forearm guns, framed canopy, twin-
    // nozzle jump pack, sensor head. Keep {16,2,12} leg.left, {20,10,50} cockpit.
    VehicleTemplate t;
    t.name = "Talon";
    t.id = TemplateId::Talon;
    t.locomotion = LocomotionClass::Walker;
    t.voxelSize = 0.125f;
    t.dims = { 32, 24, 64 };
    t.partIndex.assign(static_cast<size_t>(t.dims.x) * t.dims.y * t.dims.z, kEmptySubvoxel);

    const int torso = t.addPart("torso", PartType::Hull, 220, 0.8f);
    const int cockpit = t.addPart("cockpit", PartType::Cockpit, 50);
    const int legL = t.addPart("leg.left", PartType::Leg, 90);
    const int legR = t.addPart("leg.right", PartType::Leg, 90);
    const int armL = t.addPart("arm.left", PartType::Weapon, 60);
    const int armR = t.addPart("arm.right", PartType::Weapon, 60);
    const int jets = t.addPart("jumpjets", PartType::JumpJets, 40);
    const int sensor = t.addPart("sensor", PartType::Sensor, 25);

    for (int side = 0; side < 2; ++side) {
        const int leg = side == 0 ? legL : legR;
        const int y0 = side == 0 ? 0 : 16;
        t.fillBox({ 6, y0, 0 }, { 26, y0 + 8, 4 }, leg);      // foot
        t.fillBox({ 22, y0 + 2, 4 }, { 26, y0 + 6, 6 }, leg); // toe step
        t.carveBox({ 6, y0, 0 }, { 8, y0 + 8, 2 });           // heel bevel
        t.fillBox({ 12, y0, 4 }, { 20, y0 + 8, 16 }, leg);    // shin
        t.carveBox({ 12, y0, 4 }, { 14, y0 + 2, 10 });        // shin chamfers
        t.carveBox({ 12, y0 + 6, 4 }, { 14, y0 + 8, 10 });
        t.fillBox({ 10, y0, 16 }, { 22, y0 + 8, 20 }, leg);   // knee armor
        t.fillBox({ 12, y0 + 2, 20 }, { 20, y0 + 8, 28 }, leg); // thigh
    }

    // Torso: pelvis, waist, broad chest with collar.
    t.fillBox({ 8, 8, 26 }, { 24, 16, 30 }, torso);
    t.fillBox({ 10, 8, 30 }, { 22, 16, 36 }, torso);
    t.fillBox({ 6, 6, 36 }, { 26, 18, 48 }, torso);
    t.carveBox({ 6, 6, 36 }, { 8, 8, 40 });   // chest chamfers
    t.carveBox({ 6, 16, 36 }, { 8, 18, 40 });
    t.carveBox({ 24, 6, 36 }, { 26, 8, 40 });
    t.carveBox({ 24, 16, 36 }, { 26, 18, 40 });

    // Arms: shoulder pads, upper arms, forearm guns with muzzles.
    for (int side = 0; side < 2; ++side) {
        const int arm = side == 0 ? armL : armR;
        const int y0 = side == 0 ? 0 : 18;
        t.fillBox({ 8, y0, 42 }, { 20, y0 + 6, 48 }, arm);    // shoulder pad
        t.carveBox({ 8, y0, 46 }, { 10, y0 + 6, 48 });
        t.fillBox({ 10, y0 + 2, 32 }, { 18, y0 + 6, 42 }, arm); // upper arm
        t.fillBox({ 18, y0 + 2, 32 }, { 30, y0 + 6, 38 }, arm); // forearm gun
        t.fillBox({ 30, y0 + 3, 33 }, { 32, y0 + 5, 36 }, arm); // muzzle
    }

    // Cockpit canopy (keeps {20,10,50}), chamfered crown.
    t.fillBox({ 16, 8, 48 }, { 28, 16, 56 }, cockpit);
    t.carveBox({ 24, 8, 54 }, { 28, 16, 56 });
    t.carveBox({ 16, 8, 54 }, { 18, 10, 56 });
    t.carveBox({ 16, 14, 54 }, { 18, 16, 56 });

    // Jump pack: spine-mounted, center channel, twin nozzles.
    t.fillBox({ 0, 6, 32 }, { 6, 18, 46 }, jets);
    t.carveBox({ 0, 10, 32 }, { 2, 14, 46 });
    t.fillBox({ 0, 8, 28 }, { 4, 10, 32 }, jets);
    t.fillBox({ 0, 14, 28 }, { 4, 16, 32 }, jets);

    // Sensor head + antenna.
    t.fillBox({ 18, 10, 56 }, { 24, 14, 60 }, sensor);
    t.carveBox({ 22, 10, 58 }, { 24, 14, 60 }); // visor rake
    t.fillBox({ 20, 11, 60 }, { 22, 13, 64 }, sensor);

    t.finalize();
    return t;
}

VehicleTemplate makePilot() {
    // On-foot pilot: 1 x 1.5 x 2 m at 0.125 m voxels — a real little human.
    VehicleTemplate t;
    t.name = "Pilot";
    t.id = TemplateId::Pilot;
    t.locomotion = LocomotionClass::Pilot;
    t.voxelSize = 0.125f;
    t.dims = { 8, 12, 16 };
    t.partIndex.assign(static_cast<size_t>(t.dims.x) * t.dims.y * t.dims.z, kEmptySubvoxel);
    const int body = t.addPart("body", PartType::Hull, 40);
    t.fillBox({ 2, 3, 0 }, { 6, 5, 6 }, body);   // left leg
    t.fillBox({ 2, 7, 0 }, { 6, 9, 6 }, body);   // right leg
    t.fillBox({ 1, 3, 0 }, { 7, 5, 1 }, body);   // boots
    t.fillBox({ 1, 7, 0 }, { 7, 9, 1 }, body);
    t.fillBox({ 2, 3, 6 }, { 6, 9, 12 }, body);  // torso
    t.fillBox({ 2, 1, 6 }, { 6, 3, 11 }, body);  // left arm
    t.fillBox({ 2, 9, 6 }, { 6, 11, 11 }, body); // right arm
    t.fillBox({ 3, 4, 12 }, { 6, 8, 15 }, body); // head
    t.fillBox({ 3, 4, 15 }, { 5, 8, 16 }, body); // helmet crest
    t.finalize();
    return t;
}

VehicleTemplate makePowerStation() {
    // Sector-claiming building: plinth, pylon, chamfered reactor block with
    // radiator fins, sensor mast with crossbar. Destroy the core and the
    // sector flips neutral (§2.2).
    VehicleTemplate t;
    t.name = "PowerStation";
    t.id = TemplateId::PowerStation;
    t.locomotion = LocomotionClass::Static;
    t.dims = { 16, 16, 24 };
    t.partIndex.assign(static_cast<size_t>(t.dims.x) * t.dims.y * t.dims.z, kEmptySubvoxel);

    const int core = t.addPart("power.core", PartType::Power, 300, 0.9f);
    const int mast = t.addPart("mast", PartType::Sensor, 60);

    t.fillBox({ 1, 1, 0 }, { 15, 15, 3 }, core);   // plinth
    t.fillBox({ 5, 5, 3 }, { 11, 11, 8 }, core);   // pylon
    t.fillBox({ 3, 3, 8 }, { 13, 13, 16 }, core);  // reactor block
    for (const int cx : { 3, 11 })                 // chamfered corners
        for (const int cy : { 3, 11 }) t.carveBox({ cx, cy, 8 }, { cx + 2, cy + 2, 16 });
    t.fillBox({ 0, 7, 9 }, { 3, 9, 15 }, core);    // radiator fins
    t.fillBox({ 13, 7, 9 }, { 16, 9, 15 }, core);
    t.fillBox({ 7, 0, 9 }, { 9, 3, 15 }, core);
    t.fillBox({ 7, 13, 9 }, { 9, 16, 15 }, core);

    t.fillBox({ 7, 7, 16 }, { 9, 9, 22 }, mast);   // mast
    t.fillBox({ 4, 7, 20 }, { 12, 9, 22 }, mast);  // crossbar
    t.corePart = core;

    t.finalize();
    return t;
}

VehicleTemplate makeHostStation() {
    // The player's avatar, factory, and life (§2.1): a ziggurat fortress —
    // octagonal skirt, mid deck, command tier, reactor dome, sensor spire,
    // defense gun emplacement.
    VehicleTemplate t;
    t.name = "HostStation";
    t.id = TemplateId::HostStation;
    t.locomotion = LocomotionClass::Static;
    t.dims = { 48, 48, 28 };
    t.partIndex.assign(static_cast<size_t>(t.dims.x) * t.dims.y * t.dims.z, kEmptySubvoxel);

    const int hull = t.addPart("hull", PartType::Hull, 1200, 0.7f);
    const int reactor = t.addPart("reactor", PartType::Power, 400);
    const int sensor = t.addPart("sensor.array", PartType::Sensor, 120);
    const int gun = t.addPart("weapon.defense", PartType::Weapon, 150);

    t.fillBox({ 2, 2, 0 }, { 46, 46, 6 }, hull);   // skirt
    for (const int cx : { 2, 38 })                 // octagonal corners
        for (const int cy : { 2, 38 }) t.carveBox({ cx, cy, 0 }, { cx + 8, cy + 8, 6 });
    t.fillBox({ 8, 2, 0 }, { 40, 46, 6 }, hull);   // re-fill the cross
    t.fillBox({ 2, 8, 0 }, { 46, 40, 6 }, hull);
    for (const int cx : { 2, 40 })                 // smaller chamfer instead
        for (const int cy : { 2, 40 }) t.carveBox({ cx, cy, 0 }, { cx + 6, cy + 6, 6 });

    t.fillBox({ 8, 8, 6 }, { 40, 40, 12 }, hull);  // mid deck
    t.fillBox({ 12, 12, 12 }, { 36, 36, 16 }, hull); // command tier
    t.fillBox({ 10, 22, 6 }, { 38, 26, 13 }, hull);  // spine ridge

    t.fillBox({ 18, 18, 16 }, { 30, 30, 22 }, reactor); // dome
    t.carveBox({ 18, 18, 20 }, { 20, 20, 22 });
    t.carveBox({ 28, 18, 20 }, { 30, 20, 22 });
    t.carveBox({ 18, 28, 20 }, { 20, 30, 22 });
    t.carveBox({ 28, 28, 20 }, { 30, 30, 22 });

    t.fillBox({ 22, 22, 22 }, { 26, 26, 26 }, sensor); // spire
    t.fillBox({ 23, 23, 26 }, { 25, 25, 28 }, sensor);
    t.fillBox({ 14, 14, 22 }, { 16, 34, 23 }, sensor); // array vanes
    t.fillBox({ 32, 14, 22 }, { 34, 34, 23 }, sensor);

    t.fillBox({ 36, 20, 12 }, { 44, 28, 17 }, gun);  // emplacement
    t.fillBox({ 44, 22, 13 }, { 48, 24, 15 }, gun);  // twin barrels
    t.fillBox({ 44, 25, 13 }, { 48, 27, 15 }, gun);

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

const VehicleTemplate& VehicleTemplate::powerStation() {
    static const VehicleTemplate t = makePowerStation();
    return t;
}

const VehicleTemplate& VehicleTemplate::hostStation() {
    static const VehicleTemplate t = makeHostStation();
    return t;
}

const VehicleTemplate& VehicleTemplate::byId(TemplateId id) {
    switch (id) {
        case TemplateId::Wasp:  return waspFighter();
        case TemplateId::Brick: return brickTank();
        case TemplateId::Talon: return talonMech();
        case TemplateId::Pilot: return pilot();
        case TemplateId::PowerStation: return powerStation();
        case TemplateId::HostStation: return hostStation();
        case TemplateId::Count: break;
    }
    return brickTank();
}

} // namespace vox
