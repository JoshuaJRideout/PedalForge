# Clean Boost — Visual Design (v2)

**Status:** revised after feedback. Pending sign-off before prompts.

## What changed from v1

- **Form factor**: stompbox → **Boss-style flip-cover** (modern guitar-pedal
  vocabulary; instantly reads as a real pedal)
- **Chassis color**: hard-painted forest green → **neutral / colorless base**
  that JUCE tints at runtime (one chassis image serves every default pedal
  in this family)
- **No screws** — modern aesthetic, hardware hidden
- **Asset count**: ~4 images per pedal → **shared chassis + cover + ~3 per
  pedal** (most reuse across the default pack)

## The big asset-reuse insight

The chassis + cover + knob + LED + footswitch images are **generic and
shared**. Per-pedal customization comes from:

| What changes per pedal | How |
|---|---|
| Body color | JUCE multiply-tint on the body layer |
| Silkscreen text ("BOOST", "DRIVE", "DELAY") | Rendered by JUCE as text on top of the cover |
| Number + position of knobs | Per-pedal `PedalDesign.controls` layout |
| LED color (when lit) | JUCE bloom renderer's tint param |
| Knob colors / pointer accents | Optional secondary tint pass per knob |

So **one well-authored chassis image** is enough for Clean Boost, Overdrive,
Compressor, Delay, Reverb, etc. — all the default pedals share it. Each
pedal differs through color and layout, not through chassis art.

## Form factor — Boss-inspired, original execution

Picking the Boss family vocabulary (rectangular die-cast body, hinged metal
pedal cover over the bottom half, knobs above the cover, LED visible when
cover is down) because it's the strongest "this is a guitar pedal" visual
signal for the target audience.

**Trade-dress avoidance** (we are not making a Boss clone):
- Different chassis proportions: ours **70 × 140** (Boss ≈ 70 × 125)
- Cover takes the **bottom 55%** (Boss ≈ 60%)
- LED **centered above cover** (Boss is offset)
- Silkscreen typography is **our own choice** (avoid Boss's Helvetica-derived
  treatment)
- Our own color → effect-category associations (we'll define these as we
  build the pack — not "yellow = distortion" because that's Boss)
- No "BOSS" wordmark on the cover — ours just shows the effect name

The visual family is "Boss-inspired modern boutique stompbox." Different
enough legally; familiar enough for instant recognition.

## Asset list (generation targets)

Four prompts, all shared across the default pack:

### 1. Chassis body (the colored part)
- **Neutral gray** (~#808080), no color cast
- Boss-style die-cast aluminum form: rectangular, slightly tapered edges,
  rounded corners (~6 px radius)
- Top half (~45%) has a recessed knob bay — slightly darker neutral, like
  a sunken plateau where the knobs will sit
- Bottom half (~55%) has the **cover area** — flat, simple, designed to
  receive the cover image as an overlay
- **No screws visible** anywhere
- No silkscreen text (rendered at runtime per pedal)
- LED hole indicated just above the cover area (a 12 px circular recess)
- Hint of input/output jack holes on the sides (small, subtle)

### 2. Pedal cover (the part you step on)
- **Brushed silver / chrome** finish (untinted — stays metallic across all
  pedals)
- Hinged appearance at the top edge (subtle visual hinge line)
- Footswitch hole in the centre
- Slight specular highlight implying metal
- Matches the chassis aspect for the bottom 55%

### 3. Knob (two-tone black + brass, 12 o'clock indicator)

Reference: [`reference/good_knob.png`](reference/good_knob.png). High-end
boutique audio-gear aesthetic — McIntosh / hi-fi mixer territory. Reads
distinctive top-down without being fussy.

| | |
|---|---|
| View | Top-down, orthographic |
| Outer ring | Matte black, **fine vertical knurling** all around the perimeter, molded-plastic feel |
| Outer ring proportion | ~35–40% of radius |
| Inner cap | Polished **brass / warm gold** metal disc with **concentric brushed radial pattern** (lathe-turned look) |
| Inner cap proportion | ~60% of radius |
| Indicator | Single **bright white / cream mark** at 12 o'clock, sitting in the black ring (NOT on the brass cap) |
| Lighting | Soft overhead studio, slight specular hot-spot at top, gentle shadow at bottom |
| Output size | 256 × 256 px transparent PNG |

This is the single knob style for the entire default pack — Boost,
Overdrive, Compressor, Delay, etc. all use this image. Visual consistency
is the whole point of sharing the asset. The brass + black combo is colour-
neutral enough to sit on any chassis tint.

### 4. LED dome + bezel
- **Off state**: clear dome in chrome bezel ring
- Dome has the faintest hint of color (very subtly tinted, almost clear)
  so the unlit state still reads as a real LED
- ~16 px bezel diameter, 10 px dome
- One image reused across pedals — JUCE composites the lit color + bloom

### 5. Footswitch cap (the part visible through the cover)
- Polished chrome dome
- Round, ~30 px diameter
- This is the part that protrudes through the cover hole

## Per-pedal customization (for Clean Boost specifically)

- **Body tint colour**: warm green — `#3A6E4F` (forest, not lime)
- **Silkscreen wordmark**: "BOOST" — cream condensed sans-serif
- **Knob count**: 1
- **Knob position**: centered in the knob bay
- **LED off-tint**: faint amber (almost clear)
- **LED on color**: amber `#F59E0B`

## Visual vocabulary this establishes (for all future factory pedals)

Locking these now means every later pedal inherits them without revisiting:

| Element | Locked choice |
|---|---|
| Form factor | Boss-style flip-cover stompbox |
| Photo style | Top-down orthographic, soft even studio light, pure white background |
| Aspect | 70 × 140 (chassis), 70 × 77 (cover overlay on bottom 55%) |
| Material reads | Body = die-cast aluminum, cover = brushed steel |
| Hardware | NO visible screws |
| Knob default | Chicken-head, cream body |
| Footswitch | Chrome 3PDT dome through cover |
| LED bezel | Chrome ring, clear-with-tint dome |
| Silkscreen | Rendered at runtime as text (font: condensed sans, weight: medium) |
| Color → category | TBD (we define as we add pedals — *not* Boss conventions) |

## Footswitch state handling

**For the Boss-style cover (this pack):** one cover image, no state variants.
Press feedback is a runtime animation — translate the cover layer 2–3 px
down on mouse-down with a soft underneath shadow, ease back up on mouse-up.
Persistent on/off is signalled by the LED bloom, not by cover position.

**Future: exposed 3PDT switches** (vintage-style 1590A pedals etc.) will need
**layered art** rather than two full images:

- `switch_shaft` — the threaded post, static
- `switch_tube` — the cap/sleeve, animates downward on press
- `switch_nut` — locking nut at base, static

A whole-image translate would look wrong because only the tube actually moves
in real hardware. Defer until we add a pedal that needs it; the layered model
fits naturally into our existing layered renderer.

## What this means for the JUCE renderer

The pedal renderer becomes a **layered composite**:

```
1. Body image (tinted with pedal.bodyColor via multiply)
2. Cover image (no tint — stays metallic)
3. Footswitch cap (positioned at cover centre)
4. Knob images (positioned per pedal.controls)
5. LED off-state image (positioned per pedal.controls)
6. LED bloom (intensity-driven, composited above LED)
7. Silkscreen text (rendered with juce::Graphics::drawText)
```

Each layer is cheap (single textured quad). Total ~7 draw calls per pedal.
Tint pass is a single `juce::Graphics::setOpacity + setColour` + draw.

## To decide before writing prompts

- [ ] Approve Boss-style flip-cover form factor (or counter-propose)
- [ ] Approve neutral-base + runtime tint model
- [ ] Approve our specific proportions (70 × 140, 55% cover)
- [ ] Approve color choice for Clean Boost (`#3A6E4F` forest green)
- [ ] Approve "BOOST" as the silkscreen wordmark
- [ ] Confirm trade-dress differentiation feels safe enough

Once approved, we write five prompts (chassis, cover, knob, LED, footswitch
cap), generate small batches, manually review, pipeline-process, then wire
the assets into `FactoryDesigns.h` for Clean Boost.

After Clean Boost ships, the next default pedals (Overdrive, Compressor,
Distortion, etc.) inherit chassis + cover + knob + LED + footswitch — they
just need their own color + silkscreen + control layout.
