# Shared Knob — Default Pack

**Status:** v1 — untested.
**Reference:** [`../pedals/clean_boost/reference/good_knob.png`](../pedals/clean_boost/reference/good_knob.png)
(user-supplied, looks like the result we want).
**Audience:** every default-pack pedal uses this one knob image.
**Output target:** transparent PNG, top-down, 512 × 512.

## What we're generating

A two-tone audio-gear style knob — fine-knurled matte black outer ring with
a polished brass / warm gold lathe-turned inner cap, single white indicator
mark at 12 o'clock. McIntosh / hi-fi mixer aesthetic. Distinct from Boss's
smooth black knobs.

The reference image (`good_knob.png`) is exactly the look we want. The
prompt below describes the same object.

## Prompt

```
Photorealistic top-down product photograph of a single audio equipment
knob, viewed from directly above. Camera is perfectly overhead,
orthographic, NOT perspective, NOT angled, NOT 3/4 view, NOT side view.

The knob is a round, two-tone design:

OUTER RING: a matte black plastic outer collar with FINE VERTICAL KNURLING
— closely-spaced parallel ridges running around the entire perimeter, the
kind of knurled grip you'd see on a high-end audio mixer or hi-fi
amplifier. The black ring takes up roughly the outer 35-40% of the knob's
radius.

INNER CAP: a polished BRASS / WARM GOLD coloured metal disc occupying the
center of the knob — approximately the inner 60% of the radius. This brass
cap has a CONCENTRIC RADIAL BRUSHED PATTERN — fine circular ridges
emanating from the center, like a lathe-turned finish. The brass reads as
a real warm-yellow gold tone, not bright yellow, not orange — closer to
old polished brass than to mirror gold.

INDICATOR: a single BRIGHT WHITE / CREAM coloured mark at the 12 o'clock
position. The mark sits in the BLACK OUTER RING (not on the brass cap),
runs radially from the outer edge inward, about as wide as a small dash.
This is the only marking on the knob.

LIGHTING: soft studio lighting from directly above. A gentle highlight
across the top of the brass surface. A subtle shadow at the bottom edge
of the knob suggesting depth. Even, photographic, not harsh.

Material reads: black plastic + polished brass + white plastic indicator.
NO chrome, NO silver, NO gradients other than the lighting.

Pure white background. Isolated subject — JUST the knob, no shaft visible
beneath, no chassis around it, no shadows on the background.

Sharp focus. Professional product photography. Photorealistic. Square 1:1
crop, knob centered, small whitespace around for bounding-box detection.
```

## Negative prompts

```
perspective, 3/4, angled, tilted, side view, chrome top, mirror finish,
gradient body, smooth black plastic without knurling, fluted, indicator
dot, indicator triangle, multiple marks, numbered scale, tick marks,
number labels, knob skirt, painted, colored body, red knob, green knob,
blue knob, chassis, mounted on pedal, exposed shaft, set screw,
illustration, cartoon, sketch, drawing, 3D render obvious CGI, painting,
multiple knobs, knob array
```

## Acceptance bar

Keep:
- Two-tone (black ring + brass center) clearly visible
- Fine knurling on the black ring all the way around
- Polished brass with radial brushed pattern (not flat brass)
- Single white indicator at 12 o'clock on the BLACK ring
- Top-down, isolated

Reject:
- All-black knob (missing brass center)
- All-brass knob (missing black ring)
- Smooth black ring (no knurling)
- Mirror chrome instead of brass
- Multiple knobs in frame
- Indicator in wrong place (3, 6, 9 o'clock or anywhere else)
- Indicator on the brass cap instead of in the black ring
- Visible shaft or set-screw beneath

## Iteration log

| Date | Tool | What we changed | Result |
|------|------|-----------------|--------|
| | | | |
