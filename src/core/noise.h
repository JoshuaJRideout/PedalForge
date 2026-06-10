#pragma once
#include <cstdint>
#include "core/rng.h"

namespace vox {

// Value noise + fBm for the heightfield base (DESIGN.md §3.3.2).
// NOTE: float-based — deterministic per seed on a given build, but cross-platform
// bit-exactness is not guaranteed yet. Acceptable for M0; the chunk-hash audit
// (§7.2) is the long-term safety net, and this can move to fixed-point if audits
// show drift between platforms.

inline float latticeValue(uint64_t seed, int x, int y) {
    return static_cast<float>(hashCoords(seed, x, y, 0) >> 40) / static_cast<float>(1ull << 24);
}

inline float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

inline float valueNoise2(uint64_t seed, float x, float y) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float tx = smoothstep(x - static_cast<float>(x0));
    const float ty = smoothstep(y - static_cast<float>(y0));
    const float v00 = latticeValue(seed, x0, y0);
    const float v10 = latticeValue(seed, x0 + 1, y0);
    const float v01 = latticeValue(seed, x0, y0 + 1);
    const float v11 = latticeValue(seed, x0 + 1, y0 + 1);
    const float a = v00 + (v10 - v00) * tx;
    const float b = v01 + (v11 - v01) * tx;
    return a + (b - a) * ty; // [0, 1)
}

inline float fbm2(uint64_t seed, float x, float y, int octaves, float lacunarity = 2.0f, float gain = 0.5f) {
    float sum = 0.0f, amp = 1.0f, freq = 1.0f, norm = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * valueNoise2(seed + static_cast<uint64_t>(i) * 0x9E37ull, x * freq, y * freq);
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    return sum / norm; // [0, 1)
}

} // namespace vox
