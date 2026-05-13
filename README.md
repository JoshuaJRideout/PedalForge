# PedalForge

An open-source, community-driven virtual guitar pedalboard.

**VST3 · AU · Standalone**

---

## What Is This?

PedalForge is a plugin that lets you drag effect pedals onto a virtual pedalboard, chain them together, and process audio in real-time. Pedals are written in [FAUST](https://faust.grame.fr/) — a functional DSP language — making them safe, auditable, and easy to share.

## Building

### Prerequisites

- **CMake** 3.22+
- **FAUST** (`brew install faust` on macOS)
- **C++20** compatible compiler (Clang 14+, GCC 12+, MSVC 2022+)

### Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The plugin will be automatically copied to your system plugin folders after build.

## License

MIT
