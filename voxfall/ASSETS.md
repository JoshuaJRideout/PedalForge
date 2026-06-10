# Voxfall asset sourcing guide

What we need, where to get it, and the license rules that keep a commercial
Steam release + modding community legally clean.

## License policy (read first)

1. **CC0 / public domain strongly preferred.** No attribution requirements, no
   per-asset bookkeeping, and — critically — CC0 assets can be redistributed
   inside community forge packs without infecting them.
2. **Royalty-free-with-license (e.g. Sonniss GDC) is fine for shipped audio**,
   but those files may NOT be redistributed as raw assets (so: ship in game
   depots, do not put them in modder-editable folders).
3. **CC-BY is acceptable case-by-case** — every CC-BY asset goes in
   `ATTRIBUTION.md` the day it's added, or it doesn't go in.
4. **Never:** GPL-licensed assets (viral), "free for personal use", anything
   scraped, anything AI-generated without checking the generator's terms.
5. Every imported asset records `source`, `license`, `url` in its forge-pack
   manifest entry. The pack loader will eventually enforce presence.

## Primary sources

### Sound effects
| Source | License | What to pull |
|---|---|---|
| **Sonniss #GameAudioGDC archive** (gdc.sonniss.com, ~160 GB across years) | Royalty-free, commercial, no attribution; no raw redistribution | Weapons, explosions, debris, vehicle engines, mechanical servo loops, ambience beds — this is the motherlode for our combat audio |
| **Kenney audio packs** (kenney.nl — Impact Sounds 130 assets, Sci-Fi Sounds, UI Audio, Digital Audio) | CC0 | UI clicks, pickups, shield hits, interface feedback — and these CAN ship in modder-visible folders |
| **Freesound.org** (filter: CC0 only) | CC0 (when filtered) | Gap-fillers: specific one-offs like tank track squeaks, metal groans for structural collapse |

### Voxel models & decorations
| Source | License | What to pull |
|---|---|---|
| **Kenney Voxel Pack** (190 assets) + Kenney 3D kits | CC0 | Props/decorations: crates, barrels, fences, rubble, street furniture — straight through our `.vox` importer |
| **itch.io voxel/magicavoxel CC0 tags** (e.g. padadu's fantasy bundles, maxparata/monogon's free voxel mechas) | per-pack — take CC0 ones | City props, vehicle inspiration bases, mech variants; always check each pack's license page |
| **Kenney All-in-1** (60k+ assets, free) | CC0 | The bulk-import candidate when we build the prop pipeline |

### Textures / materials / palettes
| Source | License | What to pull |
|---|---|---|
| **ambientCG** (2000+ PBR materials) | CC0 | Material reference + future PBR pass for the GPU renderer (concrete, rusted metal, soil) |
| **Poly Haven** | CC0 | HDRIs for sky lighting in the real renderer; texture references |
| **cc0-textures.com / sharetextures** | CC0 | Backup texture sources |
| **Lospec palette list** (lospec.com) | palettes aren't copyrightable, site is community | Curated voxel color palettes per faction/biome |

### Music
| Source | License | Notes |
|---|---|---|
| **FreePD.com** | CC0 | Placeholder/jam music now |
| **Kevin MacLeod (incompetech)** | CC-BY (or one-time fee for no-attribution) | Broad catalog; goes in ATTRIBUTION.md if used |
| **OpenGameArt music (CC0 filter)** | CC0 when filtered | Synth/industrial tracks fitting the §10 direction |
| Commissioned original score | — | The §10 plan for launch; library music is a placeholder tier |

### Aggregators
- **awesome-cc0 list** (github.com/madjin/awesome-cc0) — maintained index of all
  of the above and more.
- **itch.io CC0 game assets tag** — browsable firehose.

## How assets flow into the engine

| Asset type | Format | Pipeline (exists today?) |
|---|---|---|
| Voxel props/vehicles | `.vox` | YES — `voxformat.h` parser + parts sidecar; palette → per-voxel paint |
| World decorations | `.vox` placed by gen | Prop-scatter pass: planned (tree pass is the prototype) |
| Sounds | WAV/OGG | miniaudio (planned with renderer binary); keep 48 kHz WAV masters |
| Textures/HDRIs | PNG/EXR | GPU renderer phase; we currently only *write* PNG — need stb_image or similar to read |
| Palettes | `.vox` RGBA / hex lists | YES — template paletteRgb |
| Music | OGG | miniaudio streaming (planned) |

## Repo hygiene

Assets do NOT live in this git repo (binary bloat). Plan:
- `voxfall-assets/` sibling directory or Git LFS when the team grows.
- Forge-pack layout from day one: `packs/core/{models,sounds,music,palettes}/`
  with `forge.json` carrying source/license per asset.
- CI keeps building with zero assets present (procedural fallbacks stay).
