# Voxfall — headless-completable roadmap

Everything below can be built AND verified in a headless environment (no GPU,
no audio, no Steam SDK, no user input): simulation, data, protocol, tooling,
and CPU-side algorithms. Items marked (CI-provable) get automated tests.

## World & generation
- [x] (partial) §3.3 pipeline: 4 biomes incl. city ruin grammar + domain warp, 3D cave/arch
      carving, procedural ruin grammar (enterable, collapsible buildings),
      road-graph guarantee between spawns, mirrored resource scatter (CI-provable)
- [ ] Macro-patterns as generation rulesets: Crossfire, Lanes, Archipelago,
      Undercity, Ringworld, Bunker Hill (CI-provable)
- [x] Seed validation bots: reachability flood fill, income sim, fairness
      thresholds — the ranked-pool gatekeeper (CI-provable)
- [ ] Falling debris as damage-dealing server bodies; loose-soil slump
- [ ] .vxm v2: partial-damage persistence (late-joiner gap), sector/ritual/mode metadata

## Vehicles & combat
- [x] Projectile weapons (travel time, arcs, missiles); blast damage to vehicles
- [ ] Weapon definition tables: mounts, muzzle positions, cooldowns, ammo classes
- [ ] Template roster: Host Station, Colossus (armor plates, buried reactor),
      transports, turrets, beam gates, repair pads, tier 1-3 per faction
- [ ] Hover + VTOL locomotion; ground-vehicle wall collision
- [ ] Shields, veterancy, faction stat-modifier system

## Game loop
- [ ] Host Station build queue, supply caps, reinforcement discounts, tech tiers
- [x] (annihilation) win conditions + match state machine; other modes pending:
      Wreckyard, Last Light, Colossus (capture + the Trade), Scrap Pilots
- [x] Commander AI (utility AI skirmish opponent) — verified by bot-vs-bot
      matches in CI that must reach a winner (CI-provable)
- [ ] Pathfinding: flow fields / HPA* with incremental repair on destruction
- [x] Replay system: seed + input log -> byte-identical re-simulation (CI-provable)

## Netcode
- [ ] Client prediction + reconciliation (possessed vehicle); interpolation for others
- [ ] Delta-compressed snapshots, replication priority, lag-compensated fire
- [x] Chunk re-sync repair on audit mismatch (self-healing)
- [x] Real UDP transport with fragmentation, over real sockets (CI-provable)
- [x] Dedicated server binary with tick scheduler (voxfall_server)
- [ ] Reconnect flow; sector-ownership replication

## Modding & data pipeline
- [ ] Forge pack loader: manifest parse, content hashing, load order
- [x] MagicaVoxel .vox parser + parts-annotation sidecar (real asset pipeline)
- [ ] Migrate hardcoded templates to data files; vehicle cost-balancing formula

## Rendering-adjacent (CPU-only)
- [ ] Voxel AO baked into mesh vertices; LODs; palette/faction colors; greebles
- [ ] Camera controllers + culling math as pure functions (CI-provable)
- [ ] Software raycaster rendering worlds/vehicles to PNG — visual previews
      without a GPU

## Tooling, CI, docs
- [ ] CLI tools: worldgen previewer, .vxm inspector, seed-validation runner
- [x] Fuzz tests for map/protocol parsers (found+fixed 3 real bugs); perf benchmarks pending
- [ ] Sanitizer CI job; clang-format; release packaging; server Dockerfile
- [ ] Architecture doc, protocol spec, modding spec, contributing guide

## Requires real hardware or human judgment (NOT doable here)
GPU renderer bring-up · audio · input feel tuning · Steamworks integration
(partner account, app ID) · playtesting / fun-factor decisions · art
direction · final name + trademark check · Steam page assets
