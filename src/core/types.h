#pragma once
#include <cstdint>
#include <cmath>

namespace vox {

struct Int3 {
    int x = 0, y = 0, z = 0;
    bool operator==(const Int3&) const = default;
    Int3 operator+(const Int3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Int3 operator-(const Int3& o) const { return { x - o.x, y - o.y, z - o.z }; }
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

inline float distance(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace vox
