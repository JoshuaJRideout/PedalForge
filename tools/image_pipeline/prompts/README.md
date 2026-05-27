# Prompt Library

Versioned prompt templates for generating PedalForge UI assets. Each `.md` file
in this directory contains:

- The **prompt text** that's worked for a specific asset/style combination
- **Negative prompts** (what to avoid)
- **Variants** you can swap in for colour, era, materials, etc.
- **Known failure modes** when this prompt drifts
- **Generation parameters** that worked (model, aspect, seed if applicable)

## How to use this library

1. Pick the prompt that matches what you're trying to generate
2. Customise the variants (colour, etc.) in the prompt text
3. Generate in **small batches** (4–8 images per prompt iteration)
4. Drop the outputs into `~/Desktop/Background/` (or wherever you keep raw gens)
5. Visually inspect every output — keep what's good, delete what isn't
6. Run the pipeline on the survivors:
   ```bash
   .venv/bin/python qa_process.py --input <folder>
   ```
   (no `--vlm` flag — manual eyeball replaces it)

## When a prompt produces good results

Add it to this directory as `<category>_<style>.md`. Note what worked. Future
generations have a known-good starting point instead of starting from scratch.

## When a prompt fails

Update its file with:
- What went wrong (e.g. "renders the pedal at 3/4 angle 50% of the time")
- What you tried to fix it (and whether it helped)

The library is a working notebook, not a polished spec.

## Index

### Shared assets for the default pack
These five together compose every default-pack pedal — see
[`../pedals/clean_boost/DESIGN.md`](../pedals/clean_boost/DESIGN.md) for the
architecture.

| File | Asset | Status |
|------|-------|--------|
| [shared_chassis_body.md](shared_chassis_body.md) | Neutral chassis body, cover removed | v1, untested |
| [shared_chassis_cover.md](shared_chassis_cover.md) | Brushed-silver pedal cover | v1, untested |
| [shared_knob_brass_black.md](shared_knob_brass_black.md) | Two-tone knurled brass/black knob | v1, untested |
| [shared_led_bezel.md](shared_led_bezel.md) | Off-state LED in chrome bezel | v1, untested |
| [shared_footswitch_cap.md](shared_footswitch_cap.md) | Chrome 3PDT cap (NOT used by Boss-style flip-cover pedals; reserve for traditional-style designs) | v1, untested |

### One-off / non-default pedal prompts (older approach)

| File | Asset | Status |
|------|-------|--------|
| [chassis_skeuomorphic.md](chassis_skeuomorphic.md) | Full-coloured pedal chassis (one-off) | Superseded by shared assets for the default pack |

## Conventions in prompt text

These tokens have been observed to help across multiple image-gen models:

- **"top-down view"** + **"orthographic"** + **"flat lay"** — fight the urge
  to render at 3/4. Use all three; one alone often loses to perspective bias.
- **"isolated on pure white background"** + **"product photography lighting"** —
  produces backgrounds that rembg can cleanly remove.
- **"no text, no logos, no labels"** — text always comes out garbled.
- **"sharp focus, professional photography"** — gentle steer toward photo
  over illustration.
- **"as a finished product"** — discourages exposed wires / PCBs.
