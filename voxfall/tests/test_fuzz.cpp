#include "test_framework.h"
#include "core/rng.h"
#include "net/net.h"
#include "net/replay.h"
#include "net/udp.h"
#include "vehicle/voxformat.h"
#include "world/mapfile.h"

using namespace vox;

// Parser fuzzing: every byte stream that crosses a trust boundary (network,
// mod files, replays) must survive arbitrary garbage and arbitrary mutations
// of valid input without crashing or reading out of bounds. The ASan/UBSan
// build is where these tests really earn their keep.

namespace {

std::vector<uint8_t> randomBytes(Rng& rng, size_t maxLen) {
    std::vector<uint8_t> b(rng.range(static_cast<uint32_t>(maxLen)));
    for (uint8_t& v : b) v = static_cast<uint8_t>(rng.next());
    return b;
}

std::vector<uint8_t> mutate(Rng& rng, std::vector<uint8_t> bytes) {
    if (bytes.empty()) return bytes;
    const uint32_t edits = 1 + rng.range(8);
    for (uint32_t i = 0; i < edits; ++i) {
        switch (rng.range(3)) {
            case 0: // flip a byte
                bytes[rng.range(static_cast<uint32_t>(bytes.size()))] =
                    static_cast<uint8_t>(rng.next());
                break;
            case 1: // truncate
                bytes.resize(rng.range(static_cast<uint32_t>(bytes.size())) + 1);
                break;
            case 2: // extend with junk
                bytes.push_back(static_cast<uint8_t>(rng.next()));
                break;
        }
    }
    return bytes;
}

} // namespace

TEST(fuzz_map_decoder_survives_garbage) {
    VoxelWorld w({ 48, 32, 48 });
    w.generate(2);
    const std::vector<uint8_t> valid = encodeMap(w, { "fuzz", { { { 1, 2, 3 }, 0 } } });

    Rng rng(0xF022);
    for (int i = 0; i < 300; ++i) decodeMap(randomBytes(rng, 4096));
    for (int i = 0; i < 300; ++i) decodeMap(mutate(rng, valid));
    CHECK(decodeMap(valid).has_value()); // and the original still parses
}

TEST(fuzz_replay_decoder_survives_garbage) {
    Rng rng(0xF033);
    ReplayLog log;
    log.simSeed = 1;
    VoxelWorld w({ 32, 32, 32 });
    log.mapBytes = encodeMap(w, { "", {} });
    log.joins.push_back({ 0, 0, TemplateId::Brick, { 1, 2, 3 }, 0.0f });
    log.messages.push_back({ 0, 1, { 1, 2, 3 } });
    const std::vector<uint8_t> valid = log.serialize();

    for (int i = 0; i < 300; ++i) ReplayLog::deserialize(randomBytes(rng, 2048));
    for (int i = 0; i < 300; ++i) ReplayLog::deserialize(mutate(rng, valid));
    CHECK(ReplayLog::deserialize(valid).has_value());
}

TEST(fuzz_vox_parser_survives_garbage) {
    Rng rng(0xF044);
    for (int i = 0; i < 500; ++i) parseVox(randomBytes(rng, 2048));
    // Mutations of a valid header are the nastier corpus.
    std::vector<uint8_t> seedFile = { 'V', 'O', 'X', ' ', 150, 0, 0, 0,
                                      'M', 'A', 'I', 'N', 0, 0, 0, 0, 24, 0, 0, 0,
                                      'S', 'I', 'Z', 'E', 12, 0, 0, 0, 0, 0, 0, 0,
                                      2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0 };
    for (int i = 0; i < 500; ++i) parseVox(mutate(rng, seedFile));
}

TEST(fuzz_network_sessions_survive_garbage) {
    VoxelWorld w({ 48, 32, 48 });
    w.generate(3);
    Server server(std::move(w), 1);
    Client client;
    const uint32_t peer = server.addClient(0, TemplateId::Brick, { 10.0f, 12.0f, 24.0f });
    for (const auto& msg : server.takeOutbox(peer)) client.receive(msg);
    CHECK(client.joined());

    Rng rng(0xF055);
    for (int i = 0; i < 400; ++i) {
        server.receive(peer, randomBytes(rng, 256));
        client.receive(randomBytes(rng, 256));
        // Mutated copies of real traffic.
        server.receive(peer, mutate(rng, client.makeInput({})));
        server.tick();
        for (const auto& msg : server.takeOutbox(peer))
            client.receive(rng.chance(0.3f) ? mutate(rng, msg) : msg);
    }
    // The session may have desynced from mutated snapshots — but it must
    // still be running and structurally sound.
    CHECK(server.sim().tick() == 400);
}

TEST(fuzz_fragment_channel_survives_garbage) {
    FragmentChannel channel;
    Rng rng(0xF066);
    for (int i = 0; i < 600; ++i) channel.onDatagram(randomBytes(rng, 1400));
    // A valid exchange still works afterwards.
    UdpSocket a, b;
    CHECK(a.open(0));
    CHECK(b.open(0));
    std::vector<uint8_t> msg(3000, 0xAB);
    FragmentChannel sender;
    sender.sendMessage(a, loopbackAddress(b.boundPort()), msg);
    std::vector<uint8_t> got;
    for (int tries = 0; tries < 1000 && got.empty(); ++tries)
        while (auto packet = b.receive())
            for (auto& m : channel.onDatagram(packet->second)) got = std::move(m);
    CHECK(got == msg);
}
