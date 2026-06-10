# Voxfall

Procedural voxel-world remake of the 1998 strategy/action hybrid *Urban Assault*
(working title). Full design: [`docs/design/DESIGN.md`](docs/design/DESIGN.md).

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
- [x] Kinematic locomotion controllers (tracked, jet, walker/mech, pilot) wired
      to part status: track loss = limp, engine loss = glide, leg loss =
      immobile, cockpit loss = stealable husk — `src/vehicle/locomotion.*`
- [x] Mech ("Talon") and on-foot pilot templates (§4.7 of the design)
- [x] `.vxm` static map format (§3.4): RLE-compressed world + name/spawn
      metadata, save/load, malformed-input rejection — the future map
      editor's output format — `src/world/mapfile.*`
- [x] Deterministic combat sim (`src/sim/`): world-space entities, hitscan
      into rotated sub-voxel grids, terrain blasts, death craters, drop
      pickups with magnet collection, team energy, SimEvent records
- [x] Listen-server netcode (`src/net/`): server-authoritative sessions,
      event-sourced terrain destruction, full entity snapshots, join-in-
      progress via live `.vxm` transfer, rotating chunk-hash desync audits;
      transport-agnostic (loopback in tests, Steam Networking Sockets later)
- [x] Cross-platform CI (Windows/macOS/Linux build + tests + demo smoke run)
- [x] Structural integrity (§5.3): unsupported concrete/metal clusters
      collapse after blasts, deterministically and network-synced
- [x] Possession (§4.7): eject to an on-foot pilot, board any pilotless
      vehicle in range (incl. enemy theft); Action/ControlAssign protocol
- [x] CPU meshing (`src/render/mesh.*`): greedy-merged chunk meshes with
      cross-chunk face culling; vehicle re-mesh on damage (destroyed parts
      vanish, damaged parts darken) — the renderer's algorithmic core
- [x] Unit AI (`src/ai/`): move/attack/escort orders driving the same input
      interface as players — AI and possession are interchangeable
- [x] Sector economy (§2.2): power stations claim 16x16 sectors, owned
      sectors pay energy per second, killing the station's core flips the
      sector neutral
- [x] Arena generation: 4 biomes (incl. city ruin grammar), spawn rings
      with carved route guarantees, tank-walkability validation bot
- [x] Projectiles (arcs, missiles) + blast splash damage to vehicles
- [x] Host Stations, annihilation match state machine, commander AI —
      bot-vs-bot matches resolve deterministically in CI
- [x] Replay system (byte-identical re-simulation), chunk re-sync repair
- [x] UDP transport with fragmentation + voxfall_server dedicated binary
- [x] MagicaVoxel .vox import with parts sidecar (the modding asset path)
- [x] Parser/network fuzz tests (3 real bugs found and fixed)
- [ ] Renderer binary (bgfx + SDL3): GPU upload + camera around the meshers
- [ ] Client prediction for the possessed vehicle; GameNetworkingSockets binding

## Build & run

```bash
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure   # unit tests
./build/voxfall_sim [seed]                   # headless demo
```

Requires CMake ≥ 3.22 and a C++20 compiler. No other dependencies (yet).
