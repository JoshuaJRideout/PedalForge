# Shared Chassis Cover — Default Pack

**Status:** v3 — dropped the hinge line from the cover image. The hinge
is the pivot point; if it's painted into the cover PNG it would translate
down with the rest of the plate during the press animation. The cover is
now just the flat stomp plate; the hinge can be a separate static asset
or implied where the cover top meets the chassis.
**Audience:** all default-pack factory pedals share this one image.
**Output target:** near-white painted-metal transparent PNG, top-down.
Roughly 1024 × 1024 (fits the bottom ~55% of the chassis).

## What we're generating

The **pedal cover** — the hinged painted-metal plate the user "steps on" —
as a standalone object, isolated. **In a Boss-style flip-cover design the
cover IS the switch** — the whole plate pivots on the top hinge when
stepped on, actuating the switch underneath. There is no separate
footswitch button and no hole through the plate.

**Finish must match the chassis body** — both layers get the same runtime
tint at composite time, so the cover must be the same near-white painted
surface as [`shared_chassis_body`](shared_chassis_body.md). NOT brushed
metal — a real Boss-style cover is painted body-colour, not raw steel.

JUCE composites this on top of the chassis body's lower section and
animates it tilting forward a few px on press.

## Prompt

```
Photorealistic top-down product photograph of a guitar pedal foot-cover
plate, the hinged step-on cover from a Boss-style stompbox effect pedal,
shown as an isolated standalone object on a white background.

Camera is directly overhead, perfectly orthographic, NOT perspective,
NOT angled, NOT 3/4 view.

The cover is a SOLID, UNBROKEN rectangular die-cast aluminum plate with
gently rounded corners on ALL FOUR sides. Wider than tall (landscape
orientation), approximately 1.4:1 aspect ratio. The cover surface is
COMPLETELY UNINTERRUPTED — NO holes, NO openings, NO buttons, NO
footswitch cap protruding, NO indents, NO cutouts, NO hinge lines, NO
seams, NO parting lines, NO screws. The plate IS the footswitch in a
Boss-style design (pressing the whole plate pivots it on a hinge that is
located elsewhere — outside this image — and triggers the switch
underneath).

This image must contain ONLY the flat stomp plate itself. The hinge is a
separate concern (composited or implied at runtime) and MUST NOT appear
in this image. No top-edge seam, no hinge bar, no hinge pin, no parting
line, no shadow line suggesting a hinge. The top edge of the plate is
identical to the bottom edge — both are smooth rounded edges.

Finish: SOLID NEAR-WHITE PAINTED metal, RGB approximately (235, 235, 235).
The cover is PAINTED, not raw metal — a soft matte / satin paint finish
that catches light realistically, exactly like a freshly painted Boss-style
cover before it gets its colour coat. PURE GRAYSCALE only. NO color tint
— no green, no blue, no red, no warm or cool bias. NO brushed-metal grain,
NO raw steel, NO stainless steel finish, NO silver, NO chrome. Just smooth
painted metal that can be MULTIPLY-tinted to any colour at composite time.

A gentle gradient from slightly brighter near the top edge (catching light)
to slightly darker near the bottom edge, implying soft overhead studio
lighting on a painted surface.

ABSOLUTELY NO visible screws on the cover face. NO text, NO logos, NO
branding, NO silkscreen, NO labels.

Soft even studio lighting from above. Pure white background. Isolated
subject, no chassis around it, no other components, no shadows on the
background, no props, no hands.

Sharp focus. Professional product photography. Photorealistic. Subject
centered with room around for bounding-box detection.
```

## Negative prompts

```
hinge, hinge line, hinge bar, hinge pin, hinge seam, top seam, parting
line, seam, pivot, brushed metal, brushed steel, brushed aluminum, brushed
grain, brushing, metal grain, raw metal, stainless steel, chrome, silver
finish, mirror finish, polished metal, exposed metal, center hole,
footswitch hole,
button hole, cutout, opening, perforation, break in surface, footswitch
button, footswitch cap, button, dome, switch sticking out, perspective,
3/4, angled, tilted, side view, screws, screw heads, Phillips, hex,
exposed hardware, text, logo, branding, brand name, label, silkscreen,
colored, color cast, tinted, green, blue, red, warm tone, cool tone,
saturated, knobs, chassis body, full pedal, other pedals, illustration,
cartoon, sketch, drawing, 3D render obviously CGI, painting, hands,
fingers, person
```

## Acceptance bar

Keep:
- Solid uninterrupted painted plate, top-down
- All four edges identical (rounded, no top hinge seam)
- Same near-white painted finish as the body (eyedropper R≈G≈B, no metal
  grain visible)
- NO hole, button, footswitch, or hinge anywhere on the surface
- Clean white background

Reject:
- Any hinge line / seam / parting line visible on the plate
- Brushed-metal / stainless / chrome / silver finish (we need painted)
- Any hole or cutout in the cover
- Footswitch / button visible on the cover
- Visible screws
- Any color cast (warm, cool, green, blue, etc.)
- Sharp specular blowout (we want gentle highlights)
- Anything other than the cover in frame

## Iteration log

| Date | Tool | What we changed | Result |
|------|------|-----------------|--------|
| 2026-05-25 | gpt-image-1 | Brushed silver covers — user feedback: cover and body must match because both get runtime-tinted | Switched prompt to near-white painted to match body |
