#pragma once
#include <bit>
#include <cstdint>
#include <string>
#include <vector>

namespace vox {

// Shared little-endian serialization for map files and network messages.

class ByteWriter {
public:
    std::vector<uint8_t> data;

    void u8(uint8_t v) { data.push_back(v); }
    void u32(uint32_t v) {
        data.push_back(static_cast<uint8_t>(v));
        data.push_back(static_cast<uint8_t>(v >> 8));
        data.push_back(static_cast<uint8_t>(v >> 16));
        data.push_back(static_cast<uint8_t>(v >> 24));
    }
    void u64(uint64_t v) {
        u32(static_cast<uint32_t>(v));
        u32(static_cast<uint32_t>(v >> 32));
    }
    void i32(int32_t v) { u32(static_cast<uint32_t>(v)); }
    void f32(float v) { u32(std::bit_cast<uint32_t>(v)); }
    void str(const std::string& s) {
        u32(static_cast<uint32_t>(s.size()));
        data.insert(data.end(), s.begin(), s.end());
    }
    void bytes(const std::vector<uint8_t>& b) {
        u32(static_cast<uint32_t>(b.size()));
        data.insert(data.end(), b.begin(), b.end());
    }
};

// Bounds-checked reader: any overrun sets ok=false and returns zeros; callers
// check ok once at the end (malformed input can never read out of bounds).
class ByteReader {
public:
    explicit ByteReader(const std::vector<uint8_t>& d) : data(d) {}

    bool ok = true;
    size_t pos = 0;

    uint8_t u8() {
        if (pos + 1 > data.size()) { ok = false; return 0; }
        return data[pos++];
    }
    uint32_t u32() {
        if (pos + 4 > data.size()) { ok = false; return 0; }
        const uint32_t v = static_cast<uint32_t>(data[pos])
                         | (static_cast<uint32_t>(data[pos + 1]) << 8)
                         | (static_cast<uint32_t>(data[pos + 2]) << 16)
                         | (static_cast<uint32_t>(data[pos + 3]) << 24);
        pos += 4;
        return v;
    }
    uint64_t u64() {
        const uint64_t lo = u32();
        return lo | (static_cast<uint64_t>(u32()) << 32);
    }
    int32_t i32() { return static_cast<int32_t>(u32()); }
    float f32() { return std::bit_cast<float>(u32()); }
    std::string str(uint32_t maxLen = 1 << 20) {
        const uint32_t n = u32();
        if (!ok || n > maxLen || pos + n > data.size()) { ok = false; return {}; }
        std::string s(reinterpret_cast<const char*>(data.data() + pos), n);
        pos += n;
        return s;
    }
    std::vector<uint8_t> bytes(uint32_t maxLen = 1 << 30) {
        const uint32_t n = u32();
        if (!ok || n > maxLen || pos + n > data.size()) { ok = false; return {}; }
        std::vector<uint8_t> b(data.begin() + static_cast<long>(pos),
                               data.begin() + static_cast<long>(pos + n));
        pos += n;
        return b;
    }

private:
    const std::vector<uint8_t>& data;
};

} // namespace vox
