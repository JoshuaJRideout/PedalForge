#include "vehicle/vehicle.h"
#include <algorithm>
#include <cmath>
#include <queue>

namespace vox {

int VehicleTemplate::addPart(std::string partName, PartType type, int maxHp, float armorMul) {
    parts.push_back({ std::move(partName), type, maxHp, armorMul });
    if (type == PartType::Hull && corePart < 0) corePart = static_cast<int>(parts.size()) - 1;
    return static_cast<int>(parts.size()) - 1;
}

void VehicleTemplate::fillBox(Int3 min, Int3 max, int part) {
    for (int z = min.z; z < max.z; ++z)
        for (int y = min.y; y < max.y; ++y)
            for (int x = min.x; x < max.x; ++x)
                partIndex[index({ x, y, z })] = static_cast<uint8_t>(part);
}

size_t VehicleTemplate::occupiedCount() const {
    return static_cast<size_t>(
        std::count_if(partIndex.begin(), partIndex.end(),
                      [](uint8_t v) { return v != kEmptySubvoxel; }));
}

void VehicleTemplate::finalize() {
    adjacency.assign(parts.size(), {});
    const Int3 steps[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
    for (int z = 0; z < dims.z; ++z) {
        for (int y = 0; y < dims.y; ++y) {
            for (int x = 0; x < dims.x; ++x) {
                const int a = partAt({ x, y, z });
                if (a < 0) continue;
                for (const Int3& s : steps) {
                    const int b = partAt(Int3{ x, y, z } + s);
                    if (b < 0 || b == a) continue;
                    adjacency[static_cast<size_t>(a)].insert(b);
                    adjacency[static_cast<size_t>(b)].insert(a);
                }
            }
        }
    }
}

Vehicle::Vehicle(const VehicleTemplate& tmpl) : templ(&tmpl) {
    parts.resize(tmpl.parts.size());
    for (size_t i = 0; i < parts.size(); ++i) parts[i].hp = tmpl.parts[i].maxHp;
}

HitResult Vehicle::applyHit(Int3 subvoxel, int damage, DamageType type, Rng& rng) {
    HitResult result;
    if (dead || damage <= 0) return result;

    int part = templ->partAt(subvoxel);
    if (part < 0) return result; // passed through empty space

    // Hits on already-destroyed (detached) parts strike the nearest connected
    // alive part instead — the airframe behind the missing wing.
    if (parts[static_cast<size_t>(part)].destroyed) {
        part = nearestAlivePart(part);
        if (part < 0) return result;
    }

    const PartDef& def = templ->parts[static_cast<size_t>(part)];
    // Energy weapons ignore 50% of armor (DESIGN.md §5.1).
    float armor = def.armorMul;
    if (type == DamageType::Energy) armor = (armor + 1.0f) * 0.5f;
    const int effective = std::max(1, static_cast<int>(
        std::lround(static_cast<double>(damage) * static_cast<double>(armor))));

    result.partHit = part;
    result.damageApplied = effective;
    damagePart(part, effective, rng, result);
    return result;
}

void Vehicle::damagePart(int part, int amount, Rng& rng, HitResult& out) {
    PartState& state = parts[static_cast<size_t>(part)];
    state.hp -= amount;
    if (state.hp > 0) return;

    const int overkill = -state.hp;
    state.hp = 0;
    state.destroyed = true;
    out.partDestroyed = true;

    const PartDef& def = templ->parts[static_cast<size_t>(part)];
    const bool isCore = (part == templ->corePart);
    rollDrops(def, isCore, rng, out.drops);

    if (isCore) {
        dead = true;
        out.vehicleDestroyed = true;
        return;
    }

    // Overkill bleeds 50% into the nearest connected alive part (§4.2).
    if (overkill > 0) {
        const int neighbor = nearestAlivePart(part);
        if (neighbor >= 0) {
            const int bleed = overkill / 2;
            if (bleed > 0) {
                out.bleedPart = neighbor;
                out.bleedDamage = bleed;
                // Single-step bleed: a cascading chain through bleed alone is
                // intentionally not possible; the neighbor just loses HP.
                PartState& ns = parts[static_cast<size_t>(neighbor)];
                ns.hp = std::max(0, ns.hp - bleed);
                if (ns.hp == 0 && !ns.destroyed) {
                    ns.destroyed = true;
                    rollDrops(templ->parts[static_cast<size_t>(neighbor)],
                              neighbor == templ->corePart, rng, out.drops);
                    if (neighbor == templ->corePart) {
                        dead = true;
                        out.vehicleDestroyed = true;
                    }
                }
            }
        }
    }
}

int Vehicle::nearestAlivePart(int from) const {
    std::vector<bool> visited(parts.size(), false);
    std::queue<int> frontier;
    frontier.push(from);
    visited[static_cast<size_t>(from)] = true;
    while (!frontier.empty()) {
        const int current = frontier.front();
        frontier.pop();
        // Deterministic order: std::set iterates ascending part index.
        for (int next : templ->adjacency[static_cast<size_t>(current)]) {
            if (visited[static_cast<size_t>(next)]) continue;
            if (!parts[static_cast<size_t>(next)].destroyed) return next;
            visited[static_cast<size_t>(next)] = true;
            frontier.push(next);
        }
    }
    return -1;
}

void Vehicle::rollDrops(const PartDef& def, bool isCore, Rng& rng, std::vector<DropKind>& out) {
    // Drop tables per DESIGN.md §4.4.
    if (def.type == PartType::Weapon && rng.chance(0.60f)) out.push_back(DropKind::AmmoCell);
    if ((def.type == PartType::Hull || def.type == PartType::Engine) && rng.chance(0.40f))
        out.push_back(DropKind::RepairKit);
    if (rng.chance(0.25f)) out.push_back(DropKind::EnergyShard);
    if (isCore) out.push_back(DropKind::EnergyShard); // whole-vehicle kill: bigger roll
}

bool Vehicle::isHusk() const {
    if (dead) return false;
    bool hasCockpit = false;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (templ->parts[i].type != PartType::Cockpit) continue;
        hasCockpit = true;
        if (!parts[i].destroyed) return false;
    }
    return hasCockpit;
}

bool Vehicle::hasAlivePartOfType(PartType type) const {
    return alivePartCountOfType(type) > 0;
}

int Vehicle::alivePartCountOfType(PartType type) const {
    int count = 0;
    for (size_t i = 0; i < parts.size(); ++i)
        if (!parts[i].destroyed && templ->parts[i].type == type) ++count;
    return count;
}

float Vehicle::partHpFraction(int part) const {
    const int max = templ->parts[static_cast<size_t>(part)].maxHp;
    return max <= 0 ? 0.0f : static_cast<float>(parts[static_cast<size_t>(part)].hp)
                           / static_cast<float>(max);
}

} // namespace vox
