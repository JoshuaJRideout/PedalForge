#include "test_framework.h"
#include "net/replay.h"

using namespace vox;

namespace {
VoxelWorld arena() {
    VoxelWorld w({ 96, 48, 96 });
    w.generate(555);
    return w;
}
} // namespace

TEST(replay_reproduces_session_exactly) {
    RecordingServer recording(arena(), 9);
    Client a, b;
    const uint32_t peerA = recording.addClient(
        0, TemplateId::Brick,
        { 20.0f, static_cast<float>(recording.server().sim().world().heightAt(20, 48)), 48.0f });
    const uint32_t peerB = recording.addClient(
        1, TemplateId::Brick,
        { 70.0f, static_cast<float>(recording.server().sim().world().heightAt(70, 48)), 48.0f });
    for (const auto& msg : recording.server().takeOutbox(peerA)) a.receive(msg);
    for (const auto& msg : recording.server().takeOutbox(peerB)) b.receive(msg);

    // A scripted little skirmish: drive, shoot, eject, keep ticking.
    ControlInput drive;
    drive.throttle = 1.0f;
    drive.steer = 0.2f;
    for (int t = 0; t < 300; ++t) {
        if (t % 3 == 0) recording.receive(peerA, a.makeInput(drive, t % 45 == 0, { 1.0f, -0.1f, 0.2f }));
        if (t % 4 == 0) recording.receive(peerB, b.makeInput(drive));
        if (t == 200) recording.receive(peerB, b.makeAction(ActionKind::Eject));
        recording.tick();
    }
    const uint64_t liveHash = recording.server().sim().stateHash();

    // Round-trip the log through bytes, then replay it.
    const std::vector<uint8_t> bytes = recording.log().serialize();
    const std::optional<ReplayLog> parsed = ReplayLog::deserialize(bytes);
    CHECK(parsed.has_value());
    CHECK(replayFinalHash(*parsed) == liveHash); // byte-identical re-simulation

    // Corrupt the log: the replay must not silently "work".
    std::vector<uint8_t> corrupt = bytes;
    corrupt[0] ^= 0xFF;
    CHECK(!ReplayLog::deserialize(corrupt).has_value());
}
