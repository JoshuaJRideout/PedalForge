#include "test_framework.h"
#include "vehicle/locomotion.h"

using namespace vox;

namespace {

// Flat slab of rock at height 10, with an optional cliff wall.
VoxelWorld flatWorld(int wallX = -1, int wallHeight = 0) {
    VoxelWorld w({ 64, 32, 64 });
    for (int x = 0; x < 64; ++x)
        for (int z = 0; z < 64; ++z) {
            for (int y = 0; y < 10; ++y) w.set({ x, y, z }, Material::Rock);
            if (wallX >= 0 && x >= wallX)
                for (int y = 10; y < 10 + wallHeight; ++y) w.set({ x, y, z }, Material::Rock);
        }
    return w;
}

Int3 subvoxelOf(const VehicleTemplate& t, const char* partName) {
    int target = -1;
    for (size_t i = 0; i < t.parts.size(); ++i)
        if (t.parts[i].name == partName) target = static_cast<int>(i);
    for (int z = 0; z < t.dims.z; ++z)
        for (int y = 0; y < t.dims.y; ++y)
            for (int x = 0; x < t.dims.x; ++x)
                if (t.partAt({ x, y, z }) == target) return { x, y, z };
    return { -1, -1, -1 };
}

void destroyPart(Vehicle& v, const char* partName, int damage) {
    Rng rng(99);
    v.applyHit(subvoxelOf(v.tmpl(), partName), damage, DamageType::Kinetic, rng);
}

BodyState onGround(float x, float z) {
    BodyState b;
    b.position = { x, 10.0f, z };
    b.grounded = true;
    return b;
}

} // namespace

TEST(tank_accelerates_and_is_blocked_by_cliffs) {
    VoxelWorld w = flatWorld(40, 3); // 3 m wall at x=40
    Vehicle tank(VehicleTemplate::brickTank());
    BodyState body = onGround(8.0f, 32.0f);
    ControlInput in;
    in.throttle = 1.0f; // yaw 0 = +x, straight at the wall

    bool everBlocked = false;
    float topSpeed = 0.0f;
    for (int t = 0; t < 600; ++t) { // 10 s
        const StepResult r = stepTracked(body, in, tank, w);
        everBlocked |= r.blocked;
        topSpeed = std::max(topSpeed, body.speed);
    }
    CHECK(topSpeed > 11.0f);          // reached near max speed (12)
    CHECK(everBlocked);               // the wall stopped it
    CHECK(body.position.x < 40.0f);   // never climbed the 3 m cliff
    CHECK(body.position.y == 10.0f);  // stayed on the ground plane
}

TEST(tank_mobility_degrades_with_damage) {
    VoxelWorld w = flatWorld();
    ControlInput in;
    in.throttle = 1.0f;

    // One track gone: limps at 40%.
    Vehicle limper(VehicleTemplate::brickTank());
    destroyPart(limper, "track.left", 500);
    CHECK(!limper.destroyed());
    BodyState body = onGround(8.0f, 32.0f);
    for (int t = 0; t < 300; ++t) stepTracked(body, in, limper, w);
    CHECK(body.speed > 3.0f);
    CHECK(body.speed < 6.0f);

    // Engine gone: dead in the water.
    Vehicle stalled(VehicleTemplate::brickTank());
    destroyPart(stalled, "engine", 100);
    BodyState body2 = onGround(8.0f, 32.0f);
    for (int t = 0; t < 300; ++t) stepTracked(body2, in, stalled, w);
    CHECK(body2.speed == 0.0f);
    CHECK(body2.position.x == 8.0f);
}

TEST(jet_holds_altitude_then_glides_when_engine_dies) {
    VoxelWorld w = flatWorld();
    Vehicle jet(VehicleTemplate::waspFighter());
    BodyState body;
    body.position = { 8.0f, 25.0f, 32.0f };
    body.speed = 40.0f;
    ControlInput in;
    in.throttle = 1.0f;

    for (int t = 0; t < 180; ++t) { // 3 s powered, level
        const StepResult r = stepJet(body, in, jet, w);
        CHECK(!r.collided);
    }
    CHECK(std::abs(body.position.y - 25.0f) < 0.01f); // level flight holds altitude
    CHECK(body.speed > 70.0f);                         // throttled up toward 80

    destroyPart(jet, "engine", 90);
    CHECK(!jet.destroyed());
    const float altBefore = body.position.y;
    for (int t = 0; t < 300; ++t) stepJet(body, in, jet, w); // 5 s gliding
    CHECK(body.position.y < altBefore); // trading altitude for airspeed
    CHECK(body.speed > 0.0f);           // still flying, not falling
}

TEST(jet_with_no_wings_falls_ballistically) {
    VoxelWorld w = flatWorld();
    Vehicle jet(VehicleTemplate::waspFighter());
    destroyPart(jet, "wing.left", 60);
    destroyPart(jet, "wing.right", 60);
    CHECK(!jet.destroyed());

    BodyState body;
    body.position = { 8.0f, 30.0f, 32.0f };
    body.velocity = { 10.0f, 0.0f, 0.0f }; // slow enough to land inside the test world
    ControlInput in;
    in.throttle = 1.0f;
    in.steer = 1.0f; // controls do nothing now

    const float yawBefore = body.yaw;
    bool crashed = false;
    for (int t = 0; t < 600 && !crashed; ++t) crashed = stepJet(body, in, jet, w).collided;
    CHECK(crashed);                 // hit the ground
    CHECK(body.yaw == yawBefore);   // no control authority on the way down
}

TEST(mech_jumps_and_lands) {
    VoxelWorld w = flatWorld();
    Vehicle mech(VehicleTemplate::talonMech());
    BodyState body = onGround(20.0f, 32.0f);
    ControlInput in;
    in.throttle = 0.5f;
    in.jump = true;

    stepWalker(body, in, mech, w);
    CHECK(!body.grounded); // launched
    in.jump = false;
    float peak = 0.0f;
    for (int t = 0; t < 180 && !body.grounded; ++t) {
        stepWalker(body, in, mech, w);
        peak = std::max(peak, body.position.y);
    }
    CHECK(body.grounded);             // came back down
    CHECK(peak > 11.5f);              // real jump arc (~2.5 m at 10 m/s, g=20)
    CHECK(body.position.y == 10.0f);  // landed on the slab

    // Jump jets destroyed: no more jumping.
    destroyPart(mech, "jumpjets", 40);
    in.jump = true;
    stepWalker(body, in, mech, w);
    CHECK(body.grounded);
}

TEST(mech_leg_damage_and_husk_rules) {
    VoxelWorld w = flatWorld();
    ControlInput in;
    in.throttle = 1.0f;

    // Both legs gone: immobile but alive.
    Vehicle legless(VehicleTemplate::talonMech());
    destroyPart(legless, "leg.left", 500);
    destroyPart(legless, "leg.right", 500);
    CHECK(!legless.destroyed());
    BodyState body = onGround(20.0f, 32.0f);
    for (int t = 0; t < 120; ++t) stepWalker(body, in, legless, w);
    CHECK(body.position.x == 20.0f);

    // Cockpit gone: husk — intact machine that ignores input (stealable, §4.7).
    Vehicle husk(VehicleTemplate::talonMech());
    destroyPart(husk, "cockpit", 50);
    CHECK(husk.isHusk());
    CHECK(!husk.operable());
    CHECK(!husk.destroyed());
    BodyState body2 = onGround(20.0f, 32.0f);
    for (int t = 0; t < 120; ++t) stepWalker(body2, in, husk, w);
    CHECK(body2.position.x == 20.0f);
}

TEST(pilot_walks_and_hops) {
    VoxelWorld w = flatWorld();
    Vehicle pilot(VehicleTemplate::pilot());
    BodyState body = onGround(8.0f, 32.0f);
    ControlInput in;
    in.throttle = 1.0f;

    for (int t = 0; t < 300; ++t) stepPilot(body, in, pilot, w);
    CHECK(body.speed > 4.5f);  // on-foot max ~5 m/s
    CHECK(body.speed < 5.5f);  // far slower than any vehicle

    in.jump = true;
    stepPilot(body, in, pilot, w);
    CHECK(!body.grounded);     // jetpack hop
    in.jump = false;
    for (int t = 0; t < 120 && !body.grounded; ++t) stepPilot(body, in, pilot, w);
    CHECK(body.grounded);
}
