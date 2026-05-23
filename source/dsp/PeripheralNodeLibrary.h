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

    bool isDisplayNode() const override { return true; }
    juce::String getDisplayType() const override { return "display"; }
    float getDisplayValue() const override { return displayValue; }

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

    bool isDisplayNode() const override { return true; }
    juce::String getDisplayType() const override { return "vu_meter"; }
    float getDisplayValue() const override { return peakLevel; }

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

    bool isDisplayNode() const override { return true; }
    juce::String getDisplayType() const override { return "tuner"; }
    float getDisplayValue() const override { return detectedNote; }

    float detectedFreq = 0.0f;
    float detectedCents = 0.0f;
    int detectedNote = 0; // 0-11 (C, C#, D, ...)

private:
    float lastSample = 0.0f;
    int sampleCounter = 0;
    int lastCrossingSample = 0;
};

/**
 * 7-Segment Display — Classic digital readout for numbers.
 * Great for BPM counters, patch numbers, tap tempo displays.
 * Shows integer or decimal values in retro LED-segment style.
 */
class SevenSegNode : public DSPNode
{
public:
    SevenSegNode() : DSPNode ("disp_7seg", "7-Segment Display")
    {
        addInput ("value", NodePort::Control);
        addParam ("digits", "Digits", 1.0f, 8.0f, 3.0f);
        addParam ("decimal_pos", "Decimal Position", 0.0f, 7.0f, 0.0f); // 0 = no decimal
        addParam ("hue", "Color Hue (0=red,120=grn)", 0.0f, 360.0f, 0.0f);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        if (numIn > 0 && in[0])
            displayValue = in[0][n - 1];
    }

    bool isDisplayNode() const override { return true; }
    juce::String getDisplayType() const override { return "7seg"; }
    float getDisplayValue() const override { return displayValue; }

    float displayValue = 0.0f;
};

/**
 * Text Screen — Displays a text string on the pedal face.
 * The text is set via the properties panel (param).
 * Can also be driven by a control signal to select from preset strings.
 */
class TextScreenNode : public DSPNode
{
public:
    TextScreenNode() : DSPNode ("disp_text", "Text Screen")
    {
        addInput ("select", NodePort::Control); // selects which line to highlight/show
        addParam ("line_count", "Lines", 1.0f, 8.0f, 2.0f);
        addParam ("font_size", "Font Size", 8.0f, 32.0f, 14.0f);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        if (numIn > 0 && in[0])
            selectedLine = (int) in[0][n - 1];
    }

    bool isDisplayNode() const { return true; }
    juce::String getDisplayType() const { return "text_screen"; }

    // Text lines stored here — set via properties panel
    juce::StringArray textLines { "Line 1", "Line 2" };
    int selectedLine = 0;
};

/**
 * Console Screen — Multi-line text display with scrolling.
 * Like a terminal window on the pedal face.
 * New messages push old ones up. Great for debug or status logging.
 */
class ConsoleScreenNode : public DSPNode
{
public:
    ConsoleScreenNode() : DSPNode ("disp_console", "Console Screen")
    {
        addInput ("trigger", NodePort::Gate); // rising edge pushes current value as new line
        addInput ("value", NodePort::Control);
        addParam ("rows", "Rows", 2.0f, 16.0f, 6.0f);
        addParam ("columns", "Columns", 10.0f, 40.0f, 20.0f);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        if (numIn < 2) return;
        bool trig = (in[0]) && in[0][n - 1] > 0.5f;
        if (trig && !lastTrig)
        {
            float val = in[1] ? in[1][n - 1] : 0.0f;
            int maxRows = (int) getParam("rows")->get();
            lines.add (juce::String (val, 2));
            while (lines.size() > maxRows)
                lines.remove (0);
        }
        lastTrig = trig;
    }

    void reset() override { lines.clear(); lastTrig = false; }

    bool isDisplayNode() const { return true; }
    juce::String getDisplayType() const { return "console"; }

    juce::StringArray lines;

private:
    bool lastTrig = false;
};

/**
 * Oscilloscope — Real-time waveform display.
 * Captures a rolling buffer of samples and displays the trace.
 * Supports audio signals and control signals for different time scales.
 */
class OscilloscopeNode : public DSPNode
{
public:
    static constexpr int kBufferSize = 512;

    OscilloscopeNode() : DSPNode ("disp_scope", "Oscilloscope")
    {
        addInput ("signal", NodePort::Audio);
        addInput ("trigger", NodePort::Gate); // optional external trigger
        addParam ("time_div", "Time Scale", 0.1f, 50.0f, 1.0f); // ms per division
        addParam ("gain", "Vertical Gain", 0.1f, 10.0f, 1.0f);
        addParam ("trigger_level", "Trigger Level", -1.0f, 1.0f, 0.0f);
        waveformData.resize (kBufferSize, 0.0f);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        if (numIn < 1 || !in[0]) return;
        float gain = getParam("gain")->get();
        float trigLevel = getParam("trigger_level")->get();
        bool hasExtTrig = (numIn > 1 && in[1]);

        for (int i = 0; i < n; ++i)
        {
            float sample = in[0][i] * gain;

            // Trigger detection
            if (!triggered)
            {
                if (hasExtTrig)
                {
                    if (in[1][i] > 0.5f && !lastTrigState)
                        triggered = true;
                    lastTrigState = in[1][i] > 0.5f;
                }
                else
                {
                    if (lastSample <= trigLevel && sample > trigLevel)
                        triggered = true;
                }
                lastSample = sample;
                continue;
            }

            // Capture samples into display buffer
            if (writePos < kBufferSize)
            {
                waveformData[writePos++] = sample;
            }
            else
            {
                // Buffer full — freeze display, wait for next trigger
                triggered = false;
                writePos = 0;
                displayReady = true;
            }
            lastSample = sample;
        }
    }

    void reset() override
    {
        std::fill (waveformData.begin(), waveformData.end(), 0.0f);
        writePos = 0;
        triggered = false;
        displayReady = false;
    }

    bool isDisplayNode() const override { return true; }
    juce::String getDisplayType() const override { return "oscilloscope"; }
    const std::vector<float>* getPixelData() const override { return &waveformData; }

    std::vector<float> waveformData;
    bool displayReady = false;

private:
    int writePos = 0;
    bool triggered = false;
    bool lastTrigState = false;
    float lastSample = 0.0f;
};

/**
 * Shader Display — Uses ExpressionVM to compute pixels dynamically based on math.
 * Variables: x, y (0..1), t (time), in1, in2, in3, in4
 */
class ShaderDisplayNode : public DSPNode
{
public:
    static constexpr int kWidth = 32;
    static constexpr int kHeight = 16;

    ShaderDisplayNode() : DSPNode ("disp_shader", "Shader Display")
    {
        addInput ("in1", NodePort::Control);
        addInput ("in2", NodePort::Control);
        addInput ("in3", NodePort::Control);
        addInput ("in4", NodePort::Control);
        
        pixelData.resize (kWidth * kHeight, 0.0f);

        vm.clearVars();
        var_x = vm.registerVar("x");
        var_y = vm.registerVar("y");
        var_t = vm.registerVar("t");
        var_in1 = vm.registerVar("in1");
        var_in2 = vm.registerVar("in2");
        var_in3 = vm.registerVar("in3");
        var_in4 = vm.registerVar("in4");

        setExpression ("x * y + sin(t*10)");
    }

    bool setExpression (const juce::String& expr)
    {
        expression = expr;
        bool ok = vm.compile (expr);
        if (ok)
            errorString.clear();
        else
            errorString = vm.getError();
        return ok;
    }

    juce::String getExpression() const { return expression; }
    juce::String getCompileError() const { return errorString; }

    void setFilePath (const juce::String& path) override { setExpression (path); }
    juce::String getFilePath() const override { return expression; }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        time += (float)n / (float)sr;
        
        if (!vm.isCompiled()) return;

        float in1_val = (numIn > 0 && in[0]) ? in[0][n-1] : 0.0f;
        float in2_val = (numIn > 1 && in[1]) ? in[1][n-1] : 0.0f;
        float in3_val = (numIn > 2 && in[2]) ? in[2][n-1] : 0.0f;
        float in4_val = (numIn > 3 && in[3]) ? in[3][n-1] : 0.0f;

        vm.vars[var_t] = time;
        vm.vars[var_in1] = in1_val;
        vm.vars[var_in2] = in2_val;
        vm.vars[var_in3] = in3_val;
        vm.vars[var_in4] = in4_val;

        for (int py = 0; py < kHeight; ++py)
        {
            float y_norm = (float)py / (float)(kHeight - 1);
            vm.vars[var_y] = y_norm;
            for (int px = 0; px < kWidth; ++px)
            {
                float x_norm = (float)px / (float)(kWidth - 1);
                vm.vars[var_x] = x_norm;
                pixelData[py * kWidth + px] = vm.evaluate();
            }
        }
    }

    void reset() override
    {
        time = 0.0f;
        std::fill (pixelData.begin(), pixelData.end(), 0.0f);
    }

    bool isDisplayNode() const override { return true; }
    juce::String getDisplayType() const override { return "pixel_display"; }
    const std::vector<float>* getPixelData() const override { return &pixelData; }

    std::vector<float> pixelData;
    ExpressionVM vm;
    juce::String expression;
    juce::String errorString;
    float time = 0.0f;

private:
    int var_x, var_y, var_t, var_in1, var_in2, var_in3, var_in4;
};

/**
 * Pixel Display — Addressable pixel grid for custom graphics.
 * X, Y, and Color inputs let you draw pixel-by-pixel.
 * A write trigger commits the current pixel.
 */
class PixelDisplayNode : public DSPNode
{
public:
    static constexpr int kWidth = 32;
    static constexpr int kHeight = 16;

    PixelDisplayNode() : DSPNode ("disp_pixel", "Pixel Display")
    {
        addInput ("x", NodePort::Control);
        addInput ("y", NodePort::Control);
        addInput ("color", NodePort::Control); // 0-1 = hue, or brightness for mono
        addInput ("write", NodePort::Gate);    // rising edge writes pixel
        addInput ("clear", NodePort::Gate);    // rising edge clears screen
        addParam ("mode", "Mode (0=mono,1=color)", 0.0f, 1.0f, 0.0f);
        pixelData.resize (kWidth * kHeight, 0.0f);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            // Clear
            bool clr = (numIn > 4 && in[4]) && in[4][i] > 0.5f;
            if (clr && !lastClear)
                std::fill (pixelData.begin(), pixelData.end(), 0.0f);
            lastClear = clr;

            // Write
            bool wr = (numIn > 3 && in[3]) && in[3][i] > 0.5f;
            if (wr && !lastWrite)
            {
                int x = (numIn > 0 && in[0]) ? juce::jlimit (0, kWidth - 1, (int)(in[0][i] * (kWidth - 1))) : 0;
                int y = (numIn > 1 && in[1]) ? juce::jlimit (0, kHeight - 1, (int)(in[1][i] * (kHeight - 1))) : 0;
                float c = (numIn > 2 && in[2]) ? juce::jlimit (0.0f, 1.0f, in[2][i]) : 1.0f;
                pixelData[y * kWidth + x] = c;
                dirty = true;
            }
            lastWrite = wr;
        }
    }

    void reset() override
    {
        std::fill (pixelData.begin(), pixelData.end(), 0.0f);
        dirty = true;
    }

    bool isDisplayNode() const override { return true; }
    juce::String getDisplayType() const override { return "pixel_display"; }
    const std::vector<float>* getPixelData() const override { return &pixelData; }

    std::vector<float> pixelData;
    bool dirty = true;

private:
    bool lastWrite = false;
    bool lastClear = false;
};

/**
 * Indicator Light — Simple color-changing indicator based on value thresholds.
 * Changes color based on ranges: green (normal), yellow (warning), red (alert).
 * Simpler than LED — no brightness control, just automatic color zones.
 */
class IndicatorNode : public DSPNode
{
public:
    IndicatorNode() : DSPNode ("disp_indicator", "Indicator Light")
    {
        addInput ("value", NodePort::Control);
        addParam ("yellow_thresh", "Yellow Threshold", 0.0f, 1.0f, 0.6f);
        addParam ("red_thresh", "Red Threshold", 0.0f, 1.0f, 0.85f);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        if (numIn > 0 && in[0])
            currentValue = juce::jlimit (0.0f, 1.0f, in[0][n - 1]);
    }

    bool isDisplayNode() const override { return true; }
    juce::String getDisplayType() const override { return "indicator"; }
    float getDisplayValue() const override { return currentValue; }

    float currentValue = 0.0f;
    // UI reads currentValue, compares to thresholds, and picks green/yellow/red
};

/**
 * Sound Emitter — Plays a tone or makes a sound when triggered.
 * Great for click tracks, metronome ticks, alert beeps, or fun sound effects.
 * Generates its own audio — wire the output to the audio chain or a mixer.
 */
class SoundEmitterNode : public DSPNode
{
public:
    SoundEmitterNode() : DSPNode ("disp_sound", "Sound Emitter")
    {
        addInput ("trigger", NodePort::Gate);
        addInput ("pitch", NodePort::Control);   // 0-1 = frequency
        addOutput ("audio", NodePort::Audio);
        addParam ("waveform", "Wave (0=sin,1=sq,2=saw,3=click)", 0.0f, 3.0f, 0.0f);
        addParam ("frequency", "Base Freq (Hz)", 20.0f, 8000.0f, 1000.0f);
        addParam ("duration", "Duration (ms)", 1.0f, 2000.0f, 50.0f);
        addParam ("volume", "Volume", 0.0f, 1.0f, 0.5f);
    }

    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float baseFreq = getParam("frequency")->get();
        float durMs = getParam("duration")->get();
        float vol = getParam("volume")->get();
        int wave = (int) getParam("waveform")->get();
        float durSamples = sr * durMs * 0.001f;

        for (int i = 0; i < n; ++i)
        {
            // Trigger detection
            bool trig = (numIn > 0 && in[0]) && in[0][i] > 0.5f;
            if (trig && !lastTrig)
            {
                playTimer = durSamples;
                phase = 0.0f;
            }
            lastTrig = trig;

            // Pitch modulation
            float freq = baseFreq;
            if (numIn > 1 && in[1])
                freq = baseFreq * std::pow (2.0f, (in[1][i] - 0.5f) * 4.0f); // +/- 2 octaves

            float sample = 0.0f;
            if (playTimer > 0)
            {
                float envelope = juce::jmin (1.0f, playTimer / juce::jmax (1.0f, durSamples * 0.1f)); // fade out last 10%
                float inc = freq / sr;
                
                switch (wave) {
                    case 0: sample = std::sin (phase * 6.283185f); break; // sine
                    case 1: sample = (phase < 0.5f) ? 1.0f : -1.0f; break; // square
                    case 2: sample = 2.0f * phase - 1.0f; break; // saw
                    case 3: sample = (playTimer > durSamples - 4) ? 1.0f : 0.0f; break; // click
                    default: break;
                }
                sample *= vol * envelope;
                phase += inc;
                if (phase >= 1.0f) phase -= 1.0f;
                playTimer--;
            }

            out[0][i] = sample;
        }
    }

    void reset() override { phase = 0; playTimer = 0; lastTrig = false; }

    bool isDisplayNode() const { return true; }
    juce::String getDisplayType() const { return "sound_emitter"; }

private:
    float phase = 0.0f;
    float playTimer = 0.0f;
    bool lastTrig = false;
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
