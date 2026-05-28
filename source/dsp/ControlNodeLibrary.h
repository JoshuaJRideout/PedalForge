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

    bool isControlSurface() const override { return true; }
    juce::String getControlType() const override { return "knob"; }
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

    bool isControlSurface() const override { return true; }
    juce::String getControlType() const override { return "fader"; }
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

    bool isControlSurface() const override { return true; }
    juce::String getControlType() const override { return "footswitch"; }
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

    bool isControlSurface() const override { return true; }
    juce::String getControlType() const override { return "switch"; }
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

    bool isControlSurface() const override { return true; }
    juce::String getControlType() const override { return "selector"; }
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

    // XY pads aren't yet renderable on the pedal face; treat as non-surface for
    // now so we don't spawn an invisible control. Re-enable when HardwareDrawing
    // gains an xy_pad case.
    bool isControlSurface() const override { return false; }
    juce::String getControlType() const override { return {}; }
};

//==============================================================================
class PitchDetectorNode : public DSPNode
{
public:
    PitchDetectorNode() : DSPNode ("pitch_det", "Pitch Detector")
    {
        addInput ("in", NodePort::Audio);
        addOutput ("pitch_cv", NodePort::Control);
        addOutput ("gate", NodePort::Gate);
        addParam ("sensitivity", "Sensitivity", 0.0f, 1.0f, 0.5f);
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        DSPNode::prepare (sampleRate, maxBlockSize);
        sr = sampleRate;
        pitch = 0.0f;
        gate = 0.0f;
    }
    // Simplistic zero-crossing pitch detector for demo purposes
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        float sens = getParam("sensitivity")->get();
        float threshold = 0.05f * (1.0f - sens + 0.01f);
        
        for (int i = 0; i < n; ++i)
        {
            float val = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            
            // Simple envelope
            env += 0.01f * (std::abs(val) - env);
            gate = (env > threshold) ? 1.0f : 0.0f;
            
            if (val > 0.0f && lastVal <= 0.0f && gate > 0.5f)
            {
                if (samplesSinceZero > 10)
                {
                    float freq = (float)sr / (float)samplesSinceZero;
                    // V/Oct roughly: 0V = C0 (16.35Hz)
                    if (freq > 10.0f && freq < 10000.0f)
                    {
                        float vOct = std::log2(freq / 16.3516f);
                        pitch = vOct;
                    }
                }
                samplesSinceZero = 0;
            }
            else
            {
                samplesSinceZero++;
            }
            
            lastVal = val;
            
            out[0][i] = pitch;
            if (numOut > 1 && out[1]) out[1][i] = gate;
        }
    }
private:
    float lastVal = 0.0f;
    float pitch = 0.0f;
    float gate = 0.0f;
    float env = 0.0f;
    int samplesSinceZero = 0;
    double sr = 44100.0;
};

class TransientDetectorNode : public DSPNode
{
public:
    TransientDetectorNode() : DSPNode ("transient_det", "Transient Detector")
    {
        addInput ("in", NodePort::Audio);
        addOutput ("trigger", NodePort::Gate);
        addParam ("threshold", "Threshold", 0.0f, 1.0f, 0.5f);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        float thresh = getParam("threshold")->get();
        for (int i = 0; i < n; ++i)
        {
            float val = (numIn > 0 && in[0]) ? std::abs(in[0][i]) : 0.0f;
            
            float diff = val - env;
            env += 0.05f * (val - env); // Fast attack, slow release approx
            
            if (diff > thresh && !triggered)
            {
                out[0][i] = 1.0f;
                triggered = true;
            }
            else
            {
                out[0][i] = 0.0f;
                if (diff < thresh * 0.5f) triggered = false;
            }
        }
    }
private:
    float env = 0.0f;
    bool triggered = false;
};

class ZeroCrossingNode : public DSPNode
{
public:
    ZeroCrossingNode() : DSPNode ("zero_cross", "Zero-Crossing")
    {
        addInput ("in", NodePort::Audio);
        addOutput ("out", NodePort::Gate);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
        {
            float val = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            out[0][i] = (val > 0.0f && lastVal <= 0.0f) ? 1.0f : 0.0f;
            lastVal = val;
        }
    }
private:
    float lastVal = 0.0f;
};

class PIDControllerNode : public DSPNode
{
public:
    PIDControllerNode() : DSPNode ("pid_ctrl", "PID Controller")
    {
        addInput ("setpoint", NodePort::Control);
        addInput ("process_var", NodePort::Control);
        addOutput ("control", NodePort::Control);
        addParam ("kp", "P", 0.0f, 10.0f, 1.0f);
        addParam ("ki", "I", 0.0f, 10.0f, 0.1f);
        addParam ("kd", "D", 0.0f, 10.0f, 0.05f);
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        DSPNode::prepare (sampleRate, maxBlockSize);
        integral = 0.0f;
        prevError = 0.0f;
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        float kp = getParam("kp")->get();
        float ki = getParam("ki")->get();
        float kd = getParam("kd")->get();
        
        for (int i = 0; i < n; ++i)
        {
            float sp = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            float pv = (numIn > 1 && in[1]) ? in[1][i] : 0.0f;
            
            float error = sp - pv;
            integral += error;
            float derivative = error - prevError;
            
            out[0][i] = (kp * error) + (ki * integral) + (kd * derivative);
            prevError = error;
        }
    }
private:
    float integral = 0.0f;
    float prevError = 0.0f;
};
