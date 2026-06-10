#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include "net/net.h"

namespace vox {

// Replay = initial world + every inbound server message with its tick
// (DESIGN.md §11.1: "every match is seed + event log"). Because the Sim is
// deterministic, replaying the log reproduces the entire match byte-for-byte
// — which makes replays simultaneously a feature and the desync debugger.

struct ReplayLog {
    std::vector<uint8_t> mapBytes; // encodeMap of the starting world
    uint64_t simSeed = 0;

    struct Join {
        uint64_t tick = 0;
        uint8_t team = 0;
        TemplateId vehicle = TemplateId::Brick;
        Vec3 spawn;
        float yaw = 0.0f;
    };
    std::vector<Join> joins;

    struct Message {
        uint64_t tick = 0;
        uint32_t peer = 0;
        std::vector<uint8_t> bytes;
    };
    std::vector<Message> messages;

    uint64_t endTick = 0;

    std::vector<uint8_t> serialize() const;
    static std::optional<ReplayLog> deserialize(const std::vector<uint8_t>& bytes);
};

// A Server wrapper that records everything needed to reproduce the session.
class RecordingServer {
public:
    RecordingServer(VoxelWorld world, uint64_t seed);

    uint32_t addClient(uint8_t team, TemplateId vehicle, Vec3 spawnPos, float yaw = 0.0f);
    void receive(uint32_t peerId, const std::vector<uint8_t>& msg);
    void tick();

    Server& server() { return inner; }
    const ReplayLog& log() const { return record; }

private:
    Server inner;
    ReplayLog record;
};

// Re-run a log and return the sim's final state hash (0 on malformed log).
uint64_t replayFinalHash(const ReplayLog& log);

} // namespace vox
