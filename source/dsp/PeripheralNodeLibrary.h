#pragma once
#include "DSPNode.h"

//==============================================================================
// ─── PERIPHERAL & DISPLAY NODES ─────────────────────────────────────────────
//
// Nodes that represent physical components on the pedal face:
//   - LEDs (status lights)
//   - Displays (numeric readouts)
//   - Meters (level indicators)
//   - I/O peripherals (expression pedal, CV input, etc.)
//
// These are "sinks" — they consume data from the graph and present it visually
// on the pedal face, or "sources" that read from physical peripherals.
//==============================================================================

/**
 * LED Node — A single-color status LED on the pedal face.
 * Driven by a gate or control signal:
 *   - Gate input: on/off (> 0.5 = on)
 *   - Brightness input: 0-1 for PWM-style dimming
 */
class LEDNode : public DSPNode
{
public:
    LEDNode() : DSPNode ("disp_led", "LED")
    {
        addInput ("on", NodePort::Gate);
        addInput ("brightness", NodePort::Control);
        // Color stored as hue (0-360), selectable in properties
        addParam ("hue", "Hue (0=red,120=grn,240=blu)", 0.0f, 360.0f, 0.0f);
        addParam ("default_brightness", "Default Brightness", 0.0f, 1.0f, 1.0f);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        // Read last sample for display state
        if (numIn > 0 && in[0])
            ledOn = in[0][n - 1] > 0.5f;
        if (numIn > 1 && in[1])
            brightness = in[1][n - 1];
        else
            brightness = getParam("default_brightness")->get();
    }

    bool isDisplayNode() const { return true; }
    juce::String getDisplayType() const { return "led"; }

    // Readable state for the pedal painter
    bool ledOn = false;
    float brightness = 1.0f;
};

/**
 * RGB LED Node — Full-color LED driven by R, G, B control signals.
 */
class RGBLEDNode : public DSPNode
{
public:
    RGBLEDNode() : DSPNode ("disp_rgb_led", "RGB LED")
    {
        addInput ("red", NodePort::Control);
        addInput ("green", NodePort::Control);
        addInput ("blue", NodePort::Control);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        r = (numIn > 0 && in[0]) ? juce::jlimit (0.0f, 1.0f, in[0][n - 1]) : 0.0f;
        g = (numIn > 1 && in[1]) ? juce::jlimit (0.0f, 1.0f, in[1][n - 1]) : 0.0f;
        b = (numIn > 2 && in[2]) ? juce::jlimit (0.0f, 1.0f, in[2][n - 1]) : 0.0f;
    }

    bool isDisplayNode() const { return true; }
    juce::String getDisplayType() const { return "rgb_led"; }

    float r = 0.0f, g = 0.0f, b = 0.0f;
};

/**
 * Display Node — Numeric display on the pedal face.
 * Shows a control signal value with configurable formatting.
 */
class DisplayNode : public DSPNode
{
public:
    DisplayNode() : DSPNode ("disp_display", "Display")
    {
        addInput ("value", NodePort::Control);
        addParam ("min_label", "Min Label Value", 0.0f, 10000.0f, 0.0f);
        addParam ("max_label", "Max Label Value", 0.0f, 10000.0f, 100.0f);
        addParam ("decimals", "Decimal Places", 0.0f, 4.0f, 1.0f);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        if (numIn > 0 && in[0])
        {
            float raw = in[0][n - 1];
            float mn = getParam("min_label")->get();
            float mx = getParam("max_label")->get();
            displayValue = mn + raw * (mx - mn);
        }
    }

    bool isDisplayNode() const { return true; }
    juce::String getDisplayType() const { return "display"; }

    float displayValue = 0.0f;
};

/**
 * VU Meter Node — Level meter on the pedal face.
 * Takes audio input and computes RMS/peak level for display.
 */
class VUMeterNode : public DSPNode
{
public:
    VUMeterNode() : DSPNode ("disp_vu", "VU Meter")
    {
        addInput ("audio", NodePort::Audio);
        addParam ("release", "Release (ms)", 10.0f, 2000.0f, 300.0f);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        if (numIn < 1 || !in[0]) return;
        
        float releaseMs = getParam("release")->get();
        float releaseCoeff = std::exp (-1.0f / (sr * releaseMs * 0.001f));

        for (int i = 0; i < n; ++i)
        {
            float sample = std::abs (in[0][i]);
            
            // Peak with release
            if (sample > peakLevel)
                peakLevel = sample;
            else
                peakLevel *= releaseCoeff;

            // RMS accumulator
            rmsSum += sample * sample;
            rmsCount++;
        }

        // Update RMS every ~1024 samples
        if (rmsCount >= 1024)
        {
            rmsLevel = std::sqrt (rmsSum / (float)rmsCount);
            rmsSum = 0.0f;
            rmsCount = 0;
        }
    }

    void reset() override { peakLevel = 0; rmsLevel = 0; rmsSum = 0; rmsCount = 0; }

    bool isDisplayNode() const { return true; }
    juce::String getDisplayType() const { return "vu_meter"; }

    float peakLevel = 0.0f;
    float rmsLevel = 0.0f;

private:
    float rmsSum = 0.0f;
    int rmsCount = 0;
};

/**
 * Tuner Display Node — Shows pitch detection info on the pedal face.
 * Outputs detected note name, cents sharp/flat, and frequency.
 */
class TunerDisplayNode : public DSPNode
{
public:
    TunerDisplayNode() : DSPNode ("disp_tuner", "Tuner Display")
    {
        addInput ("audio", NodePort::Audio);
        addOutput ("frequency", NodePort::Control);
        addOutput ("cents", NodePort::Control);
        addParam ("ref_freq", "Reference A4 (Hz)", 420.0f, 460.0f, 440.0f);
    }

    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numIn < 1 || !in[0]) return;

        // Simple zero-crossing pitch detection
        float refA4 = getParam("ref_freq")->get();

        for (int i = 0; i < n; ++i)
        {
            float sample = in[0][i];
            if (lastSample <= 0.0f && sample > 0.0f)
            {
                // Zero crossing detected
                if (lastCrossingSample > 0)
                {
                    float period = (float)(sampleCounter - lastCrossingSample);
                    if (period > 0)
                    {
                        float freq = sr / period;
                        // Simple low-pass on frequency estimate
                        detectedFreq = detectedFreq * 0.9f + freq * 0.1f;
                        
                        // Calculate cents from nearest note
                        if (detectedFreq > 20.0f)
                        {
                            float semitones = 12.0f * std::log2 (detectedFreq / refA4);
                            float nearestSemi = std::round (semitones);
                            detectedCents = (semitones - nearestSemi) * 100.0f;
                            
                            int noteIndex = ((int)nearestSemi % 12 + 12) % 12;
                            detectedNote = noteIndex;
                        }
                    }
                }
                lastCrossingSample = sampleCounter;
            }
            lastSample = sample;
            sampleCounter++;
        }

        // Output
        for (int i = 0; i < n; ++i)
        {
            if (numOut > 0 && out[0]) out[0][i] = detectedFreq / 2000.0f; // normalize
            if (numOut > 1 && out[1]) out[1][i] = detectedCents / 50.0f + 0.5f; // -50..+50 cents → 0..1
        }
    }

    void reset() override { detectedFreq = 0; detectedCents = 0; lastSample = 0; sampleCounter = 0; lastCrossingSample = 0; }

    bool isDisplayNode() const { return true; }
    juce::String getDisplayType() const { return "tuner"; }

    float detectedFreq = 0.0f;
    float detectedCents = 0.0f;
    int detectedNote = 0; // 0-11 (C, C#, D, ...)

private:
    float lastSample = 0.0f;
    int sampleCounter = 0;
    int lastCrossingSample = 0;
};

//==============================================================================
// ─── I/O PERIPHERALS ────────────────────────────────────────────────────────
//==============================================================================

/**
 * Expression Pedal Input — Reads an expression pedal and outputs 0-1 control signal.
 * Expression pedals are typically CC#11 or CC#4 via MIDI, or 0-3.3V analog.
 */
class ExpressionPedalNode : public DSPNode
{
public:
    ExpressionPedalNode() : DSPNode ("io_expression", "Expression Pedal")
    {
        addOutput ("position", NodePort::Control);
        addParam ("cc", "MIDI CC#", 0.0f, 127.0f, 11.0f);
        addParam ("channel", "Channel", 0.0f, 16.0f, 0.0f);
        addParam ("invert", "Invert", 0.0f, 1.0f, 0.0f);
        addParam ("min", "Min Output", 0.0f, 1.0f, 0.0f);
        addParam ("max", "Max Output", 0.0f, 1.0f, 1.0f);
    }

    void process (const float**, int, float** out, int, int n) override
    {
        int targetCC = (int) getParam("cc")->get();
        int ch = (int) getParam("channel")->get();
        bool invert = getParam("invert")->get() > 0.5f;
        float mn = getParam("min")->get();
        float mx = getParam("max")->get();

        if (midiBuffer)
        {
            for (const auto metadata : *midiBuffer)
            {
                auto msg = metadata.getMessage();
                if (ch > 0 && msg.getChannel() != ch) continue;
                if (msg.isController() && msg.getControllerNumber() == targetCC)
                    rawPosition = msg.getControllerValue() / 127.0f;
            }
        }

        float pos = invert ? (1.0f - rawPosition) : rawPosition;
        float mapped = mn + pos * (mx - mn);
        for (int i = 0; i < n; ++i)
            out[0][i] = mapped;
    }

    void reset() override { rawPosition = 0.0f; }

private:
    float rawPosition = 0.0f;
};

/**
 * Footswitch Input — A dedicated footswitch peripheral (separate from the toggle control node).
 * Handles momentary and latching modes, with configurable hold behavior.
 */
class FootswitchNode : public DSPNode
{
public:
    FootswitchNode() : DSPNode ("io_footswitch", "Footswitch")
    {
        addOutput ("state", NodePort::Gate);
        addOutput ("tap", NodePort::Gate);     // pulse on each press
        addParam ("mode", "Mode (0=momentary,1=latch)", 0.0f, 1.0f, 1.0f);
        addParam ("pressed", "Pressed", 0.0f, 1.0f, 0.0f); // driven by the pedal UI
    }

    void process (const float**, int, float** out, int numOut, int n) override
    {
        bool pressed = getParam("pressed")->get() > 0.5f;
        bool isLatch = getParam("mode")->get() > 0.5f;
        bool tapPulse = false;

        if (pressed && !lastPressed)
        {
            tapPulse = true;
            if (isLatch)
                latchState = !latchState;
        }
        lastPressed = pressed;

        float stateOut = isLatch ? (latchState ? 1.0f : 0.0f) : (pressed ? 1.0f : 0.0f);

        for (int i = 0; i < n; ++i)
        {
            if (numOut > 0) out[0][i] = stateOut;
            if (numOut > 1) out[1][i] = tapPulse ? 1.0f : 0.0f;
        }
    }

    void reset() override { latchState = false; lastPressed = false; }

    bool isControlSurface() const { return true; }
    juce::String getControlType() const { return "footswitch"; }

private:
    bool latchState = false;
    bool lastPressed = false;
};

/**
 * CV Input — External control voltage input for modular-style setups.
 * Reads audio-rate CV and scales/offsets it to a usable control range.
 */
class CVInputNode : public DSPNode
{
public:
    CVInputNode() : DSPNode ("io_cv_in", "CV Input")
    {
        addInput ("cv", NodePort::Audio);
        addOutput ("out", NodePort::Control);
        addParam ("scale", "Scale", 0.0f, 10.0f, 1.0f);
        addParam ("offset", "Offset", -5.0f, 5.0f, 0.0f);
    }

    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float scale = getParam("scale")->get();
        float offset = getParam("offset")->get();
        for (int i = 0; i < n; ++i)
        {
            float cv = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            out[0][i] = cv * scale + offset;
        }
    }
};

/**
 * CV Output — Sends a control signal as audio-rate CV.
 */
class CVOutputNode : public DSPNode
{
public:
    CVOutputNode() : DSPNode ("io_cv_out", "CV Output")
    {
        addInput ("in", NodePort::Control);
        addOutput ("cv", NodePort::Audio);
        addParam ("scale", "Scale", 0.0f, 10.0f, 1.0f);
        addParam ("offset", "Offset", -5.0f, 5.0f, 0.0f);
    }

    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float scale = getParam("scale")->get();
        float offset = getParam("offset")->get();
        for (int i = 0; i < n; ++i)
        {
            float val = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            out[0][i] = val * scale + offset;
        }
    }
};
