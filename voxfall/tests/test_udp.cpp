#include <chrono>
#include <thread>
#include "test_framework.h"
#include "net/udp.h"

using namespace vox;

namespace {
VoxelWorld smallArena() {
    VoxelWorld w({ 96, 48, 96 });
    w.generate(321);
    return w;
}
} // namespace

TEST(udp_fragmentation_roundtrip) {
    UdpSocket a, b;
    CHECK(a.open(0));
    CHECK(b.open(0));

    // A message far bigger than one datagram (forces ~35 fragments).
    std::vector<uint8_t> big(42000);
    for (size_t i = 0; i < big.size(); ++i) big[i] = static_cast<uint8_t>(i * 31);

    FragmentChannel sender, receiver;
    sender.sendMessage(a, loopbackAddress(b.boundPort()), big);

    std::vector<uint8_t> got;
    for (int tries = 0; tries < 1000 && got.empty(); ++tries) {
        while (auto packet = b.receive()) {
            for (auto& msg : receiver.onDatagram(packet->second)) got = std::move(msg);
        }
    }
    CHECK(got == big); // reassembled byte-identical
}

TEST(udp_client_joins_and_syncs_over_real_sockets) {
    UdpServerHost host(smallArena(), 4, 0); // ephemeral port
    CHECK(host.listening());

    Client replica;
    UdpClientLink link;
    CHECK(link.connect(loopbackAddress(host.port()), 0));

    // Pump both ends until the (multi-fragment) Welcome lands.
    for (int i = 0; i < 600 && !replica.joined(); ++i) {
        host.pumpOnce();
        link.pump(replica);
    }
    CHECK(replica.joined());
    CHECK(replica.world()->contentHash() == host.server().sim().world().contentHash());

    // Drive over the wire and verify replication.
    ControlInput in;
    in.throttle = 1.0f;
    for (int i = 0; i < 120; ++i) {
        link.send(replica.makeInput(in));
        host.pumpOnce();
        link.pump(replica);
    }
    const VehicleEntity* serverSide = host.server().sim().find(replica.myEntity());
    CHECK(serverSide != nullptr);
    CHECK(serverSide->body.speed > 0.0f); // input arrived, tank moving

    // Datagram delivery is asynchronous (macOS delivers late where Linux
    // loopback is effectively synchronous): compare positions only at a
    // matched tick, retrying while the final update is in flight.
    bool matched = false;
    for (int i = 0; i < 300 && !matched; ++i) {
        host.pumpOnce();
        for (int j = 0; j < 100 && replica.lastTick() < host.server().sim().tick(); ++j) {
            link.pump(replica);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        const VehicleEntity* clientSide = replica.entity(replica.myEntity());
        matched = clientSide != nullptr
               && replica.lastTick() == host.server().sim().tick()
               && clientSide->body.position.x == serverSide->body.position.x;
    }
    CHECK(matched);

    // Garbage and stranger datagrams must be ignored, not crash anything.
    UdpSocket stranger;
    CHECK(stranger.open(0));
    const uint8_t junk[5] = { 1, 2, 3, 4, 5 };
    stranger.sendTo(loopbackAddress(host.port()), junk, sizeof(junk));
    host.pumpOnce();
    CHECK(host.server().sim().tick() > 0); // still alive
}
