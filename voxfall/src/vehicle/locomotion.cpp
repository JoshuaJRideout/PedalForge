#include "vehicle/locomotion.h"
#include <algorithm>
#include <cmath>

namespace vox {

namespace {

float groundHeight(const VoxelWorld& w, float x, float z) {
    return static_cast<float>(w.heightAt(static_cast<int>(std::floor(x)),
                                         static_cast<int>(std::floor(z))));
}

bool solidAt(const VoxelWorld& w, const Vec3& p) {
    return materialInfo(w.at({ static_cast<int>(std::floor(p.x)),
                               static_cast<int>(std::floor(p.y)),
                               static_cast<int>(std::floor(p.z)) }))
        .solid;
}

int waterDepth(const VoxelWorld& w, float x, float z) {
    const int gx = static_cast<int>(std::floor(x));
    const int gz = static_cast<int>(std::floor(z));
    int depth = 0;
    for (int y = w.heightAt(gx, gz); y < w.seaLevel(); ++y) ++depth;
    return depth;
}

// Mobility factor from paired locomotion parts: full pair = 1, one lost = limp,
// all lost = immobile.
float pairMobility(const Vehicle& v, PartType type, float limpFactor) {
    int total = 0;
    for (const PartDef& d : v.tmpl().parts)
        if (d.type == type) ++total;
    const int alive = v.alivePartCountOfType(type);
    if (total == 0 || alive == 0) return 0.0f;
    return alive == total ? 1.0f : limpFactor;
}

float approach(float value, float target, float rate) {
    if (value < target) return std::min(target, value + rate);
    return std::max(target, value - rate);
}

// Shared ground move for tracked/walker/pilot: steer, accelerate, slope-check,
// snap to terrain.
StepResult groundMove(BodyState& body, const ControlInput& in, const VoxelWorld& world,
                      float mobility, float maxSpeed, float accel, float turnRate,
                      float maxClimbPerCell, int fordDepth) {
    StepResult r;
    body.yaw += in.steer * turnRate * mobility * kTickDt;
    body.speed = approach(body.speed, in.throttle * maxSpeed * mobility, accel * kTickDt);

    const float nx = body.position.x + std::cos(body.yaw) * body.speed * kTickDt;
    const float nz = body.position.z + std::sin(body.yaw) * body.speed * kTickDt;

    // Cell-based slope check: climbing into the next column is allowed only if
    // its surface is within the per-cell climb budget.
    const float targetGround = groundHeight(world, nx, nz);
    if (targetGround - body.position.y > maxClimbPerCell) {
        body.speed = 0.0f;
        r.blocked = true;
        return r;
    }
    if (waterDepth(world, nx, nz) > fordDepth) {
        body.speed = 0.0f;
        r.blocked = true;
        return r;
    }

    body.position.x = nx;
    body.position.z = nz;
    body.position.y = targetGround;
    body.grounded = true;
    return r;
}

// Airborne phase shared by walker jumps and pilot hops: ballistic with partial
// horizontal authority, landing snap.
void airborneMove(BodyState& body, const ControlInput& in, const VoxelWorld& world,
                  float mobility, float maxSpeed, float accel, float airControl) {
    const float targetVx = std::cos(body.yaw) * in.throttle * maxSpeed * mobility;
    const float targetVz = std::sin(body.yaw) * in.throttle * maxSpeed * mobility;
    body.velocity.x = approach(body.velocity.x, targetVx, accel * airControl * kTickDt);
    body.velocity.z = approach(body.velocity.z, targetVz, accel * airControl * kTickDt);
    body.velocity.y -= kGravity * kTickDt;

    body.position.x += body.velocity.x * kTickDt;
    body.position.z += body.velocity.z * kTickDt;
    body.position.y += body.velocity.y * kTickDt;

    const float ground = groundHeight(world, body.position.x, body.position.z);
    if (body.position.y <= ground) {
        body.position.y = ground;
        body.velocity = {};
        body.speed = 0.0f;
        body.grounded = true;
    }
}

StepResult legged(BodyState& body, const ControlInput& in, const Vehicle& v,
                  const VoxelWorld& world, float maxSpeed, float accel, float turnRate,
                  float maxClimbPerCell, float jumpSpeed, float airControl, bool needsJets) {
    StepResult r;
    float mobility = v.operable() ? pairMobility(v, PartType::Leg, 0.35f) : 0.0f;
    // Pilots and other leg-less templates walk on their core.
    bool hasLegs = false;
    for (const PartDef& d : v.tmpl().parts) hasLegs |= (d.type == PartType::Leg);
    if (!hasLegs) mobility = v.operable() ? 1.0f : 0.0f;

    if (body.grounded) {
        const ControlInput effective = v.operable() ? in : ControlInput{};
        r = groundMove(body, effective, world, mobility, maxSpeed, accel, turnRate,
                       maxClimbPerCell, /*fordDepth=*/1);
        const bool jetsOk = !needsJets || v.hasAlivePartOfType(PartType::JumpJets);
        if (v.operable() && in.jump && jetsOk && mobility > 0.0f) {
            body.grounded = false;
            body.velocity = { std::cos(body.yaw) * body.speed, jumpSpeed,
                              std::sin(body.yaw) * body.speed };
        }
    } else {
        airborneMove(body, v.operable() ? in : ControlInput{}, world, mobility, maxSpeed,
                     accel, airControl);
    }
    return r;
}

} // namespace

StepResult stepTracked(BodyState& body, const ControlInput& in, const Vehicle& v,
                       const VoxelWorld& world, const TrackedParams& p) {
    const bool powered = v.operable() && v.hasAlivePartOfType(PartType::Engine);
    const float mobility = powered ? pairMobility(v, PartType::Track, 0.4f) : 0.0f;
    const ControlInput effective = powered ? in : ControlInput{};
    return groundMove(body, effective, world, mobility, p.maxSpeed, p.accel, p.turnRate,
                      p.maxClimbPerCell, p.fordDepth);
}

StepResult stepJet(BodyState& body, const ControlInput& in, const Vehicle& v,
                   const VoxelWorld& world, const JetParams& p) {
    StepResult r;
    const bool powered = v.operable() && v.hasAlivePartOfType(PartType::Engine);
    const int wings = v.alivePartCountOfType(PartType::Wing);
    const float control = !v.operable() ? 0.0f : (wings >= 2 ? 1.0f : wings == 1 ? 0.45f : 0.0f);

    if (control <= 0.0f) {
        // Both wings gone (or husk/dead): ballistic. Wing loss is dramatic (§4.5).
        body.velocity.y -= kGravity * kTickDt;
        body.position.x += body.velocity.x * kTickDt;
        body.position.y += body.velocity.y * kTickDt;
        body.position.z += body.velocity.z * kTickDt;
        r.collided = solidAt(world, body.position);
        return r;
    }

    body.yaw += in.steer * p.turnRate * control * kTickDt;
    body.pitchAngle = std::clamp(
        body.pitchAngle + in.pitch * p.pitchRate * control * kTickDt, -p.maxPitch, p.maxPitch);

    if (powered) {
        const float target = std::clamp(in.throttle, 0.0f, 1.0f) * p.maxSpeed;
        body.speed = approach(body.speed, target, p.accel * kTickDt);
    } else {
        // Unpowered: the nose settles toward a glide angle unless the pilot
        // fights it, and diving trades altitude for airspeed against drag.
        if (in.pitch == 0.0f)
            body.pitchAngle = approach(body.pitchAngle, -0.25f, 0.4f * kTickDt);
        body.speed += (-std::sin(body.pitchAngle) * kGravity * 0.8f - p.glideDrag * body.speed)
                    * kTickDt;
        body.speed = std::max(0.0f, body.speed);
    }

    // Stall: below minimum airspeed the nose drops whether you like it or not.
    if (body.speed < p.minSpeed)
        body.pitchAngle = std::max(body.pitchAngle - 1.5f * p.pitchRate * kTickDt, -p.maxPitch);

    const float cp = std::cos(body.pitchAngle);
    body.velocity = { cp * std::cos(body.yaw) * body.speed, std::sin(body.pitchAngle) * body.speed,
                      cp * std::sin(body.yaw) * body.speed };
    body.position.x += body.velocity.x * kTickDt;
    body.position.y += body.velocity.y * kTickDt;
    body.position.z += body.velocity.z * kTickDt;

    r.collided = solidAt(world, body.position);
    return r;
}

StepResult stepWalker(BodyState& body, const ControlInput& in, const Vehicle& v,
                      const VoxelWorld& world, const WalkerParams& p) {
    return legged(body, in, v, world, p.maxSpeed, p.accel, p.turnRate, p.maxClimbPerCell,
                  p.jumpSpeed, p.airControl, /*needsJets=*/true);
}

StepResult stepPilot(BodyState& body, const ControlInput& in, const Vehicle& v,
                     const VoxelWorld& world, const PilotParams& p) {
    return legged(body, in, v, world, p.maxSpeed, p.accel, p.turnRate, p.maxClimbPerCell,
                  p.jumpSpeed, p.airControl, /*needsJets=*/false);
}

} // namespace vox
