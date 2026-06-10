#pragma once
#include <cstdint>
#include "core/bytes.h"
#include "core/types.h"
#include "sim/sim.h"

namespace vox {

// Wire protocol (DESIGN.md §7.2): server-authoritative. Terrain destruction
// travels as events applied deterministically by all clients; vehicles travel
// as snapshots; chunk-hash audits catch divergence. Transport-agnostic: these
// are payloads for any reliable-ordered channel (Steam Networking Sockets in
// production, an in-memory loopback in tests).

constexpr uint32_t kProtocolVersion = 1;

enum class MsgType : uint8_t {
    Welcome = 1,   // server->client: your entity, the map, full state
    Input = 2,     // client->server: control input (+ optional fire command)
    TickUpdate = 3,// server->clients: tick, snapshots, events, pickups
    Audit = 4,     // server->clients: rotating chunk hash check
};

struct InputMsg {
    ControlInput input;
    bool fire = false;
    Vec3 fireDir{ 1.0f, 0.0f, 0.0f };
};

inline void writeVec3(ByteWriter& w, const Vec3& v) {
    w.f32(v.x);
    w.f32(v.y);
    w.f32(v.z);
}

inline Vec3 readVec3(ByteReader& r) {
    Vec3 v;
    v.x = r.f32();
    v.y = r.f32();
    v.z = r.f32();
    return v;
}

inline void writeEntitySnapshot(ByteWriter& w, const VehicleEntity& e) {
    w.u32(e.id);
    w.u8(static_cast<uint8_t>(e.tmpl->id));
    w.u8(e.team);
    w.u8(e.state.destroyed() ? 1 : 0);
    writeVec3(w, e.body.position);
    writeVec3(w, e.body.velocity);
    w.f32(e.body.yaw);
    w.f32(e.body.pitchAngle);
    w.f32(e.body.speed);
    w.u8(e.body.grounded ? 1 : 0);
    w.i32(e.ammo);
    w.u8(static_cast<uint8_t>(e.tmpl->parts.size()));
    for (size_t i = 0; i < e.tmpl->parts.size(); ++i) {
        w.i32(e.state.partHp(static_cast<int>(i)));
        w.u8(e.state.partAlive(static_cast<int>(i)) ? 1 : 0);
    }
}

inline void writeEvent(ByteWriter& w, const SimEvent& ev) {
    w.u8(static_cast<uint8_t>(ev.type));
    w.u32(ev.entity);
    w.i32(ev.part);
    w.u32(ev.pickup);
    w.u8(static_cast<uint8_t>(ev.kind));
    writeVec3(w, ev.position);
    writeVec3(w, ev.blast.center);
    w.f32(ev.blast.radius);
    w.i32(ev.blast.damage);
    w.u8(static_cast<uint8_t>(ev.blast.type));
}

inline SimEvent readEvent(ByteReader& r) {
    SimEvent ev;
    ev.type = static_cast<SimEvent::Type>(r.u8());
    ev.entity = r.u32();
    ev.part = r.i32();
    ev.pickup = r.u32();
    ev.kind = static_cast<DropKind>(r.u8());
    ev.position = readVec3(r);
    ev.blast.center = readVec3(r);
    ev.blast.radius = r.f32();
    ev.blast.damage = r.i32();
    ev.blast.type = static_cast<DamageType>(r.u8());
    return ev;
}

} // namespace vox
