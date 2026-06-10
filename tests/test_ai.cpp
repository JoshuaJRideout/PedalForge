#include "test_framework.h"
#include "ai/orders.h"

using namespace vox;

namespace {
VoxelWorld flatWorld() {
    VoxelWorld w({ 128, 48, 128 });
    for (int x = 0; x < 128; ++x)
        for (int z = 0; z < 128; ++z)
            for (int y = 0; y < 10; ++y) w.set({ x, y, z }, Material::Rock);
    return w;
}

float dist2d(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}
} // namespace

TEST(ai_move_order_reaches_target) {
    Sim sim(flatWorld(), 1);
    AiController ai;
    // Spawn facing away from the target: must turn around first.
    const uint32_t tank = sim.spawnVehicle(TemplateId::Brick, 0, { 30.0f, 10.0f, 30.0f }, 3.1f);
    Order move;
    move.type = OrderType::MoveTo;
    move.target = { 90.0f, 10.0f, 80.0f };
    ai.setOrder(tank, move);

    for (int t = 0; t < 60 * 30; ++t) { // up to 30 s
        ai.tick(sim);
        sim.step();
        if (dist2d(sim.find(tank)->body.position, move.target) <= AiController::kArriveRadius + 0.5f)
            break;
    }
    CHECK(dist2d(sim.find(tank)->body.position, move.target)
          <= AiController::kArriveRadius + 0.5f);
    for (int t = 0; t < 120; ++t) { // 2 s to brake after arriving
        ai.tick(sim);
        sim.step();
    }
    CHECK(sim.find(tank)->body.speed < 0.5f); // stopped on arrival
}

TEST(ai_attack_order_closes_and_destroys) {
    Sim sim(flatWorld(), 2);
    AiController ai;
    const uint32_t hunter = sim.spawnVehicle(TemplateId::Brick, 0, { 20.0f, 10.0f, 64.0f }, 0.0f);
    const uint32_t prey = sim.spawnVehicle(TemplateId::Brick, 1, { 110.0f, 10.0f, 64.0f }, 0.0f);

    Order attack;
    attack.type = OrderType::Attack;
    attack.targetEntity = prey;
    ai.setOrder(hunter, attack);

    bool destroyed = false;
    for (int t = 0; t < 60 * 60 && !destroyed; ++t) { // up to 60 s
        ai.tick(sim);
        sim.step();
        destroyed = sim.find(prey)->state.destroyed();
    }
    CHECK(destroyed); // closed to range and shot it to pieces
}

TEST(ai_squad_order_and_escort) {
    Sim sim(flatWorld(), 3);
    AiController ai;
    const uint32_t lead = sim.spawnVehicle(TemplateId::Brick, 0, { 30.0f, 10.0f, 60.0f }, 0.0f);
    const uint32_t wingA = sim.spawnVehicle(TemplateId::Brick, 0, { 24.0f, 10.0f, 54.0f }, 0.0f);
    const uint32_t wingB = sim.spawnVehicle(TemplateId::Brick, 0, { 24.0f, 10.0f, 66.0f }, 0.0f);

    // Lead moves out; wingmen are ordered to escort the lead.
    Order move;
    move.type = OrderType::MoveTo;
    move.target = { 100.0f, 10.0f, 60.0f };
    ai.setOrder(lead, move);
    Squad escorts;
    escorts.members = { wingA, wingB };
    escorts.order.type = OrderType::Escort;
    escorts.order.targetEntity = lead;
    ai.setSquadOrder(escorts);

    for (int t = 0; t < 60 * 30; ++t) {
        ai.tick(sim);
        sim.step();
    }
    const Vec3 leadPos = sim.find(lead)->body.position;
    CHECK(dist2d(leadPos, move.target) <= AiController::kArriveRadius + 0.5f);
    CHECK(dist2d(sim.find(wingA)->body.position, leadPos)
          <= AiController::kEscortDistance + 2.0f);
    CHECK(dist2d(sim.find(wingB)->body.position, leadPos)
          <= AiController::kEscortDistance + 2.0f);
}

TEST(ai_ignores_dead_and_pilotless_units) {
    Sim sim(flatWorld(), 4);
    AiController ai;
    const uint32_t tank = sim.spawnVehicle(TemplateId::Brick, 0, { 30.0f, 10.0f, 30.0f }, 0.0f);
    Order move;
    move.type = OrderType::MoveTo;
    move.target = { 90.0f, 10.0f, 30.0f };
    ai.setOrder(tank, move);

    // Eject the pilot: the AI must not drive an empty hull.
    sim.eject(tank);
    const float x = sim.find(tank)->body.position.x;
    for (int t = 0; t < 120; ++t) {
        ai.tick(sim);
        sim.step();
    }
    CHECK(sim.find(tank)->body.position.x == x);
}
