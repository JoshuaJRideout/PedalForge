#pragma once
#include "DSPNode.h"

//==============================================================================
// ─── CONTROL SURFACE NODES ──────────────────────────────────────────────────
//
// These nodes represent user-facing hardware controls on the pedal.
// They output a control signal that can be wired to anything in the graph.
// Only things connected to these nodes are exposed as parameters on the pedal.
//==============================================================================

/**
 * Knob Node — Continuous rotary control.
 * Outputs a control signal from min to max.
 * The "value" param IS the knob — it maps to a rotary knob on the pedal face.
 */
class KnobNode : public DSPNode
{
public:
    KnobNode() : DSPNode ("ctrl_knob", "Knob")
    {
        addOutput ("out", NodePort::Control);
        addParam ("value", "Value", 0.0f, 1.0f, 0.5f);
        addParam ("min", "Min", 0.0f, 1000.0f, 0.0f);
        addParam ("max", "Max", 0.0f, 1000.0f, 1.0f);
        // Curve: 0 = linear, 1 = logarithmic, 2 = exponential
        addParam ("curve", "Curve (0=lin,1=log,2=exp)", 0.0f, 2.0f, 0.0f);
    }

    void process (const float**, int, float** out, int, int n) override
    {
        float norm = getParam("value")->get();
        float mn = getParam("min")->get();
        float mx = getParam("max")->get();
        int curve = (int) getParam("curve")->get();
        
        float mapped = norm;
        switch (curve) {
            case 1: mapped = std::log10(1.0f + norm * 9.0f); break; // log
            case 2: mapped = norm * norm; break; // exponential
            default: break; // linear
        }
        
        float val = mn + mapped * (mx - mn);
        for (int i = 0; i < n; ++i)
            out[0][i] = val;
    }

    bool isControlSurface() const { return true; }
    juce::String getControlType() const { return "knob"; }
};

/**
 * Fader Node — Linear slider control.
 * Identical to Knob functionally, but maps to a fader/slider on the pedal face.
 */
class FaderNode : public DSPNode
{
public:
    FaderNode() : DSPNode ("ctrl_fader", "Fader")
    {
        addOutput ("out", NodePort::Control);
        addParam ("value", "Value", 0.0f, 1.0f, 0.5f);
        addParam ("min", "Min", 0.0f, 1000.0f, 0.0f);
        addParam ("max", "Max", 0.0f, 1000.0f, 1.0f);
    }

    void process (const float**, int, float** out, int, int n) override
    {
        float norm = getParam("value")->get();
        float mn = getParam("min")->get();
        float mx = getParam("max")->get();
        float val = mn + norm * (mx - mn);
        for (int i = 0; i < n; ++i)
            out[0][i] = val;
    }

    bool isControlSurface() const { return true; }
    juce::String getControlType() const { return "fader"; }
};

/**
 * Button Node — Momentary gate.
 * Outputs 1.0 while pressed, 0.0 when released.
 * Maps to a momentary footswitch or button on the pedal face.
 */
class ButtonNode : public DSPNode
{
public:
    ButtonNode() : DSPNode ("ctrl_button", "Button")
    {
        addOutput ("out", NodePort::Gate);
        addParam ("pressed", "Pressed", 0.0f, 1.0f, 0.0f);
    }

    void process (const float**, int, float** out, int, int n) override
    {
        float val = getParam("pressed")->get() > 0.5f ? 1.0f : 0.0f;
        for (int i = 0; i < n; ++i)
            out[0][i] = val;
    }

    bool isControlSurface() const { return true; }
    juce::String getControlType() const { return "button"; }
};

/**
 * Toggle Node — Latching on/off switch.
 * Outputs 1.0 when on, 0.0 when off.
 * Maps to a toggle switch or latching footswitch on the pedal face.
 */
class ToggleNode : public DSPNode
{
public:
    ToggleNode() : DSPNode ("ctrl_toggle", "Toggle")
    {
        addOutput ("out", NodePort::Gate);
        addParam ("state", "State", 0.0f, 1.0f, 0.0f);
    }

    void process (const float**, int, float** out, int, int n) override
    {
        float val = getParam("state")->get() > 0.5f ? 1.0f : 0.0f;
        for (int i = 0; i < n; ++i)
            out[0][i] = val;
    }

    bool isControlSurface() const { return true; }
    juce::String getControlType() const { return "toggle"; }
};

/**
 * Selector Node — Multi-position switch / dropdown.
 * Outputs an integer value (0 to N-1) as a control signal.
 * Maps to a rotary selector or dropdown on the pedal face.
 */
class SelectorNode : public DSPNode
{
public:
    SelectorNode() : DSPNode ("ctrl_selector", "Selector")
    {
        addOutput ("out", NodePort::Control);
        addParam ("selection", "Selection", 0.0f, 15.0f, 0.0f);
        addParam ("positions", "Positions", 2.0f, 16.0f, 4.0f);
    }

    void process (const float**, int, float** out, int, int n) override
    {
        int positions = (int) getParam("positions")->get();
        int sel = juce::jlimit (0, positions - 1, (int) getParam("selection")->get());
        float val = (float) sel;
        for (int i = 0; i < n; ++i)
            out[0][i] = val;
    }

    bool isControlSurface() const { return true; }
    juce::String getControlType() const { return "selector"; }
};

/**
 * XY Pad Node — Two-dimensional control surface.
 * Outputs X and Y as separate control signals (0-1 each).
 * Maps to a touchpad or XY controller on the pedal face.
 */
class XYPadNode : public DSPNode
{
public:
    XYPadNode() : DSPNode ("ctrl_xy", "XY Pad")
    {
        addOutput ("x", NodePort::Control);
        addOutput ("y", NodePort::Control);
        addParam ("x", "X", 0.0f, 1.0f, 0.5f);
        addParam ("y", "Y", 0.0f, 1.0f, 0.5f);
    }

    void process (const float**, int, float** out, int numOut, int n) override
    {
        float xv = getParam("x")->get();
        float yv = getParam("y")->get();
        for (int i = 0; i < n; ++i)
        {
            if (numOut > 0) out[0][i] = xv;
            if (numOut > 1) out[1][i] = yv;
        }
    }

    bool isControlSurface() const { return true; }
    juce::String getControlType() const { return "xy_pad"; }
};
