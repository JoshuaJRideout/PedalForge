#pragma once
#include "HardwareDrawing.h"
#include "ControlState.h"
#include "StyleColor.h"

//==============================================================================
// StyleKit — the `style` axis of the type × style × colorway engine
// (docs/control-catalog.md §6). A StyleKit knows how to render a set of control
// types. The user picks one kit per pedal ("render this in LCARS"); a kit may be
// partial and any type it doesn't implement falls back to the default kit
// per-type, so kits can ship incrementally and still look coherent.
//
// PHASE 0 NOTE: the DefaultStyleKit reproduces today's EXACT visuals by
// delegating to the existing HardwareDrawing functions verbatim (sourcing rich
// data — waveforms, text, pixels — from ControlState). It is intentionally
// colorway-unaware, exactly like the code it replaces, so the refactor is
// behaviour-preserving. New kits (LCARS, race, …) are the ones that emit
// emphasis + role and honour the Colorway. See the doc for the build sequence.
//==============================================================================
namespace pf
{

//==============================================================================
/** Abstract render strategy for control types. */
class StyleKit
{
public:
    virtual ~StyleKit() = default;

    /** Stable id, e.g. "default", "lcars", "race". */
    virtual juce::String getId() const = 0;

    /** True if this kit implements `type` itself (else the caller falls back to
        the default kit for that one type). */
    virtual bool draws (const juce::String& type) const = 0;

    /** Render one control. `colorway` is honoured by themed kits; the default
        kit ignores it (preserving legacy behaviour). `custom` carries per-control
        image/colour overrides and may be null. */
    virtual void draw (juce::Graphics& g,
                       const juce::String& type,
                       juce::Rectangle<float> area,
                       const ControlState& state,
                       const Colorway& colorway,
                       const HardwareDrawing::CustomStyles* custom) const = 0;

    /** On-theme controls this kit renders best — surfaced first in the UI. */
    virtual juce::StringArray signatureTypes() const { return {}; }
};

//==============================================================================
/** The universal fallback kit: pixel-identical to the pre-refactor renderer. */
class DefaultStyleKit : public StyleKit
{
public:
    juce::String getId() const override { return "default"; }

    // The default kit is the catch-all — it draws every type (its `else` branch
    // matches HardwareDrawing::drawForType's placeholder).
    bool draws (const juce::String&) const override { return true; }

    void draw (juce::Graphics& g,
               const juce::String& type,
               juce::Rectangle<float> area,
               const ControlState& state,
               const Colorway& /*colorway*/,
               const HardwareDrawing::CustomStyles* custom) const override
    {
        const float value = state.value;

        if (type == "knob" || type == "slider")          HardwareDrawing::drawKnob (g, area, value, custom);
        else if (type == "xypad")                         HardwareDrawing::drawXYPad (g, area, state.x, state.y, custom);
        else if (type == "joystick")                      HardwareDrawing::drawJoystick (g, area, state.x, state.y, custom);
        else if (type == "switch")                        HardwareDrawing::drawSwitch (g, area, value, custom);
        else if (type == "selector")                      HardwareDrawing::drawSelector (g, area, value, custom);
        else if (type == "led")                           HardwareDrawing::drawLED (g, area, value, custom);
        else if (type == "footswitch")                    HardwareDrawing::drawFootswitch (g, area, value, custom);
        else if (type == "fader")                         HardwareDrawing::drawFader (g, area, value, custom);
        // Display types
        else if (type == "7seg")                          HardwareDrawing::draw7Seg (g, area, value * 999.0f, 3, juce::Colour (0xFFFF3333), custom);
        else if (type == "display")                       HardwareDrawing::drawNumericDisplay (g, area, value, custom);
        else if (type == "vu_meter")                      HardwareDrawing::drawVUMeter (g, area, value, custom);
        else if (type == "indicator")                     HardwareDrawing::drawIndicator (g, area, value, 0.6f, 0.85f, custom);
        else if (type == "oscilloscope")                  HardwareDrawing::drawOscilloscope (g, area, state.buffer, state.bufferLen, custom);
        else if (type == "rgb_led")                       HardwareDrawing::drawRGBLED (g, area, value, value * 0.5f, 1.0f - value, custom);
        else if (type == "led_toggle")                    HardwareDrawing::drawRGBLED (g, area, value > 0.5f ? 1.0f : 0.0f, value > 0.5f ? 1.0f : 0.0f, 0.0f, custom);
        else if (type == "text_screen" || type == "easy_display")
        {
            juce::StringArray lines = state.text.isEmpty() ? juce::StringArray { "Ready" } : state.text;
            HardwareDrawing::drawTextScreen (g, area, lines, -1, custom);
        }
        else if (type == "console")
        {
            juce::StringArray lines = state.text.isEmpty() ? juce::StringArray { "[log]" } : state.text;
            HardwareDrawing::drawTextScreen (g, area, lines, -1, custom);
        }
        else if (type == "pixel_display")                 HardwareDrawing::drawPixelDisplay (g, area, state.buffer, 32, 16, false, custom);
        else if (type == "graphic")                       HardwareDrawing::drawGraphic (g, area, custom);
        else if (type == "label")                         HardwareDrawing::drawTextLabel (g, area, state.text.isEmpty() ? juce::String ("LABEL") : state.text[0], custom);
        else if (type == "file_loader" || type == "file_browser" || type == "plugin_browser"
                 || type == "library_loader" || type == "overlay_launcher")
        {
            // Loaders/launchers: keep HardwareDrawing's button placeholder so the
            // designer preview is unchanged. (Live call sites draw their own
            // labelled buttons and don't route through the kit.)
            HardwareDrawing::drawForType (g, type, area, value, custom);
        }
        else { g.setColour (juce::Colours::grey); g.drawRect (area, 1.0f); }
    }
};

//==============================================================================
/** Registry of available kits + the per-type fallback rule. Phase 0 ships only
    the default kit; new kits register here as they land. */
namespace StyleKitRegistry
{
    inline DefaultStyleKit& defaultKit()
    {
        static DefaultStyleKit k;
        return k;
    }

    /** All non-default kits, keyed by id. New kits push_back here at startup. */
    inline std::vector<StyleKit*>& kits()
    {
        static std::vector<StyleKit*> v;
        return v;
    }

    inline StyleKit* find (const juce::String& id)
    {
        if (id.isEmpty() || id == "default") return &defaultKit();
        for (auto* k : kits())
            if (k->getId() == id) return k;
        return &defaultKit();
    }

    /** Resolve the kit that should draw `type` for the requested `kitId`,
        applying the partial-kit fallback: the requested kit if it draws this
        type, otherwise the default kit. */
    inline StyleKit& resolve (const juce::String& kitId, const juce::String& type)
    {
        auto* k = find (kitId);
        if (k != nullptr && k != &defaultKit() && k->draws (type))
            return *k;
        return defaultKit();
    }

    /** Convenience: draw a control through the resolved kit. */
    inline void draw (juce::Graphics& g,
                      const juce::String& kitId,
                      const juce::String& type,
                      juce::Rectangle<float> area,
                      const ControlState& state,
                      const Colorway& colorway,
                      const HardwareDrawing::CustomStyles* custom)
    {
        resolve (kitId, type).draw (g, type, area, state, colorway, custom);
    }
}

} // namespace pf
