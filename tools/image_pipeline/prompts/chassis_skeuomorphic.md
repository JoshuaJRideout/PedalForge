# Chassis — Skeuomorphic Stompbox

First-cut prompt for generating photorealistic, top-down stompbox chassis
images. Written based on failure modes observed in earlier batches:
- 3/4 perspective drift
- Exposed mechanical parts (switch undersides, PCBs)
- Lit-up LEDs (we want them OFF — runtime glow added in JUCE)
- Cartoon/illustration style instead of photo

## Status: untested

Run a small batch (4 images) and check the failure rate before adding more
variants. Update this file with what you observe.

## Prompt template

Substitute the bracketed variants:

```
Photorealistic product photograph of a guitar effect pedal, viewed from
directly above. Orthographic top-down view, flat lay, NOT perspective, NOT
3/4 view, NOT angled. Camera is directly overhead looking straight down.

The pedal is a [STYLE_DESCRIPTOR] stompbox with a [CHASSIS_COLOUR] painted
metal enclosure. [N_KNOBS] [KNOB_STYLE] knobs are arranged across the top
portion. One [LED_COLOUR] indicator LED, dome unlit and dark. One 3PDT
footswitch with a [SWITCH_FINISH] cap at the bottom centre.

Pedal is a FINISHED PRODUCT — fully assembled, no exposed wires, no visible
PCB, no switch undersides, no internal components.

Isolated on pure white background, no shadows, no props, no hands, no other
instruments or pedals in frame. Product photography lighting, soft and even,
no harsh highlights.

No text. No logos. No labels. No graphics on the chassis.

Sharp focus. Professional studio photography. Square aspect ratio. 1024x1024.
```

## Variants

| Token | Options |
|-------|---------|
| `[STYLE_DESCRIPTOR]` | `vintage analog`, `modern boutique`, `industrial gritty`, `clean minimalist`, `psychedelic 60s` |
| `[CHASSIS_COLOUR]` | `forest green`, `cream`, `metallic silver`, `matte black`, `burgundy`, `electric blue`, `sunburst yellow`, `coral pink` |
| `[N_KNOBS]` | `2`, `3`, `4`, `5` (4 is the most common stompbox pattern) |
| `[KNOB_STYLE]` | `chicken-head`, `chrome top-hat`, `black plastic with white indicator line`, `vintage skirted` |
| `[LED_COLOUR]` | `red`, `green`, `amber`, `blue`, `white` |
| `[SWITCH_FINISH]` | `chrome`, `brushed steel`, `matte black plastic`, `brass` |

## Known failure modes to watch for

- **Perspective drift** — even with "orthographic + flat lay + NOT perspective"
  you'll see some 3/4 outputs. Reject and regenerate; don't waste pipeline
  time on them.
- **LED lit anyway** — "dome unlit and dark" sometimes ignored; the model
  loves rendering glowing LEDs. If 50%+ are lit, accept that and we'll handle
  the on-state visually via JUCE.
- **Phantom knobs/switches** — model adds extras. Specify count precisely.
- **Letters / numbers on chassis** — "no text" is sometimes ignored. Reject
  any output with visible characters.
- **Subtle skew** — pedal isn't quite square to the camera. Tolerable if
  small; reject if pronounced.

## Generation parameters that have worked

(Fill in once we've tested with each tool)

| Tool | Settings | Acceptance rate (rough) |
|------|----------|-------------------------|
| `gpt-image-1` | square, high quality | (untested with this prompt) |
| Adobe Firefly | square, photo style | (untested with this prompt) |

## Example outputs

(Drop links / thumbnails here as we confirm working variants)

---

## Negative prompts (if your tool supports them)

```
perspective view, 3/4 view, angled view, side view, isometric, tilted,
exposed wiring, visible PCB, switch underside, internal components,
cartoon, illustration, sketch, painting, 3D render, CGI, watermark,
text, logo, branding, hands, fingers, person, instrument, background props
```
