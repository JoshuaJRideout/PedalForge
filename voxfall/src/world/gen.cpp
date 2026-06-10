#include "world/gen.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include "core/noise.h"
#include "core/rng.h"

namespace vox {

namespace {

void layerColumn(VoxelWorld& w, int x, int z, int h, int seaLevel) {
    h = std::clamp(h, 3, w.size().y - 1);
    for (int y = 0; y < h; ++y) {
        Material m = Material::Rock;
        if (y < 2) m = Material::Bedrock;
        else if (y >= h - 3) m = Material::Soil;
        w.set({ x, y, z }, m);
    }
    for (int y = h; y < seaLevel; ++y) w.set({ x, y, z }, Material::Water);
    for (int y = std::max(h, seaLevel); y < w.size().y; ++y) w.set({ x, y, z }, Material::Air);
}

int biomeHeight(const ArenaParams& p, int x, int z, int baseHeight, int amplitude) {
    const float fx = static_cast<float>(x), fz = static_cast<float>(z);
    switch (p.biome) {
        case Biome::Dunes:
            return baseHeight
                 + static_cast<int>(fbm2(p.seed, fx / 48.0f, fz / 48.0f, 4)
                                    * static_cast<float>(amplitude));
        case Biome::Canyons: {
            // Ridged noise: sharp crests, flat-ish floors.
            const float n = fbm2(p.seed, fx / 64.0f, fz / 64.0f, 4);
            const float ridge = 1.0f - std::abs(n * 2.0f - 1.0f);
            return baseHeight + static_cast<int>(ridge * ridge * static_cast<float>(amplitude) * 1.8f);
        }
        case Biome::Archipelago: {
            // fBm clusters near 0.5: spread the *deviation* so islands rise
            // out of a sea that covers roughly half the map.
            const float n = fbm2(p.seed, fx / 56.0f, fz / 56.0f, 4);
            return baseHeight + 2
                 + static_cast<int>((n - 0.5f) * static_cast<float>(amplitude) * 2.6f);
        }
        case Biome::ShatteredCity:
            // Gentle plain: the buildings carry the verticality.
            return baseHeight
                 + static_cast<int>(fbm2(p.seed, fx / 80.0f, fz / 80.0f, 3) * 6.0f);
    }
    return baseHeight;
}

void hardrockRibs(VoxelWorld& w, uint64_t seed, float threshold) {
    const Int3 size = w.size();
    for (int z = 0; z < size.z; ++z) {
        for (int x = 0; x < size.x; ++x) {
            const float rib = fbm2(seed ^ 0xB07E5ull, static_cast<float>(x) / 96.0f,
                                   static_cast<float>(z) / 96.0f, 3);
            if (rib < threshold) continue;
            const int top = w.heightAt(x, z) - 6;
            for (int y = 2; y < top; ++y)
                if (w.at({ x, y, z }) == Material::Rock) w.set({ x, y, z }, Material::Hardrock);
        }
    }
}

void crystals(VoxelWorld& w, uint64_t seed) {
    const Int3 size = w.size();
    for (int z = 0; z < size.z; ++z)
        for (int x = 0; x < size.x; ++x)
            for (int y = 2; y < size.y; ++y)
                if (w.at({ x, y, z }) == Material::Rock
                    && (hashCoords(seed ^ 0xC57A1ull, x / 3, y / 3, z / 3) & 0x3FF) == 0)
                    w.set({ x, y, z }, Material::Crystal);
}

// Ruin grammar (§3.3.4): hollow concrete shells with floor slabs, door gaps,
// and an erosion pass. They stand on terrain, so structural integrity (§5.3)
// makes them collapsible in play.
void placeBuilding(VoxelWorld& w, Rng& rng, int bx, int bz, int width, int depth, int floors) {
    const int ground = w.heightAt(bx + width / 2, bz + depth / 2);
    const int floorHeight = 4;
    const int top = std::min(ground + floors * floorHeight, w.size().y - 2);

    for (int x = bx; x < bx + width; ++x) {
        for (int z = bz; z < bz + depth; ++z) {
            const bool wall = x == bx || x == bx + width - 1 || z == bz || z == bz + depth - 1;
            for (int y = ground; y < top; ++y) {
                const bool slab = (y - ground) % floorHeight == 0;
                if (wall || slab) w.set({ x, y, z }, Material::Concrete);
            }
        }
    }
    // Door gaps on two sides.
    const int doorX = bx + 2 + static_cast<int>(rng.range(static_cast<uint32_t>(width - 4)));
    const int doorZ = bz + 2 + static_cast<int>(rng.range(static_cast<uint32_t>(depth - 4)));
    for (int y = ground + 1; y < ground + 3; ++y) {
        w.set({ doorX, y, bz }, Material::Air);
        w.set({ doorX + 1, y, bz }, Material::Air);
        w.set({ bx, y, doorZ }, Material::Air);
        w.set({ bx, y, doorZ + 1 }, Material::Air);
    }
    // Erosion: some buildings are already half-ruined.
    if (rng.chance(0.4f)) {
        const int bites = 2 + static_cast<int>(rng.range(4));
        for (int i = 0; i < bites; ++i) {
            const float ex = static_cast<float>(bx) + rng.unit() * static_cast<float>(width);
            const float ez = static_cast<float>(bz) + rng.unit() * static_cast<float>(depth);
            const float ey = static_cast<float>(ground) + rng.unit() * static_cast<float>(top - ground);
            w.applyBlast({ { ex, ey, ez }, 2.0f + rng.unit() * 2.5f, 400,
                           DamageType::Explosive });
        }
    }
}

void cityPass(VoxelWorld& w, const ArenaParams& p) {
    Rng cityRng(p.seed ^ 0xC177ull);
    const Int3 size = w.size();
    const int cell = 26;
    for (int bz = 16; bz + 20 < size.z - 16; bz += cell) {
        for (int bx = 16; bx + 20 < size.x - 16; bx += cell) {
            if ((hashCoords(p.seed ^ 0xB17Dull, bx / cell, 0, bz / cell) & 3) == 0)
                continue; // empty lot
            const int width = 8 + static_cast<int>(cityRng.range(9));
            const int depth = 8 + static_cast<int>(cityRng.range(9));
            const int floors = 2 + static_cast<int>(cityRng.range(4));
            placeBuilding(w, cityRng, bx, bz, width, depth, floors);
        }
    }
}

int waterDepthAt(const VoxelWorld& w, int x, int z) {
    return std::max(0, w.seaLevel() - w.heightAt(x, z));
}

// Carve a tank-passable ramped corridor between two points (§3.3.5).
void carveCorridor(VoxelWorld& w, Int3 a, Int3 b) {
    const int steps = std::max(std::abs(b.x - a.x), std::abs(b.z - a.z));
    if (steps == 0) return;
    const int hA = w.heightAt(a.x, a.z), hB = w.heightAt(b.x, b.z);
    int prev = hA;
    for (int s = 0; s <= steps; ++s) {
        const float t = static_cast<float>(s) / static_cast<float>(steps);
        const int x = a.x + static_cast<int>(std::round(t * static_cast<float>(b.x - a.x)));
        const int z = a.z + static_cast<int>(std::round(t * static_cast<float>(b.z - a.z)));
        int target = hA + static_cast<int>(std::round(t * static_cast<float>(hB - hA)));
        target = std::clamp(target, prev - 1, prev + 1); // rampable
        target = std::max(target, w.seaLevel() - 1);     // causeway over deep water
        for (int dx = -2; dx <= 2; ++dx) {
            for (int dz = -2; dz <= 2; ++dz) {
                const int cx = x + dx, cz = z + dz;
                if (cx < 0 || cz < 0 || cx >= w.size().x || cz >= w.size().z) continue;
                const int h = w.heightAt(cx, cz);
                if (h > target)
                    for (int y = target; y < w.size().y; ++y) w.set({ cx, y, cz }, Material::Air);
                else
                    for (int y = std::max(2, h); y < target; ++y)
                        w.set({ cx, y, cz }, Material::Rock);
            }
        }
        prev = target;
    }
}

} // namespace

MapMeta generateArena(VoxelWorld& world, const ArenaParams& p) {
    const Int3 size = world.size();
    const int baseHeight = size.y / 3;
    const int amplitude = size.y / 3;
    const int seaLevel = p.biome == Biome::Archipelago ? baseHeight + 4 : baseHeight - 2;
    world.setSeaLevel(seaLevel);

    for (int z = 0; z < size.z; ++z)
        for (int x = 0; x < size.x; ++x)
            layerColumn(world, x, z, biomeHeight(p, x, z, baseHeight, amplitude), seaLevel);

    hardrockRibs(world, p.seed, p.biome == Biome::Canyons ? 0.30f : 0.38f);
    crystals(world, p.seed);
    if (p.biome == Biome::ShatteredCity) cityPass(world, p);

    // Spawns on a ring, route-carved in a cycle so all are ground-connected.
    MapMeta meta;
    meta.name = "arena";
    const float cx = static_cast<float>(size.x) * 0.5f;
    const float cz = static_cast<float>(size.z) * 0.5f;
    const float radius = static_cast<float>(std::min(size.x, size.z)) * 0.36f;
    for (int t = 0; t < p.teams; ++t) {
        const float angle = 6.2831853f * static_cast<float>(t) / static_cast<float>(p.teams);
        const int sx = static_cast<int>(cx + std::cos(angle) * radius);
        const int sz = static_cast<int>(cz + std::sin(angle) * radius);
        // Flatten a small spawn pad above sea level.
        const int pad = std::max(world.heightAt(sx, sz), seaLevel + 1);
        for (int dx = -4; dx <= 4; ++dx)
            for (int dz = -4; dz <= 4; ++dz) {
                const int h = world.heightAt(sx + dx, sz + dz);
                for (int y = h; y < pad; ++y) world.set({ sx + dx, y, sz + dz }, Material::Rock);
                for (int y = pad; y < size.y; ++y) world.set({ sx + dx, y, sz + dz }, Material::Air);
            }
        meta.spawns.push_back({ { sx, world.heightAt(sx, sz), sz }, static_cast<uint8_t>(t) });
    }
    for (size_t i = 0; i + 1 < meta.spawns.size(); ++i)
        carveCorridor(world, meta.spawns[i].position, meta.spawns[i + 1].position);
    if (meta.spawns.size() > 2)
        carveCorridor(world, meta.spawns.back().position, meta.spawns.front().position);
    return meta;
}

bool validateArena(const VoxelWorld& world, const MapMeta& meta) {
    if (meta.spawns.size() < 2) return true;
    const Int3 size = world.size();
    std::vector<uint8_t> visited(static_cast<size_t>(size.x) * size.z, 0);
    auto idx = [&](int x, int z) { return static_cast<size_t>(z) * size.x + x; };

    const Int3 start = meta.spawns[0].position;
    std::queue<std::pair<int, int>> frontier;
    frontier.push({ start.x, start.z });
    visited[idx(start.x, start.z)] = 1;
    while (!frontier.empty()) {
        const auto [x, z] = frontier.front();
        frontier.pop();
        const int h = world.heightAt(x, z);
        const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
        for (const auto& d : dirs) {
            const int nx = x + d[0], nz = z + d[1];
            if (nx < 0 || nz < 0 || nx >= size.x || nz >= size.z) continue;
            if (visited[idx(nx, nz)]) continue;
            const int nh = world.heightAt(nx, nz);
            if (nh - h > 1) continue;                  // too steep for tanks
            if (waterDepthAt(world, nx, nz) > 2) continue; // too deep to ford
            visited[idx(nx, nz)] = 1;
            frontier.push({ nx, nz });
        }
    }
    for (const MapSpawn& s : meta.spawns)
        if (!visited[idx(s.position.x, s.position.z)]) return false;
    return true;
}

} // namespace vox
