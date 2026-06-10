#include "test_framework.h"
#include "net/net.h"

using namespace vox;

namespace {

VoxelWorld arena() {
    VoxelWorld w({ 96, 48, 96 });
    w.generate(777);
    return w;
}

// In-memory loopback "transport": pump all queued server->client messages.
void pump(Server& server, uint32_t peerId, Client& client) {
    for (const std::vector<uint8_t>& msg : server.takeOutbox(peerId)) client.receive(msg);
}

float groundY(const VoxelWorld& w, int x, int z) {
    return static_cast<float>(w.heightAt(x, z));
}

} // namespace

TEST(net_join_synchronizes_full_state) {
    VoxelWorld world = arena();
    const uint64_t worldHash = world.contentHash();
    Server server(std::move(world), 1);
    Client client;

    const uint32_t peer = server.addClient(0, TemplateId::Brick,
                                           { 20.0f, groundY(server.sim().world(), 20, 48), 48.0f });
    pump(server, peer, client);

    CHECK(client.joined());
    CHECK(client.myEntity() == server.entityOf(peer));
    CHECK(client.world()->contentHash() == worldHash);
    CHECK(client.entities().size() == 1);
    CHECK(client.entity(client.myEntity())->tmpl->id == TemplateId::Brick);
}

TEST(net_input_drives_server_and_replicates_back) {
    Server server(arena(), 2);
    Client client;
    const uint32_t peer = server.addClient(0, TemplateId::Brick,
                                           { 20.0f, groundY(server.sim().world(), 20, 48), 48.0f });
    pump(server, peer, client);

    ControlInput in;
    in.throttle = 1.0f;
    for (int t = 0; t < 120; ++t) { // 2 s of driving
        server.receive(peer, client.makeInput(in));
        server.tick();
        pump(server, peer, client);
    }

    const VehicleEntity* serverEntity = server.sim().find(server.entityOf(peer));
    const VehicleEntity* replicaEntity = client.entity(client.myEntity());
    CHECK(serverEntity->body.position.x > 25.0f); // actually moved
    CHECK(replicaEntity->body.position.x == serverEntity->body.position.x);
    CHECK(replicaEntity->body.speed == serverEntity->body.speed);
    CHECK(client.lastTick() == server.sim().tick());
}

TEST(net_destruction_keeps_worlds_in_sync) {
    Server server(arena(), 3);
    Client client;
    const float gy = groundY(server.sim().world(), 30, 48);
    const uint32_t peer = server.addClient(0, TemplateId::Brick, { 30.0f, gy, 48.0f });
    server.addClient(1, TemplateId::Brick, { 50.0f, groundY(server.sim().world(), 50, 48), 48.0f });
    pump(server, peer, client);

    // Shell the ground repeatedly; terrain events must reproduce identically.
    ControlInput idle;
    for (int volley = 0; volley < 5; ++volley) {
        server.receive(peer, client.makeInput(idle, /*fire=*/true, { 1.0f, -0.5f, 0.3f }));
        for (int t = 0; t < 10; ++t) {
            server.tick();
            pump(server, peer, client);
        }
    }
    CHECK(client.world()->contentHash() == server.sim().world().contentHash());
    CHECK(!client.desyncDetected()); // audits ran (50 ticks) and agreed
}

TEST(net_part_damage_replicates) {
    Server server(arena(), 4);
    Client client;
    const uint32_t peer = server.addClient(0, TemplateId::Brick,
                                           { 30.0f, groundY(server.sim().world(), 30, 48), 48.0f });
    // Enemy tank straight ahead on flat-ish ground at the same height.
    const uint32_t enemy = server.sim().spawnVehicle(
        TemplateId::Brick, 1, { 38.0f, groundY(server.sim().world(), 30, 48), 48.0f }, 0.0f);
    pump(server, peer, client);

    ControlInput idle;
    for (int i = 0; i < 12; ++i) {
        server.receive(peer, client.makeInput(idle, /*fire=*/true, { 1.0f, 0.0f, 0.0f }));
        server.tick();
        pump(server, peer, client);
    }

    const VehicleEntity* serverEnemy = server.sim().find(enemy);
    const VehicleEntity* replicaEnemy = client.entity(enemy);
    CHECK(replicaEnemy != nullptr);
    bool anyDamage = false;
    for (size_t p = 0; p < serverEnemy->tmpl->parts.size(); ++p) {
        anyDamage |= serverEnemy->state.partHp(static_cast<int>(p))
                   < serverEnemy->tmpl->parts[p].maxHp;
        CHECK(replicaEnemy->state.partHp(static_cast<int>(p))
              == serverEnemy->state.partHp(static_cast<int>(p)));
        CHECK(replicaEnemy->state.partAlive(static_cast<int>(p))
              == serverEnemy->state.partAlive(static_cast<int>(p)));
    }
    CHECK(anyDamage); // shots actually landed
}

TEST(net_late_joiner_gets_destroyed_world) {
    Server server(arena(), 5);
    Client first;
    const uint32_t peerA = server.addClient(0, TemplateId::Brick,
                                            { 20.0f, groundY(server.sim().world(), 20, 48), 48.0f });
    pump(server, peerA, first);

    // Blow up some terrain before the second player arrives.
    server.sim().applyBlast({ { 48.0f, groundY(server.sim().world(), 48, 48) - 1.0f, 48.0f },
                              5.0f, 800, DamageType::Seismic });
    server.tick();
    pump(server, peerA, first);

    Client second;
    const uint32_t peerB = server.addClient(1, TemplateId::Brick,
                                            { 70.0f, groundY(server.sim().world(), 70, 48), 48.0f });
    pump(server, peerB, second);

    CHECK(second.joined());
    CHECK(second.world()->contentHash() == server.sim().world().contentHash());
    CHECK(second.entities().size() == 2); // sees both vehicles
    // And both clients agree with each other after the next tick.
    server.tick();
    pump(server, peerA, first);
    pump(server, peerB, second);
    CHECK(first.world()->contentHash() == second.world()->contentHash());
}

TEST(net_audit_catches_tampered_world) {
    Server server(arena(), 6);
    Client client;
    const uint32_t peer = server.addClient(0, TemplateId::Brick,
                                           { 20.0f, groundY(server.sim().world(), 20, 48), 48.0f });
    pump(server, peer, client);

    // Corrupt one voxel on the client (simulated desync/bad memory/cheat).
    client.mutableWorld()->set({ 5, 5, 5 }, Material::Metal);

    // Audits rotate one chunk per 30 ticks; cover the whole 3x2x3 chunk grid.
    bool flagged = false;
    for (int t = 0; t < 30 * 20 && !flagged; ++t) {
        server.tick();
        pump(server, peer, client);
        flagged = client.desyncDetected();
    }
    CHECK(flagged);
}
