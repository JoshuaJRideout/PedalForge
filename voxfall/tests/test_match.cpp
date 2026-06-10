#include "test_framework.h"
#include "game/match.h"

using namespace vox;

namespace {
VoxelWorld battleground() {
    VoxelWorld w({ 160, 48, 160 });
    for (int x = 0; x < 160; ++x)
        for (int z = 0; z < 160; ++z)
            for (int y = 0; y < 10; ++y) w.set({ x, y, z }, Material::Rock);
    return w;
}

MapMeta twoSpawns() {
    MapMeta meta;
    meta.spawns = { { { 35, 10, 80 }, 0 }, { { 125, 10, 80 }, 1 } };
    return meta;
}

int runMatch(uint64_t seed, uint64_t* finalHash = nullptr, uint64_t* endTick = nullptr) {
    Match m(battleground(), twoSpawns(), seed);
    const uint64_t maxTicks = 12ull * 60 * 60; // 12 sim-minutes
    while (!m.finished() && m.sim().tick() < maxTicks) m.tick();
    if (finalHash) *finalHash = m.sim().stateHash();
    if (endTick) *endTick = m.sim().tick();
    return m.winner();
}
} // namespace

TEST(match_bot_vs_bot_resolves_with_a_winner) {
    uint64_t tick = 0;
    const int winner = runMatch(42, nullptr, &tick);
    CHECK(winner == 0 || winner == 1);   // somebody actually won
    CHECK(tick < 12ull * 60 * 60);       // ...before the stalemate cap
}

TEST(match_is_deterministic) {
    uint64_t hashA = 0, hashB = 0;
    const int winnerA = runMatch(7, &hashA);
    const int winnerB = runMatch(7, &hashB);
    CHECK(winnerA == winnerB);
    CHECK(hashA == hashB); // entire battle byte-identical
}

TEST(match_commanders_expand_economy) {
    Match m(battleground(), twoSpawns(), 11);
    for (int t = 0; t < 60 * 30 && !m.finished(); ++t) m.tick(); // 30 s
    // Both teams should have claimed sectors near home by now.
    int owned0 = 0, owned1 = 0;
    for (int sx = 0; sx < 10; ++sx)
        for (int sz = 0; sz < 10; ++sz) {
            const int owner = m.sim().sectorOwner({ sx, 0, sz });
            if (owner == 0) ++owned0;
            if (owner == 1) ++owned1;
        }
    CHECK(owned0 >= 1);
    CHECK(owned1 >= 1);
}

TEST(match_loser_forces_detonate) {
    Match m(battleground(), twoSpawns(), 42);
    const uint64_t maxTicks = 12ull * 60 * 60;
    while (!m.finished() && m.sim().tick() < maxTicks) m.tick();
    CHECK(m.finished());
    const int loser = 1 - m.winner();
    for (const VehicleEntity& e : m.sim().entities())
        if (e.team == loser) CHECK(e.state.destroyed()); // host rule (§2.1)
}
