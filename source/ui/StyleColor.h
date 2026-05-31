#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// StyleColor — the colorway axis (docs/control-catalog.md §6.5).
//
// The core trick (modelled on Apple's tinted app icons): a StyleKit's draw()
// code NEVER emits colours. For each painted element it emits an EMPHASIS
// (0..1 — how hot / active / important this region is) plus a ROLE tag. The
// active Colorway resolves that (emphasis, role) pair to an actual Colour.
//
// Because contrast is carried by emphasis (lightness/intensity) rather than by
// hue, structure survives any recolour: a tach in a blue colorway is blue up to
// the redline, and the redline is a *brighter* blue — danger == high emphasis ==
// the bright end of the ramp. Same draw code, any hue.
//
// Two resolution modes:
//   Tint     — one seed hue → a ramp; emphasis picks position. Max multiplier.
//   Semantic — discrete colours per role; status uses literal ok/warn/danger.
//
// A kit author writes draw() once; it works under both modes and any colorway.
//==============================================================================
namespace pf
{

/** What a painted element *means*, so the colorway can resolve it sensibly. */
enum class Role
{
    Chrome = 0, // panels, bezels, rings, tracks — structural surfaces
    Accent,     // the active/live element — knob pointer, fill, lit segment
    Text,       // labels, readouts
    Glow,       // lit-control bloom / addressable-light default tint
    Status      // status lamps, meter zones, redlines (emphasis = severity)
};

//==============================================================================
/** A resolvable colour scheme. Build one from a single seed for the headline
    multiplier, or specify discrete colours for literal safety cues. */
struct Colorway
{
    enum class Mode { Tint, Semantic };
    Mode mode = Mode::Semantic;

    // Structural anchors (used in both modes).
    juce::Colour bg      { 0xFF14141C };  // chassis / deepest surface
    juce::Colour surface { 0xFF1E1E2E };  // raised panel / control body
    juce::Colour edge    { 0xFF3A3A4A };  // bevel / ring / outline
    juce::Colour text    { 0xFFE0E0E0 };

    // Tint mode ramp endpoints: emphasis 0 → rampLow, 1 → rampHigh.
    juce::Colour rampLow  { 0xFF243B66 };
    juce::Colour rampHigh { 0xFF66B2FF };

    // Semantic mode discrete roles.
    juce::Colour accent { 0xFFF59E0B };
    juce::Colour glow   { 0xFF33FF66 };
    juce::Colour ok     { 0xFF33FF66 };
    juce::Colour warn   { 0xFFFFAA33 };
    juce::Colour danger { 0xFFFF3333 };

    //── resolution ───────────────────────────────────────────────────────────
    /** Resolve (emphasis, role) → Colour. `emphasis` is clamped to 0..1. */
    juce::Colour resolve (float emphasis, Role role) const noexcept
    {
        const float e = juce::jlimit (0.0f, 1.0f, emphasis);

        switch (role)
        {
            case Role::Text:
                // Emphasis dims secondary text; full emphasis = primary text.
                return text.withMultipliedAlpha (juce::jmap (e, 0.55f, 1.0f));

            case Role::Chrome:
                // Recessed (bg) → raised (edge) by emphasis.
                return e < 0.5f ? bg.interpolatedWith (surface, e * 2.0f)
                                : surface.interpolatedWith (edge, (e - 0.5f) * 2.0f);

            case Role::Accent:
                return ramp (e);

            case Role::Glow:
                return ramp (e).brighter (0.2f);

            case Role::Status:
                if (mode == Mode::Tint)
                    return ramp (e);                 // danger = bright end of hue
                // Semantic: severity bands.
                return e > 0.85f ? danger : (e > 0.6f ? warn : ok);
        }
        return text;
    }

    /** Position along the active "active-element" ramp. In Tint mode this is the
        seed-hue ramp; in Semantic mode it walks accent→a brighter accent so
        accent-role controls still respond to emphasis. */
    juce::Colour ramp (float e) const noexcept
    {
        e = juce::jlimit (0.0f, 1.0f, e);
        if (mode == Mode::Tint)
            return lerpHSB (rampLow, rampHigh, e);
        return lerpHSB (accent.darker (0.5f), accent.brighter (0.3f), e);
    }

    //── factories ─────────────────────────────────────────────────────────────
    /** Build a full Tint colorway from one seed hue (Apple-tinted-icon style).
        Fills the ramp + dark structural anchors and auto-corrects text contrast. */
    static Colorway tintFromSeed (juce::Colour seed)
    {
        Colorway c;
        c.mode = Mode::Tint;

        const float hue = seed.getHue();
        const float sat = juce::jlimit (0.0f, 1.0f, seed.getSaturation());

        c.rampLow  = juce::Colour::fromHSV (hue, juce::jmin (1.0f, sat * 0.9f), 0.42f, 1.0f);
        c.rampHigh = juce::Colour::fromHSV (hue, juce::jmin (1.0f, sat * 0.85f + 0.1f), 1.0f, 1.0f);

        // Near-neutral, hue-tinted dark chassis surfaces.
        c.bg      = juce::Colour::fromHSV (hue, sat * 0.25f, 0.07f, 1.0f);
        c.surface = juce::Colour::fromHSV (hue, sat * 0.22f, 0.14f, 1.0f);
        c.edge    = juce::Colour::fromHSV (hue, sat * 0.20f, 0.28f, 1.0f);

        c.accent = seed;
        c.glow   = c.rampHigh;
        c.text   = contrastingText (c.bg, hue, sat);
        return c;
    }

    /** Pick a legible text colour for the given background, tinted toward the
        scheme hue so it still reads as part of the colorway. */
    static juce::Colour contrastingText (juce::Colour background, float hue, float sat) noexcept
    {
        const bool darkBg = background.getPerceivedBrightness() < 0.5f;
        return darkBg ? juce::Colour::fromHSV (hue, sat * 0.15f, 0.92f, 1.0f)
                      : juce::Colour::fromHSV (hue, sat * 0.30f, 0.10f, 1.0f);
    }

private:
    /** Interpolate two colours through HSB so a ramp stays vivid (no muddy
        midpoint the way straight RGB lerp produces). */
    static juce::Colour lerpHSB (juce::Colour a, juce::Colour b, float t) noexcept
    {
        t = juce::jlimit (0.0f, 1.0f, t);

        float ha = a.getHue(), hb = b.getHue();
        // shortest path around the hue wheel
        float dh = hb - ha;
        if (dh > 0.5f)  dh -= 1.0f;
        if (dh < -0.5f) dh += 1.0f;
        const float h = ha + dh * t;

        const float s = juce::jmap (t, a.getSaturation(), b.getSaturation());
        const float v = juce::jmap (t, a.getBrightness(), b.getBrightness());
        const float al = juce::jmap (t, a.getFloatAlpha(), b.getFloatAlpha());
        return juce::Colour::fromHSV (h < 0 ? h + 1.0f : (h > 1 ? h - 1.0f : h), s, v, al);
    }
};

} // namespace pf
