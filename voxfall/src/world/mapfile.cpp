#include "world/mapfile.h"
#include <cstdio>

namespace vox {

namespace {

constexpr uint32_t kMagic = 0x314D5856; // "VXM1" little-endian
constexpr uint32_t kVersion = 1;

void putU32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v));
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v >> 16));
    out.push_back(static_cast<uint8_t>(v >> 24));
}

void putI32(std::vector<uint8_t>& out, int32_t v) { putU32(out, static_cast<uint32_t>(v)); }

// Bounds-checked little-endian reader.
struct Reader {
    const std::vector<uint8_t>& data;
    size_t pos = 0;
    bool ok = true;

    uint8_t u8() {
        if (pos + 1 > data.size()) { ok = false; return 0; }
        return data[pos++];
    }
    uint32_t u32() {
        if (pos + 4 > data.size()) { ok = false; return 0; }
        const uint32_t v = static_cast<uint32_t>(data[pos]) | (static_cast<uint32_t>(data[pos + 1]) << 8)
                         | (static_cast<uint32_t>(data[pos + 2]) << 16)
                         | (static_cast<uint32_t>(data[pos + 3]) << 24);
        pos += 4;
        return v;
    }
    int32_t i32() { return static_cast<int32_t>(u32()); }
};

} // namespace

std::vector<uint8_t> encodeMap(const VoxelWorld& world, const MapMeta& meta) {
    std::vector<uint8_t> out;
    const Int3 dims = world.size();

    putU32(out, kMagic);
    putU32(out, kVersion);
    putI32(out, dims.x);
    putI32(out, dims.y);
    putI32(out, dims.z);
    putI32(out, world.seaLevel());

    putU32(out, static_cast<uint32_t>(meta.name.size()));
    out.insert(out.end(), meta.name.begin(), meta.name.end());
    putU32(out, static_cast<uint32_t>(meta.spawns.size()));
    for (const MapSpawn& s : meta.spawns) {
        putI32(out, s.position.x);
        putI32(out, s.position.y);
        putI32(out, s.position.z);
        out.push_back(s.team);
    }

    // RLE over canonical order (y-major, then z, then x — matches storage, so
    // runs follow horizontal rows). Each run: material byte + u32 length.
    const size_t runCountPos = out.size();
    putU32(out, 0); // patched below
    uint32_t runCount = 0;
    Material current = world.at({ 0, 0, 0 });
    uint32_t runLength = 0;
    auto flush = [&] {
        out.push_back(static_cast<uint8_t>(current));
        putU32(out, runLength);
        ++runCount;
    };
    for (int y = 0; y < dims.y; ++y) {
        for (int z = 0; z < dims.z; ++z) {
            for (int x = 0; x < dims.x; ++x) {
                const Material m = world.at({ x, y, z });
                if (m == current) {
                    ++runLength;
                } else {
                    flush();
                    current = m;
                    runLength = 1;
                }
            }
        }
    }
    flush();
    out[runCountPos] = static_cast<uint8_t>(runCount);
    out[runCountPos + 1] = static_cast<uint8_t>(runCount >> 8);
    out[runCountPos + 2] = static_cast<uint8_t>(runCount >> 16);
    out[runCountPos + 3] = static_cast<uint8_t>(runCount >> 24);
    return out;
}

std::optional<LoadedMap> decodeMap(const std::vector<uint8_t>& bytes) {
    Reader r{ bytes };
    if (r.u32() != kMagic) return std::nullopt;
    if (r.u32() != kVersion) return std::nullopt;

    const Int3 dims{ r.i32(), r.i32(), r.i32() };
    const int seaLevel = r.i32();
    if (!r.ok || dims.x <= 0 || dims.y <= 0 || dims.z <= 0) return std::nullopt;
    // Reject absurd dimensions before allocating (max L map is 2048x2048x192, §3.1).
    if (dims.x > 4096 || dims.y > 1024 || dims.z > 4096) return std::nullopt;

    MapMeta meta;
    const uint32_t nameLen = r.u32();
    if (!r.ok || r.pos + nameLen > bytes.size()) return std::nullopt;
    meta.name.assign(reinterpret_cast<const char*>(bytes.data() + r.pos), nameLen);
    r.pos += nameLen;

    const uint32_t spawnCount = r.u32();
    if (!r.ok || spawnCount > 1024) return std::nullopt;
    for (uint32_t i = 0; i < spawnCount; ++i) {
        MapSpawn s;
        s.position = { r.i32(), r.i32(), r.i32() };
        s.team = r.u8();
        meta.spawns.push_back(s);
    }

    LoadedMap result{ VoxelWorld(dims), std::move(meta) };
    result.world.setSeaLevel(seaLevel);

    const uint64_t totalVoxels =
        static_cast<uint64_t>(dims.x) * static_cast<uint64_t>(dims.y) * static_cast<uint64_t>(dims.z);
    const uint32_t runCount = r.u32();
    uint64_t written = 0;
    Int3 p{ 0, 0, 0 };
    for (uint32_t i = 0; i < runCount; ++i) {
        const uint8_t materialByte = r.u8();
        const uint32_t length = r.u32();
        if (!r.ok || materialByte >= static_cast<uint8_t>(Material::Count)) return std::nullopt;
        if (written + length > totalVoxels) return std::nullopt;
        const Material m = static_cast<Material>(materialByte);
        for (uint32_t n = 0; n < length; ++n) {
            if (m != Material::Air) result.world.set(p, m); // worlds start as air
            if (++p.x == dims.x) {
                p.x = 0;
                if (++p.z == dims.z) {
                    p.z = 0;
                    ++p.y;
                }
            }
        }
        written += length;
    }
    if (!r.ok || written != totalVoxels) return std::nullopt;
    return result;
}

bool saveMapFile(const std::string& path, const VoxelWorld& world, const MapMeta& meta) {
    const std::vector<uint8_t> bytes = encodeMap(world, meta);
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const size_t n = std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return n == bytes.size();
}

std::optional<LoadedMap> loadMapFile(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return std::nullopt;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        std::fclose(f);
        return std::nullopt;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    const size_t n = std::fread(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    if (n != bytes.size()) return std::nullopt;
    return decodeMap(bytes);
}

} // namespace vox
