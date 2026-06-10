#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vox {

struct Image {
    int width = 0, height = 0;
    std::vector<uint8_t> rgb; // 3 bytes per pixel, row-major

    Image(int w, int h) : width(w), height(h), rgb(static_cast<size_t>(w) * h * 3, 0) {}
    void put(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        const size_t i = (static_cast<size_t>(y) * width + x) * 3;
        rgb[i] = r;
        rgb[i + 1] = g;
        rgb[i + 2] = b;
    }
};

// Dependency-free PNG writer (8-bit RGB). Uses zlib "stored" (uncompressed)
// deflate blocks — perfectly valid PNG, larger files, zero dependencies.
// Headless preview tooling only; shipping asset I/O gets real compression.
std::vector<uint8_t> encodePng(const Image& img);
bool writePng(const std::string& path, const Image& img);

} // namespace vox
