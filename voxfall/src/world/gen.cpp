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
    // Window grid: per floor, punched through every facade (skip corners).
    for (int floor = 0; floor < floors; ++floor) {
        const int wy = ground + floor * floorHeight + 2;
        if (wy + 1 >= top) break;
        for (int x = bx + 2; x < bx + width - 2; x += 3) {
            w.set({ x, wy, bz }, Material::Air);
            w.set({ x, wy + 1, bz }, Material::Air);
            w.set({ x, wy, bz + depth - 1 }, Material::Air);
            w.set({ x, wy + 1, bz + depth - 1 }, Material::Air);
        }
        for (int z = bz + 2; z < bz + depth - 2; z += 3) {
            w.set({ bx, wy, z }, Material::Air);
            w.set({ bx, wy + 1, z }, Material::Air);
            w.set({ bx + width - 1, wy, z }, Material::Air);
            w.set({ bx + width - 1, wy + 1, z }, Material::Air);
        }
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
            const uint64_t lot = hashCoords(p.seed ^ 0xB17Dull, bx / cell, 0, bz / cell);
            if ((lot & 3) == 0) continue; // empty lot (park: the tree pass fills it)
            // Real-world block mix: slabs, setback towers, warehouses.
            const int style = static_cast<int>((lot >> 2) % 10);
            if (style < 5) { // residential slab
                const int width = 8 + static_cast<int>(cityRng.range(9));
                const int depth = 8 + static_cast<int>(cityRng.range(9));
                const int floors = 2 + static_cast<int>(cityRng.range(4));
                placeBuilding(w, cityRng, bx, bz, width, depth, floors);
            } else if (style < 8) { // office tower with setback + antenna
                const int width = 8 + static_cast<int>(cityRng.range(4));
                const int depth = 8 + static_cast<int>(cityRng.range(4));
                const int floors = 5 + static_cast<int>(cityRng.range(4));
                placeBuilding(w, cityRng, bx, bz, width, depth, floors);
                // Setback crown: a smaller block on top, then a mast.
                const int ground = w.heightAt(bx + width / 2, bz + depth / 2);
                const int crownBase = ground; // heightAt now includes the roof
                const int cw = std::max(4, width - 4), cd = std::max(4, depth - 4);
                for (int x = bx + 2; x < bx + 2 + cw; ++x)
                    for (int z = bz + 2; z < bz + 2 + cd; ++z) {
                        const bool wall = x == bx + 2 || x == bx + 1 + cw || z == bz + 2
                                       || z == bz + 1 + cd;
                        for (int y = crownBase; y < std::min(crownBase + 4, w.size().y - 3); ++y)
                            if (wall || y == crownBase + 3)
                                w.set({ x, y, z }, Material::Concrete);
                    }
                const int mastX = bx + width / 2, mastZ = bz + depth / 2;
                for (int y = crownBase + 4; y < std::min(crownBase + 8, w.size().y - 1); ++y)
                    w.set({ mastX, y, mastZ }, Material::Metal);
            } else { // warehouse: wide, one tall hall, skylight strips
                const int width = 14 + static_cast<int>(cityRng.range(5));
                const int depth = 10 + static_cast<int>(cityRng.range(5));
                const int ground = w.heightAt(bx + width / 2, bz + depth / 2);
                const int top = std::min(ground + 6, w.size().y - 2);
                for (int x = bx; x < bx + width; ++x)
                    for (int z = bz; z < bz + depth; ++z) {
                        const bool wall = x == bx || x == bx + width - 1 || z == bz
                                       || z == bz + depth - 1;
                        for (int y = ground; y < top; ++y)
                            if (wall || y == top - 1) w.set({ x, y, z }, Material::Metal);
                    }
                for (int x = bx + 3; x < bx + width - 3; x += 4) // skylights
                    for (int z = bz + 2; z < bz + depth - 2; ++z)
                        w.set({ x, top - 1, z }, Material::Air);
                // Big door.
                for (int y = ground; y < ground + 4; ++y)
                    for (int z = bz + depth / 2 - 2; z < bz + depth / 2 + 2; ++z)
                        w.set({ bx, y, z }, Material::Air);
            }
        }
    }
}

int waterDepthAt(const VoxelWorld& w, int x, int z) {
    return std::max(0, w.seaLevel() - w.heightAt(x, z));
}

// Tree scatter (the concept-art greenery): trunk + leaf crown on dry soil.
// Runs before spawn pads and corridors, which clear their own ground.
void treePass(VoxelWorld& w, const ArenaParams& p, float density) {
    const Int3 size = w.size();
    for (int z = 4; z < size.z - 4; ++z) {
        for (int x = 4; x < size.x - 4; ++x) {
            const uint32_t threshold = static_cast<uint32_t>(density * 1024.0f);
            if (hashCoords(p.seed ^ 0x7AEE5ull, x, 0, z) % 1024 >= threshold) continue;
            const int ground = w.heightAt(x, z);
            if (ground <= w.seaLevel() + 1 || ground + 9 >= size.y) continue;
            if (w.at({ x, ground - 1, z }) != Material::Soil) continue;

            const uint64_t h = hashCoords(p.seed ^ 0x73EEull, x, 1, z);
            const int trunk = 3 + static_cast<int>(h % 3);
            for (int y = ground; y < ground + trunk; ++y) w.set({ x, y, z }, Material::Wood);
            // Leaf crown: stacked discs, widest at the bottom.
            const int crownBase = ground + trunk;
            for (int layer = 0; layer < 4; ++layer) {
                const int r = layer < 2 ? 2 : 1;
                for (int dx = -r; dx <= r; ++dx)
                    for (int dz = -r; dz <= r; ++dz) {
                        if (std::abs(dx) == r && std::abs(dz) == r && r > 1) continue;
                        const Int3 cell{ x + dx, crownBase + layer, z + dz };
                        if (w.inBounds(cell) && w.at(cell) == Material::Air)
                            w.set(cell, Material::Foliage);
                    }
            }
        }
    }
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
    // Greenery: parks between city ruins, scattered woods elsewhere.
    treePass(world, p,
             p.biome == Biome::ShatteredCity ? 0.012f
             : p.biome == Biome::Canyons     ? 0.006f
                                             : 0.010f);

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
