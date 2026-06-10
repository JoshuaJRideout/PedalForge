#include "vehicle/vehicle.h"

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
    // "Wasp" fighter: 6 x 4 x 2 m. Tapered fuselage, delta wings with tip
    // fences, dorsal fin, canopy, twin-notch exhaust.
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

    // Fuselage (keeps {12,8,4} hull) + dorsal fin at the tail.
    t.fillBox({ 4, 6, 2 }, { 19, 10, 6 }, hull);
    t.carveBox({ 4, 6, 5 }, { 7, 7, 6 });   // rear shoulder chamfer
    t.carveBox({ 4, 9, 5 }, { 7, 10, 6 });
    t.fillBox({ 4, 7, 6 }, { 7, 9, 8 }, hull); // fin base
    t.fillBox({ 4, 7, 8 - 1 }, { 5, 9, 8 }, hull);

    // Delta wings, swept leading edge, thin profile (keep {10,2,4} in left).
    for (int step = 0; step < 6; ++step) {
        const int x = 14 - step;                  // rearward from x=14
        const int reach = 5 - step;               // outboard reach grows
        t.fillBox({ x, reach, 3 }, { x + 1, 6, 5 }, wingL);
        t.fillBox({ x, 10, 3 }, { x + 1, 16 - reach, 5 }, wingR);
    }
    t.fillBox({ 8, 6, 3 }, { 15, 6, 5 }, wingL); // (no-op guard row)
    t.fillBox({ 8, 0, 3 }, { 11, 1, 6 }, wingL); // left tip fence
    t.fillBox({ 8, 15, 3 }, { 11, 16, 6 }, wingR);

    // Engine: rear block with twin carved exhausts (keeps {2,8,4}).
    t.fillBox({ 0, 6, 2 }, { 4, 10, 6 }, engine);
    t.carveBox({ 0, 6, 2 }, { 1, 7, 4 });
    t.carveBox({ 0, 9, 2 }, { 1, 10, 4 });

    // Nose cannon: tapered cone.
    t.fillBox({ 19, 6, 2 }, { 21, 10, 6 }, cannon);
    t.fillBox({ 21, 7, 3 }, { 23, 9, 5 }, cannon);
    t.fillBox({ 23, 7, 3 }, { 24, 9, 4 }, cannon);

    // Canopy (sensor): raked windscreen.
    t.fillBox({ 11, 7, 6 }, { 16, 9, 7 }, sensor);
    t.fillBox({ 12, 7, 7 }, { 15, 9, 8 }, sensor);

    t.finalize();
    return t;
}

VehicleTemplate makeBrick() {
    // "Brick" tank: 5 x 3 x 2.5 m. Sloped glacis, rounded turret with barrel
    // and muzzle brake, wheel-notched tracks, rear engine deck with grille.
    // Mesh-bounds test expects full extents: x 0..20, z 0..10.
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

    // Tracks: full length, rounded ends, wheel notches along the bottom.
    t.fillBox({ 0, 0, 0 }, { 20, 3, 4 }, trackL);
    t.fillBox({ 0, 9, 0 }, { 20, 12, 4 }, trackR);
    for (int side = 0; side < 2; ++side) {
        const int y0 = side == 0 ? 0 : 9;
        t.carveBox({ 0, y0, 3 }, { 2, y0 + 3, 4 });   // sloped front idler
        t.carveBox({ 18, y0, 3 }, { 20, y0 + 3, 4 }); // sloped rear
        for (int x = 3; x < 18; x += 4)               // road-wheel notches
            t.carveBox({ x, y0, 0 }, { x + 1, y0 + 3, 1 });
    }

    // Engine: rear deck with exhaust grille slots.
    t.fillBox({ 0, 3, 2 }, { 5, 9, 7 }, engine);
    t.carveBox({ 0, 4, 5 }, { 1, 5, 7 });
    t.carveBox({ 0, 7, 5 }, { 1, 8, 7 });

    // Hull with stepped glacis nose.
    t.fillBox({ 5, 3, 2 }, { 16, 9, 8 }, hull);
    t.fillBox({ 16, 3, 2 }, { 18, 9, 6 }, hull);
    t.fillBox({ 18, 4, 2 }, { 20, 8, 5 }, hull); // prow
    t.fillBox({ 16, 4, 6 }, { 17, 8, 7 }, hull); // glacis step

    // Turret: rounded dome (keeps {8,5,8}) + barrel + muzzle brake.
    t.fillBox({ 6, 4, 8 }, { 13, 8, 10 }, turret);
    t.carveBox({ 6, 4, 9 }, { 7, 5, 10 });
    t.carveBox({ 6, 7, 9 }, { 7, 8, 10 });
    t.carveBox({ 12, 4, 9 }, { 13, 5, 10 });
    t.carveBox({ 12, 7, 9 }, { 13, 8, 10 });
    t.fillBox({ 13, 5, 8 }, { 18, 7, 9 }, turret);  // barrel
    t.fillBox({ 18, 5, 8 }, { 20, 7, 10 }, turret); // muzzle brake

    t.finalize();
    return t;
}

VehicleTemplate makeTalon() {
    // "Talon" mid mech: 4 x 3 x 8 m humanoid. Feet/shin/knee/thigh legs,
    // waisted torso, shoulder-padded weapon arms, canopy cockpit, twin-nozzle
    // jump pack, sensor head with antenna.
    // Keep {8,1,6} in leg.left and {10,5,25} in cockpit.
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

    for (int side = 0; side < 2; ++side) {
        const int leg = side == 0 ? legL : legR;
        const int y0 = side == 0 ? 0 : 8;
        t.fillBox({ 3, y0, 0 }, { 13, y0 + 4, 2 }, leg);     // foot (toes forward)
        t.fillBox({ 11, y0 + 1, 2 }, { 13, y0 + 3, 3 }, leg);// toe step
        t.fillBox({ 6, y0, 2 }, { 10, y0 + 4, 8 }, leg);     // shin ({8,1,6} left)
        t.fillBox({ 5, y0, 8 }, { 11, y0 + 4, 10 }, leg);    // knee armor
        t.fillBox({ 6, y0 + 1, 10 }, { 10, y0 + 4, 14 }, leg); // thigh
    }

    // Torso: pelvis, waist, broad chest.
    t.fillBox({ 4, 4, 13 }, { 12, 8, 15 }, torso);
    t.fillBox({ 5, 4, 15 }, { 11, 8, 18 }, torso);
    t.fillBox({ 3, 3, 18 }, { 13, 9, 24 }, torso);

    // Arms: shoulder pads outside the chest, forearm guns thrust forward.
    for (int side = 0; side < 2; ++side) {
        const int arm = side == 0 ? armL : armR;
        const int y0 = side == 0 ? 0 : 9;
        t.fillBox({ 4, y0, 21 }, { 10, y0 + 3, 24 }, arm);   // shoulder pad
        t.fillBox({ 5, y0 + 1, 16 }, { 9, y0 + 3, 21 }, arm);// upper arm
        t.fillBox({ 9, y0 + 1, 16 }, { 15, y0 + 3, 19 }, arm);// forearm gun
        t.fillBox({ 15, y0 + 1, 16 }, { 16, y0 + 2, 18 }, arm);// muzzle
    }

    // Cockpit canopy (keeps {10,5,25}), chamfered crown.
    t.fillBox({ 8, 4, 24 }, { 14, 8, 28 }, cockpit);
    t.carveBox({ 12, 4, 27 }, { 14, 8, 28 });
    t.carveBox({ 8, 4, 27 }, { 9, 5, 28 });
    t.carveBox({ 8, 7, 27 }, { 9, 8, 28 });

    // Jump pack: spine-mounted, twin nozzles below.
    t.fillBox({ 0, 3, 16 }, { 3, 9, 23 }, jets);
    t.carveBox({ 0, 5, 16 }, { 1, 7, 23 }); // center channel
    t.fillBox({ 0, 4, 14 }, { 2, 5, 16 }, jets);
    t.fillBox({ 0, 7, 14 }, { 2, 8, 16 }, jets);

    // Sensor head + antenna.
    t.fillBox({ 9, 5, 28 }, { 12, 7, 30 }, sensor);
    t.fillBox({ 10, 5, 30 }, { 11, 6, 32 }, sensor);

    t.finalize();
    return t;
}

VehicleTemplate makePilot() {
    // On-foot pilot: 1 x 1.5 x 2 m. Actual little human: legs, torso, arms, head.
    VehicleTemplate t;
    t.name = "Pilot";
    t.id = TemplateId::Pilot;
    t.locomotion = LocomotionClass::Pilot;
    t.dims = { 4, 6, 8 };
    t.partIndex.assign(static_cast<size_t>(t.dims.x) * t.dims.y * t.dims.z, kEmptySubvoxel);
    const int body = t.addPart("body", PartType::Hull, 40);
    t.fillBox({ 1, 1, 0 }, { 3, 2, 3 }, body); // left leg
    t.fillBox({ 1, 4, 0 }, { 3, 5, 3 }, body); // right leg
    t.fillBox({ 1, 1, 3 }, { 3, 5, 6 }, body); // torso
    t.fillBox({ 1, 0, 3 }, { 3, 1, 5 }, body); // left arm
    t.fillBox({ 1, 5, 3 }, { 3, 6, 5 }, body); // right arm
    t.fillBox({ 1, 2, 6 }, { 3, 4, 8 }, body); // head
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
