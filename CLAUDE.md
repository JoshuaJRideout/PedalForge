# Voxfall — project handoff

Voxfall is a remake of the 1998 strategy/action hybrid *Urban Assault* in a
fixed-size, fully destructible voxel world. C++20, zero external dependencies
so far, multiplayer-first, targeting Windows/macOS/Linux via Steam.

The founding design document is `docs/design/DESIGN.md` — read it for the
game's rules, modes, factions, netcode architecture, and roadmap. The concept
image (`docs/design/concept-art.png`) is the visual bar. `ROADMAP.md` tracks
implementation status; `ASSETS.md` is the asset-sourcing + license policy.

## How this repo came to be

Developed inside JoshuaJRideout/PedalForge (PR #1) by Claude Code web
sessions, then extracted via `git subtree split` with full history. The
PedalForge branch `claude/urban-assault-voxel-design-lf3pvq` still contains
the pre-split history and an un-stripped copy — cleanup of that PR is a
pending task (see "Pending chores").

## Build, test, run

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
ctest --test-dir build --output-on-failure   # 82 tests, ~1 s total
./build/voxfall_sim 1337                     # headless demo: worldgen + combat log
./build/voxfall_preview 1337 /tmp/out        # renders PNGs: biomes, 24-unit faction lineup
./build/voxfall_server 27600 1337            # 60 Hz headless dedicated server (UDP)
```

Requires CMake ≥ 3.22 + any C++20 compiler. No other dependencies. CI
(`.github/workflows/ci.yml`) builds + tests on ubuntu/macos/windows — enable
Actions on the repo.

There is also a sanitizer config used throughout development:
`cmake -B build-asan -S . -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"`.
Keep it green; it has caught real bugs (3 remotely-triggerable parser bugs
found by the fuzz tests, a release-build hang from signed overflow).

## What exists and works (all headless, all tested)

- **World** (`src/world/`): deterministic seeded generation; 4 biomes (Dunes,
  Canyons, Archipelago, ShatteredCity with windowed buildings in 3 archetypes,
  trees); blast destruction with per-material HP; structural-integrity
  collapse; spawn rings with carved route guarantees; `validateArena`
  walkability bot; whole-world + per-32³-chunk hashing; `.vxm` map format
  (save/load, the future map editor's output).
- **Vehicles** (`src/vehicle/`): sub-voxel templates with named parts, group
  HP, armor, overkill bleed, husk rule (cockpit ≠ core); per-template voxel
  scale (0.0625–0.25 m); per-voxel paint + palettes; MagicaVoxel `.vox`
  importer with parts sidecar; **4 factions × 6 unit classes** (fighter,
  tank, mech, pilot, power station, host station) each with a real-world
  design language — see `factions.cpp` header comment; faction stats
  (costs, Choir regen); locomotion controllers (tracked/jet/walker/pilot)
  where damage changes handling live.
- **Sim** (`src/sim/`): hitscan + projectiles into rotated sub-voxel grids;
  blast splash vs vehicles; death craters; drop pickups with magnet collect;
  sector/energy economy; possession (eject/board/steal); deterministic
  SimEvent log; full-state hashing.
- **Game** (`src/game/`, `src/ai/`): move/attack/escort orders; commander AI;
  annihilation Match — bot-vs-bot wars resolve deterministically in CI in
  under a second.
- **Net** (`src/net/`): server-authoritative sessions; event-sourced terrain;
  join-in-progress via live map transfer; chunk-hash audits with self-healing
  re-sync; replays (byte-identical re-simulation); real UDP transport with
  fragmentation + dedicated server binary; parser/network fuzz tests.
- **Render groundwork** (`src/render/`): greedy meshing with cross-chunk
  culling; damage-state vehicle re-mesh; software raycaster producing PNG
  screenshots headless (the preview tool's engine).

## What needs testing by a human (priority order)

1. **Repo bring-up**: push this branch as `main` of JoshuaJRideout/voxfall,
   enable GitHub Actions, confirm CI green on all three OSes.
2. **First human macOS build**: CI covers macos-14, but no human has built or
   run the binaries on real hardware. Run the four executables above; eyeball
   the preview PNGs.
3. **Real-network multiplayer**: `voxfall_server` on one machine, but there is
   no interactive client binary yet — the UDP client (`UdpClientLink`) has
   only been driven by tests over loopback. Cross-machine LAN play needs the
   renderer/input client first.
4. **Renderer bring-up (the big one, needs GPU)**: bgfx + SDL3 binary wrapping
   the existing meshers (`meshChunk`, `meshVehicle`) + camera + input →
   first interactive build. Everything below it (sim, net, content) is ready.
5. **Fun-factor gate (M0)**: once rendered — "is shooting a wing off fun?"
   Nothing can test this but a human with a mouse.
6. **Asset downloads** per `ASSETS.md` (Sonniss GDC bundle, Kenney packs) —
   multi-GB, belongs on a dev machine, NOT committed to this repo.

## Pending chores

- Strip `voxfall/`, `docs/urban-assault-remake/`, and
  `.github/workflows/voxfall-ci.yml` from the PedalForge branch; repoint and
  close PedalForge PR #1 once this repo is confirmed live.
- `stb_image` (or similar) for *reading* PNGs — we can only write them, which
  blocks texture asset import.
- Faction stat differentiation is minimal (costs + regen); §4.6 has more.
- Wasp got the full concept-art treatment (procedural sculpt + paint at
  0.0625 m); other units are box-sculpted at 0.125 m — same treatment applies.

## Gotchas

- **Determinism is the contract.** Same seed + same inputs = byte-identical
  state (`Sim::stateHash`). Anything nondeterministic breaks replays, the
  desync auditor, and several tests. All randomness flows through seeded
  `Rng` instances.
- **Tests reference template sub-voxel coordinates** (e.g. Wasp `{48,32,10}`
  = hull). Resculpting must keep those points in the same parts, or update
  the coordinated spots (tests/demo/preview) together.
- **Part adjacency matters**: overkill damage bleeds into the nearest
  *connected* part; `test_factions` rejects floating parts (this caught a
  real bug — a thigh that never touched its pelvis suppressed bleed).
- The UDP layer has **no retransmit** — loopback/LAN only by design; internet
  play goes through GameNetworkingSockets per DESIGN.md §7.2.
- `applyBlast` validates inputs because clients replay network events —
  fuzzing found infinite loops and giant allocations; keep guards when
  touching parsers.
- Windows: `NOMINMAX` before winsock; `/tmp` doesn't exist (tests use
  CWD-relative paths).

## Suggested next session opener

"Read CLAUDE.md, DESIGN.md and ROADMAP.md. Then let's build the bgfx+SDL3
renderer binary around meshChunk/meshVehicle so I can fly the Wasp."
