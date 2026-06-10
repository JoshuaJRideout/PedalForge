#pragma once
#include <cstdint>
#include <map>
#include <vector>
#include "sim/sim.h"

namespace vox {

// Order-driven unit AI (DESIGN.md §2.3): the strategy layer issues orders;
// this controller turns them into the same ControlInput/fire calls a human
// possession uses. AI and players are interchangeable per entity by design —
// possessing a unit simply means the server stops asking the AI for input.
// Runs server-side only; clients just see the resulting snapshots.

enum class OrderType : uint8_t { Hold, MoveTo, Attack, Escort };

struct Order {
    OrderType type = OrderType::Hold;
    Vec3 target;               // MoveTo
    uint32_t targetEntity = 0; // Attack / Escort
};

struct Squad {
    std::vector<uint32_t> members;
    Order order;
};

class AiController {
public:
    static constexpr float kArriveRadius = 3.0f;
    static constexpr float kWeaponRange = 80.0f;
    static constexpr int kFireCooldownTicks = 30;
    static constexpr float kEscortDistance = 8.0f;

    void setOrder(uint32_t entityId, const Order& order) { orders[entityId] = order; }
    void clearOrder(uint32_t entityId) { orders.erase(entityId); }
    const Order* orderOf(uint32_t entityId) const;

    // Convenience: same order to every member (spread arrival offsets later).
    void setSquadOrder(const Squad& squad) {
        for (uint32_t id : squad.members) setOrder(id, squad.order);
    }

    // Drive all ordered entities for this tick. Call before sim.step().
    void tick(Sim& sim);

private:
    void drive(Sim& sim, VehicleEntity& e, Order& order); // may complete the order

    std::map<uint32_t, Order> orders; // ordered map: deterministic iteration
    std::map<uint32_t, int> cooldown;
};

} // namespace vox
