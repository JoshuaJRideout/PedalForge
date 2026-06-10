#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include "vehicle/vehicle.h"

namespace vox {

// Faction rosters (DESIGN.md §4.6), each with a real-world design language:
//   Kessler Combine — heavy eastern-bloc industry: A-10-style gunship,
//     KV-2-style slab-turret heavy tank, twin-cannon barrel mech.
//   Mirage Protocol — faceted stealth drones: flying-wing UCAV, low wheeled
//     recon scout, reverse-joint spindle mech.
//   Choir — organic bio-craft: forward-swept fighter, rounded grav-tank,
//     digitigrade mech with halo array.
// Vanguard (NATO-modern) lives in templates.cpp as the original roster.

namespace {

float profileAt(std::initializer_list<std::pair<float, float>> pts, float x) {
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
}

void setPalette(VehicleTemplate& t, std::initializer_list<std::array<uint8_t, 3>> colors) {
    int i = 1;
    for (const auto& c : colors) {
        t.paletteRgb[i][0] = c[0];
        t.paletteRgb[i][1] = c[1];
        t.paletteRgb[i][2] = c[2];
        ++i;
    }
}

void put(VehicleTemplate& t, int x, int y, int z, int part, uint8_t color) {
    const size_t i = t.index({ x, y, z });
    t.partIndex[i] = static_cast<uint8_t>(part);
    t.paint[i] = color;
}

void initGrid(VehicleTemplate& t) {
    const size_t volume = static_cast<size_t>(t.dims.x) * t.dims.y * t.dims.z;
    t.partIndex.assign(volume, kEmptySubvoxel);
    t.paint.assign(volume, 0);
}

// ---------------------------------------------------------------- Kessler ---

VehicleTemplate makeKesselFighter() {
    // "Grinder": A-10 language — straight fat wings, twin engine pods above
    // the tail, twin fins on a joined stabilizer, a gun for a nose.
    VehicleTemplate t;
    t.name = "Grinder";
    t.id = TemplateId::KesselFighter;
    t.locomotion = LocomotionClass::Jet;
    t.voxelSize = 0.0625f;
    t.dims = { 96, 88, 28 };
    initGrid(t);
    enum : uint8_t { Olive = 1, Gray, Steel, Glass, Yellow, Nozzle, Teeth };
    setPalette(t, { { 96, 104, 76 }, { 128, 132, 124 }, { 88, 92, 96 }, { 30, 34, 40 },
                    { 208, 170, 50 }, { 60, 62, 66 }, { 220, 220, 214 } });

    const int hull = t.addPart("hull", PartType::Hull, 220, 0.75f); // famously tanky
    const int wingL = t.addPart("wing.left", PartType::Wing, 70);
    const int wingR = t.addPart("wing.right", PartType::Wing, 70);
    const int engine = t.addPart("engine", PartType::Engine, 100);
    const int gun = t.addPart("weapon.cannon", PartType::Weapon, 60);
    const int sensor = t.addPart("sensor", PartType::Sensor, 25);

    for (int z = 0; z < t.dims.z; ++z)
        for (int y = 0; y < t.dims.y; ++y)
            for (int x = 0; x < t.dims.x; ++x) {
                const float fx = x + 0.5f, fz = z + 0.5f;
                const float yc = y + 0.5f - 44.0f, ay = std::abs(yc);
                int part = -1;
                uint8_t color = 0;

                // Tube fuselage with blunt nose.
                const float hw = profileAt({ { 0, 4.6f }, { 18, 5.4f }, { 70, 5.6f },
                                             { 88, 4.4f }, { 96, 2.6f } }, fx);
                const float top = profileAt({ { 0, 13.0f }, { 56, 13.6f }, { 78, 12.6f },
                                              { 96, 10.8f } }, fx);
                const float bot = profileAt({ { 0, 7.0f }, { 40, 6.2f }, { 96, 7.0f } }, fx);
                const float zc = (top + bot) * 0.5f, hh = (top - bot) * 0.5f;
                const float sy = ay / hw, sz = (fz - zc) / hh;
                if (sy * sy + sz * sz <= 1.0f) {
                    part = fx >= 78.0f ? gun : hull;
                    color = fz <= 8.0f ? Gray : Olive;
                    if (fx >= 90.0f) color = Steel; // gun muzzle face
                    if (fx >= 80.0f && fx < 90.0f && fz <= 10.0f) color = Teeth; // jaw flash
                    if (fx >= 80.0f && fx < 84.0f && fz > 10.0f) color = Yellow; // nose ring
                }

                // Canopy: framed greenhouse well forward.
                const float ex = (fx - 72.0f) / 7.0f, ey = yc / 2.6f, ez = (fz - 13.0f) / 3.6f;
                if (ex * ex + ey * ey + ez * ez <= 1.0f && fz >= 13.5f) {
                    part = sensor;
                    color = Glass;
                }

                // Long straight wings, slight droop at tips.
                if (part < 0 && ay >= 5.0f && ay <= 42.0f) {
                    const float k = (ay - 5.0f) / 37.0f;
                    const float droop = k * 1.6f;
                    if (fx >= 36.0f - 4.0f * k && fx <= 56.0f - 2.0f * k
                        && std::abs(fz - (10.5f - droop)) <= 1.6f - 0.5f * k) {
                        part = yc < 0 ? wingL : wingR;
                        color = fz - (10.5f - droop) <= -0.4f ? Gray : Olive;
                        if (ay >= 38.5f) color = Yellow; // tip marking
                    }
                }
                // Underwing stores (stubby bombs).
                if (part < 0 && std::abs(fz - 7.6f) <= 1.1f && fx >= 40.0f && fx <= 50.0f) {
                    for (float rail : { 14.0f, 22.0f, 30.0f }) {
                        if (std::abs(ay - rail) <= 1.1f) {
                            part = yc < 0 ? wingL : wingR;
                            color = Steel;
                        }
                    }
                }

                // Twin engine pods above the rear fuselage.
                if (part < 0) {
                    for (float side : { -8.0f, 8.0f }) {
                        const float dy = yc - side, dz = fz - 15.5f;
                        const float r2 = dy * dy + dz * dz;
                        if (fx >= 12.0f && fx <= 34.0f && r2 <= 3.4f * 3.4f) {
                            part = engine;
                            color = (fx < 15.0f || r2 >= 2.6f * 2.6f) ? Gray : Nozzle;
                            if (fx < 14.0f && r2 <= 2.0f * 2.0f) color = Nozzle;
                        }
                    }
                }

                // Tail: joined horizontal stabilizer + twin end-plate fins.
                if (part < 0 && fx >= 2.0f && fx <= 14.0f) {
                    if (ay <= 16.0f && std::abs(fz - 11.0f) <= 1.2f) {
                        part = hull;
                        color = Olive;
                    } else if (std::abs(ay - 15.0f) <= 1.1f && fz >= 11.0f && fz <= 24.0f
                               && fx >= 3.0f && fx <= 12.0f - (fz - 11.0f) * 0.25f) {
                        part = hull;
                        color = fz >= 21.0f ? Yellow : Olive;
                    }
                }

                if (part >= 0) put(t, x, y, z, part, color);
            }
    t.finalize();
    return t;
}

VehicleTemplate makeKesselTank() {
    // "Bastion": KV-2 language — towering slab turret, short heavy gun,
    // skirted wide tracks, riveted look via paint banding.
    VehicleTemplate t;
    t.name = "Bastion";
    t.id = TemplateId::KesselTank;
    t.locomotion = LocomotionClass::Tracked;
    t.voxelSize = 0.125f;
    t.dims = { 44, 28, 28 };
    initGrid(t);
    enum : uint8_t { Green = 1, DarkGreen, Steel, Rust, Yellow, Black };
    setPalette(t, { { 86, 98, 66 }, { 62, 72, 50 }, { 100, 102, 104 }, { 122, 84, 56 },
                    { 208, 170, 50 }, { 40, 40, 42 } });

    const int hull = t.addPart("hull", PartType::Hull, 340, 0.6f);
    const int trackL = t.addPart("track.left", PartType::Track, 110);
    const int trackR = t.addPart("track.right", PartType::Track, 110);
    const int engine = t.addPart("engine", PartType::Engine, 120);
    const int turret = t.addPart("weapon.turret", PartType::Weapon, 120);

    t.fillBox({ 0, 0, 0 }, { 44, 7, 9 }, trackL);
    t.fillBox({ 0, 21, 0 }, { 44, 28, 9 }, trackR);
    for (int side = 0; side < 2; ++side) {
        const int y0 = side == 0 ? 0 : 21;
        t.carveBox({ 0, y0, 6 }, { 4, y0 + 7, 9 });
        t.carveBox({ 40, y0, 6 }, { 44, y0 + 7, 9 });
        for (int x = 4; x < 40; x += 7) t.carveBox({ x, y0, 0 }, { x + 3, y0 + 7, 2 });
    }
    t.fillBox({ 0, 7, 5 }, { 12, 21, 14 }, engine);
    t.carveBox({ 0, 9, 11 }, { 2, 12, 14 });
    t.carveBox({ 0, 16, 11 }, { 2, 19, 14 });
    t.fillBox({ 12, 7, 5 }, { 38, 21, 15 }, hull);
    t.fillBox({ 38, 9, 5 }, { 44, 19, 12 }, hull); // prow wedge
    // The famous oversized slab turret.
    t.fillBox({ 14, 8, 15 }, { 32, 20, 27 }, turret);
    t.carveBox({ 14, 8, 25 }, { 16, 10, 27 });
    t.carveBox({ 14, 18, 25 }, { 16, 20, 27 });
    t.fillBox({ 32, 11, 18 }, { 42, 17, 23 }, turret); // mantlet
    t.fillBox({ 42, 12, 19 }, { 44, 16, 22 }, turret); // stub barrel

    // Paint: green body, dark band, steel tracks, yellow turret stripe.
    for (int z = 0; z < t.dims.z; ++z)
        for (int y = 0; y < t.dims.y; ++y)
            for (int x = 0; x < t.dims.x; ++x) {
                const int part = t.partAt({ x, y, z });
                if (part < 0) continue;
                uint8_t c = Green;
                if (part == trackL || part == trackR) c = z < 3 ? Black : Steel;
                else if (part == engine) c = DarkGreen;
                else if (part == turret) {
                    c = z >= 24 ? DarkGreen : Green;
                    if (z >= 20 && z < 22) c = Yellow; // command stripe
                    if (x >= 42) c = Black;            // muzzle
                } else if (z <= 6) c = DarkGreen;
                if (part == hull && (x % 9) == 0 && z >= 7 && z <= 14) c = Rust; // weld seams
                t.paint[t.index({ x, y, z })] = c;
            }
    t.finalize();
    return t;
}

VehicleTemplate makeKesselMech() {
    // "Jotun": squat barrel torso on thick legs, twin shoulder cannons.
    VehicleTemplate t;
    t.name = "Jotun";
    t.id = TemplateId::KesselMech;
    t.locomotion = LocomotionClass::Walker;
    t.voxelSize = 0.125f;
    t.dims = { 36, 28, 56 };
    initGrid(t);
    enum : uint8_t { Green = 1, DarkGreen, Steel, Yellow, Glass, Black };
    setPalette(t, { { 86, 98, 66 }, { 62, 72, 50 }, { 100, 102, 104 }, { 208, 170, 50 },
                    { 34, 40, 46 }, { 40, 40, 42 } });

    const int torso = t.addPart("torso", PartType::Hull, 280, 0.7f);
    const int cockpit = t.addPart("cockpit", PartType::Cockpit, 60);
    const int legL = t.addPart("leg.left", PartType::Leg, 120);
    const int legR = t.addPart("leg.right", PartType::Leg, 120);
    const int armL = t.addPart("arm.left", PartType::Weapon, 80);
    const int armR = t.addPart("arm.right", PartType::Weapon, 80);
    const int jets = t.addPart("jumpjets", PartType::JumpJets, 50);
    const int sensor = t.addPart("sensor", PartType::Sensor, 30);

    for (int side = 0; side < 2; ++side) {
        const int leg = side == 0 ? legL : legR;
        const int y0 = side == 0 ? 1 : 17;
        t.fillBox({ 6, y0, 0 }, { 30, y0 + 10, 5 }, leg);   // wide foot
        t.fillBox({ 10, y0 + 1, 5 }, { 22, y0 + 9, 18 }, leg); // thick column
        t.fillBox({ 8, y0, 18 }, { 24, y0 + 10, 24 }, leg);  // hip block
    }
    t.fillBox({ 6, 6, 22 }, { 30, 22, 42 }, torso); // barrel body
    t.carveBox({ 6, 6, 22 }, { 8, 8, 26 });
    t.carveBox({ 6, 20, 22 }, { 8, 22, 26 });
    t.carveBox({ 28, 6, 38 }, { 30, 22, 42 });
    t.fillBox({ 24, 9, 38 }, { 34, 19, 46 }, cockpit); // jutting brow cab
    t.fillBox({ 26, 11, 46 }, { 32, 17, 48 }, sensor); // periscope row
    // Twin shoulder cannons.
    for (int side = 0; side < 2; ++side) {
        const int arm = side == 0 ? armL : armR;
        const int y0 = side == 0 ? 0 : 21;
        t.fillBox({ 8, y0, 40 }, { 18, y0 + 7, 50 }, arm);  // breech box
        t.fillBox({ 18, y0 + 2, 43 }, { 36, y0 + 5, 47 }, arm); // long barrel
    }
    t.fillBox({ 0, 8, 26 }, { 6, 20, 40 }, jets);
    t.carveBox({ 0, 12, 26 }, { 2, 16, 40 });

    for (int z = 0; z < t.dims.z; ++z)
        for (int y = 0; y < t.dims.y; ++y)
            for (int x = 0; x < t.dims.x; ++x) {
                const int part = t.partAt({ x, y, z });
                if (part < 0) continue;
                uint8_t c = Green;
                if (part == legL || part == legR) c = z < 5 ? Black : DarkGreen;
                else if (part == cockpit) c = (x >= 32) ? Glass : Green;
                else if (part == sensor) c = Glass;
                else if (part == armL || part == armR) c = x >= 18 ? Steel : DarkGreen;
                else if (part == jets) c = Steel;
                if (part == torso && z >= 40) c = Yellow; // hazard collar
                t.paint[t.index({ x, y, z })] = c;
            }
    t.finalize();
    return t;
}

// ----------------------------------------------------------------- Mirage ---

VehicleTemplate makeMirageFighter() {
    // "Specter": flying-wing UCAV — pure delta, no tail, sensor ridge
    // instead of a canopy, sawtooth trailing edge.
    VehicleTemplate t;
    t.name = "Specter";
    t.id = TemplateId::MirageFighter;
    t.locomotion = LocomotionClass::Jet;
    t.voxelSize = 0.0625f;
    t.dims = { 72, 96, 16 };
    initGrid(t);
    enum : uint8_t { Tan = 1, Taupe, DarkFacet, Orange, Nozzle };
    setPalette(t, { { 176, 162, 134 }, { 142, 132, 112 }, { 96, 92, 84 },
                    { 222, 122, 42 }, { 56, 56, 58 } });

    const int hull = t.addPart("hull", PartType::Hull, 130, 1.0f); // fragile by design
    const int wingL = t.addPart("wing.left", PartType::Wing, 45);
    const int wingR = t.addPart("wing.right", PartType::Wing, 45);
    const int engine = t.addPart("engine", PartType::Engine, 70);
    const int gunPod = t.addPart("weapon.pod", PartType::Weapon, 35);
    const int sensor = t.addPart("sensor", PartType::Sensor, 20);

    for (int z = 0; z < t.dims.z; ++z)
        for (int y = 0; y < t.dims.y; ++y)
            for (int x = 0; x < t.dims.x; ++x) {
                const float fx = x + 0.5f, fz = z + 0.5f;
                const float yc = y + 0.5f - 48.0f, ay = std::abs(yc);
                int part = -1;
                uint8_t color = 0;

                // Delta planform: nose at x=68 center, tips at x=6, |yc|=44.
                const float xle = 68.0f - ay * 1.32f;
                float xte = 6.0f;
                // Sawtooth trailing edge.
                const int tooth = static_cast<int>(ay / 11.0f);
                xte += (tooth % 2 == 0) ? 0.0f : 5.0f;
                if (ay <= 44.0f && fx >= xte && fx <= xle) {
                    // Lens thickness: fat centerline, knife-edge tips.
                    const float th = 4.6f * (1.0f - ay / 48.0f) + 0.8f;
                    if (std::abs(fz - 7.0f) <= th * 0.5f) {
                        part = ay <= 11.0f ? hull : (yc < 0 ? wingL : wingR);
                        // Faceted two-tone: bands along sweep lines.
                        const int band = static_cast<int>((fx + ay * 0.7f) / 9.0f);
                        color = (band % 2 == 0) ? Tan : Taupe;
                        if (fx >= xle - 1.6f) color = DarkFacet; // LE
                        if (fx <= xte + 1.2f) color = DarkFacet; // TE
                    }
                }
                // Engine: center rear; intake slot on top, nozzle slot at TE.
                if (part == hull && fx <= 26.0f) part = engine;
                if (part == engine && fx <= 9.0f && std::abs(fz - 7.0f) <= 1.2f
                    && ay <= 5.0f)
                    color = Nozzle;
                // Sensor ridge instead of a canopy.
                if (ay <= 2.4f && fx >= 38.0f && fx <= 58.0f && fz >= 9.0f) {
                    const float hump = 9.0f + (1.0f - std::abs(fx - 48.0f) / 10.0f) * 2.6f;
                    if (fz <= hump) {
                        part = sensor;
                        color = Orange;
                    }
                }
                // Chin gun pod.
                if (ay <= 2.0f && fx >= 50.0f && fx <= 62.0f && fz <= 5.0f && fz >= 3.0f) {
                    part = gunPod;
                    color = DarkFacet;
                }

                if (part >= 0) put(t, x, y, z, part, color);
            }
    t.finalize();
    return t;
}

VehicleTemplate makeMirageTank() {
    // "Dart": low wheeled recon — wedge hull, six wheels, flat one-man
    // turret with a needle gun, desert facets.
    VehicleTemplate t;
    t.name = "Dart";
    t.id = TemplateId::MirageTank;
    t.locomotion = LocomotionClass::Tracked; // wheels are visual; model is ground
    t.voxelSize = 0.125f;
    t.dims = { 40, 22, 16 };
    initGrid(t);
    enum : uint8_t { Tan = 1, Taupe, DarkFacet, Orange, Black };
    setPalette(t, { { 176, 162, 134 }, { 142, 132, 112 }, { 96, 92, 84 },
                    { 222, 122, 42 }, { 38, 38, 40 } });

    const int hull = t.addPart("hull", PartType::Hull, 180, 0.85f);
    const int trackL = t.addPart("track.left", PartType::Track, 60);
    const int trackR = t.addPart("track.right", PartType::Track, 60);
    const int engine = t.addPart("engine", PartType::Engine, 80);
    const int turret = t.addPart("weapon.turret", PartType::Weapon, 50);

    // Six wheels per side as the "track" parts.
    for (int side = 0; side < 2; ++side) {
        const int wheels = side == 0 ? trackL : trackR;
        const int y0 = side == 0 ? 0 : 18;
        for (int w = 0; w < 3; ++w) {
            const int cx = 7 + w * 12;
            for (int x = cx - 3; x <= cx + 3; ++x)
                for (int z = 0; z < 7; ++z) {
                    const float dx = x + 0.5f - cx, dz = z + 0.5f - 3.5f;
                    if (dx * dx + dz * dz <= 3.4f * 3.4f)
                        for (int y = y0; y < y0 + 4; ++y)
                            put(t, x, y, z, wheels, Black);
                }
        }
    }
    // Wedge hull over the wheels.
    for (int z = 4; z < 12; ++z)
        for (int y = 2; y < 20; ++y)
            for (int x = 2; x < 40; ++x) {
                const float taper = (x - 2) / 38.0f;            // sharp nose
                const float topZ = 11.0f - taper * 0.0f;
                const float noseCut = x >= 30 ? (x - 30) * 0.55f : 0.0f;
                if (z + noseCut <= topZ && z >= 4) {
                    const int part = x < 10 ? engine : hull;
                    const int band = (x + y) / 7;
                    put(t, x, y, z, part, (band % 2 == 0) ? Tan : Taupe);
                }
            }
    // Flat turret + needle gun.
    t.fillBox({ 14, 7, 12 }, { 24, 15, 15 }, turret);
    t.fillBox({ 24, 10, 12 }, { 38, 12, 14 }, turret);
    for (int z = 12; z < 15; ++z)
        for (int y = 7; y < 15; ++y)
            for (int x = 14; x < 38; ++x)
                if (t.partAt({ x, y, z }) == turret)
                    t.paint[t.index({ x, y, z })] = x >= 24 ? DarkFacet : Taupe;
    // Sensor mast dot.
    put(t, 16, 10, 15, turret, Orange);
    put(t, 16, 11, 15, turret, Orange);

    t.finalize();
    return t;
}

VehicleTemplate makeMirageMech() {
    // "Stilt": reverse-joint spindle legs, tiny body, one big orange eye,
    // long rail arm.
    VehicleTemplate t;
    t.name = "Stilt";
    t.id = TemplateId::MirageMech;
    t.locomotion = LocomotionClass::Walker;
    t.voxelSize = 0.125f;
    t.dims = { 36, 24, 58 };
    initGrid(t);
    enum : uint8_t { Tan = 1, Taupe, DarkFacet, Orange, Black };
    setPalette(t, { { 176, 162, 134 }, { 142, 132, 112 }, { 96, 92, 84 },
                    { 222, 122, 42 }, { 38, 38, 40 } });

    const int torso = t.addPart("torso", PartType::Hull, 160, 0.95f);
    const int cockpit = t.addPart("cockpit", PartType::Cockpit, 40);
    const int legL = t.addPart("leg.left", PartType::Leg, 70);
    const int legR = t.addPart("leg.right", PartType::Leg, 70);
    const int rail = t.addPart("arm.rail", PartType::Weapon, 55);
    const int armR = t.addPart("arm.claw", PartType::Weapon, 45);
    const int jets = t.addPart("jumpjets", PartType::JumpJets, 35);
    const int sensor = t.addPart("sensor", PartType::Sensor, 20);

    // Reverse-joint legs: shin forward-low, knee aft-high, thigh forward.
    for (int side = 0; side < 2; ++side) {
        const int leg = side == 0 ? legL : legR;
        const int y0 = side == 0 ? 2 : 16;
        t.fillBox({ 14, y0, 0 }, { 28, y0 + 6, 3 }, leg);     // splay foot
        t.fillBox({ 18, y0 + 1, 3 }, { 22, y0 + 5, 14 }, leg);// lower strut
        t.fillBox({ 10, y0 + 1, 14 }, { 22, y0 + 5, 18 }, leg);// aft knee
        t.fillBox({ 10, y0 + 1, 18 }, { 14, y0 + 5, 30 }, leg);// upper strut
        t.fillBox({ 8, y0, 30 }, { 18, y0 + 6, 34 }, leg);    // hip
    }
    t.fillBox({ 8, 6, 32 }, { 22, 18, 44 }, torso); // compact pod body
    t.carveBox({ 8, 6, 32 }, { 10, 8, 36 });
    t.carveBox({ 8, 16, 32 }, { 10, 18, 36 });
    t.fillBox({ 20, 8, 36 }, { 26, 16, 44 }, cockpit); // forward sensor cab
    // The eye.
    for (int z = 38; z < 42; ++z)
        for (int y = 10; y < 14; ++y) put(t, 25, y, z, cockpit, Orange);
    t.fillBox({ 12, 9, 44 }, { 18, 15, 48 }, sensor); // antenna nub
    t.fillBox({ 14, 10, 48 }, { 16, 12, 54 }, sensor);
    // Rail arm (left) and claw stub (right) — mounts reach the torso sides.
    t.fillBox({ 6, 0, 36 }, { 12, 7, 42 }, rail);
    t.fillBox({ 12, 0, 38 }, { 36, 3, 41 }, rail);
    t.fillBox({ 6, 17, 36 }, { 14, 24, 42 }, armR);
    t.fillBox({ 14, 21, 37 }, { 20, 23, 40 }, armR);
    t.fillBox({ 2, 8, 34 }, { 8, 16, 42 }, jets);

    for (int z = 0; z < t.dims.z; ++z)
        for (int y = 0; y < t.dims.y; ++y)
            for (int x = 0; x < t.dims.x; ++x) {
                const int part = t.partAt({ x, y, z });
                if (part < 0 || t.paint[t.index({ x, y, z })] != 0) continue;
                uint8_t c = Tan;
                if (part == legL || part == legR) c = z < 3 ? Black : Taupe;
                else if (part == rail) c = x >= 12 ? DarkFacet : Taupe;
                else if (part == armR || part == jets) c = Taupe;
                else if (part == cockpit) c = DarkFacet;
                else if (part == sensor) c = Orange;
                else {
                    const int band = (x + z) / 6;
                    c = (band % 2 == 0) ? Tan : Taupe;
                }
                t.paint[t.index({ x, y, z })] = c;
            }
    t.finalize();
    return t;
}

// ------------------------------------------------------------------ Choir ---

VehicleTemplate makeChoirFighter() {
    // "Hymn": forward-swept organic fighter — curved fuselage, FSW wings,
    // canards, V-tail, glowing crystal canopy.
    VehicleTemplate t;
    t.name = "Hymn";
    t.id = TemplateId::ChoirFighter;
    t.locomotion = LocomotionClass::Jet;
    t.voxelSize = 0.0625f;
    t.dims = { 88, 72, 24 };
    initGrid(t);
    enum : uint8_t { Teal = 1, Violet, Ivory, Crystal, Glow };
    setPalette(t, { { 44, 132 ,140 }, { 116, 72, 160 }, { 214, 212, 200 },
                    { 150, 230, 230 }, { 90, 220, 210 } });

    const int hull = t.addPart("hull", PartType::Hull, 160, 0.9f);
    const int wingL = t.addPart("wing.left", PartType::Wing, 55);
    const int wingR = t.addPart("wing.right", PartType::Wing, 55);
    const int engine = t.addPart("engine", PartType::Engine, 85);
    const int lance = t.addPart("weapon.lance", PartType::Weapon, 40);
    const int sensor = t.addPart("sensor", PartType::Sensor, 25);

    for (int z = 0; z < t.dims.z; ++z)
        for (int y = 0; y < t.dims.y; ++y)
            for (int x = 0; x < t.dims.x; ++x) {
                const float fx = x + 0.5f, fz = z + 0.5f;
                const float yc = y + 0.5f - 36.0f, ay = std::abs(yc);
                int part = -1;
                uint8_t color = 0;

                // Sinuous fuselage: round cross-section, gentle S in height.
                const float hw = profileAt({ { 0, 3.4f }, { 22, 5.4f }, { 46, 5.8f },
                                             { 66, 4.2f }, { 82, 2.2f }, { 88, 0.8f } }, fx);
                const float zc = profileAt({ { 0, 11.0f }, { 30, 10.0f }, { 60, 10.6f },
                                             { 88, 12.0f } }, fx);
                const float hh = profileAt({ { 0, 3.2f }, { 30, 4.0f }, { 70, 3.2f },
                                             { 88, 1.6f } }, fx);
                const float sy = ay / hw, sz = (fz - zc) / hh;
                if (sy * sy + sz * sz <= 1.0f) {
                    part = fx >= 78.0f ? lance : (fx < 22.0f ? engine : hull);
                    color = fz <= zc - 1.0f ? Ivory : Teal;
                    if (fx >= 78.0f) color = Crystal; // energy lance tip
                    if (fx < 6.0f) color = Glow;      // engine bloom
                }

                // Crystal canopy blister.
                const float ex = (fx - 64.0f) / 7.5f, ey = yc / 2.4f, ez = (fz - 12.5f) / 3.4f;
                if (ex * ex + ey * ey + ez * ez <= 1.0f && fz >= 12.5f) {
                    part = sensor;
                    color = Crystal;
                }

                // Forward-swept wings: root aft, tips forward.
                if (part < 0 && ay >= 5.0f && ay <= 33.0f) {
                    const float k = (ay - 5.0f) / 28.0f;
                    const float xle = 34.0f + 24.0f * k; // sweeps FORWARD
                    const float xte = 18.0f + 10.0f * k;
                    const float halfTh = 1.6f - 0.7f * k;
                    if (fx >= xte && fx <= xle && std::abs(fz - (10.0f + k * 1.2f)) <= halfTh) {
                        part = yc < 0 ? wingL : wingR;
                        color = fz <= 9.4f ? Ivory : Teal;
                        if (fx >= xle - 1.6f) color = Violet; // LE veins
                        if (ay >= 30.0f) color = Glow;        // wingtip glow
                    }
                }
                // Canards near the nose.
                if (part < 0 && ay >= 4.0f && ay <= 13.0f && std::abs(fz - 12.0f) <= 0.9f) {
                    const float k = (ay - 4.0f) / 9.0f;
                    if (fx >= 66.0f - 4.0f * k && fx <= 76.0f - 7.0f * k) {
                        part = hull;
                        color = Violet;
                    }
                }
                // V-tail.
                if (part < 0 && fx >= 2.0f && fx <= 16.0f) {
                    const float spread = 3.0f + (fz - 11.0f) * 0.85f;
                    if (fz >= 11.0f && fz <= 22.0f && std::abs(ay - spread) <= 1.0f
                        && fx <= 16.0f - (fz - 11.0f) * 0.6f) {
                        part = hull;
                        color = fz >= 19.0f ? Glow : Teal;
                    }
                }

                if (part >= 0) put(t, x, y, z, part, color);
            }
    t.finalize();
    return t;
}

VehicleTemplate makeChoirTank() {
    // "Tide": rounded grav-lozenge with a glowing skirt and a dorsal fin
    // turret — no tracks to shoot off (the skirt IS the track part).
    VehicleTemplate t;
    t.name = "Tide";
    t.id = TemplateId::ChoirTank;
    t.locomotion = LocomotionClass::Tracked;
    t.voxelSize = 0.125f;
    t.dims = { 40, 28, 18 };
    initGrid(t);
    enum : uint8_t { Teal = 1, Violet, Ivory, Glow, Dark };
    setPalette(t, { { 44, 132, 140 }, { 116, 72, 160 }, { 214, 212, 200 },
                    { 90, 220, 210 }, { 36, 48, 52 } });

    const int hull = t.addPart("hull", PartType::Hull, 220, 0.8f);
    const int skirtL = t.addPart("track.left", PartType::Track, 70);
    const int skirtR = t.addPart("track.right", PartType::Track, 70);
    const int engine = t.addPart("engine", PartType::Engine, 90);
    const int fin = t.addPart("weapon.fin", PartType::Weapon, 60);

    for (int z = 0; z < t.dims.z; ++z)
        for (int y = 0; y < t.dims.y; ++y)
            for (int x = 0; x < t.dims.x; ++x) {
                const float fx = x + 0.5f - 20.0f;
                const float fy = y + 0.5f - 14.0f;
                const float fz = z + 0.5f;
                // Lozenge: ellipsoid flattened, hovering (z 2..13).
                const float ex = fx / 19.0f, ey = fy / 13.0f, ez = (fz - 7.5f) / 5.5f;
                const float d = ex * ex + ey * ey + ez * ez * ez * ez;
                if (d <= 1.0f && fz >= 2.0f) {
                    int part = hull;
                    uint8_t color = fz <= 5.0f ? Ivory : Teal;
                    if (fz >= 2.0f && fz < 4.0f) {
                        part = fy < 0 ? skirtL : skirtR; // grav skirt halves
                        color = Glow;
                    }
                    if (fx < -12.0f) {
                        part = engine;
                        color = fz >= 6.0f ? Violet : Ivory;
                    }
                    if (d >= 0.86f && fz >= 6.0f) color = Violet; // rim band
                    put(t, x, y, z, part, color);
                }
            }
    // Dorsal fin cannon.
    for (int x = 14; x < 34; ++x)
        for (int z = 12; z < 18; ++z) {
            const float k = (x - 14) / 20.0f;
            if (z <= 12 + (1.0f - std::abs(k - 0.4f) * 2.0f) * 6.0f)
                for (int y = 13; y < 15; ++y)
                    put(t, x, y, z, fin, z >= 15 ? Glow : Teal);
        }
    t.finalize();
    return t;
}

VehicleTemplate makeChoirMech() {
    // "Psalter": digitigrade smooth legs, egg torso, floating halo array.
    VehicleTemplate t;
    t.name = "Psalter";
    t.id = TemplateId::ChoirMech;
    t.locomotion = LocomotionClass::Walker;
    t.voxelSize = 0.125f;
    t.dims = { 32, 26, 60 };
    initGrid(t);
    enum : uint8_t { Teal = 1, Violet, Ivory, Glow, Dark };
    setPalette(t, { { 44, 132, 140 }, { 116, 72, 160 }, { 214, 212, 200 },
                    { 90, 220, 210 }, { 36, 48, 52 } });

    const int torso = t.addPart("torso", PartType::Hull, 190, 0.85f);
    const int cockpit = t.addPart("cockpit", PartType::Cockpit, 45);
    const int legL = t.addPart("leg.left", PartType::Leg, 85);
    const int legR = t.addPart("leg.right", PartType::Leg, 85);
    const int armL = t.addPart("arm.left", PartType::Weapon, 55);
    const int armR = t.addPart("arm.right", PartType::Weapon, 55);
    const int jets = t.addPart("jumpjets", PartType::JumpJets, 40);
    const int halo = t.addPart("sensor", PartType::Sensor, 30);

    for (int side = 0; side < 2; ++side) {
        const int leg = side == 0 ? legL : legR;
        const int y0 = side == 0 ? 2 : 16;
        t.fillBox({ 10, y0, 0 }, { 24, y0 + 8, 3 }, leg);      // hoof
        t.fillBox({ 14, y0 + 1, 3 }, { 20, y0 + 7, 12 }, leg); // lower
        t.fillBox({ 8, y0 + 1, 12 }, { 18, y0 + 7, 16 }, leg); // back knee
        t.fillBox({ 8, y0 + 1, 16 }, { 14, y0 + 7, 28 }, leg); // upper
    }
    // Egg torso via ellipsoid.
    for (int z = 26; z < 48; ++z)
        for (int y = 4; y < 22; ++y)
            for (int x = 4; x < 26; ++x) {
                const float ex = (x + 0.5f - 14.0f) / 9.5f;
                const float ey = (y + 0.5f - 13.0f) / 8.5f;
                const float ez = (z + 0.5f - 37.0f) / 11.0f;
                if (ex * ex + ey * ey + ez * ez <= 1.0f) {
                    const bool front = x >= 19 && z >= 36 && z < 44 && y >= 9 && y < 17;
                    put(t, x, y, z,
                        front ? cockpit : torso,
                        front ? Glow : ((z + x) % 9 < 5 ? Teal : Ivory));
                }
            }
    // Slender arm pods, mounted into the egg's flanks.
    for (int side = 0; side < 2; ++side) {
        const int arm = side == 0 ? armL : armR;
        const int yIn = side == 0 ? 2 : 19;
        t.fillBox({ 10, yIn, 34 }, { 16, yIn + 5, 44 }, arm);
        t.fillBox({ 16, yIn + 1, 36 }, { 30, yIn + 4, 40 }, arm);
    }
    t.fillBox({ 0, 9, 32 }, { 5, 17, 42 }, jets);
    // Halo: ring above the torso.
    for (int x = 6; x < 24; ++x)
        for (int y = 6; y < 20; ++y) {
            const float dx = x + 0.5f - 14.5f, dy = y + 0.5f - 13.0f;
            const float r = std::sqrt(dx * dx + dy * dy);
            if (r >= 5.0f && r <= 7.0f)
                for (int z = 50; z < 53; ++z) put(t, x, y, z, halo, Glow);
        }
    t.fillBox({ 12, 11, 47 }, { 17, 15, 50 }, halo); // stem

    for (int z = 0; z < t.dims.z; ++z)
        for (int y = 0; y < t.dims.y; ++y)
            for (int x = 0; x < t.dims.x; ++x) {
                const int part = t.partAt({ x, y, z });
                if (part < 0 || t.paint[t.index({ x, y, z })] != 0) continue;
                uint8_t c = Ivory;
                if (part == legL || part == legR) c = z < 3 ? Violet : Ivory;
                else if (part == armL || part == armR) c = x >= 16 ? Violet : Teal;
                else if (part == jets) c = Teal;
                t.paint[t.index({ x, y, z })] = c;
            }
    t.finalize();
    return t;
}

// ----------------------------------------------------------------- pilots ---

VehicleTemplate makeFactionPilot(TemplateId id, const char* name,
                                 std::initializer_list<std::array<uint8_t, 3>> palette,
                                 int helmetStyle) {
    VehicleTemplate t;
    t.name = name;
    t.id = id;
    t.locomotion = LocomotionClass::Pilot;
    t.voxelSize = 0.125f;
    t.dims = { 8, 12, 16 };
    initGrid(t);
    setPalette(t, palette); // 1 = suit, 2 = trim, 3 = visor
    const int body = t.addPart("body", PartType::Hull, 40);

    t.fillBox({ 2, 3, 0 }, { 6, 5, 6 }, body);
    t.fillBox({ 2, 7, 0 }, { 6, 9, 6 }, body);
    t.fillBox({ 1, 3, 0 }, { 7, 5, 1 }, body);
    t.fillBox({ 1, 7, 0 }, { 7, 9, 1 }, body);
    t.fillBox({ 2, 3, 6 }, { 6, 9, 12 }, body);
    t.fillBox({ 2, 1, 6 }, { 6, 3, 11 }, body);
    t.fillBox({ 2, 9, 6 }, { 6, 11, 11 }, body);
    t.fillBox({ 3, 4, 12 }, { 6, 8, 15 }, body);
    if (helmetStyle == 1) t.fillBox({ 2, 3, 14 }, { 7, 9, 16 }, body);  // wide brim
    if (helmetStyle == 2) t.fillBox({ 3, 4, 15 }, { 5, 8, 16 }, body);  // crest
    if (helmetStyle == 3) t.fillBox({ 2, 4, 12 }, { 3, 8, 16 }, body);  // hood

    for (int z = 0; z < t.dims.z; ++z)
        for (int y = 0; y < t.dims.y; ++y)
            for (int x = 0; x < t.dims.x; ++x) {
                if (t.partAt({ x, y, z }) < 0) continue;
                uint8_t c = 1;                       // suit
                if (z < 1 || (z >= 6 && z < 8)) c = 2; // boots + belt trim
                if (z >= 12) c = (x >= 5 && z >= 13 && z < 15) ? 3 : 2; // visor in helmet
                t.paint[t.index({ x, y, z })] = c;
            }
    t.finalize();
    return t;
}

} // namespace

const VehicleTemplate& factionTemplate(Faction f, UnitClass c) {
    static const VehicleTemplate kesselFighter = makeKesselFighter();
    static const VehicleTemplate kesselTank = makeKesselTank();
    static const VehicleTemplate kesselMech = makeKesselMech();
    static const VehicleTemplate kesselPilot = makeFactionPilot(
        TemplateId::KesselPilot, "KesselPilot",
        { { 86, 98, 66 }, { 60, 62, 58 }, { 40, 44, 48 } }, 1);
    static const VehicleTemplate mirageFighter = makeMirageFighter();
    static const VehicleTemplate mirageTank = makeMirageTank();
    static const VehicleTemplate mirageMech = makeMirageMech();
    static const VehicleTemplate miragePilot = makeFactionPilot(
        TemplateId::MiragePilot, "MiragePilot",
        { { 176, 162, 134 }, { 110, 102, 88 }, { 222, 122, 42 } }, 0);
    static const VehicleTemplate choirFighter = makeChoirFighter();
    static const VehicleTemplate choirTank = makeChoirTank();
    static const VehicleTemplate choirMech = makeChoirMech();
    static const VehicleTemplate choirPilot = makeFactionPilot(
        TemplateId::ChoirPilot, "ChoirPilot",
        { { 214, 212, 200 }, { 44, 132, 140 }, { 90, 220, 210 } }, 3);

    switch (f) {
        case Faction::Vanguard:
            switch (c) {
                case UnitClass::Fighter: return VehicleTemplate::waspFighter();
                case UnitClass::Tank: return VehicleTemplate::brickTank();
                case UnitClass::Mech: return VehicleTemplate::talonMech();
                default: return VehicleTemplate::pilot();
            }
        case Faction::Kessler:
            switch (c) {
                case UnitClass::Fighter: return kesselFighter;
                case UnitClass::Tank: return kesselTank;
                case UnitClass::Mech: return kesselMech;
                default: return kesselPilot;
            }
        case Faction::Mirage:
            switch (c) {
                case UnitClass::Fighter: return mirageFighter;
                case UnitClass::Tank: return mirageTank;
                case UnitClass::Mech: return mirageMech;
                default: return miragePilot;
            }
        default:
            switch (c) {
                case UnitClass::Fighter: return choirFighter;
                case UnitClass::Tank: return choirTank;
                case UnitClass::Mech: return choirMech;
                default: return choirPilot;
            }
    }
}

} // namespace vox
