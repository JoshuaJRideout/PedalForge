#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <vector>

//==============================================================================
// ControlState — the unified value carrier for the type × style render engine.
//
// Today HardwareDrawing dispatches on a single `float value`, which is already
// lossy (rgb_led is faked by passing value, value*0.5, 1-value). ControlState
// replaces that single float with one struct that carries every value-shape the
// control catalog needs, so multi-value controls (XY pads, lit knobs, EQ bars,
// scopes, annunciators) become first-class instead of faked.
//
// See docs/control-catalog.md §3. The 7 interaction archetypes map onto these
// fields:
//   1 continuous scalar  -> value
//   2 discrete           -> value (index)
//   3 2D / spatial       -> x, y, z
//   4 vector / array     -> array / arrayLen
//   5 signal display     -> buffer / bufferLen
//   6 text / selection   -> text
//   7 addressable light  -> light            (cross-cutting, §5.2)
// plus the cross-cutting status / zone metadata used by gauges, shift-lights,
// annunciators and meters.
//==============================================================================
namespace pf
{

/** Addressable light channel (archetype 7). Generalises the faked rgb_led into
    a value any control can expose — driven from the DSP graph. */
struct Light
{
    float r = 0.0f, g = 0.0f, b = 0.0f;
    float brightness = 1.0f;

    bool isOn() const noexcept { return (r + g + b) * brightness > 0.001f; }

    juce::Colour toColour() const noexcept
    {
        return juce::Colour::fromFloatRGBA (juce::jlimit (0.0f, 1.0f, r),
                                            juce::jlimit (0.0f, 1.0f, g),
                                            juce::jlimit (0.0f, 1.0f, b),
                                            1.0f)
                    .withMultipliedBrightness (juce::jlimit (0.0f, 1.0f, brightness));
    }
};

/** Annunciator / alarm state machine (cross-cutting, §5.2). A status lamp is no
    longer just on/off — it can blink and be acknowledged, matching cockpit /
    SCADA master-caution behaviour. */
enum class Status
{
    Off = 0,
    On,
    Blink,
    Ack       // was raised, operator acknowledged — typically steady/dimmed
};

//==============================================================================
/** One control's current value(s) for a single draw pass. All array / buffer
    pointers are non-owning and must outlive the draw call. Unused fields keep
    their defaults, so a plain knob just sets `value` exactly as before. */
struct ControlState
{
    // archetype 1 / 2 — primary scalar or discrete index
    float value = 0.0f;

    // archetype 3 — spatial axes (XY pad, joystick, vector pad)
    float x = 0.0f, y = 0.0f, z = 0.0f;

    // archetype 4 — bars / steps / bands (multislider, EQ, step grid, PIP…)
    const float* array = nullptr;
    int arrayLen = 0;

    // archetype 5 — waveform / rolling history samples (scope, spectrum, trend…)
    const float* buffer = nullptr;
    int bufferLen = 0;

    // archetype 7 — addressable light
    Light light;

    // archetype 6 — text lines / labels (text screen, scribble strip, browser)
    juce::StringArray text;

    //── cross-cutting metadata (§5) ──────────────────────────────────────────
    // annunciator / status lamp state
    Status status = Status::Off;

    // threshold bands for gauges / shift-lights / meters: flat pairs of
    // [position(0..1), emphasis(0..1)]. A renderer walks these to colour zones
    // (e.g. a tach redline = a high-emphasis band near 1.0). Non-owning.
    const float* zones = nullptr;
    int zonesLen = 0;   // number of floats (= 2 × band count)

    //── convenience constructors ─────────────────────────────────────────────
    ControlState() = default;
    ControlState (float v) : value (v) {}          // implicit: legacy single-float call sites

    static ControlState xy (float px, float py)
    {
        ControlState s; s.x = px; s.y = py; return s;
    }
};

//==============================================================================
/** Assemble a ControlState from a pedal instance's live value maps in one place,
    so every render call site fills the same carriers from the same sources.
    `data`/`texts` are optional (nullable). The returned state holds non-owning
    pointers into `data`'s vector, which must outlive the draw call (it lives in
    the instance map, so this is safe for an immediate draw). `controlData` is a
    single vector per control; it is exposed as BOTH `array` (bar/step renderers)
    and `buffer` (waveform/history renderers) — the renderer picks the reading. */
inline ControlState buildControlState (const juce::String& id,
                                       float value,
                                       const std::map<juce::String, std::vector<float>>* data = nullptr,
                                       const std::map<juce::String, juce::String>* texts = nullptr,
                                       const std::map<juce::String, float>* values = nullptr)
{
    ControlState s (value);

    // 2D / spatial axes — XY pad / joystick store per-axis values under
    // "<id>_x" / "<id>_y". Default to centre so an unwired pad sits mid-pad.
    s.x = 0.5f;
    s.y = 0.5f;
    if (values != nullptr)
    {
        auto ix = values->find (id + "_x");
        if (ix != values->end()) s.x = ix->second;
        auto iy = values->find (id + "_y");
        if (iy != values->end()) s.y = iy->second;
    }

    if (data != nullptr)
    {
        auto it = data->find (id);
        if (it != data->end() && ! it->second.empty())
        {
            s.array     = it->second.data();
            s.arrayLen  = (int) it->second.size();
            s.buffer    = it->second.data();
            s.bufferLen = (int) it->second.size();
        }
    }

    if (texts != nullptr)
    {
        auto it = texts->find (id);
        if (it != texts->end() && it->second.isNotEmpty())
            s.text.addLines (it->second);
    }

    return s;
}

} // namespace pf
