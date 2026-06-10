#include "sim/sim.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace vox {

namespace {

uint64_t fnv(uint64_t h, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        h ^= (v >> (i * 8)) & 0xFF;
        h *= 0x100000001B3ull;
    }
    return h;
}

uint64_t fnvF(uint64_t h, float f) { return fnv(h, std::bit_cast<uint32_t>(f)); }

Vec3 normalized(Vec3 v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len <= 0.0f) return { 1.0f, 0.0f, 0.0f };
    return { v.x / len, v.y / len, v.z / len };
}

} // namespace

Sim::Sim(VoxelWorld world, uint64_t seed) : voxels(std::move(world)), rng(seed) {}

uint32_t Sim::spawnVehicle(TemplateId id, uint8_t team, Vec3 position, float yaw) {
    const VehicleTemplate& tmpl = VehicleTemplate::byId(id);
    VehicleEntity e(nextEntityId++, team, tmpl);
    e.body.position = position;
    e.body.yaw = yaw;
    e.body.grounded = tmpl.locomotion != LocomotionClass::Jet;
    vehicles.push_back(std::move(e));
    return vehicles.back().id;
}

VehicleEntity* Sim::find(uint32_t id) {
    for (VehicleEntity& e : vehicles)
        if (e.id == id) return &e;
    return nullptr;
}

const VehicleEntity* Sim::find(uint32_t id) const {
    for (const VehicleEntity& e : vehicles)
        if (e.id == id) return &e;
    return nullptr;
}

void Sim::setInput(uint32_t id, const ControlInput& input) {
    if (VehicleEntity* e = find(id)) e->input = input;
}

std::optional<Int3> Sim::worldToSubvoxel(const VehicleEntity& e, Vec3 p) const {
    const Int3 dims = e.tmpl->dims;
    const float dx = p.x - e.body.position.x;
    const float dz = p.z - e.body.position.z;
    const float up = p.y - e.body.position.y;
    // Template axes: x = forward, y = side, z = up; anchor = bottom center.
    const float c = std::cos(e.body.yaw), s = std::sin(e.body.yaw);
    const float fwd = c * dx + s * dz;
    const float side = -s * dx + c * dz;

    const Int3 sub{ static_cast<int>(std::floor(fwd / kSubvoxelSize + static_cast<float>(dims.x) * 0.5f)),
                    static_cast<int>(std::floor(side / kSubvoxelSize + static_cast<float>(dims.y) * 0.5f)),
                    static_cast<int>(std::floor(up / kSubvoxelSize)) };
    // Template grids store height in z; partAt expects (x=fwd, y=side, z=up).
    if (e.tmpl->partAt({ sub.x, sub.y, sub.z }) < 0) return std::nullopt;
    return sub;
}

void Sim::recordPartEvents(VehicleEntity& target, const HitResult& hit, Vec3 at) {
    if (hit.partDestroyed) {
        SimEvent ev;
        ev.type = SimEvent::Type::PartDestroyed;
        ev.entity = target.id;
        ev.part = hit.partHit;
        ev.position = at;
        pending.push_back(ev);
    }
    if (hit.bleedPart >= 0 && !target.state.partAlive(hit.bleedPart)) {
        SimEvent ev;
        ev.type = SimEvent::Type::PartDestroyed;
        ev.entity = target.id;
        ev.part = hit.bleedPart;
        ev.position = at;
        pending.push_back(ev);
    }
    if (hit.vehicleDestroyed) {
        SimEvent ev;
        ev.type = SimEvent::Type::VehicleDestroyed;
        ev.entity = target.id;
        ev.position = at;
        pending.push_back(ev);
        // A dead power station releases its sector (§2.2).
        if (const auto it = stations.find(target.id); it != stations.end()) {
            sectors.erase(it->second);
            stations.erase(it);
        }
        // Vehicle-mass crater (§4.2): scale with sub-voxel volume.
        const float mass = static_cast<float>(target.tmpl->occupiedCount());
        applyBlast({ at, std::clamp(mass / 250.0f, 1.5f, 6.0f),
                     static_cast<int>(std::clamp(mass / 4.0f, 60.0f, 400.0f)),
                     DamageType::Explosive });
    }
    for (DropKind kind : hit.drops) {
        Pickup p;
        p.id = nextPickupId++;
        p.kind = kind;
        p.position = { at.x + rng.unit() * 2.0f - 1.0f, at.y + 0.5f,
                       at.z + rng.unit() * 2.0f - 1.0f };
        p.despawnTick = tickCount + kPickupLifetimeTicks;
        drops.push_back(p);
        SimEvent ev;
        ev.type = SimEvent::Type::PickupSpawned;
        ev.pickup = p.id;
        ev.kind = kind;
        ev.position = p.position;
        pending.push_back(ev);
    }
}

VehicleEntity* Sim::vehicleAt(Vec3 p, uint32_t exclude, Int3& subvoxelOut) {
    for (VehicleEntity& target : vehicles) {
        if (target.id == exclude || target.state.destroyed()) continue;
        const Int3 dims = target.tmpl->dims;
        const float horiz =
            static_cast<float>(std::max(dims.x, dims.y)) * kSubvoxelSize * 0.5f + 0.3f;
        const float height = static_cast<float>(dims.z) * kSubvoxelSize + 0.3f;
        if (std::abs(p.x - target.body.position.x) > horiz) continue;
        if (std::abs(p.z - target.body.position.z) > horiz) continue;
        const float up = p.y - target.body.position.y;
        if (up < -0.3f || up > height) continue;
        if (const std::optional<Int3> sub = worldToSubvoxel(target, p)) {
            subvoxelOut = *sub;
            return &target;
        }
    }
    return nullptr;
}

uint32_t Sim::launch(uint32_t shooter, Vec3 dir, float speed, const WeaponSpec& spec,
                     float gravityFactor) {
    VehicleEntity* src = find(shooter);
    if (!src || !src->state.operable() || src->ammo <= 0) return 0;
    if (!src->state.hasAlivePartOfType(PartType::Weapon)) return 0;
    --src->ammo;

    const Vec3 d = normalized(dir);
    const float muzzleUp = static_cast<float>(src->tmpl->dims.z) * kSubvoxelSize * 0.6f;
    Projectile p;
    p.id = nextProjectileId++;
    p.shooter = shooter;
    p.position = { src->body.position.x + d.x * 1.5f,
                   src->body.position.y + muzzleUp + d.y * 1.5f,
                   src->body.position.z + d.z * 1.5f };
    p.velocity = { d.x * speed, d.y * speed, d.z * speed };
    p.spec = spec;
    p.gravityFactor = gravityFactor;
    p.expireTick = tickCount + 10 * 60;
    ordnance.push_back(p);
    return p.id;
}

void Sim::stepProjectiles() {
    for (size_t i = 0; i < ordnance.size();) {
        Projectile& p = ordnance[i];
        bool impact = false;
        p.velocity.y -= kGravity * p.gravityFactor * kTickDt;

        // Sub-step so fast shells cannot tunnel through vehicles or walls.
        const float moveLen = std::sqrt(p.velocity.x * p.velocity.x
                                        + p.velocity.y * p.velocity.y
                                        + p.velocity.z * p.velocity.z)
                            * kTickDt;
        const int substeps = std::max(1, static_cast<int>(std::ceil(moveLen / 0.5f)));
        for (int s = 0; s < substeps && !impact; ++s) {
            p.position.x += p.velocity.x * kTickDt / static_cast<float>(substeps);
            p.position.y += p.velocity.y * kTickDt / static_cast<float>(substeps);
            p.position.z += p.velocity.z * kTickDt / static_cast<float>(substeps);

            Int3 sub;
            if (VehicleEntity* target = vehicleAt(p.position, p.shooter, sub)) {
                const HitResult hit =
                    target->state.applyHit(sub, p.spec.damage, p.spec.type, rng);
                if (hit.partHit >= 0) recordPartEvents(*target, hit, p.position);
                if (p.spec.blastRadius > 0.0f)
                    applyBlast({ p.position, p.spec.blastRadius,
                                 std::max(p.spec.blastDamage, p.spec.damage), p.spec.type });
                impact = true;
            } else if (materialInfo(voxels.at({ static_cast<int>(std::floor(p.position.x)),
                                                static_cast<int>(std::floor(p.position.y)),
                                                static_cast<int>(std::floor(p.position.z)) }))
                           .solid) {
                applyBlast({ p.position, std::max(p.spec.blastRadius, 0.75f),
                             std::max(p.spec.blastDamage, p.spec.damage), p.spec.type });
                impact = true;
            }
        }

        if (impact || p.expireTick <= tickCount) {
            ordnance.erase(ordnance.begin() + static_cast<long>(i));
        } else {
            ++i;
        }
    }
}

std::optional<HitscanResult> Sim::fire(uint32_t shooter, Vec3 dir, const WeaponSpec& spec) {
    VehicleEntity* src = find(shooter);
    if (!src || !src->state.operable() || src->ammo <= 0) return std::nullopt;
    if (!src->state.hasAlivePartOfType(PartType::Weapon)) return std::nullopt;
    --src->ammo;

    const Vec3 d = normalized(dir);
    const float muzzleUp = static_cast<float>(src->tmpl->dims.z) * kSubvoxelSize * 0.6f;
    const Vec3 origin{ src->body.position.x, src->body.position.y + muzzleUp,
                       src->body.position.z };

    HitscanResult result;
    const float stepLen = 0.125f; // half a sub-voxel: cannot tunnel through grids
    for (float t = 1.0f; t <= spec.range; t += stepLen) {
        const Vec3 p{ origin.x + d.x * t, origin.y + d.y * t, origin.z + d.z * t };

        Int3 sub;
        if (VehicleEntity* target = vehicleAt(p, shooter, sub)) {
            const HitResult hit = target->state.applyHit(sub, spec.damage, spec.type, rng);
            if (hit.partHit >= 0) {
                recordPartEvents(*target, hit, p);
                if (spec.blastRadius > 0.0f)
                    applyBlast({ p, spec.blastRadius,
                                 std::max(spec.blastDamage, spec.damage), spec.type });
                result.hitVehicle = true;
                result.entity = target->id;
                result.hit = hit;
                result.impact = p;
                return result;
            }
        }

        if (materialInfo(voxels.at({ static_cast<int>(std::floor(p.x)),
                                     static_cast<int>(std::floor(p.y)),
                                     static_cast<int>(std::floor(p.z)) }))
                .solid) {
            applyBlast({ p, std::max(spec.blastRadius, 0.75f),
                         std::max(spec.blastDamage, spec.damage), spec.type });
            result.hitTerrain = true;
            result.impact = p;
            return result;
        }
    }
    return result; // flew off into the distance
}

int Sim::sectorOwner(Int3 sector) const {
    const auto it = sectors.find({ sector.x, sector.z });
    return it == sectors.end() ? -1 : static_cast<int>(it->second);
}

uint32_t Sim::buildPowerStation(uint8_t team, Vec3 position) {
    const Int3 sector = sectorOf(position);
    if (sectorOwner(sector) >= 0) return 0;        // contested ground: clear it first
    if (energy[team & 3] < kPowerStationCost) return 0;
    energy[team & 3] -= kPowerStationCost;

    position.y = static_cast<float>(voxels.heightAt(static_cast<int>(std::floor(position.x)),
                                                    static_cast<int>(std::floor(position.z))));
    const uint32_t id = spawnVehicle(TemplateId::PowerStation, team, position, 0.0f);
    sectors[{ sector.x, sector.z }] = team;
    stations[id] = { sector.x, sector.z };
    return id;
}

uint32_t Sim::eject(uint32_t vehicleId) {
    VehicleEntity* v = find(vehicleId);
    if (!v || !v->hasPilot || v->state.destroyed()) return 0;
    if (v->tmpl->id == TemplateId::Pilot) return 0; // can't eject from yourself

    // Step out beside the left flank, snapped to the ground.
    const float side = static_cast<float>(v->tmpl->dims.y) * kSubvoxelSize * 0.5f + 1.0f;
    const float c = std::cos(v->body.yaw), s = std::sin(v->body.yaw);
    Vec3 pos{ v->body.position.x - s * side, 0.0f, v->body.position.z + c * side };
    pos.y = static_cast<float>(voxels.heightAt(static_cast<int>(std::floor(pos.x)),
                                               static_cast<int>(std::floor(pos.z))));

    v->hasPilot = false;
    v->input = {};
    const uint32_t pilotId = spawnVehicle(TemplateId::Pilot, v->team, pos, v->body.yaw);
    return pilotId; // note: spawn may invalidate v
}

bool Sim::board(uint32_t pilotId, uint32_t vehicleId) {
    VehicleEntity* pilot = find(pilotId);
    VehicleEntity* target = find(vehicleId);
    if (!pilot || !target || pilot->tmpl->id != TemplateId::Pilot) return false;
    if (pilot->state.destroyed() || target->state.destroyed()) return false;
    if (target->hasPilot) return false;
    if (target->tmpl->id == TemplateId::Pilot) return false;
    if (distance(pilot->body.position, target->body.position) > kBoardRange) return false;
    // A shot-out cockpit means there is nowhere to sit until it's repaired (§4.7).
    bool hasCockpit = false, cockpitAlive = false;
    for (size_t i = 0; i < target->tmpl->parts.size(); ++i) {
        if (target->tmpl->parts[i].type != PartType::Cockpit) continue;
        hasCockpit = true;
        cockpitAlive |= target->state.partAlive(static_cast<int>(i));
    }
    if (hasCockpit && !cockpitAlive) return false;

    target->hasPilot = true;
    target->input = {};
    for (size_t i = 0; i < vehicles.size(); ++i) {
        if (vehicles[i].id == pilotId) {
            vehicles.erase(vehicles.begin() + static_cast<long>(i));
            break;
        }
    }
    return true;
}

void Sim::applyBlast(const BlastEvent& e) {
    voxels.applyBlast(e);
    SimEvent ev;
    ev.type = SimEvent::Type::Blast;
    ev.blast = e;
    ev.position = e.center;
    pending.push_back(ev);

    // Splash vs vehicles: march from the blast center toward each vehicle and
    // damage the entry sub-voxel with linear falloff. Iterates by index —
    // chained death craters may grow the vector's event log but never the
    // vehicle list itself.
    if (e.damage <= 0 || e.radius <= 0.0f) return;
    for (size_t i = 0; i < vehicles.size(); ++i) {
        VehicleEntity& v = vehicles[i];
        if (v.state.destroyed()) continue;
        const Vec3 center{ v.body.position.x,
                           v.body.position.y
                               + static_cast<float>(v.tmpl->dims.z) * kSubvoxelSize * 0.5f,
                           v.body.position.z };
        const float bound =
            static_cast<float>(std::max({ v.tmpl->dims.x, v.tmpl->dims.y, v.tmpl->dims.z }))
            * kSubvoxelSize;
        const float centerDist = distance(e.center, center);
        if (centerDist > e.radius + bound) continue;

        const Vec3 dir = centerDist > 0.01f
                             ? Vec3{ (center.x - e.center.x) / centerDist,
                                     (center.y - e.center.y) / centerDist,
                                     (center.z - e.center.z) / centerDist }
                             : Vec3{ 1.0f, 0.0f, 0.0f };
        for (float t = 0.0f; t <= centerDist + 0.2f; t += 0.2f) {
            const Vec3 p{ e.center.x + dir.x * t, e.center.y + dir.y * t,
                          e.center.z + dir.z * t };
            if (const std::optional<Int3> sub = worldToSubvoxel(v, p)) {
                if (t > e.radius) break; // armor's outer skin is beyond the blast
                const float falloff = 1.0f - t / e.radius;
                const int damage = static_cast<int>(static_cast<float>(e.damage) * falloff);
                if (damage > 0) {
                    const HitResult hit = v.state.applyHit(*sub, damage, e.type, rng);
                    if (hit.partHit >= 0) recordPartEvents(v, hit, p);
                }
                break;
            }
        }
    }
}

void Sim::collectPickups() {
    for (size_t i = 0; i < drops.size();) {
        const Pickup& p = drops[i];
        bool collected = false;
        for (VehicleEntity& e : vehicles) {
            if (!e.state.operable()) continue;
            if (distance(p.position, { e.body.position.x, e.body.position.y + 1.0f,
                                       e.body.position.z })
                > kPickupRadius)
                continue;
            switch (p.kind) {
                case DropKind::AmmoCell:
                    e.ammo += 50;
                    break;
                case DropKind::RepairKit: {
                    const int part = e.state.lowestDamagedPart();
                    if (part >= 0 && e.state.partHpFraction(part) < 1.0f) {
                        const int max = e.tmpl->parts[static_cast<size_t>(part)].maxHp;
                        e.state.repairPart(part, std::max(1, max / 4)); // 25% (§4.4)
                    } else {
                        energy[e.team & 3] += 10; // banks as energy at full health
                    }
                    break;
                }
                case DropKind::EnergyShard:
                    energy[e.team & 3] += 25;
                    break;
            }
            SimEvent ev;
            ev.type = SimEvent::Type::PickupCollected;
            ev.pickup = p.id;
            ev.entity = e.id;
            ev.kind = p.kind;
            ev.position = p.position;
            pending.push_back(ev);
            collected = true;
            break;
        }
        if (collected || drops[i].despawnTick <= tickCount) {
            drops.erase(drops.begin() + static_cast<long>(i));
        } else {
            ++i;
        }
    }
}

void Sim::step() {
    ++tickCount;
    for (VehicleEntity& e : vehicles) {
        if (e.state.destroyed()) continue;
        const ControlInput in = e.hasPilot ? e.input : ControlInput{};
        switch (e.tmpl->locomotion) {
            case LocomotionClass::Tracked:
                stepTracked(e.body, in, e.state, voxels);
                break;
            case LocomotionClass::Jet: {
                const StepResult r = stepJet(e.body, in, e.state, voxels);
                if (r.collided) {
                    // Terrain crash: heavy core damage per tick in the dirt.
                    const HitResult hit = e.state.applyHit(
                        { e.tmpl->dims.x / 2, e.tmpl->dims.y / 2, e.tmpl->dims.z / 2 }, 150,
                        DamageType::Kinetic, rng);
                    if (hit.partHit >= 0) recordPartEvents(e, hit, e.body.position);
                }
                break;
            }
            case LocomotionClass::Walker:
                stepWalker(e.body, in, e.state, voxels);
                break;
            case LocomotionClass::Pilot:
                stepPilot(e.body, in, e.state, voxels);
                break;
            case LocomotionClass::Static:
                break; // buildings don't move
        }
    }
    stepProjectiles();
    collectPickups();

    // Sector income (§2.2): owned sectors pay out once a second.
    if (tickCount % kIncomeIntervalTicks == 0)
        for (const auto& [sector, team] : sectors) energy[team & 3] += kSectorIncome;
}

uint64_t Sim::stateHash() const {
    uint64_t h = 0xCBF29CE484222325ull;
    h = fnv(h, tickCount);
    h = fnv(h, voxels.contentHash());
    for (const VehicleEntity& e : vehicles) {
        h = fnv(h, e.id);
        h = fnv(h, e.team);
        h = fnv(h, static_cast<uint64_t>(e.tmpl->id));
        h = fnvF(h, e.body.position.x);
        h = fnvF(h, e.body.position.y);
        h = fnvF(h, e.body.position.z);
        h = fnvF(h, e.body.yaw);
        h = fnvF(h, e.body.speed);
        h = fnv(h, e.body.grounded ? 1 : 0);
        h = fnv(h, e.hasPilot ? 1 : 0);
        h = fnv(h, static_cast<uint64_t>(e.ammo));
        h = fnv(h, e.state.destroyed() ? 1 : 0);
        for (size_t i = 0; i < e.tmpl->parts.size(); ++i) {
            h = fnv(h, static_cast<uint64_t>(e.state.partHp(static_cast<int>(i))));
            h = fnv(h, e.state.partAlive(static_cast<int>(i)) ? 1 : 0);
        }
    }
    for (const Projectile& p : ordnance) {
        h = fnv(h, p.id);
        h = fnvF(h, p.position.x);
        h = fnvF(h, p.position.y);
        h = fnvF(h, p.position.z);
    }
    for (const Pickup& p : drops) {
        h = fnv(h, p.id);
        h = fnv(h, static_cast<uint64_t>(p.kind));
        h = fnvF(h, p.position.x);
        h = fnvF(h, p.position.y);
        h = fnvF(h, p.position.z);
    }
    for (int e : energy) h = fnv(h, static_cast<uint64_t>(e));
    for (const auto& [sector, team] : sectors) {
        h = fnv(h, static_cast<uint64_t>(static_cast<uint32_t>(sector.first)));
        h = fnv(h, static_cast<uint64_t>(static_cast<uint32_t>(sector.second)));
        h = fnv(h, team);
    }
    return h;
}

} // namespace vox
