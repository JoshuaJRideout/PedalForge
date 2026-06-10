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
    // "Wasp" fighter, concept-art pass: 6 x 4 x 2 m at 0.0625 m voxels
    // (96x64x32), sculpted procedurally — superellipse fuselage with nose
    // taper, LERX strakes into a bubble canopy, swept wings with wingtip
    // missiles, canted twin tails, stabilators, twin nozzles — and painted:
    // navy top / light underside / dark glass / white missiles and fin tips.
    VehicleTemplate t;
    t.name = "Wasp";
    t.id = TemplateId::Wasp;
    t.locomotion = LocomotionClass::Jet;
    t.voxelSize = 0.0625f;
    t.dims = { 96, 64, 32 };
    const size_t volume = static_cast<size_t>(t.dims.x) * t.dims.y * t.dims.z;
    t.partIndex.assign(volume, kEmptySubvoxel);
    t.paint.assign(volume, 0);

    const int hull = t.addPart("hull", PartType::Hull, 180, 0.9f);
    const int wingL = t.addPart("wing.left", PartType::Wing, 60);
    const int wingR = t.addPart("wing.right", PartType::Wing, 60);
    const int engine = t.addPart("engine", PartType::Engine, 90);
    const int cannon = t.addPart("weapon.cannon", PartType::Weapon, 40);
    const int sensor = t.addPart("sensor", PartType::Sensor, 25);

    // Paint palette.
    enum : uint8_t { Navy = 1, DarkBlue, Belly, Glass, White, Nozzle, Red };
    const uint8_t colors[][3] = { { 58, 92, 164 },  { 38, 60, 116 }, { 158, 168, 184 },
                                  { 26, 32, 46 },   { 214, 218, 224 }, { 66, 68, 74 },
                                  { 182, 58, 48 } };
    for (int i = 0; i < 7; ++i)
        for (int k = 0; k < 3; ++k) t.paletteRgb[i + 1][k] = colors[i][k];

    // Piecewise-linear profile sampler.
    auto profile = [](std::initializer_list<std::pair<float, float>> pts, float x) {
        const auto* prev = pts.begin();
        for (const auto& pt : pts) {
            if (x <= pt.first) {
                if (pt.first == prev->first) return pt.second;
                const float k = (x - prev->first) / (pt.first - prev->first);
                return prev->second + (pt.second - prev->second) * k;
            }
            prev = &pt;
        }
        return prev->second;
    };

    for (int z = 0; z < 32; ++z) {
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 96; ++x) {
                const float fx = static_cast<float>(x) + 0.5f;
                const float yc = static_cast<float>(y) + 0.5f - 32.0f;
                const float ay = std::abs(yc);
                const float fz = static_cast<float>(z) + 0.5f;

                int part = -1;
                uint8_t color = 0;

                // --- Twin nozzles (tail end) ---
                if (fx < 7.0f) {
                    for (float side : { -4.4f, 4.4f }) {
                        const float dy = yc - side, dz = fz - 9.5f;
                        const float r2 = dy * dy + dz * dz;
                        if (r2 <= 3.3f * 3.3f && (fx > 2.5f || r2 >= 1.9f * 1.9f)) {
                            part = engine;
                            color = Nozzle;
                        }
                    }
                }

                // --- Fuselage (superellipse cross-section) ---
                if (part < 0 && fx >= 6.0f) {
                    const float hw = profile({ { 6, 6.2f }, { 26, 7.6f }, { 48, 8.0f },
                                               { 62, 6.8f }, { 74, 4.9f }, { 86, 2.9f },
                                               { 96, 0.9f } }, fx);
                    const float top = profile({ { 6, 13.5f }, { 30, 14.0f }, { 56, 15.0f },
                                                { 78, 13.0f }, { 96, 10.0f } }, fx);
                    const float bot = profile({ { 6, 7.5f }, { 22, 6.4f }, { 64, 6.4f },
                                                { 84, 7.6f }, { 96, 8.6f } }, fx);
                    const float zc = (top + bot) * 0.5f, hh = (top - bot) * 0.5f;
                    const float sy = ay / hw, sz = (fz - zc) / hh;
                    if (sy * sy + sz * sz * sz * sz <= 1.0f) {
                        part = fx >= 88.0f ? cannon : (fx < 28.0f ? engine : hull);
                        color = fz <= 8.5f ? Belly : Navy;
                        if (ay <= 1.6f && fz >= top - 1.6f) color = DarkBlue; // spine
                        if (fx >= 93.0f) color = DarkBlue;                    // radome
                    }
                }

                // --- Canopy bubble (dark glass, sensor part): elongated,
                // sits forward, only the raised bubble shows above the spine ---
                {
                    const float ex = (fx - 71.5f) / 8.5f, ey = yc / 2.6f,
                                ez = (fz - 13.5f) / 3.9f;
                    const float d = ex * ex + ey * ey + ez * ez;
                    if (d <= 1.0f && fz >= 14.0f) {
                        part = sensor;
                        color = d >= 0.78f ? White : Glass; // framed glass
                    }
                }

                // --- LERX strakes (thin, sweep from wing root to canopy) ---
                if (part < 0 && ay >= 4.5f && ay <= 8.5f) {
                    const float xle = 58.0f + (8.5f - ay) * 4.6f;
                    if (fx >= 52.0f && fx <= xle && std::abs(fz - 10.8f) <= 1.0f) {
                        part = yc < 0 ? wingL : wingR;
                        color = Navy;
                    }
                }

                // --- Main wings (swept LE, tapered thickness) ---
                if (part < 0 && ay >= 7.0f && ay <= 29.0f) {
                    const float k = (ay - 7.0f) / 22.0f;
                    const float xle = 58.0f - 19.0f * k;
                    const float xte = 30.0f + 3.0f * k;
                    const float halfTh = 1.8f - 0.8f * k;
                    if (fx >= xte && fx <= xle && std::abs(fz - 10.0f) <= halfTh) {
                        part = yc < 0 ? wingL : wingR;
                        color = fz <= 9.2f ? Belly : Navy;
                        if (fx >= xle - 1.8f) color = White;       // leading-edge stripe
                        if (ay >= 23.5f && ay <= 26.5f) color = White; // tip band marking
                    }
                }

                // --- Wingtip rails + missiles (white) ---
                if (part < 0 && ay >= 28.6f && ay <= 31.4f) {
                    const float dy = ay - 30.0f, dz = fz - 10.0f;
                    float r = 1.45f;
                    if (fx > 50.0f) r -= (fx - 50.0f) * 0.16f; // nose cone
                    if (fx >= 28.0f && fx <= 58.0f && dy * dy + dz * dz <= r * r && r > 0.3f) {
                        part = yc < 0 ? wingL : wingR;
                        color = White;
                        if (fx >= 54.0f) color = Red; // seeker tip
                    }
                }

                // --- Twin canted vertical tails ---
                if (part < 0 && fz >= 12.0f && fz <= 27.0f) {
                    const float lean = 9.6f + (fz - 12.0f) * 0.22f; // canted outward
                    if (std::abs(ay - lean) <= 1.1f) {
                        const float xf = 25.0f - (fz - 12.0f) * 0.95f;
                        const float xa = 3.0f + (fz - 12.0f) * 0.15f;
                        if (fx >= xa && fx <= xf) {
                            part = hull;
                            color = fz >= 23.5f ? White : Navy;
                            if (fz >= 23.5f && fx >= xf - 3.0f) color = Red; // tip flash
                        }
                    }
                }

                // --- Stabilators (small rear wings) ---
                if (part < 0 && ay >= 7.0f && ay <= 19.0f && std::abs(fz - 9.0f) <= 1.0f) {
                    const float k = (ay - 7.0f) / 12.0f;
                    const float xle = 24.0f - 9.0f * k;
                    const float xte = 5.0f + 3.0f * k;
                    if (fx >= xte && fx <= xle) {
                        part = hull;
                        color = fz <= 8.8f ? Belly : Navy;
                    }
                }

                // --- Intake scoops under the LERX ---
                if (part < 0 && ay >= 6.5f && ay <= 10.0f && fx >= 40.0f && fx <= 58.0f
                    && fz >= 6.0f && fz <= 9.0f) {
                    part = engine;
                    color = Belly;
                }

                if (part >= 0) {
                    t.partIndex[t.index({ x, y, z })] = static_cast<uint8_t>(part);
                    t.paint[t.index({ x, y, z })] = color;
                }
            }
        }
    }

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

    // NATO paint: olive body, dark tracks, steel barrel, white turret number.
    {
        enum : uint8_t { Olive = 1, DarkOlive, TrackDark, Steel, White };
        const uint8_t pal[][3] = { { 102, 108, 78 }, { 74, 80, 58 }, { 48, 48, 52 },
                                   { 118, 122, 126 }, { 214, 214, 210 } };
        for (int i = 0; i < 5; ++i)
            for (int k = 0; k < 3; ++k) t.paletteRgb[i + 1][k] = pal[i][k];
        t.paint.assign(t.partIndex.size(), 0);
        for (int z = 0; z < t.dims.z; ++z)
            for (int y = 0; y < t.dims.y; ++y)
                for (int x = 0; x < t.dims.x; ++x) {
                    const int part = t.partAt({ x, y, z });
                    if (part < 0) continue;
                    uint8_t c = Olive;
                    if (part == trackL || part == trackR) c = TrackDark;
                    else if (part == engine) c = DarkOlive;
                    else if (part == turret) c = x >= 26 ? Steel : Olive;
                    else if (z <= 5) c = DarkOlive;
                    if (part == turret && x >= 15 && x <= 17 && z >= 17) c = White;
                    t.paint[t.index({ x, y, z })] = c;
                }
    }

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
        // Thigh hugs the pelvis side (mirrored per side so both connect).
        const int ty = side == 0 ? y0 + 2 : y0;
        t.fillBox({ 12, ty, 20 }, { 20, ty + 6, 28 }, leg); // thigh
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

    // Gunmetal paint with navy plates, dark joints, glass canopy.
    {
        enum : uint8_t { Gunmetal = 1, Navy, Joint, Glass, Steel };
        const uint8_t pal[][3] = { { 118, 126, 138 }, { 58, 92, 164 }, { 52, 56, 62 },
                                   { 30, 36, 50 }, { 150, 154, 160 } };
        for (int i = 0; i < 5; ++i)
            for (int k = 0; k < 3; ++k) t.paletteRgb[i + 1][k] = pal[i][k];
        t.paint.assign(t.partIndex.size(), 0);
        for (int z = 0; z < t.dims.z; ++z)
            for (int y = 0; y < t.dims.y; ++y)
                for (int x = 0; x < t.dims.x; ++x) {
                    const int part = t.partAt({ x, y, z });
                    if (part < 0) continue;
                    uint8_t c = Gunmetal;
                    if (part == legL || part == legR) c = z < 16 ? Joint : Gunmetal;
                    else if (part == cockpit) c = Glass;
                    else if (part == sensor) c = Navy;
                    else if (part == armL || part == armR) c = x >= 18 ? Steel : Navy;
                    else if (part == jets) c = Joint;
                    else if (part == torso && z >= 18 && z < 24) c = Navy; // chest plate
                    t.paint[t.index({ x, y, z })] = c;
                }
    }

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
    // Navy flight suit, white trim, dark visor.
    {
        const uint8_t pal[][3] = { { 58, 92, 164 }, { 210, 212, 216 }, { 26, 32, 46 } };
        for (int i = 0; i < 3; ++i)
            for (int k = 0; k < 3; ++k) t.paletteRgb[i + 1][k] = pal[i][k];
        t.paint.assign(t.partIndex.size(), 0);
        for (int z = 0; z < t.dims.z; ++z)
            for (int y = 0; y < t.dims.y; ++y)
                for (int x = 0; x < t.dims.x; ++x) {
                    if (t.partAt({ x, y, z }) < 0) continue;
                    uint8_t c = 1;
                    if (z < 1 || (z >= 6 && z < 8)) c = 2;
                    if (z >= 12) c = (x >= 5 && z >= 13 && z < 15) ? 3 : 2;
                    t.paint[t.index({ x, y, z })] = c;
                }
    }
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
        case TemplateId::KesselFighter: return factionTemplate(Faction::Kessler, UnitClass::Fighter);
        case TemplateId::KesselTank: return factionTemplate(Faction::Kessler, UnitClass::Tank);
        case TemplateId::KesselMech: return factionTemplate(Faction::Kessler, UnitClass::Mech);
        case TemplateId::KesselPilot: return factionTemplate(Faction::Kessler, UnitClass::PilotUnit);
        case TemplateId::MirageFighter: return factionTemplate(Faction::Mirage, UnitClass::Fighter);
        case TemplateId::MirageTank: return factionTemplate(Faction::Mirage, UnitClass::Tank);
        case TemplateId::MirageMech: return factionTemplate(Faction::Mirage, UnitClass::Mech);
        case TemplateId::MiragePilot: return factionTemplate(Faction::Mirage, UnitClass::PilotUnit);
        case TemplateId::ChoirFighter: return factionTemplate(Faction::Choir, UnitClass::Fighter);
        case TemplateId::ChoirTank: return factionTemplate(Faction::Choir, UnitClass::Tank);
        case TemplateId::ChoirMech: return factionTemplate(Faction::Choir, UnitClass::Mech);
        case TemplateId::ChoirPilot: return factionTemplate(Faction::Choir, UnitClass::PilotUnit);
        case TemplateId::Count: break;
    }
    return brickTank();
}

} // namespace vox
