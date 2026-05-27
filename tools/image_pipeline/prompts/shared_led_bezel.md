# Shared LED + Bezel — Default Pack

**Status:** v1 — untested.
**Audience:** every default-pack pedal uses this one LED image.
**Output target:** transparent PNG, top-down, 256 × 256.

## What we're generating

A single guitar-pedal indicator LED in its **off** state, set in a chrome
bezel ring. The dome is clear with the faintest hint of color (so an unlit
real-LED still reads as a real LED rather than an empty hole). JUCE
composites the lit-state glow + bloom on top at runtime — we never ship
images of lit LEDs.

## Prompt

```
Photorealistic top-down product photograph of a single guitar-pedal
indicator LED in its OFF, UNLIT state — viewed from directly above.
Camera is perfectly overhead, orthographic, NOT perspective, NOT angled.

The LED is set in a small CHROME / POLISHED STEEL BEZEL ring. The bezel
is a circular metal ring that surrounds the LED dome, approximately the
outer 30% of the total diameter. The bezel has a soft specular highlight
implying overhead light hitting polished metal.

Inside the bezel: the LED DOME — a small rounded clear / transparent /
slightly-tinted plastic dome. The dome should be DARK and UNLIT — no glow,
no bloom, no emission, no halo, NOT illuminated. It should read as a real
LED on a powered-down pedal. The dome may have the faintest hint of
amber / yellow / clear-glass tint that suggests the LED colour without
actually glowing.

A subtle specular highlight near the top of the dome implying overhead
studio lighting. The dome should look like glass / clear plastic, with
slight transparency, NOT like a flat-colored disc.

NO glow. NO halo. NO bloom. NO bright center. NO illumination of any
kind. The LED is OFF.

Material reads: polished chrome bezel + clear glass/plastic dome.

Pure white background. Isolated subject — just the LED-in-bezel, no
surrounding chassis, no other components, no shadows on the background.

Sharp focus. Professional product photography. Photorealistic. Square
1:1 crop, subject centered with whitespace around for bounding-box
detection.
```

## Negative prompts

```
glowing LED, bright LED, illuminated LED, lit LED, light emission,
bloom, halo, glare, light rays, red LED on, green LED on, blue LED on,
saturated color, bright color, gradient glow, neon, perspective, 3/4,
angled, tilted, side view, multiple LEDs, LED bar, LED strip,
illustration, cartoon, sketch, 3D render obvious CGI, hands, mounted
on chassis, exposed leads, anode, cathode, wires
```

## Acceptance bar

Keep:
- Chrome bezel ring visible around the dome
- Dome is dark / unlit / clear or faintly tinted
- Top-down, isolated, white background
- Reads as a real LED off, not a flat circle

Reject:
- Glowing / lit LED of any colour
- Bright saturated center
- Visible halo or bloom (we add those at runtime)
- Wire leads / solder pads visible
- Multiple LEDs in frame

## Iteration log

| Date | Tool | What we changed | Result |
|------|------|-----------------|--------|
| | | | |
