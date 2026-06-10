# Voxfall

Procedural voxel-world remake of the 1998 strategy/action hybrid *Urban Assault*
(working title). Full design: [`../docs/urban-assault-remake/DESIGN.md`](../docs/urban-assault-remake/DESIGN.md).

This directory is a standalone CMake project (no dependency on the surrounding
repo) so it can be extracted into its own repository when it grows up.

## Status — M0 in progress

Headless core, zero external dependencies:

- [x] Deterministic seeded world generation (heightfield fBm, material layers,
      hardrock ribs, crystal veins, sea level) — `src/world/`
- [x] Terrain destruction: blast events, per-material HP, damage accumulation,
      whole-world + per-chunk hashing for sync audits
- [x] Vehicle sub-voxel templates segmented into parts (hull/wing/engine/weapon/…)
      with group HP, armor, damage-type interaction, overkill bleed, part
      detachment, and deterministic loot drops — `src/vehicle/`
- [x] Unit tests + headless demo (`voxfall_sim`)
- [ ] Kinematic vehicle controllers (per locomotion class)
- [ ] Renderer (bgfx + SDL3): greedy-meshed chunks, vehicle re-mesh on damage
- [ ] 2-player listen-server sync (event-sourced destruction, snapshot vehicles)

## Build & run

```bash
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure   # unit tests
./build/voxfall_sim [seed]                   # headless demo
```

Requires CMake ≥ 3.22 and a C++20 compiler. No other dependencies (yet).
