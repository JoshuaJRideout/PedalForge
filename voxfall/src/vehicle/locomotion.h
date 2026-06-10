#pragma once
#include "core/types.h"
#include "vehicle/vehicle.h"
#include "world/world.h"

namespace vox {

// Kinematic locomotion controllers (DESIGN.md §4.5, §5.5): cheap, deterministic
// per-class movement models stepped at a fixed tick. Controllers read the
// vehicle's part status every step, so battle damage changes handling live:
// lost track = limp, dead engine = glider, dead legs = immobile, husk = inert.

constexpr float kTickDt = 1.0f / 60.0f;
constexpr float kGravity = 20.0f; // m/s^2 — gamey on purpose

struct ControlInput {
    float throttle = 0.0f; // -1..1: drive/thrust
    float steer = 0.0f;    // -1..1: yaw rate
    float pitch = 0.0f;    // -1..1: climb/dive (air only)
    bool jump = false;     // mech jump jets / pilot hop
};

struct BodyState {
    Vec3 position;          // world meters; y = up
    Vec3 velocity;          // used while airborne / for jets
    float yaw = 0.0f;       // radians, 0 = +x
    float pitchAngle = 0.0f;// radians (air vehicles)
    float speed = 0.0f;     // scalar ground speed / airspeed
    bool grounded = false;
};

struct StepResult {
    bool blocked = false;  // ground move denied this tick (slope/water/edge)
    bool collided = false; // air vehicle entered solid terrain (crash)
};

struct TrackedParams {
    float maxSpeed = 12.0f;
    float accel = 8.0f;
    float turnRate = 1.6f;       // rad/s
    float maxClimbPerCell = 1.2f;// ~tan(50 deg) per 1 m horizontal (§4.5)
    int fordDepth = 2;           // max water depth in voxels
};

struct JetParams {
    float maxSpeed = 80.0f;
    float minSpeed = 25.0f;  // stall threshold
    float accel = 15.0f;
    float turnRate = 1.2f;   // rad/s at full control
    float pitchRate = 0.9f;  // rad/s
    float maxPitch = 0.8f;   // rad
    float glideDrag = 0.15f; // 1/s, engine-out airspeed decay
};

struct WalkerParams {
    float maxSpeed = 8.0f;
    float accel = 10.0f;
    float turnRate = 2.2f;
    float maxClimbPerCell = 2.1f; // mechs step over low walls (§4.5)
    float jumpSpeed = 10.0f;      // jump-jet launch, vertical m/s
    float airControl = 0.3f;      // horizontal authority while airborne
};

struct PilotParams {
    float maxSpeed = 5.0f;
    float accel = 25.0f;
    float turnRate = 4.0f;
    float maxClimbPerCell = 1.1f;
    float jumpSpeed = 7.0f; // weak jetpack hop (§4.7)
    float airControl = 0.5f;
};

StepResult stepTracked(BodyState& body, const ControlInput& in, const Vehicle& v,
                       const VoxelWorld& world, const TrackedParams& p = {});
StepResult stepJet(BodyState& body, const ControlInput& in, const Vehicle& v,
                   const VoxelWorld& world, const JetParams& p = {});
StepResult stepWalker(BodyState& body, const ControlInput& in, const Vehicle& v,
                      const VoxelWorld& world, const WalkerParams& p = {});
StepResult stepPilot(BodyState& body, const ControlInput& in, const Vehicle& v,
                     const VoxelWorld& world, const PilotParams& p = {});

} // namespace vox
