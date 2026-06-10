#include "render/raycast.h"
#include <algorithm>
#include <cmath>

namespace vox {

namespace {

struct Rgb {
    float r, g, b;
};

Rgb materialColor(Material m) {
    switch (m) {
        case Material::Soil:     return { 139, 110, 72 };
        case Material::Rock:     return { 128, 128, 132 };
        case Material::Hardrock: return { 72, 70, 84 };
        case Material::Concrete: return { 172, 168, 160 };
        case Material::Metal:    return { 150, 160, 175 };
        case Material::Water:    return { 48, 110, 180 };
        case Material::Crystal:  return { 120, 230, 210 };
        case Material::Bedrock:  return { 30, 30, 34 };
        case Material::Wood:     return { 112, 80, 48 };
        case Material::Foliage:  return { 64, 138, 56 };
        default:                 return { 255, 0, 255 };
    }
}

Rgb sky(float dirY) {
    const float t = std::clamp(dirY * 0.5f + 0.5f, 0.0f, 1.0f);
    return { 210 + (140 - 210) * t, 226 + (185 - 226) * t, 240 + (235 - 240) * t };
}

Vec3 normalize3(Vec3 v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return { v.x / len, v.y / len, v.z / len };
}

Vec3 cross3(Vec3 a, Vec3 b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

// Amanatides & Woo voxel traversal over a unit grid. solidAt(cell) -> hit id
// (< 0 = empty). Returns hit id, entry-face axis (0/1/2) and distance.
template <typename SolidFn>
int ddaTrace(Vec3 origin, Vec3 dir, float maxDist, SolidFn solidAt, int& faceAxis,
             float& dist) {
    Int3 cell{ static_cast<int>(std::floor(origin.x)), static_cast<int>(std::floor(origin.y)),
               static_cast<int>(std::floor(origin.z)) };
    const float* d = &dir.x;
    int* c = &cell.x;
    int step[3];
    float tMax[3], tDelta[3];
    for (int i = 0; i < 3; ++i) {
        const float di = d[i];
        step[i] = di > 0 ? 1 : -1;
        tDelta[i] = di != 0.0f ? std::abs(1.0f / di) : 1e30f;
        const float o = (&origin.x)[i];
        const float boundary = di > 0 ? std::floor(o) + 1.0f : std::floor(o);
        tMax[i] = di != 0.0f ? (boundary - o) / di : 1e30f;
    }
    float t = 0.0f;
    faceAxis = 1;
    while (t < maxDist) {
        const int hit = solidAt(cell);
        if (hit >= 0) {
            dist = t;
            return hit;
        }
        const int axis = tMax[0] < tMax[1] ? (tMax[0] < tMax[2] ? 0 : 2)
                                           : (tMax[1] < tMax[2] ? 1 : 2);
        t = tMax[axis];
        tMax[axis] += tDelta[axis];
        c[axis] += step[axis];
        faceAxis = axis;
    }
    return -1;
}

template <typename SolidFn, typename ColorFn>
Image renderGrid(const PreviewCamera& cam, int width, int height, float maxDist,
                 SolidFn solidAt, ColorFn colorOf) {
    Image img(width, height);
    const Vec3 forward = normalize3({ cam.lookAt.x - cam.position.x,
                                      cam.lookAt.y - cam.position.y,
                                      cam.lookAt.z - cam.position.z });
    const Vec3 right = normalize3(cross3(forward, { 0, 1, 0 }));
    const Vec3 up = cross3(right, forward);
    const float tanHalf = std::tan(cam.fovDegrees * 3.14159265f / 360.0f);
    const float aspect = static_cast<float>(width) / static_cast<float>(height);

    for (int py = 0; py < height; ++py) {
        for (int px = 0; px < width; ++px) {
            const float u = (2.0f * (static_cast<float>(px) + 0.5f) / width - 1.0f) * tanHalf
                          * aspect;
            const float v = (1.0f - 2.0f * (static_cast<float>(py) + 0.5f) / height) * tanHalf;
            const Vec3 dir = normalize3({ forward.x + right.x * u + up.x * v,
                                          forward.y + right.y * u + up.y * v,
                                          forward.z + right.z * u + up.z * v });
            int faceAxis = 1;
            float dist = 0.0f;
            const int hit = ddaTrace(cam.position, dir, maxDist, solidAt, faceAxis, dist);

            Rgb c;
            if (hit < 0) {
                c = sky(dir.y);
            } else {
                c = colorOf(hit);
                // Face lighting: tops bright, x-faces medium, z-faces dimmer.
                const float light = faceAxis == 1 ? (dir.y < 0 ? 1.0f : 0.45f)
                                  : faceAxis == 0 ? 0.8f
                                                  : 0.62f;
                const float fog = std::exp(-dist * 0.0035f);
                const Rgb skyC = sky(dir.y);
                c = { (c.r * light) * fog + skyC.r * (1 - fog),
                      (c.g * light) * fog + skyC.g * (1 - fog),
                      (c.b * light) * fog + skyC.b * (1 - fog) };
            }
            img.put(px, py, static_cast<uint8_t>(std::clamp(c.r, 0.0f, 255.0f)),
                    static_cast<uint8_t>(std::clamp(c.g, 0.0f, 255.0f)),
                    static_cast<uint8_t>(std::clamp(c.b, 0.0f, 255.0f)));
        }
    }
    return img;
}

Image downsample2x(const Image& src) {
    Image out(src.width / 2, src.height / 2);
    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) {
            int sum[3] = { 0, 0, 0 };
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    const size_t i =
                        (static_cast<size_t>(y * 2 + dy) * src.width + (x * 2 + dx)) * 3;
                    sum[0] += src.rgb[i];
                    sum[1] += src.rgb[i + 1];
                    sum[2] += src.rgb[i + 2];
                }
            out.put(x, y, static_cast<uint8_t>(sum[0] / 4), static_cast<uint8_t>(sum[1] / 4),
                    static_cast<uint8_t>(sum[2] / 4));
        }
    }
    return out;
}

} // namespace

Image renderWorld(const VoxelWorld& world, const PreviewCamera& cam, int width, int height) {
    return renderGrid(
        cam, width, height, 700.0f,
        [&](Int3 cell) {
            if (cell.y < 0) return static_cast<int>(Material::Bedrock); // never fall through
            const Material m = world.at(cell);
            return (materialInfo(m).solid || m == Material::Water) ? static_cast<int>(m) : -1;
        },
        [&](int id) { return materialColor(static_cast<Material>(id)); });
}

Image renderVehicle(const VehicleTemplate& tmpl, const Vehicle& state, int width, int height,
                    float yawDegrees) {
    // Distinct part palette (faction paint comes later).
    static const Rgb palette[] = {
        { 70, 110, 190 },  { 200, 70, 60 },  { 90, 170, 90 },  { 220, 170, 60 },
        { 160, 90, 180 },  { 80, 190, 190 }, { 220, 120, 170 },{ 150, 150, 150 },
    };
    const float yaw = yawDegrees * 3.14159265f / 180.0f;
    const float c = std::cos(yaw), s = std::sin(yaw);

    // Camera orbits the model; the model itself stays axis-aligned in its
    // grid, so rotate the *ray space* by -yaw around the grid center.
    const Vec3 center{ static_cast<float>(tmpl.dims.x) * 0.5f,
                       static_cast<float>(tmpl.dims.z) * 0.5f,
                       static_cast<float>(tmpl.dims.y) * 0.5f };
    const float radius = std::max({ center.x, center.y, center.z }) * 2.6f;
    PreviewCamera cam;
    cam.lookAt = center;
    cam.position = { center.x + radius * 0.85f, center.y + radius * 0.55f,
                     center.z + radius * 0.85f };
    cam.fovDegrees = 40.0f;

    // 2x supersample: rotated-lattice sampling aliases badly at 1x.
    return downsample2x(renderGrid(
        cam, width * 2, height * 2, radius * 4.0f,
        [&](Int3 cell) {
            // Ray space (x, y=up, z=side) -> template grid (x, y=side, z=up),
            // rotated by -yaw around the vertical axis through the center.
            const float rx = static_cast<float>(cell.x) + 0.5f - center.x;
            const float rz = static_cast<float>(cell.z) + 0.5f - center.z;
            const int gx = static_cast<int>(std::floor(c * rx + s * rz + center.x));
            const int gy = static_cast<int>(std::floor(-s * rx + c * rz + center.z));
            const int part = tmpl.partAt({ gx, gy, cell.y });
            if (part < 0 || !state.partAlive(part)) return -1;
            const float f = state.partHpFraction(part);
            const int shade = f >= 0.75f ? 0 : f >= 0.5f ? 1 : f >= 0.25f ? 2 : 3;
            return part * 4 + shade;
        },
        [&](int id) {
            const Rgb base = palette[(id / 4) % 8];
            const float k = 1.0f - 0.22f * static_cast<float>(id % 4); // damage charring
            return Rgb{ base.r * k, base.g * k, base.b * k };
        }));
}

} // namespace vox
