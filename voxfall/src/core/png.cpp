#include "core/png.h"
#include <cstdio>

namespace vox {

namespace {

uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc = 0xFFFFFFFFu) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        init = true;
    }
    for (size_t i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}

void putU32be(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v >> 24));
    out.push_back(static_cast<uint8_t>(v >> 16));
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v));
}

void chunk(std::vector<uint8_t>& out, const char type[4], const std::vector<uint8_t>& payload) {
    putU32be(out, static_cast<uint32_t>(payload.size()));
    const size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), payload.begin(), payload.end());
    const uint32_t crc = crc32(out.data() + start, out.size() - start) ^ 0xFFFFFFFFu;
    putU32be(out, crc);
}

} // namespace

std::vector<uint8_t> encodePng(const Image& img) {
    // Raw scanlines: filter byte 0 + RGB row.
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(img.height) * (1 + static_cast<size_t>(img.width) * 3));
    for (int y = 0; y < img.height; ++y) {
        raw.push_back(0);
        const size_t row = static_cast<size_t>(y) * img.width * 3;
        raw.insert(raw.end(), img.rgb.begin() + static_cast<long>(row),
                   img.rgb.begin() + static_cast<long>(row + static_cast<size_t>(img.width) * 3));
    }

    // zlib stream: header + stored deflate blocks + adler32.
    std::vector<uint8_t> z;
    z.push_back(0x78);
    z.push_back(0x01);
    size_t pos = 0;
    while (pos < raw.size()) {
        const size_t n = std::min<size_t>(65535, raw.size() - pos);
        z.push_back(pos + n >= raw.size() ? 1 : 0); // BFINAL + BTYPE=00 (stored)
        z.push_back(static_cast<uint8_t>(n));
        z.push_back(static_cast<uint8_t>(n >> 8));
        z.push_back(static_cast<uint8_t>(~n));
        z.push_back(static_cast<uint8_t>(~n >> 8));
        z.insert(z.end(), raw.begin() + static_cast<long>(pos),
                 raw.begin() + static_cast<long>(pos + n));
        pos += n;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t byte : raw) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    putU32be(z, (b << 16) | a);

    std::vector<uint8_t> out{ 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    std::vector<uint8_t> ihdr;
    putU32be(ihdr, static_cast<uint32_t>(img.width));
    putU32be(ihdr, static_cast<uint32_t>(img.height));
    ihdr.push_back(8); // bit depth
    ihdr.push_back(2); // color type: truecolor RGB
    ihdr.push_back(0); // compression
    ihdr.push_back(0); // filter
    ihdr.push_back(0); // interlace
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});
    return out;
}

bool writePng(const std::string& path, const Image& img) {
    const std::vector<uint8_t> bytes = encodePng(img);
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const size_t n = std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return n == bytes.size();
}

} // namespace vox
