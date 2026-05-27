# Shared Chassis Body — Default Pack

**Status:** v1 — untested.
**Audience:** all default-pack factory pedals share this one image.
**Output target:** neutral gray transparent PNG, top-down, ~1024×2048 (2:1
portrait, matches our 70×140 PedalDesign aspect).

## What we're generating

A Boss-style guitar pedal chassis **viewed from directly above** with a
**completely smooth, unmarked, hole-free top surface**. Painted in pure
**neutral grayscale** (no color cast) so JUCE can multiply a per-pedal tint
over it at runtime.

Knob positions, LED position, the cover, the footswitch, jack labels, and
silkscreen text are ALL separate layers composited on top at runtime. This
image must contain none of them — it is the blank canvas only.

## Prompt

```
Photorealistic top-down product photograph of a guitar effect pedal
enclosure body — a completely BLANK, UNDRILLED, FEATURELESS chassis with
absolutely nothing on its top face. Camera is directly overhead, perfectly
orthographic, no perspective, no 3/4 angle, no tilt.

Shape: a Boss-style rectangular die-cast aluminum enclosure with gently
rounded corners (subtle radius). Tall portrait orientation, about 2:1
aspect ratio (taller than wide). Slightly tapered/sloped front section is
fine but the top face must remain entirely flat and smooth.

The TOP SURFACE is COMPLETELY SMOOTH and FEATURELESS — no holes, no
drilled openings, no knob holes, no LED hole, no footswitch hole, no
indents, no recessed wells, no cavities, no bays, no plateaus, no
silkscreen, no labels, no logos, no text, no graphics, no markings of any
kind. It is a single uninterrupted flat surface. Imagine a brand-new
enclosure straight from the metal shop before any machining or paint
graphics are applied.

Finish: SOLID NEAR-WHITE paint, RGB approximately (235, 235, 235) — a very
light off-white / pale neutral, like an unpainted primed surface. PURE
GRAYSCALE only. NO color tint of any kind — no green, no blue, no red, no
warm or cool bias, no cream or beige tint. The paint is a soft matte /
satin finish that catches light realistically but reads as completely
near-white so it can be MULTIPLY-tinted to any color at composite time
(a darker base would muddy the runtime tint).

The LEFT and RIGHT side walls must be COMPLETELY SMOOTH — NO jack holes,
NO input or output ports, NO openings, NO knobs on the side, NO MIDI
ports, NO USB ports, NO power jacks. Jacks are a separate composable
layer that JUCE positions at runtime; this image must NOT contain any.

ABSOLUTELY NO visible screws anywhere — modern hidden-hardware aesthetic.

NO text. NO letters. NO logos. NO branding. NO labels. NO silkscreen of
any kind on the top face.

NO cover, NO knobs, NO footswitch, NO LED — the top face must be
completely empty and undrilled.

Soft, even studio lighting from above with a gentle highlight to show the
metal surface convincingly. Pure white background. Isolated subject, no
shadows on the background, no props, no hands.

Sharp focus. Professional product photography. Photorealistic. Square or
slightly-portrait crop, subject centered, room around the chassis for
accurate bounding-box detection.
```

## Negative prompts (if your tool supports them)

```
side jacks, jack holes, input jack, output jack, TRS jack, phone jack,
1/4 inch jack, side ports, USB port, MIDI port, power jack, DC jack,
holes, drilled, knob holes, LED hole, footswitch hole, openings, cavities,
recessed wells, indents, plateaus, bays, machined cutouts, perspective view,
3/4 view, angled, side, isometric, tilted, screws, visible screws, screw
heads, exposed screws, hex screws, Phillips screws, text, letters, words,
logos, branding, label, silkscreen, printed graphics, colored, color cast,
tinted, green, blue, red, warm tone, cool tone, saturated, knobs, knob
caps, cover, pedal cover, footswitch, switch, LED dome, jack labels,
illustration, cartoon, sketch, drawing, 3D render obvious CGI, painting,
hands, fingers, person, instrument, multiple pedals, background
```

## Acceptance bar

When eyeballing the outputs, keep ones that have:

- True top-down view (no tilt)
- Completely smooth, unmarked, hole-free top face
- Pure neutral gray, no color cast (drop into Photoshop / preview eyedropper
  and check that R≈G≈B)
- No screws, no text, no graphics
- Clean white background (rembg-able)
- Crisp rounded-rectangle silhouette

Reject ones that have any of:

- Any holes or cavities on the top face
- Any color cast (warm, cool, green, blue, etc.)
- Perspective / tilt / 3D angle
- A cover or knobs or footswitch on the chassis
- Visible screws
- Any text / silkscreen / logos
- Cartoonish / illustrated style

## Iteration log

(Fill in as we generate)

| Date | Tool | What we changed | Result |
|------|------|-----------------|--------|
| | | | |
