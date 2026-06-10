#include "net/replay.h"
#include "core/bytes.h"
#include "world/mapfile.h"

namespace vox {

namespace {
constexpr uint32_t kReplayMagic = 0x31504552; // "REP1"
} // namespace

std::vector<uint8_t> ReplayLog::serialize() const {
    ByteWriter w;
    w.u32(kReplayMagic);
    w.u64(simSeed);
    w.u64(endTick);
    w.bytes(mapBytes);
    w.u32(static_cast<uint32_t>(joins.size()));
    for (const Join& j : joins) {
        w.u64(j.tick);
        w.u8(j.team);
        w.u8(static_cast<uint8_t>(j.vehicle));
        w.f32(j.spawn.x);
        w.f32(j.spawn.y);
        w.f32(j.spawn.z);
        w.f32(j.yaw);
    }
    w.u32(static_cast<uint32_t>(messages.size()));
    for (const Message& m : messages) {
        w.u64(m.tick);
        w.u32(m.peer);
        w.bytes(m.bytes);
    }
    return std::move(w.data);
}

std::optional<ReplayLog> ReplayLog::deserialize(const std::vector<uint8_t>& bytes) {
    ByteReader r(bytes);
    if (r.u32() != kReplayMagic) return std::nullopt;
    ReplayLog log;
    log.simSeed = r.u64();
    log.endTick = r.u64();
    log.mapBytes = r.bytes();
    const uint32_t joinCount = r.u32();
    if (!r.ok || joinCount > 64) return std::nullopt;
    for (uint32_t i = 0; i < joinCount; ++i) {
        ReplayLog::Join j;
        j.tick = r.u64();
        j.team = r.u8();
        j.vehicle = static_cast<TemplateId>(r.u8());
        j.spawn.x = r.f32();
        j.spawn.y = r.f32();
        j.spawn.z = r.f32();
        j.yaw = r.f32();
        log.joins.push_back(j);
    }
    const uint32_t msgCount = r.u32();
    if (!r.ok || msgCount > (1u << 24)) return std::nullopt;
    for (uint32_t i = 0; i < msgCount; ++i) {
        ReplayLog::Message m;
        m.tick = r.u64();
        m.peer = r.u32();
        m.bytes = r.bytes(1 << 20);
        log.messages.push_back(std::move(m));
    }
    if (!r.ok) return std::nullopt;
    return log;
}

RecordingServer::RecordingServer(VoxelWorld world, uint64_t seed)
    : inner(VoxelWorld(world), seed) { // copy: the original goes into the log
    record.simSeed = seed;
    record.mapBytes = encodeMap(world, { "replay-start", {} });
}

uint32_t RecordingServer::addClient(uint8_t team, TemplateId vehicle, Vec3 spawnPos, float yaw) {
    record.joins.push_back({ inner.sim().tick(), team, vehicle, spawnPos, yaw });
    return inner.addClient(team, vehicle, spawnPos, yaw);
}

void RecordingServer::receive(uint32_t peerId, const std::vector<uint8_t>& msg) {
    record.messages.push_back({ inner.sim().tick(), peerId, msg });
    inner.receive(peerId, msg);
}

void RecordingServer::tick() {
    inner.tick();
    record.endTick = inner.sim().tick();
}

uint64_t replayFinalHash(const ReplayLog& log) {
    std::optional<LoadedMap> map = decodeMap(log.mapBytes);
    if (!map) return 0;
    Server server(std::move(map->world), log.simSeed);

    size_t joinIdx = 0;
    size_t msgIdx = 0;
    // Joins recorded at tick T happened before the tick that advanced to T+1.
    while (server.sim().tick() < log.endTick) {
        const uint64_t now = server.sim().tick();
        while (joinIdx < log.joins.size() && log.joins[joinIdx].tick == now) {
            const ReplayLog::Join& j = log.joins[joinIdx++];
            server.addClient(j.team, j.vehicle, j.spawn, j.yaw);
        }
        while (msgIdx < log.messages.size() && log.messages[msgIdx].tick == now) {
            const ReplayLog::Message& m = log.messages[msgIdx++];
            server.receive(m.peer, m.bytes);
        }
        server.tick();
    }
    return server.sim().stateHash();
}

} // namespace vox
