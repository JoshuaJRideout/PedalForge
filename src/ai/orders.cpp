#include "ai/orders.h"
#include <algorithm>
#include <cmath>

namespace vox {

namespace {

// Smallest signed angle from a to b, in radians.
float angleDelta(float a, float b) {
    float d = b - a;
    while (d > 3.14159265f) d -= 6.2831853f;
    while (d < -3.14159265f) d += 6.2831853f;
    return d;
}

float yawTo(const Vec3& from, const Vec3& to) {
    return std::atan2(to.z - from.z, to.x - from.x);
}

float horizontalDistance(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

// Steer toward a point: full throttle when roughly facing it, gentle while
// turning. Good enough for flat-ish ground; pathfinding replaces the straight
// line in M1 (flow fields, §11.1).
ControlInput steerTo(const VehicleEntity& e, const Vec3& target) {
    ControlInput in;
    const float want = yawTo(e.body.position, target);
    const float d = angleDelta(e.body.yaw, want);
    in.steer = std::clamp(d * 2.0f, -1.0f, 1.0f);
    in.throttle = std::abs(d) < 0.8f ? 1.0f : 0.3f;
    return in;
}

} // namespace

const Order* AiController::orderOf(uint32_t entityId) const {
    const auto it = orders.find(entityId);
    return it == orders.end() ? nullptr : &it->second;
}

void AiController::tick(Sim& sim) {
    for (auto& [id, order] : orders) {
        VehicleEntity* e = sim.find(id);
        if (!e || !e->state.operable() || !e->hasPilot) continue;
        drive(sim, *e, order);
    }
    for (auto& [id, ticks] : cooldown)
        if (ticks > 0) --ticks;
}

void AiController::drive(Sim& sim, VehicleEntity& e, Order& order) {
    switch (order.type) {
        case OrderType::Hold:
            sim.setInput(e.id, {});
            break;

        case OrderType::MoveTo: {
            const float dist = horizontalDistance(e.body.position, order.target);
            if (dist <= kArriveRadius) {
                // Sticky arrival: done means done — no brake-overshoot
                // oscillation around the waypoint.
                order.type = OrderType::Hold;
                sim.setInput(e.id, {});
                break;
            }
            ControlInput in = steerTo(e, order.target);
            // Ease off approaching the waypoint so braking distance fits.
            in.throttle *= std::clamp(dist / 15.0f, 0.2f, 1.0f);
            sim.setInput(e.id, in);
            break;
        }

        case OrderType::Attack: {
            const VehicleEntity* target = sim.find(order.targetEntity);
            if (!target || target->state.destroyed()) {
                sim.setInput(e.id, {}); // target down: hold (commander reassigns)
                break;
            }
            const float dist = horizontalDistance(e.body.position, target->body.position);
            // Close to weapon range, then stop and shoot.
            if (dist > kWeaponRange * 0.8f) {
                sim.setInput(e.id, steerTo(e, target->body.position));
            } else {
                sim.setInput(e.id, {});
            }
            if (dist <= kWeaponRange && cooldown[e.id] <= 0) {
                const float muzzleUp =
                    static_cast<float>(e.tmpl->dims.z) * Sim::kSubvoxelSize * 0.6f;
                const float aimUp =
                    static_cast<float>(target->tmpl->dims.z) * Sim::kSubvoxelSize * 0.4f;
                const Vec3 dir{ target->body.position.x - e.body.position.x,
                                (target->body.position.y + aimUp)
                                    - (e.body.position.y + muzzleUp),
                                target->body.position.z - e.body.position.z };
                sim.fire(e.id, dir);
                cooldown[e.id] = kFireCooldownTicks;
            }
            break;
        }

        case OrderType::Escort: {
            const VehicleEntity* ward = sim.find(order.targetEntity);
            if (!ward || ward->state.destroyed()) {
                sim.setInput(e.id, {});
                break;
            }
            if (horizontalDistance(e.body.position, ward->body.position) > kEscortDistance)
                sim.setInput(e.id, steerTo(e, ward->body.position));
            else
                sim.setInput(e.id, {});
            break;
        }
    }
}

} // namespace vox
