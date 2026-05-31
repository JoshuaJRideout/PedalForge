#pragma once
#include "StyleKit.h"

//==============================================================================
// NeonStyleKit — the first non-default StyleKit, proving the `style` axis of the
// type × style × colorway engine end-to-end (docs/control-catalog.md §6).
//
// Aesthetic: flat, minimal, "glowing FUI" — no skeuomorphic bevels. A knob is a
// dark flat disc with a bright value-ARC sweeping its rim (vs the default's
// beveled ellipse + single pointer line). Everything reads its colour from the
// active Colorway via cwColour(), so a neon pedal still responds to colorways;
// when no colorway is set it falls back to a built-in cyan-ish neon palette.
//
// It is PARTIAL on purpose: draws() returns true only for the core interactive
// types it restyles. Any other type (displays, scopes, loaders…) falls through
// to the default kit per-type via StyleKitRegistry::resolve — so the kit can
// ship incrementally and a mixed pedal still looks coherent.
//==============================================================================
namespace pf
{

class NeonStyleKit : public StyleKit
{
public:
    juce::String getId() const override { return "neon"; }

    juce::StringArray signatureTypes() const override
    {
        return { "knob", "fader", "led", "switch", "footswitch", "selector" };
    }

    bool draws (const juce::String& type) const override
    {
        return type == "knob" || type == "fader" || type == "led"
            || type == "switch" || type == "footswitch" || type == "selector";
    }

    void draw (juce::Graphics& g,
               const juce::String& type,
               juce::Rectangle<float> area,
               const ControlState& state,
               const Colorway& colorway,
               const HardwareDrawing::CustomStyles* custom) const override
    {
        // Image overrides still win — a user who supplied artwork wants it shown
        // regardless of kit. Defer to the default kit, which handles all the
        // imageMain/imageStates plumbing.
        if (custom != nullptr && (custom->imageMain.isNotEmpty()
                                  || ! custom->imageStates.isEmpty()))
        {
            StyleKitRegistry::defaultKit().draw (g, type, area, state, colorway, custom);
            return;
        }

        const pf::Colorway* cw = colorway.active ? &colorway : nullptr;
        const float v = state.value;

        if      (type == "knob")        drawKnob (g, area, v, cw);
        else if (type == "fader")       drawFader (g, area, v, cw);
        else if (type == "led")         drawLed (g, area, v, cw, custom);
        else if (type == "switch")      drawToggle (g, area, v, cw);
        else if (type == "footswitch")  drawStomp (g, area, v, cw);
        else if (type == "selector")    drawSelector (g, area, v, cw, custom);
        else
            StyleKitRegistry::defaultKit().draw (g, type, area, state, colorway, custom);
    }

private:
    // Neon fallback palette (used when no colorway is active). Cyan-forward so
    // the kit has a clear identity even on a colorway-less pedal.
    static juce::Colour fillCol   (const pf::Colorway* cw) { return col (cw, 0.08f, Role::Chrome, juce::Colour (0xFF0E1419)); }
    static juce::Colour trackCol  (const pf::Colorway* cw) { return col (cw, 0.40f, Role::Chrome, juce::Colour (0xFF1B2B33)); }
    static juce::Colour accentCol (const pf::Colorway* cw, float e) { return col (cw, e, Role::Accent, juce::Colour (0xFF22E0D0)); }
    static juce::Colour glowCol   (const pf::Colorway* cw) { return col (cw, 1.0f, Role::Glow, juce::Colour (0xFF2FF0E0)); }

    static juce::Colour col (const pf::Colorway* cw, float e, Role r, juce::Colour fallback)
    {
        if (cw != nullptr && cw->active) return cw->resolve (e, r);
        return fallback;
    }

    //── knob: flat disc + rim value-arc + endpoint dot ────────────────────────
    static void drawKnob (juce::Graphics& g, juce::Rectangle<float> area, float value, const pf::Colorway* cw)
    {
        const float cx = area.getCentreX(), cy = area.getCentreY();
        const float r  = juce::jmin (area.getWidth(), area.getHeight()) * 0.45f;
        const float arc = juce::degreesToRadians (270.0f);
        const float a0  = -arc * 0.5f;

        // Flat body
        g.setColour (fillCol (cw));
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);

        const float ringR = r * 0.86f;
        const float thick = juce::jmax (2.0f, r * 0.16f);

        // Unlit track
        juce::Path track;
        track.addCentredArc (cx, cy, ringR, ringR, 0.0f, a0, a0 + arc, true);
        g.setColour (trackCol (cw));
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Lit value arc (emphasis tracks value → brighter when hotter under Tint)
        juce::Path lit;
        lit.addCentredArc (cx, cy, ringR, ringR, 0.0f, a0, a0 + arc * juce::jlimit (0.0f, 1.0f, value), true);
        g.setColour (accentCol (cw, value));
        g.strokePath (lit, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Endpoint dot
        const float ang = a0 + arc * juce::jlimit (0.0f, 1.0f, value);
        const float dx = cx + std::sin (ang + juce::MathConstants<float>::pi) * 0.0f + ringR * std::cos (ang - juce::MathConstants<float>::halfPi);
        const float dy = cy + ringR * std::sin (ang - juce::MathConstants<float>::halfPi);
        const float dot = juce::jmax (2.0f, r * 0.13f);
        g.setColour (glowCol (cw));
        g.fillEllipse (dx - dot, dy - dot, dot * 2.0f, dot * 2.0f);
    }

    //── fader: thin slot + glowing cap bar ────────────────────────────────────
    static void drawFader (juce::Graphics& g, juce::Rectangle<float> area, float value, const pf::Colorway* cw)
    {
        auto b = area.reduced (area.getWidth() * 0.32f, area.getHeight() * 0.08f);
        const float slotW = juce::jmax (2.0f, b.getWidth() * 0.18f);
        juce::Rectangle<float> slot (b.getCentreX() - slotW * 0.5f, b.getY(), slotW, b.getHeight());

        g.setColour (trackCol (cw));
        g.fillRoundedRectangle (slot, slotW * 0.5f);

        // Lit fill from bottom up to value
        const float fillTop = b.getBottom() - b.getHeight() * juce::jlimit (0.0f, 1.0f, value);
        juce::Rectangle<float> fill (slot.getX(), fillTop, slotW, b.getBottom() - fillTop);
        g.setColour (accentCol (cw, value));
        g.fillRoundedRectangle (fill, slotW * 0.5f);

        // Cap bar
        const float capH = juce::jmax (3.0f, b.getHeight() * 0.05f);
        const float capW = b.getWidth();
        juce::Rectangle<float> cap (b.getCentreX() - capW * 0.5f, fillTop - capH * 0.5f, capW, capH);
        g.setColour (glowCol (cw));
        g.fillRoundedRectangle (cap, capH * 0.5f);
    }

    //── led: flat ring + glowing core ─────────────────────────────────────────
    static void drawLed (juce::Graphics& g, juce::Rectangle<float> area, float value,
                         const pf::Colorway* cw, const HardwareDrawing::CustomStyles* custom)
    {
        const float cx = area.getCentreX(), cy = area.getCentreY();
        const float r  = juce::jmin (area.getWidth(), area.getHeight()) * 0.34f;
        const bool on  = value > 0.5f;

        juce::Colour core = (cw != nullptr && cw->active)
                              ? cw->resolve (1.0f, Role::Glow)
                              : (custom != nullptr ? custom->customColour : juce::Colour (0xFF2FF0E0));

        g.setColour (trackCol (cw));
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, juce::jmax (1.0f, r * 0.18f));

        if (on)
        {
            g.setGradientFill (juce::ColourGradient (core, cx, cy,
                                                     core.withAlpha (0.0f), cx, cy - r * 2.2f, true));
            g.fillEllipse (cx - r * 1.8f, cy - r * 1.8f, r * 3.6f, r * 3.6f);
            g.setColour (core.brighter (0.2f));
        }
        else
        {
            g.setColour (core.darker (0.85f));
        }
        g.fillEllipse (cx - r * 0.62f, cy - r * 0.62f, r * 1.24f, r * 1.24f);
    }

    //── switch (toggle): pill track + sliding knob ────────────────────────────
    static void drawToggle (juce::Graphics& g, juce::Rectangle<float> area, float value, const pf::Colorway* cw)
    {
        auto b = area.reduced (area.getWidth() * 0.06f, area.getHeight() * 0.28f);
        const float rad = b.getHeight() * 0.5f;
        const bool on = value > 0.5f;

        g.setColour (on ? accentCol (cw, 0.9f) : trackCol (cw));
        g.fillRoundedRectangle (b, rad);

        const float kr = rad * 0.82f;
        const float kx = on ? (b.getRight() - rad) : (b.getX() + rad);
        g.setColour (glowCol (cw));
        g.fillEllipse (kx - kr, b.getCentreY() - kr, kr * 2.0f, kr * 2.0f);
    }

    //── footswitch: ring + glowing centre when engaged ────────────────────────
    static void drawStomp (juce::Graphics& g, juce::Rectangle<float> area, float value, const pf::Colorway* cw)
    {
        const float cx = area.getCentreX(), cy = area.getCentreY();
        const float r  = juce::jmin (area.getWidth(), area.getHeight()) * 0.42f;
        const bool on  = value > 0.5f;

        g.setColour (trackCol (cw));
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, juce::jmax (2.0f, r * 0.14f));

        const float ir = r * 0.6f;
        g.setColour (on ? accentCol (cw, 0.95f) : fillCol (cw));
        g.fillEllipse (cx - ir, cy - ir, ir * 2.0f, ir * 2.0f);
        if (on)
        {
            g.setColour (glowCol (cw).withAlpha (0.5f));
            g.drawEllipse (cx - ir, cy - ir, ir * 2.0f, ir * 2.0f, juce::jmax (1.0f, r * 0.08f));
        }
    }

    //── selector: tick ring + lit active tick + arm ───────────────────────────
    static void drawSelector (juce::Graphics& g, juce::Rectangle<float> area, float value,
                              const pf::Colorway* cw, const HardwareDrawing::CustomStyles* custom)
    {
        const int positions = juce::jlimit (2, 16, custom != nullptr ? custom->positions : 4);
        int sel = value > 1.0f ? (int) std::floor (value + 0.5f)
                               : (int) std::floor (value * (float) positions);
        sel = juce::jlimit (0, positions - 1, sel);

        const float cx = area.getCentreX(), cy = area.getCentreY();
        const float r  = juce::jmin (area.getWidth(), area.getHeight()) * 0.45f;
        const float arc = juce::degreesToRadians (270.0f);
        const float a0  = -arc * 0.5f - juce::MathConstants<float>::halfPi;

        g.setColour (fillCol (cw));
        g.fillEllipse (cx - r * 0.7f, cy - r * 0.7f, r * 1.4f, r * 1.4f);

        for (int i = 0; i < positions; ++i)
        {
            const float frac = positions > 1 ? (float) i / (float) (positions - 1) : 0.5f;
            const float a = a0 + frac * arc;
            const float x0 = cx + r * 0.78f * std::cos (a), y0 = cy + r * 0.78f * std::sin (a);
            const float x1 = cx + r * 1.0f  * std::cos (a), y1 = cy + r * 1.0f  * std::sin (a);
            g.setColour (i == sel ? accentCol (cw, 0.95f) : trackCol (cw));
            g.drawLine (x0, y0, x1, y1, juce::jmax (1.5f, r * 0.08f));
        }

        // Arm to the active tick
        const float frac = positions > 1 ? (float) sel / (float) (positions - 1) : 0.5f;
        const float a = a0 + frac * arc;
        g.setColour (glowCol (cw));
        g.drawLine (cx, cy, cx + r * 0.62f * std::cos (a), cy + r * 0.62f * std::sin (a),
                    juce::jmax (1.5f, r * 0.1f));
    }
};

//==============================================================================
/** Register the built-in non-default kits with the registry. Idempotent — safe
    to call from every editor construction (plugin + standalone both build an
    editor). Kit instances are function-local statics so they outlive the
    registry's non-owning pointers for the process lifetime. */
inline void registerBuiltinStyleKits()
{
    static NeonStyleKit neon;
    auto& kits = StyleKitRegistry::kits();
    for (auto* k : kits)
        if (k->getId() == neon.getId()) return;   // already registered
    kits.push_back (&neon);
}

} // namespace pf
