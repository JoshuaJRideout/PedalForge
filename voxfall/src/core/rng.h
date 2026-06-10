#pragma once
#include <cstdint>

namespace vox {

// Deterministic splitmix64 stream. All gameplay randomness (drops, worldgen
// scatter) must flow through seeded Rng instances so server and clients agree.
class Rng {
public:
    explicit Rng(uint64_t seed) : state(seed) {}

    uint64_t next() {
        state += 0x9E3779B97F4A7C15ull;
        uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    // Uniform in [0, n)
    uint32_t range(uint32_t n) { return n == 0 ? 0 : static_cast<uint32_t>(next() % n); }

    // Uniform in [0, 1)
    float unit() { return static_cast<float>(next() >> 40) / static_cast<float>(1ull << 24); }

    // Bernoulli with probability p
    bool chance(float p) { return unit() < p; }

private:
    uint64_t state;
};

// Stateless coordinate hash for worldgen scatter (crystal veins etc.).
inline uint64_t hashCoords(uint64_t seed, int x, int y, int z) {
    uint64_t h = seed;
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(x)) * 0x9E3779B97F4A7C15ull;
    h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ull;
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(y)) * 0xC2B2AE3D27D4EB4Full;
    h = (h ^ (h >> 27)) * 0x94D049BB133111EBull;
    h ^= static_cast<uint64_t>(static_cast<uint32_t>(z)) * 0x165667B19E3779F9ull;
    return h ^ (h >> 31);
}

} // namespace vox
