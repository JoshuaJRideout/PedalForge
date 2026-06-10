#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <vector>
#include "net/protocol.h"
#include "sim/sim.h"
#include "world/mapfile.h"

namespace vox {

// Listen-server + client replica (DESIGN.md §7.2, §11.3 tier 1).
// Both are transport-agnostic message pumps: bytes in, bytes out. Tests wire
// them together with an in-memory loopback; production wires them to Steam
// Networking Sockets. The server is the only simulation authority.

class Server {
public:
    static constexpr int kAuditIntervalTicks = 30;

    Server(VoxelWorld world, uint64_t seed);

    // Registers a peer, spawns their vehicle, queues the Welcome message.
    uint32_t addClient(uint8_t team, TemplateId vehicle, Vec3 spawnPos, float yaw = 0.0f);

    void receive(uint32_t peerId, const std::vector<uint8_t>& msg);

    // Advance one tick and queue TickUpdate (+ periodic Audit) to every peer.
    void tick();

    std::vector<std::vector<uint8_t>> takeOutbox(uint32_t peerId);

    Sim& sim() { return simulation; }
    const Sim& sim() const { return simulation; }
    uint32_t entityOf(uint32_t peerId) const;

private:
    struct Peer {
        uint32_t id = 0;
        uint32_t entityId = 0;
        std::vector<std::vector<uint8_t>> outbox;
    };

    std::vector<uint8_t> buildWelcome(const Peer& peer);
    Peer* findPeer(uint32_t peerId);

    Sim simulation;
    std::vector<Peer> peers;
    uint32_t nextPeerId = 1;
    uint64_t auditCursor = 0;
};

class Client {
public:
    void receive(const std::vector<uint8_t>& msg);

    // Build an Input message for the server (works once joined).
    std::vector<uint8_t> makeInput(const ControlInput& input, bool fire = false,
                                   Vec3 fireDir = { 1.0f, 0.0f, 0.0f }) const;

    bool joined() const { return replicaWorld.has_value(); }
    uint32_t myEntity() const { return myEntityId; }
    uint64_t lastTick() const { return tick; }
    bool desyncDetected() const { return desync; }

    const VoxelWorld* world() const { return replicaWorld ? &*replicaWorld : nullptr; }
    VoxelWorld* mutableWorld() { return replicaWorld ? &*replicaWorld : nullptr; } // tests
    const VehicleEntity* entity(uint32_t id) const;
    const std::vector<VehicleEntity>& entities() const { return replicaEntities; }
    const std::vector<Pickup>& pickups() const { return replicaPickups; }
    const std::vector<SimEvent>& lastEvents() const { return events; } // VFX hooks

private:
    void applySnapshot(ByteReader& r);

    std::optional<VoxelWorld> replicaWorld;
    std::vector<VehicleEntity> replicaEntities;
    std::vector<Pickup> replicaPickups;
    std::vector<SimEvent> events;
    uint32_t myEntityId = 0;
    uint64_t tick = 0;
    bool desync = false;
};

} // namespace vox
