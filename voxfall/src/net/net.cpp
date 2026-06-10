#include "net/net.h"
#include <algorithm>
#include <utility>

namespace vox {

namespace {

// Chunk coords arrive over the network: validate before any arithmetic
// (fuzzing found garbage * kChunkSize overflowing into release-build UB).
bool validChunkCoord(const VoxelWorld& world, Int3 c) {
    const int n = VoxelWorld::kChunkSize;
    const Int3 size = world.size();
    return c.x >= 0 && c.y >= 0 && c.z >= 0 && c.x * n < size.x && c.y * n < size.y
        && c.z * n < size.z;
}

// RLE one 32^3 chunk's materials (repair payload — same scheme as .vxm).
void writeChunkPayload(ByteWriter& w, const VoxelWorld& world, Int3 chunkCoord) {
    const int n = VoxelWorld::kChunkSize;
    const Int3 base{ chunkCoord.x * n, chunkCoord.y * n, chunkCoord.z * n };
    Material current = world.at(base);
    uint32_t run = 0;
    for (int y = 0; y < n; ++y)
        for (int z = 0; z < n; ++z)
            for (int x = 0; x < n; ++x) {
                const Material m = world.at(base + Int3{ x, y, z });
                if (m == current) {
                    ++run;
                } else {
                    w.u8(static_cast<uint8_t>(current));
                    w.u32(run);
                    current = m;
                    run = 1;
                }
            }
    w.u8(static_cast<uint8_t>(current));
    w.u32(run);
}

bool applyChunkPayload(ByteReader& r, VoxelWorld& world, Int3 chunkCoord) {
    const int n = VoxelWorld::kChunkSize;
    const Int3 base{ chunkCoord.x * n, chunkCoord.y * n, chunkCoord.z * n };
    uint64_t written = 0;
    const uint64_t total = static_cast<uint64_t>(n) * n * n;
    Int3 p{ 0, 0, 0 };
    while (written < total && r.ok) {
        const uint8_t mat = r.u8();
        const uint32_t run = r.u32();
        // run == 0 would loop forever on malformed input (found by fuzzing).
        if (!r.ok || run == 0 || mat >= static_cast<uint8_t>(Material::Count)
            || written + run > total)
            return false;
        for (uint32_t i = 0; i < run; ++i) {
            const Int3 cell = base + p;
            if (world.inBounds(cell)) world.set(cell, static_cast<Material>(mat));
            if (++p.x == n) {
                p.x = 0;
                if (++p.z == n) {
                    p.z = 0;
                    ++p.y;
                }
            }
        }
        written += run;
    }
    return r.ok && written == total;
}

} // namespace

// ---------------------------------------------------------------- Server ----

Server::Server(VoxelWorld world, uint64_t seed) : simulation(std::move(world), seed) {}

Server::Peer* Server::findPeer(uint32_t peerId) {
    for (Peer& p : peers)
        if (p.id == peerId) return &p;
    return nullptr;
}

uint32_t Server::entityOf(uint32_t peerId) const {
    for (const Peer& p : peers)
        if (p.id == peerId) return p.entityId;
    return 0;
}

uint32_t Server::addClient(uint8_t team, TemplateId vehicle, Vec3 spawnPos, float yaw) {
    Peer peer;
    peer.id = nextPeerId++;
    peer.entityId = simulation.spawnVehicle(vehicle, team, spawnPos, yaw);
    peer.outbox.push_back(buildWelcome(peer));
    peers.push_back(std::move(peer));
    return peers.back().id;
}

std::vector<uint8_t> Server::buildWelcome(const Peer& peer) {
    ByteWriter w;
    w.u8(static_cast<uint8_t>(MsgType::Welcome));
    w.u32(kProtocolVersion);
    w.u32(peer.entityId);
    w.u64(simulation.tick());
    // Full current world (join-in-progress gets destruction baked in). Note:
    // partial per-voxel damage accumulation is not yet in the map format; a
    // voxel at 90% damage arrives pristine on the late joiner. Tracked for v2.
    w.bytes(encodeMap(simulation.world(), { "live", {} }));
    w.u32(static_cast<uint32_t>(simulation.entities().size()));
    for (const VehicleEntity& e : simulation.entities()) writeEntitySnapshot(w, e);
    return std::move(w.data);
}

void Server::receive(uint32_t peerId, const std::vector<uint8_t>& msg) {
    Peer* peer = findPeer(peerId);
    if (!peer) return;
    ByteReader r(msg);
    const MsgType type = static_cast<MsgType>(r.u8());

    if (type == MsgType::Input) {
        ControlInput input;
        input.throttle = r.f32();
        input.steer = r.f32();
        input.pitch = r.f32();
        input.jump = r.u8() != 0;
        const bool fire = r.u8() != 0;
        const Vec3 fireDir = readVec3(r);
        if (!r.ok) return; // malformed input is dropped, never trusted

        simulation.setInput(peer->entityId, input);
        if (fire) simulation.fire(peer->entityId, fireDir);
        return;
    }

    if (type == MsgType::ChunkRequest) {
        const Int3 coord{ r.i32(), r.i32(), r.i32() };
        if (!r.ok || !validChunkCoord(simulation.world(), coord)) return;
        ByteWriter w;
        w.u8(static_cast<uint8_t>(MsgType::ChunkData));
        w.i32(coord.x);
        w.i32(coord.y);
        w.i32(coord.z);
        writeChunkPayload(w, simulation.world(), coord);
        peer->outbox.push_back(std::move(w.data));
        return;
    }

    if (type == MsgType::Action) {
        const ActionKind action = static_cast<ActionKind>(r.u8());
        const uint32_t target = r.u32();
        if (!r.ok) return;

        uint32_t newEntity = 0;
        if (action == ActionKind::Eject) {
            newEntity = simulation.eject(peer->entityId);
        } else if (action == ActionKind::Board) {
            if (simulation.board(peer->entityId, target)) newEntity = target;
        }
        if (newEntity != 0) {
            peer->entityId = newEntity;
            ByteWriter w;
            w.u8(static_cast<uint8_t>(MsgType::ControlAssign));
            w.u32(newEntity);
            peer->outbox.push_back(std::move(w.data));
        }
    }
}

void Server::tick() {
    simulation.step();
    const std::vector<SimEvent> events = simulation.takeEvents();

    ByteWriter w;
    w.u8(static_cast<uint8_t>(MsgType::TickUpdate));
    w.u64(simulation.tick());
    w.u32(static_cast<uint32_t>(simulation.entities().size()));
    for (const VehicleEntity& e : simulation.entities()) writeEntitySnapshot(w, e);
    w.u32(static_cast<uint32_t>(events.size()));
    for (const SimEvent& ev : events) writeEvent(w, ev);
    w.u32(static_cast<uint32_t>(simulation.pickups().size()));
    for (const Pickup& p : simulation.pickups()) {
        w.u32(p.id);
        w.u8(static_cast<uint8_t>(p.kind));
        writeVec3(w, p.position);
        w.u64(p.despawnTick);
    }
    w.u32(static_cast<uint32_t>(simulation.projectiles().size()));
    for (const Projectile& p : simulation.projectiles()) {
        w.u32(p.id);
        writeVec3(w, p.position);
        writeVec3(w, p.velocity);
    }
    const std::vector<uint8_t> update = w.data;

    std::optional<std::vector<uint8_t>> audit;
    if (simulation.tick() % kAuditIntervalTicks == 0) {
        const Int3 size = simulation.world().size();
        const Int3 chunks{ (size.x + VoxelWorld::kChunkSize - 1) / VoxelWorld::kChunkSize,
                           (size.y + VoxelWorld::kChunkSize - 1) / VoxelWorld::kChunkSize,
                           (size.z + VoxelWorld::kChunkSize - 1) / VoxelWorld::kChunkSize };
        const uint64_t total = static_cast<uint64_t>(chunks.x) * chunks.y * chunks.z;
        const uint64_t index = auditCursor++ % total;
        const Int3 coord{ static_cast<int>(index % chunks.x),
                          static_cast<int>((index / chunks.x) % chunks.y),
                          static_cast<int>(index / (static_cast<uint64_t>(chunks.x) * chunks.y)) };
        ByteWriter a;
        a.u8(static_cast<uint8_t>(MsgType::Audit));
        a.i32(coord.x);
        a.i32(coord.y);
        a.i32(coord.z);
        a.u64(simulation.world().chunkHash(coord));
        audit = std::move(a.data);
    }

    for (Peer& peer : peers) {
        peer.outbox.push_back(update);
        if (audit) peer.outbox.push_back(*audit);
    }
}

std::vector<std::vector<uint8_t>> Server::takeOutbox(uint32_t peerId) {
    Peer* peer = findPeer(peerId);
    if (!peer) return {};
    return std::exchange(peer->outbox, {});
}

// ---------------------------------------------------------------- Client ----

const VehicleEntity* Client::entity(uint32_t id) const {
    for (const VehicleEntity& e : replicaEntities)
        if (e.id == id) return &e;
    return nullptr;
}

std::vector<uint8_t> Client::makeInput(const ControlInput& input, bool fire, Vec3 fireDir) const {
    ByteWriter w;
    w.u8(static_cast<uint8_t>(MsgType::Input));
    w.f32(input.throttle);
    w.f32(input.steer);
    w.f32(input.pitch);
    w.u8(input.jump ? 1 : 0);
    w.u8(fire ? 1 : 0);
    writeVec3(w, fireDir);
    return std::move(w.data);
}

std::vector<uint8_t> Client::makeAction(ActionKind action, uint32_t target) const {
    ByteWriter w;
    w.u8(static_cast<uint8_t>(MsgType::Action));
    w.u8(static_cast<uint8_t>(action));
    w.u32(target);
    return std::move(w.data);
}

void Client::applySnapshot(ByteReader& r) {
    const uint32_t count = r.u32();
    std::vector<uint32_t> seen;
    for (uint32_t i = 0; i < count && r.ok; ++i) {
        const uint32_t id = r.u32();
        seen.push_back(id);
        const TemplateId tmplId = static_cast<TemplateId>(r.u8());
        const uint8_t team = r.u8();
        const bool dead = r.u8() != 0;

        VehicleEntity* e = nullptr;
        for (VehicleEntity& existing : replicaEntities)
            if (existing.id == id) e = &existing;
        if (!e) {
            if (tmplId >= TemplateId::Count) return;
            replicaEntities.emplace_back(id, team, VehicleTemplate::byId(tmplId));
            e = &replicaEntities.back();
        }

        e->body.position = readVec3(r);
        e->body.velocity = readVec3(r);
        e->body.yaw = r.f32();
        e->body.pitchAngle = r.f32();
        e->body.speed = r.f32();
        e->body.grounded = r.u8() != 0;
        e->hasPilot = r.u8() != 0;
        e->ammo = r.i32();
        const uint8_t partCount = r.u8();
        for (uint8_t part = 0; part < partCount && r.ok; ++part) {
            const int hp = r.i32();
            const bool alive = r.u8() != 0;
            if (part < e->tmpl->parts.size())
                e->state.replicatePartState(part, hp, !alive);
        }
        (void)dead; // core-part replication implies it; kept on the wire for robustness
    }
    if (!r.ok) return;
    // Snapshots are full lists: anything we know that the server didn't send
    // no longer exists (e.g. a pilot consumed by boarding).
    std::erase_if(replicaEntities, [&](const VehicleEntity& e) {
        return std::find(seen.begin(), seen.end(), e.id) == seen.end();
    });
}

void Client::receive(const std::vector<uint8_t>& msg) {
    ByteReader r(msg);
    const MsgType type = static_cast<MsgType>(r.u8());

    switch (type) {
        case MsgType::Welcome: {
            if (r.u32() != kProtocolVersion) return;
            myEntityId = r.u32();
            tick = r.u64();
            const std::vector<uint8_t> mapBytes = r.bytes();
            std::optional<LoadedMap> map = decodeMap(mapBytes);
            if (!r.ok || !map) return;
            replicaWorld.emplace(std::move(map->world));
            replicaEntities.clear();
            applySnapshot(r);
            break;
        }
        case MsgType::TickUpdate: {
            if (!joined()) return;
            tick = r.u64();
            applySnapshot(r);
            events.clear();
            const uint32_t eventCount = r.u32();
            for (uint32_t i = 0; i < eventCount && r.ok; ++i) {
                const SimEvent ev = readEvent(r);
                // Terrain destruction is event-sourced: apply blasts locally,
                // deterministically reproducing the server's world (§7.2).
                if (ev.type == SimEvent::Type::Blast) replicaWorld->applyBlast(ev.blast);
                events.push_back(ev);
            }
            replicaPickups.clear();
            const uint32_t pickupCount = r.u32();
            for (uint32_t i = 0; i < pickupCount && r.ok; ++i) {
                Pickup p;
                p.id = r.u32();
                p.kind = static_cast<DropKind>(r.u8());
                p.position = readVec3(r);
                p.despawnTick = r.u64();
                replicaPickups.push_back(p);
            }
            replicaProjectiles.clear();
            const uint32_t projectileCount = r.u32();
            for (uint32_t i = 0; i < projectileCount && r.ok; ++i) {
                Projectile p;
                p.id = r.u32();
                p.position = readVec3(r);
                p.velocity = readVec3(r);
                replicaProjectiles.push_back(p);
            }
            break;
        }
        case MsgType::ControlAssign: {
            const uint32_t id = r.u32();
            if (r.ok) myEntityId = id;
            break;
        }
        case MsgType::Audit: {
            if (!joined()) return;
            const Int3 coord{ r.i32(), r.i32(), r.i32() };
            const uint64_t serverHash = r.u64();
            if (r.ok && validChunkCoord(*replicaWorld, coord)
                && replicaWorld->chunkHash(coord) != serverHash) {
                desync = true;
                // Ask for the authoritative chunk (§7.2 "chunk re-sync").
                ByteWriter w;
                w.u8(static_cast<uint8_t>(MsgType::ChunkRequest));
                w.i32(coord.x);
                w.i32(coord.y);
                w.i32(coord.z);
                outbox.push_back(std::move(w.data));
            }
            break;
        }
        case MsgType::ChunkData: {
            if (!joined()) return;
            const Int3 coord{ r.i32(), r.i32(), r.i32() };
            if (!r.ok || !validChunkCoord(*replicaWorld, coord)) return;
            if (applyChunkPayload(r, *replicaWorld, coord)) desync = false;
            break;
        }
        default:
            break;
    }
}

} // namespace vox
