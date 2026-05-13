#pragma once

#include "DSPNode.h"
#include "ExpressionVM.h"
#include <cmath>
#include <algorithm>
#include <vector>

//==============================================================================
// ─── AUDIO I/O ───────────────────────────────────────────────────────────────
// Channel-selectable input/output nodes.
// Channel 1-2 = Main bus (L/R), Channel 3-4 = Aux bus / FX Return / Send.

class AudioInputNode : public DSPNode
{
public:
    AudioInputNode() : DSPNode ("audio_input", "Audio In")
    {
        addOutput ("out");
        addParam ("channel", "Channel", 1.0f, 32.0f, 1.0f);
    }

    void process (const float**, int, float** out, int numOut, int n) override
    {
        // Output is filled by DSPGraph based on channel param — just pass through
        // If graph hasn't filled it, output silence
        (void) out; (void) numOut; (void) n;
    }

    int getSelectedChannel() { return (int) getParam("channel")->get(); }
};

class AudioOutputNode : public DSPNode
{
public:
    AudioOutputNode() : DSPNode ("audio_output", "Audio Out")
    {
        addInput ("in");
        addParam ("channel", "Channel", 1.0f, 32.0f, 1.0f);
    }

    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numIn > 0 && numOut > 0)
            std::copy (in[0], in[0] + n, out[0]);
    }

    int getSelectedChannel() { return (int) getParam("channel")->get(); }
};

class MidiInputNode : public DSPNode
{
public:
    MidiInputNode() : DSPNode ("midi_input", "MIDI In")
    {
        addOutput ("pitch", NodePort::Midi);
        addOutput ("velocity", NodePort::Midi);
        addOutput ("gate", NodePort::Midi);
        addParam ("channel", "Channel", 0.0f, 16.0f, 0.0f); // 0 = omni
    }

    void process (const float**, int, float** out, int numOut, int n) override
    {
        int ch = (int) getParam("channel")->get();
        if (midiBuffer)
        {
            for (const auto metadata : *midiBuffer)
            {
                auto msg = metadata.getMessage();
                if (ch > 0 && msg.getChannel() != ch) continue;

                if (msg.isNoteOn())
                {
                    currentNote = msg.getNoteNumber() / 127.0f;
                    currentVelocity = msg.getFloatVelocity();
                    gate = 1.0f;
                }
                else if (msg.isNoteOff() && msg.getNoteNumber() / 127.0f == currentNote)
                {
                    gate = 0.0f;
                    currentVelocity = 0.0f;
                }
            }
        }
        if (numOut >= 3)
        {
            for (int i = 0; i < n; ++i)
            {
                out[0][i] = currentNote;
                out[1][i] = currentVelocity;
                out[2][i] = gate;
            }
        }
    }
private:
    float currentNote = 0.0f;
    float currentVelocity = 0.0f;
    float gate = 0.0f;
};

class MidiOutputNode : public DSPNode
{
public:
    MidiOutputNode() : DSPNode ("midi_output", "MIDI Out")
    {
        addInput ("pitch", NodePort::Midi);
        addInput ("velocity", NodePort::Midi);
        addInput ("gate", NodePort::Midi);
        addParam ("channel", "Channel", 1.0f, 16.0f, 1.0f);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        if (numIn < 3 || !midiBuffer) return;
        
        int ch = (int) getParam("channel")->get();
        
        for (int i = 0; i < n; ++i)
        {
            bool g = in[2][i] > 0.5f;
            if (g && !lastGate)
            {
                int note = juce::jlimit (0, 127, (int)(in[0][i] * 127.0f));
                float vel = juce::jlimit (0.0f, 1.0f, in[1][i]);
                midiBuffer->addEvent (juce::MidiMessage::noteOn (ch, note, vel), i);
                lastNote = note;
            }
            else if (!g && lastGate)
            {
                midiBuffer->addEvent (juce::MidiMessage::noteOff (ch, lastNote), i);
            }
            lastGate = g;
        }
    }
private:
    bool lastGate = false;
    int lastNote = 0;
};

//==============================================================================
// ─── UTILITY ─────────────────────────────────────────────────────────────────

class GainNode : public DSPNode
{
public:
    GainNode() : DSPNode ("gain", "Gain")
    {
        addInput ("in"); addInput ("gain_cv", NodePort::Control);
        addOutput ("out");
        addParam ("gain", "Gain", -60.0f, 24.0f, 0.0f); // dB
    }

    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float paramGain = getParam("gain")->get();
        for (int i = 0; i < n; ++i)
        {
            float cv = (numIn > 1) ? in[1][i] : 0.0f;
            float g = (cv != 0.0f) ? cv : std::pow (10.0f, paramGain / 20.0f);
            out[0][i] = in[0][i] * g;
        }
    }
};

class MixNode : public DSPNode
{
public:
    MixNode() : DSPNode ("mix", "Mix")
    {
        addInput ("dry"); addInput ("wet"); addInput ("mix_cv", NodePort::Control);
        addOutput ("out");
        addParam ("mix", "Mix", 0.0f, 1.0f, 0.5f);
    }

    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float paramMix = getParam("mix")->get();
        if (numIn >= 2)
        {
            for (int i = 0; i < n; ++i)
            {
                float cv = (numIn > 2) ? in[2][i] : 0.0f;
                float m = (cv != 0.0f) ? juce::jlimit(0.0f, 1.0f, cv) : paramMix;
                out[0][i] = in[0][i] * (1.0f - m) + in[1][i] * m;
            }
        }
        else if (numIn >= 1)
            std::copy (in[0], in[0] + n, out[0]);
    }
};

class SplitNode : public DSPNode
{
public:
    SplitNode() : DSPNode ("split", "Split")
    {
        addInput ("in"); addOutput ("out_a"); addOutput ("out_b");
    }

    void process (const float** in, int, float** out, int numOut, int n) override
    {
        std::copy (in[0], in[0] + n, out[0]);
        if (numOut > 1) std::copy (in[0], in[0] + n, out[1]);
    }
};

//==============================================================================
// ─── FILTERS ─────────────────────────────────────────────────────────────────

class LowPassNode : public DSPNode
{
public:
    LowPassNode() : DSPNode ("lowpass", "Low Pass")
    {
        addInput ("in");
        addInput ("freq_cv", NodePort::Control);
        addInput ("q_cv", NodePort::Control);
        addOutput ("out");
        addParam ("freq", "Frequency", 20.0f, 20000.0f, 1000.0f);
        addParam ("q", "Resonance", 0.1f, 10.0f, 0.707f);
    }

    void prepare (double sampleRate, int bs) override
    {
        DSPNode::prepare (sampleRate, bs);
        z1 = z2 = 0.0f;
    }

    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float paramF = getParam("freq")->get();
        float paramQ = getParam("q")->get();

        for (int i = 0; i < n; ++i)
        {
            float fcv = (numIn > 1) ? in[1][i] : 0.0f;
            float qcv = (numIn > 2) ? in[2][i] : 0.0f;
            float f = (fcv != 0.0f) ? juce::jlimit(20.0f, 20000.0f, fcv) : paramF;
            float q = (qcv != 0.0f) ? juce::jlimit(0.1f, 10.0f, qcv) : paramQ;
            updateCoeffs(f, q);

            float x = in[0][i];
            float y = b0 * x + b1 * x1 + b2 * x2 - a1 * z1 - a2 * z2;
            x2 = x1; x1 = x; z2 = z1; z1 = y;
            out[0][i] = y;
        }
    }

    void reset() override { z1 = z2 = x1 = x2 = 0.0f; }

private:
    float b0=1, b1=0, b2=0, a1=0, a2=0;
    float z1=0, z2=0, x1=0, x2=0;

    void updateCoeffs (float f, float q)
    {
        float w0 = 2.0f * juce::MathConstants<float>::pi * f / (float)sr;
        float cosw = std::cos(w0), sinw = std::sin(w0);
        float alpha = sinw / (2.0f * q);
        float norm = 1.0f / (1.0f + alpha);
        b0 = ((1.0f - cosw) / 2.0f) * norm;
        b1 = (1.0f - cosw) * norm;
        b2 = b0;
        a1 = (-2.0f * cosw) * norm;
        a2 = (1.0f - alpha) * norm;
    }
};

class HighPassNode : public DSPNode
{
public:
    HighPassNode() : DSPNode ("highpass", "High Pass")
    {
        addInput ("in");
        addInput ("freq_cv", NodePort::Control);
        addInput ("q_cv", NodePort::Control);
        addOutput ("out");
        addParam ("freq", "Frequency", 20.0f, 20000.0f, 200.0f);
        addParam ("q", "Resonance", 0.1f, 10.0f, 0.707f);
    }

    void prepare (double sampleRate, int bs) override
    {
        DSPNode::prepare (sampleRate, bs);
        z1 = z2 = 0.0f;
    }

    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float paramF = getParam("freq")->get();
        float paramQ = getParam("q")->get();

        for (int i = 0; i < n; ++i)
        {
            float fcv = (numIn > 1) ? in[1][i] : 0.0f;
            float qcv = (numIn > 2) ? in[2][i] : 0.0f;
            float f = (fcv != 0.0f) ? juce::jlimit(20.0f, 20000.0f, fcv) : paramF;
            float q = (qcv != 0.0f) ? juce::jlimit(0.1f, 10.0f, qcv) : paramQ;
            updateCoeffs(f, q);

            float x = in[0][i];
            float y = b0 * x + b1 * x1 + b2 * x2 - a1 * z1 - a2 * z2;
            x2 = x1; x1 = x; z2 = z1; z1 = y;
            out[0][i] = y;
        }
    }

    void reset() override { z1 = z2 = x1 = x2 = 0.0f; }

private:
    float b0=1, b1=0, b2=0, a1=0, a2=0;
    float z1=0, z2=0, x1=0, x2=0;

    void updateCoeffs (float f, float q)
    {
        float w0 = 2.0f * juce::MathConstants<float>::pi * f / (float)sr;
        float cosw = std::cos(w0), sinw = std::sin(w0);
        float alpha = sinw / (2.0f * q);
        float norm = 1.0f / (1.0f + alpha);
        b0 = ((1.0f + cosw) / 2.0f) * norm;
        b1 = -(1.0f + cosw) * norm;
        b2 = b0;
        a1 = (-2.0f * cosw) * norm;
        a2 = (1.0f - alpha) * norm;
    }
};

class AllPassNode : public DSPNode
{
public:
    AllPassNode() : DSPNode ("allpass", "All Pass")
    {
        addInput ("in"); addOutput ("out");
        addParam ("freq", "Frequency", 20.0f, 20000.0f, 1000.0f);
        addParam ("q", "Q", 0.1f, 10.0f, 0.707f);
    }

    void prepare (double sampleRate, int bs) override
    {
        DSPNode::prepare (sampleRate, bs);
        z1 = z2 = x1 = x2 = 0.0f;
    }

    void process (const float** in, int, float** out, int, int n) override
    {
        updateCoeffs();
        for (int i = 0; i < n; ++i)
        {
            float x = in[0][i];
            float y = b0 * x + b1 * x1 + b2 * x2 - a1 * z1 - a2 * z2;
            x2 = x1; x1 = x; z2 = z1; z1 = y;
            out[0][i] = y;
        }
    }

    void reset() override { z1 = z2 = x1 = x2 = 0.0f; }

private:
    float b0=1, b1=0, b2=0, a1=0, a2=0;
    float z1=0, z2=0, x1=0, x2=0;

    void updateCoeffs()
    {
        float f = getParam("freq")->get();
        float q = getParam("q")->get();
        float w0 = 2.0f * juce::MathConstants<float>::pi * f / (float)sr;
        float cosw = std::cos(w0), sinw = std::sin(w0);
        float alpha = sinw / (2.0f * q);
        float norm = 1.0f / (1.0f + alpha);
        b0 = (1.0f - alpha) * norm;
        b1 = (-2.0f * cosw) * norm;
        b2 = 1.0f; // (1 + alpha) * norm = 1
        a1 = b1;
        a2 = b0;
    }
};

class ToneStackNode : public DSPNode
{
public:
    ToneStackNode() : DSPNode ("tonestack", "Tone Stack")
    {
        addInput ("in"); addOutput ("out");
        addParam ("bass", "Bass", -12.0f, 12.0f, 0.0f);
        addParam ("mid", "Mid", -12.0f, 12.0f, 0.0f);
        addParam ("treble", "Treble", -12.0f, 12.0f, 0.0f);
    }

    void prepare (double sampleRate, int bs) override
    {
        DSPNode::prepare (sampleRate, bs);
        lpZ1 = lpZ2 = hpZ1 = hpZ2 = 0.0f;
    }

    void process (const float** in, int, float** out, int, int n) override
    {
        float bass   = std::pow (10.0f, getParam("bass")->get() / 20.0f);
        float mid    = std::pow (10.0f, getParam("mid")->get() / 20.0f);
        float treble = std::pow (10.0f, getParam("treble")->get() / 20.0f);

        // Simple 3-band split: LP < 250Hz, HP > 2kHz, mid = remainder
        float lpFreq = 250.0f, hpFreq = 2000.0f;
        float lpCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * lpFreq / (float)sr);
        float hpCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * hpFreq / (float)sr);

        for (int i = 0; i < n; ++i)
        {
            float x = in[0][i];
            lpZ1 += lpCoeff * (x - lpZ1);
            float lo = lpZ1;
            hpZ1 += hpCoeff * (x - hpZ1);
            float hi = x - hpZ1;
            float midSig = x - lo - hi;
            out[0][i] = lo * bass + midSig * mid + hi * treble;
        }
    }

    void reset() override { lpZ1 = lpZ2 = hpZ1 = hpZ2 = 0.0f; }

private:
    float lpZ1=0, lpZ2=0, hpZ1=0, hpZ2=0;
};

//==============================================================================
// ─── DRIVE / NONLINEAR ───────────────────────────────────────────────────────

class SoftClipNode : public DSPNode
{
public:
    SoftClipNode() : DSPNode ("softclip", "Soft Clip")
    {
        addInput ("in"); addInput ("drive_cv", NodePort::Control);
        addOutput ("out");
        addParam ("drive", "Drive", 1.0f, 100.0f, 10.0f);
    }

    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float paramD = getParam("drive")->get();
        for (int i = 0; i < n; ++i)
        {
            float cv = (numIn > 1) ? in[1][i] : 0.0f;
            float d = (cv != 0.0f) ? juce::jlimit(1.0f, 100.0f, cv) : paramD;
            out[0][i] = std::tanh (in[0][i] * d) / std::tanh (d);
        }
    }
};

class HardClipNode : public DSPNode
{
public:
    HardClipNode() : DSPNode ("hardclip", "Hard Clip")
    {
        addInput ("in"); addInput ("drive_cv", NodePort::Control);
        addOutput ("out");
        addParam ("drive", "Drive", 1.0f, 100.0f, 10.0f);
        addParam ("threshold", "Threshold", 0.01f, 1.0f, 0.5f);
    }

    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float paramD = getParam("drive")->get();
        float t = getParam("threshold")->get();
        for (int i = 0; i < n; ++i)
        {
            float cv = (numIn > 1) ? in[1][i] : 0.0f;
            float d = (cv != 0.0f) ? juce::jlimit(1.0f, 100.0f, cv) : paramD;
            out[0][i] = juce::jlimit (-t, t, in[0][i] * d);
        }
    }
};

//==============================================================================
// ─── LFO ─────────────────────────────────────────────────────────────────────

class LFONode : public DSPNode
{
public:
    LFONode() : DSPNode ("lfo", "LFO")
    {
        addInput ("rate_cv", NodePort::Control);
        addOutput ("out", NodePort::Control);
        addParam ("rate", "Rate", 0.05f, 20.0f, 1.0f);   // Hz
        addParam ("depth", "Depth", 0.0f, 1.0f, 1.0f);
        addParam ("shape", "Shape", 0.0f, 3.0f, 0.0f);   // 0=sine, 1=tri, 2=saw, 3=square
    }

    void prepare (double sampleRate, int bs) override
    {
        DSPNode::prepare (sampleRate, bs);
        phase = 0.0;
    }

    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float paramRate = getParam("rate")->get();
        float depth = getParam("depth")->get();
        int shape   = (int) getParam("shape")->get();

        for (int i = 0; i < n; ++i)
        {
            float rcv = (numIn > 0) ? in[0][i] : 0.0f;
            float rate = (rcv != 0.0f) ? juce::jlimit(0.05f, 20.0f, rcv) : paramRate;
            double inc = rate / sr;

            float v = 0.0f;
            switch (shape)
            {
                case 0: v = std::sin (phase * 2.0 * juce::MathConstants<double>::pi); break;
                case 1: v = (float)(2.0 * std::abs (2.0 * phase - 1.0) - 1.0); break;
                case 2: v = (float)(2.0 * phase - 1.0); break;
                case 3: v = phase < 0.5 ? 1.0f : -1.0f; break;
            }
            out[0][i] = v * depth;
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
        }
    }

    void reset() override { phase = 0.0; }

private:
    double phase = 0.0;
};

//==============================================================================
// ─── DELAY ───────────────────────────────────────────────────────────────────

class DelayNode : public DSPNode
{
public:
    DelayNode() : DSPNode ("delay", "Delay")
    {
        addInput ("in");
        addInput ("time_cv", NodePort::Control);
        addInput ("fb_cv", NodePort::Control);
        addOutput ("out");
        addParam ("time", "Time", 0.001f, 2.0f, 0.3f);       // seconds
        addParam ("feedback", "Feedback", 0.0f, 0.95f, 0.4f);
    }

    void prepare (double sampleRate, int bs) override
    {
        DSPNode::prepare (sampleRate, bs);
        int maxSamples = (int)(sampleRate * 2.5);
        buffer.resize (maxSamples, 0.0f);
        writePos = 0;
    }

    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float paramTime = getParam("time")->get();
        float paramFB   = getParam("feedback")->get();

        for (int i = 0; i < n; ++i)
        {
            float tcv = (numIn > 1) ? in[1][i] : 0.0f;
            float fcv = (numIn > 2) ? in[2][i] : 0.0f;
            float time = (tcv != 0.0f) ? juce::jlimit(0.001f, 2.0f, tcv) : paramTime;
            float fb   = (fcv != 0.0f) ? juce::jlimit(0.0f, 0.95f, fcv) : paramFB;
            int delaySamples = juce::jlimit (1, (int)buffer.size() - 1, (int)(time * sr));

            int readPos = writePos - delaySamples;
            if (readPos < 0) readPos += (int)buffer.size();

            float delayed = buffer[readPos];
            float input = in[0][i] + delayed * fb;
            buffer[writePos] = input;
            writePos = (writePos + 1) % (int)buffer.size();
            out[0][i] = delayed;
        }
    }

    void reset() override
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

private:
    std::vector<float> buffer;
    int writePos = 0;
};

class ModDelayNode : public DSPNode
{
public:
    ModDelayNode() : DSPNode ("mod_delay", "Mod Delay")
    {
        addInput ("in"); addInput ("mod", NodePort::Control); addOutput ("out");
        addParam ("time", "Time", 0.001f, 0.05f, 0.007f);   // seconds (chorus range)
        addParam ("depth", "Mod Depth", 0.0f, 0.01f, 0.003f);
        addParam ("feedback", "Feedback", 0.0f, 0.95f, 0.0f);
    }

    void prepare (double sampleRate, int bs) override
    {
        DSPNode::prepare (sampleRate, bs);
        int maxSamples = (int)(sampleRate * 0.1);
        buffer.resize (maxSamples, 0.0f);
        writePos = 0;
    }

    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float baseTime = getParam("time")->get();
        float depth    = getParam("depth")->get();
        float fb       = getParam("feedback")->get();

        for (int i = 0; i < n; ++i)
        {
            float mod = (numIn >= 2) ? in[1][i] : 0.0f;
            float delaySec = baseTime + mod * depth;
            float delaySamp = juce::jlimit (1.0f, (float)buffer.size() - 2.0f, (float)(delaySec * sr));

            // Linear interpolation
            int d0 = (int) delaySamp;
            float frac = delaySamp - d0;
            int r0 = ((writePos - d0) + (int)buffer.size()) % (int)buffer.size();
            int r1 = ((writePos - d0 - 1) + (int)buffer.size()) % (int)buffer.size();
            float delayed = buffer[r0] * (1.0f - frac) + buffer[r1] * frac;

            buffer[writePos] = in[0][i] + delayed * fb;
            writePos = (writePos + 1) % (int)buffer.size();
            out[0][i] = delayed;
        }
    }

    void reset() override
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

private:
    std::vector<float> buffer;
    int writePos = 0;
};

//==============================================================================
// ─── DYNAMICS ────────────────────────────────────────────────────────────────

class CompressorNode : public DSPNode
{
public:
    CompressorNode() : DSPNode ("compressor", "Compressor")
    {
        addInput ("in"); addInput ("thresh_cv", NodePort::Control);
        addOutput ("out");
        addParam ("threshold", "Threshold", -60.0f, 0.0f, -20.0f);
        addParam ("ratio", "Ratio", 1.0f, 20.0f, 4.0f);
        addParam ("attack", "Attack", 0.1f, 100.0f, 10.0f);    // ms
        addParam ("release", "Release", 10.0f, 1000.0f, 100.0f); // ms
    }

    void prepare (double sampleRate, int bs) override
    {
        DSPNode::prepare (sampleRate, bs);
        envelope = 0.0f;
    }

    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float paramThresh = getParam("threshold")->get();
        float ratio   = getParam("ratio")->get();
        float attMs   = getParam("attack")->get();
        float relMs   = getParam("release")->get();
        float attCoef = 1.0f - std::exp (-1.0f / ((float)sr * attMs * 0.001f));
        float relCoef = 1.0f - std::exp (-1.0f / ((float)sr * relMs * 0.001f));

        for (int i = 0; i < n; ++i)
        {
            float tcv = (numIn > 1) ? in[1][i] : 0.0f;
            float thresh = (tcv != 0.0f) ? tcv : paramThresh;
            float x = in[0][i];
            float absX = std::abs (x);
            float dB = 20.0f * std::log10 (absX + 1e-10f);

            if (dB > envelope) envelope += attCoef * (dB - envelope);
            else               envelope += relCoef * (dB - envelope);

            float gainReduction = 0.0f;
            if (envelope > thresh)
                gainReduction = (envelope - thresh) * (1.0f - 1.0f / ratio);

            float gainLin = std::pow (10.0f, -gainReduction / 20.0f);
            out[0][i] = x * gainLin;
        }
    }

    void reset() override { envelope = 0.0f; }

private:
    float envelope = 0.0f;
};

class NoiseGateNode : public DSPNode
{
public:
    NoiseGateNode() : DSPNode ("noisegate", "Noise Gate")
    {
        addInput ("in"); addInput ("thresh_cv", NodePort::Control);
        addOutput ("out");
        addParam ("threshold", "Threshold", -80.0f, 0.0f, -40.0f);
        addParam ("attack", "Attack", 0.1f, 50.0f, 1.0f);
        addParam ("release", "Release", 10.0f, 500.0f, 50.0f);
    }

    void prepare (double sampleRate, int bs) override
    {
        DSPNode::prepare (sampleRate, bs);
        gateGain = 0.0f;
    }

    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float paramThresh = getParam("threshold")->get();
        float attMs   = getParam("attack")->get();
        float relMs   = getParam("release")->get();
        float attCoef = 1.0f - std::exp (-1.0f / ((float)sr * attMs * 0.001f));
        float relCoef = 1.0f - std::exp (-1.0f / ((float)sr * relMs * 0.001f));

        for (int i = 0; i < n; ++i)
        {
            float tcv = (numIn > 1) ? in[1][i] : 0.0f;
            float thresh = (tcv != 0.0f) ? tcv : paramThresh;
            float dB = 20.0f * std::log10 (std::abs (in[0][i]) + 1e-10f);
            float target = (dB > thresh) ? 1.0f : 0.0f;
            float coef = (target > gateGain) ? attCoef : relCoef;
            gateGain += coef * (target - gateGain);
            out[0][i] = in[0][i] * gateGain;
        }
    }

    void reset() override { gateGain = 0.0f; }

private:
    float gateGain = 0.0f;
};

//==============================================================================
// ─── REVERB ──────────────────────────────────────────────────────────────────

class SchroederReverbNode : public DSPNode
{
public:
    SchroederReverbNode() : DSPNode ("reverb", "Reverb")
    {
        addInput ("in");
        addInput ("size_cv", NodePort::Control);
        addInput ("mix_cv", NodePort::Control);
        addOutput ("out");
        addParam ("size", "Size", 0.0f, 1.0f, 0.5f);
        addParam ("damping", "Damping", 0.0f, 1.0f, 0.5f);
        addParam ("mix", "Mix", 0.0f, 1.0f, 0.3f);
    }

    void prepare (double sampleRate, int bs) override
    {
        DSPNode::prepare (sampleRate, bs);
        float scale = (float)(sampleRate / 44100.0);
        int combLens[] = { 1116, 1188, 1277, 1356 };
        int apLens[]   = { 556, 441 };
        for (int i = 0; i < 4; ++i) { comb[i].resize ((int)(combLens[i] * scale), 0.0f); combIdx[i] = 0; combFilt[i] = 0.0f; }
        for (int i = 0; i < 2; ++i) { ap[i].resize ((int)(apLens[i] * scale), 0.0f); apIdx[i] = 0; }
    }

    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float paramSize = getParam("size")->get();
        float damp = getParam("damping")->get();
        float paramMix = getParam("mix")->get();

        for (int i = 0; i < n; ++i)
        {
            float scv = (numIn > 1) ? in[1][i] : 0.0f;
            float mcv = (numIn > 2) ? in[2][i] : 0.0f;
            float size = ((scv != 0.0f) ? juce::jlimit(0.0f, 1.0f, scv) : paramSize) * 0.28f + 0.7f;
            float mix  = (mcv != 0.0f) ? juce::jlimit(0.0f, 1.0f, mcv) : paramMix;
            float x = in[0][i] * 0.5f;
            float wet = 0.0f;

            // 4 parallel comb filters
            for (int c = 0; c < 4; ++c)
            {
                float del = comb[c][combIdx[c]];
                combFilt[c] = del * (1.0f - damp) + combFilt[c] * damp;
                comb[c][combIdx[c]] = x + combFilt[c] * size;
                combIdx[c] = (combIdx[c] + 1) % (int)comb[c].size();
                wet += del;
            }
            wet *= 0.25f;

            // 2 series allpass
            for (int a = 0; a < 2; ++a)
            {
                float del = ap[a][apIdx[a]];
                float apOut = -wet + del;
                ap[a][apIdx[a]] = wet + del * 0.5f;
                apIdx[a] = (apIdx[a] + 1) % (int)ap[a].size();
                wet = apOut;
            }

            out[0][i] = in[0][i] * (1.0f - mix) + wet * mix;
        }
    }

    void reset() override
    {
        for (int i = 0; i < 4; ++i) { std::fill(comb[i].begin(), comb[i].end(), 0.0f); combFilt[i] = 0.0f; }
        for (int i = 0; i < 2; ++i) std::fill(ap[i].begin(), ap[i].end(), 0.0f);
    }

private:
    std::vector<float> comb[4], ap[2];
    int combIdx[4] = {}, apIdx[2] = {};
    float combFilt[4] = {};
};

//==============================================================================
// ─── LOGIC GATES (Wiremod Tier 1) ────────────────────────────────────────────
// All logic nodes operate on control signals: >0.5 = true, <=0.5 = false
// Output: 1.0 = true, 0.0 = false

class ANDGateNode : public DSPNode
{
public:
    ANDGateNode() : DSPNode ("and_gate", "AND Gate")
    {
        addInput ("a", NodePort::Gate); addInput ("b", NodePort::Gate);
        addOutput ("out", NodePort::Gate);
    }
    void process (const float** in, int numIn, float** out, int, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            bool a = (numIn > 0) ? in[0][i] > 0.5f : false;
            bool b = (numIn > 1) ? in[1][i] > 0.5f : false;
            out[0][i] = (a && b) ? 1.0f : 0.0f;
        }
    }
};

class ORGateNode : public DSPNode
{
public:
    ORGateNode() : DSPNode ("or_gate", "OR Gate")
    {
        addInput ("a", NodePort::Gate); addInput ("b", NodePort::Gate);
        addOutput ("out", NodePort::Gate);
    }
    void process (const float** in, int numIn, float** out, int, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            bool a = (numIn > 0) ? in[0][i] > 0.5f : false;
            bool b = (numIn > 1) ? in[1][i] > 0.5f : false;
            out[0][i] = (a || b) ? 1.0f : 0.0f;
        }
    }
};

class NOTGateNode : public DSPNode
{
public:
    NOTGateNode() : DSPNode ("not_gate", "NOT Gate")
    {
        addInput ("in", NodePort::Gate);
        addOutput ("out", NodePort::Gate);
    }
    void process (const float** in, int, float** out, int, int n) override
    {
        for (int i = 0; i < n; ++i)
            out[0][i] = (in[0][i] > 0.5f) ? 0.0f : 1.0f;
    }
};

class XORGateNode : public DSPNode
{
public:
    XORGateNode() : DSPNode ("xor_gate", "XOR Gate")
    {
        addInput ("a", NodePort::Gate); addInput ("b", NodePort::Gate);
        addOutput ("out", NodePort::Gate);
    }
    void process (const float** in, int numIn, float** out, int, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            bool a = (numIn > 0) ? in[0][i] > 0.5f : false;
            bool b = (numIn > 1) ? in[1][i] > 0.5f : false;
            out[0][i] = (a != b) ? 1.0f : 0.0f;
        }
    }
};

//==============================================================================
// ─── COMPARATOR ──────────────────────────────────────────────────────────────

class ComparatorNode : public DSPNode
{
public:
    ComparatorNode() : DSPNode ("comparator", "Comparator")
    {
        addInput ("a"); addInput ("b");
        addOutput ("out", NodePort::Control);
        addParam ("mode", "Mode", 0.0f, 4.0f, 0.0f);  // 0=>, 1=<, 2===, 3=>=, 4=<=
        addParam ("threshold", "Threshold", -1.0f, 1.0f, 0.0f); // Used when b is disconnected
    }
    void process (const float** in, int numIn, float** out, int, int n) override
    {
        int mode = (int) getParam("mode")->get();
        float thresh = getParam("threshold")->get();
        for (int i = 0; i < n; ++i)
        {
            float a = in[0][i];
            float b = (numIn > 1) ? in[1][i] : thresh;
            bool result = false;
            switch (mode) {
                case 0: result = a > b; break;
                case 1: result = a < b; break;
                case 2: result = std::abs(a - b) < 0.001f; break;
                case 3: result = a >= b; break;
                case 4: result = a <= b; break;
            }
            out[0][i] = result ? 1.0f : 0.0f;
        }
    }
};

//==============================================================================
// ─── LATCH / TOGGLE ──────────────────────────────────────────────────────────
// Input goes high → output toggles. Classic flip-flop for footswitch→bypass.

class LatchNode : public DSPNode
{
public:
    LatchNode() : DSPNode ("latch", "Latch / Toggle")
    {
        addInput ("trigger", NodePort::Control);
        addInput ("reset", NodePort::Control);
        addOutput ("out", NodePort::Control);
    }
    void prepare (double sampleRate, int bs) override
    {
        DSPNode::prepare (sampleRate, bs);
        state = false; prevTrig = false; prevReset = false;
    }
    void process (const float** in, int numIn, float** out, int, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            bool trig = in[0][i] > 0.5f;
            bool reset = (numIn > 1) ? in[1][i] > 0.5f : false;

            // Rising edge detect
            if (trig && !prevTrig) state = !state;
            if (reset && !prevReset) state = false;

            prevTrig = trig;
            prevReset = reset;
            out[0][i] = state ? 1.0f : 0.0f;
        }
    }
    void reset() override { state = false; prevTrig = false; prevReset = false; }
private:
    bool state = false, prevTrig = false, prevReset = false;
};

//==============================================================================
// ─── MUX / SELECTOR ─────────────────────────────────────────────────────────
// Routes one of two inputs to output based on a control signal (A/B switch)

class MuxNode : public DSPNode
{
public:
    MuxNode() : DSPNode ("mux", "Mux / A|B Switch")
    {
        addInput ("a"); addInput ("b");
        addInput ("sel", NodePort::Control);
        addOutput ("out");
    }
    void process (const float** in, int numIn, float** out, int, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            bool sel = (numIn > 2) ? in[2][i] > 0.5f : false;
            float a = in[0][i];
            float b = (numIn > 1) ? in[1][i] : 0.0f;
            out[0][i] = sel ? b : a;
        }
    }
};

//==============================================================================
// ─── CONSTANT ────────────────────────────────────────────────────────────────

class ConstantNode : public DSPNode
{
public:
    ConstantNode() : DSPNode ("constant", "Constant")
    {
        addOutput ("out", NodePort::Control);
        addParam ("value", "Value", -1000.0f, 1000.0f, 1.0f);
    }
    void process (const float**, int, float** out, int, int n) override
    {
        float v = getParam("value")->get();
        for (int i = 0; i < n; ++i) out[0][i] = v;
    }
};

//==============================================================================
// ─── MATH OPERATIONS (Wiremod Tier 2) ────────────────────────────────────────

class AddNode : public DSPNode
{
public:
    AddNode() : DSPNode ("add", "Add")
    {
        addInput ("a"); addInput ("b");
        addOutput ("out");
    }
    void process (const float** in, int numIn, float** out, int, int n) override
    {
        for (int i = 0; i < n; ++i)
            out[0][i] = in[0][i] + ((numIn > 1) ? in[1][i] : 0.0f);
    }
};

class MultiplyNode : public DSPNode
{
public:
    MultiplyNode() : DSPNode ("multiply", "Multiply")
    {
        addInput ("a"); addInput ("b");
        addOutput ("out");
        addParam ("scale", "Scale", -10.0f, 10.0f, 1.0f); // Used when b disconnected
    }
    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float scale = getParam("scale")->get();
        for (int i = 0; i < n; ++i)
            out[0][i] = in[0][i] * ((numIn > 1) ? in[1][i] : scale);
    }
};

class DivideNode : public DSPNode
{
public:
    DivideNode() : DSPNode ("divide", "Divide")
    {
        addInput ("a"); addInput ("b");
        addOutput ("out");
    }
    void process (const float** in, int numIn, float** out, int, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            float b = (numIn > 1) ? in[1][i] : 1.0f;
            out[0][i] = (std::abs(b) > 1e-10f) ? in[0][i] / b : 0.0f;
        }
    }
};

class ModuloNode : public DSPNode
{
public:
    ModuloNode() : DSPNode ("modulo", "Modulo")
    {
        addInput ("a"); addInput ("b");
        addOutput ("out");
        addParam ("divisor", "Divisor", 0.001f, 100.0f, 1.0f);
    }
    void process (const float** in, int numIn, float** out, int, int n) override
    {
        float div = getParam("divisor")->get();
        for (int i = 0; i < n; ++i)
        {
            float b = (numIn > 1) ? in[1][i] : div;
            out[0][i] = (std::abs(b) > 1e-10f) ? std::fmod(in[0][i], b) : 0.0f;
        }
    }
};

//==============================================================================
// ─── RANGER / REMAP ──────────────────────────────────────────────────────────
// Maps input from [inMin..inMax] → [outMin..outMax]
// With optional logarithmic curve for freq mapping

class RangerNode : public DSPNode
{
public:
    RangerNode() : DSPNode ("ranger", "Ranger / Remap")
    {
        addInput ("in");
        addOutput ("out");
        addParam ("in_min", "In Min", -1000.0f, 1000.0f, 0.0f);
        addParam ("in_max", "In Max", -1000.0f, 1000.0f, 1.0f);
        addParam ("out_min", "Out Min", -20000.0f, 20000.0f, 20.0f);
        addParam ("out_max", "Out Max", -20000.0f, 20000.0f, 20000.0f);
        addParam ("curve", "Curve", 0.0f, 1.0f, 0.0f);  // 0=linear, 1=log
    }
    void process (const float** in, int, float** out, int, int n) override
    {
        float iMin = getParam("in_min")->get(),  iMax = getParam("in_max")->get();
        float oMin = getParam("out_min")->get(), oMax = getParam("out_max")->get();
        float curve = getParam("curve")->get();
        float iRange = iMax - iMin;
        if (std::abs(iRange) < 1e-10f) iRange = 1.0f;

        for (int i = 0; i < n; ++i)
        {
            float norm = juce::jlimit (0.0f, 1.0f, (in[0][i] - iMin) / iRange);
            if (curve > 0.01f)
                norm = std::pow (norm, 1.0f + curve * 3.0f); // log-ish curve
            out[0][i] = oMin + norm * (oMax - oMin);
        }
    }
};

//==============================================================================
// ─── SMOOTH / SLEW ───────────────────────────────────────────────────────────
// Smoothly transition between values. Prevents clicking on parameter jumps.

class SmoothNode : public DSPNode
{
public:
    SmoothNode() : DSPNode ("smooth", "Smooth / Slew")
    {
        addInput ("in");
        addOutput ("out");
        addParam ("rise", "Rise (ms)", 0.1f, 1000.0f, 10.0f);
        addParam ("fall", "Fall (ms)", 0.1f, 1000.0f, 10.0f);
    }
    void prepare (double sampleRate, int bs) override { DSPNode::prepare(sampleRate, bs); current = 0.0f; }
    void process (const float** in, int, float** out, int, int n) override
    {
        float riseMs = getParam("rise")->get();
        float fallMs = getParam("fall")->get();
        float riseCoef = 1.0f - std::exp(-1.0f / ((float)sr * riseMs * 0.001f));
        float fallCoef = 1.0f - std::exp(-1.0f / ((float)sr * fallMs * 0.001f));

        for (int i = 0; i < n; ++i)
        {
            float target = in[0][i];
            float coef = (target > current) ? riseCoef : fallCoef;
            current += coef * (target - current);
            out[0][i] = current;
        }
    }
    void reset() override { current = 0.0f; }
private:
    float current = 0.0f;
};

//==============================================================================
// ─── CLAMP ───────────────────────────────────────────────────────────────────

class ClampNode : public DSPNode
{
public:
    ClampNode() : DSPNode ("clamp", "Clamp")
    {
        addInput ("in"); addOutput ("out");
        addParam ("min", "Min", -10.0f, 10.0f, -1.0f);
        addParam ("max", "Max", -10.0f, 10.0f, 1.0f);
    }
    void process (const float** in, int, float** out, int, int n) override
    {
        float mn = getParam("min")->get(), mx = getParam("max")->get();
        for (int i = 0; i < n; ++i)
            out[0][i] = juce::jlimit(mn, mx, in[0][i]);
    }
};

//==============================================================================
// ─── ABS / NEGATE / INVERT ───────────────────────────────────────────────────

class AbsNode : public DSPNode
{
public:
    AbsNode() : DSPNode ("abs", "Abs (Rectify)")
    {
        addInput ("in"); addOutput ("out");
    }
    void process (const float** in, int, float** out, int, int n) override
    {
        for (int i = 0; i < n; ++i) out[0][i] = std::abs(in[0][i]);
    }
};

class NegateNode : public DSPNode
{
public:
    NegateNode() : DSPNode ("negate", "Negate (Invert)")
    {
        addInput ("in"); addOutput ("out");
    }
    void process (const float** in, int, float** out, int, int n) override
    {
        for (int i = 0; i < n; ++i) out[0][i] = -in[0][i];
    }
};

//==============================================================================
// ─── ENVELOPE FOLLOWER ───────────────────────────────────────────────────────
// Detects the amplitude envelope of the input signal.
// Wire to ranger → filter freq for auto-wah!

class EnvelopeFollowerNode : public DSPNode
{
public:
    EnvelopeFollowerNode() : DSPNode ("env_follower", "Envelope Follower")
    {
        addInput ("in");
        addOutput ("out", NodePort::Control);
        addParam ("attack", "Attack (ms)", 0.1f, 100.0f, 5.0f);
        addParam ("release", "Release (ms)", 1.0f, 1000.0f, 50.0f);
        addParam ("sensitivity", "Sensitivity", 0.1f, 10.0f, 1.0f);
    }
    void prepare (double sampleRate, int bs) override { DSPNode::prepare(sampleRate, bs); env = 0.0f; }
    void process (const float** in, int, float** out, int, int n) override
    {
        float attMs = getParam("attack")->get();
        float relMs = getParam("release")->get();
        float sens  = getParam("sensitivity")->get();
        float attC  = 1.0f - std::exp(-1.0f / ((float)sr * attMs * 0.001f));
        float relC  = 1.0f - std::exp(-1.0f / ((float)sr * relMs * 0.001f));

        for (int i = 0; i < n; ++i)
        {
            float absIn = std::abs(in[0][i]) * sens;
            float coef = (absIn > env) ? attC : relC;
            env += coef * (absIn - env);
            out[0][i] = juce::jlimit(0.0f, 1.0f, env);
        }
    }
    void reset() override { env = 0.0f; }
private:
    float env = 0.0f;
};

//==============================================================================
// ─── SAMPLE & HOLD ───────────────────────────────────────────────────────────
// Captures value of 'in' when 'trigger' goes high. Holds until next trigger.

class SampleHoldNode : public DSPNode
{
public:
    SampleHoldNode() : DSPNode ("sample_hold", "Sample & Hold")
    {
        addInput ("in"); addInput ("trigger", NodePort::Control);
        addOutput ("out");
    }
    void prepare (double sampleRate, int bs) override { DSPNode::prepare(sampleRate, bs); held = 0.0f; prevTrig = false; }
    void process (const float** in, int numIn, float** out, int, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            bool trig = (numIn > 1) ? in[1][i] > 0.5f : false;
            if (trig && !prevTrig) held = in[0][i];
            prevTrig = trig;
            out[0][i] = held;
        }
    }
    void reset() override { held = 0.0f; prevTrig = false; }
private:
    float held = 0.0f;
    bool prevTrig = false;
};

//==============================================================================
// ─── CLOCK / TIMER ───────────────────────────────────────────────────────────
// Generates a pulse train at a given BPM or Hz.

class ClockNode : public DSPNode
{
public:
    ClockNode() : DSPNode ("clock", "Clock / Timer")
    {
        addOutput ("pulse", NodePort::Control);
        addOutput ("ramp", NodePort::Control);
        addParam ("bpm", "BPM", 20.0f, 300.0f, 120.0f);
        addParam ("duty", "Duty Cycle", 0.01f, 0.99f, 0.5f);
    }
    void prepare (double sampleRate, int bs) override { DSPNode::prepare(sampleRate, bs); phase = 0.0; }
    void process (const float**, int, float** out, int numOut, int n) override
    {
        float bpm  = getParam("bpm")->get();
        float duty = getParam("duty")->get();
        double freq = bpm / 60.0;
        double inc = freq / sr;

        for (int i = 0; i < n; ++i)
        {
            out[0][i] = (phase < duty) ? 1.0f : 0.0f;
            if (numOut > 1) out[1][i] = (float) phase;
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
        }
    }
    void reset() override { phase = 0.0; }
private:
    double phase = 0.0;
};

//==============================================================================
// ─── COUNTER ─────────────────────────────────────────────────────────────────
// Counts rising edges on trigger. Wraps at max. Output is current count.

class CounterNode : public DSPNode
{
public:
    CounterNode() : DSPNode ("counter", "Counter")
    {
        addInput ("trigger", NodePort::Control);
        addInput ("reset", NodePort::Control);
        addOutput ("count", NodePort::Control);
        addParam ("max", "Max Count", 1.0f, 64.0f, 8.0f);
    }
    void prepare (double sampleRate, int bs) override { DSPNode::prepare(sampleRate, bs); count = 0; prevTrig = false; prevReset = false; }
    void process (const float** in, int numIn, float** out, int, int n) override
    {
        int maxCount = (int) getParam("max")->get();
        for (int i = 0; i < n; ++i)
        {
            bool trig = in[0][i] > 0.5f;
            bool rst  = (numIn > 1) ? in[1][i] > 0.5f : false;

            if (rst && !prevReset) count = 0;
            if (trig && !prevTrig) { count++; if (count >= maxCount) count = 0; }

            prevTrig = trig;
            prevReset = rst;
            out[0][i] = (float) count;
        }
    }
    void reset() override { count = 0; prevTrig = false; prevReset = false; }
private:
    int count = 0;
    bool prevTrig = false, prevReset = false;
};

//==============================================================================
// ─── SEQUENCER ───────────────────────────────────────────────────────────────
// Steps through 8 values on each clock pulse. Classic Wiremod sequencer chip.

class SequencerNode : public DSPNode
{
public:
    SequencerNode() : DSPNode ("sequencer", "Sequencer (8-step)")
    {
        addInput ("clock", NodePort::Control);
        addInput ("reset", NodePort::Control);
        addOutput ("out", NodePort::Control);
        addParam ("steps", "Steps", 1.0f, 8.0f, 4.0f);
        addParam ("s1", "Step 1", 0.0f, 1.0f, 0.0f);
        addParam ("s2", "Step 2", 0.0f, 1.0f, 0.25f);
        addParam ("s3", "Step 3", 0.0f, 1.0f, 0.5f);
        addParam ("s4", "Step 4", 0.0f, 1.0f, 0.75f);
        addParam ("s5", "Step 5", 0.0f, 1.0f, 1.0f);
        addParam ("s6", "Step 6", 0.0f, 1.0f, 0.75f);
        addParam ("s7", "Step 7", 0.0f, 1.0f, 0.5f);
        addParam ("s8", "Step 8", 0.0f, 1.0f, 0.25f);
    }
    void prepare (double sampleRate, int bs) override { DSPNode::prepare(sampleRate, bs); step = 0; prevClk = false; prevRst = false; }
    void process (const float** in, int numIn, float** out, int, int n) override
    {
        int numSteps = (int) getParam("steps")->get();
        float vals[8];
        vals[0] = getParam("s1")->get(); vals[1] = getParam("s2")->get();
        vals[2] = getParam("s3")->get(); vals[3] = getParam("s4")->get();
        vals[4] = getParam("s5")->get(); vals[5] = getParam("s6")->get();
        vals[6] = getParam("s7")->get(); vals[7] = getParam("s8")->get();

        for (int i = 0; i < n; ++i)
        {
            bool clk = in[0][i] > 0.5f;
            bool rst = (numIn > 1) ? in[1][i] > 0.5f : false;

            if (rst && !prevRst) step = 0;
            if (clk && !prevClk) { step++; if (step >= numSteps) step = 0; }

            prevClk = clk;
            prevRst = rst;
            out[0][i] = vals[step];
        }
    }
    void reset() override { step = 0; prevClk = false; prevRst = false; }
private:
    int step = 0;
    bool prevClk = false, prevRst = false;
};

//==============================================================================
// ─── EXPRESSION NODE (E2 Chip) ───────────────────────────────────────────────
// Scriptable DSP node — type math expressions to process audio!
// Inspired by Wiremod's Expression2 from Garry's Mod.

class ExpressionNode : public DSPNode
{
public:
    ExpressionNode() : DSPNode ("expression", "Expression (E2)")
    {
        addInput ("in"); addInput ("in2");
        addOutput ("out");
        addParam ("p1", "Param 1", -100.0f, 100.0f, 1.0f);
        addParam ("p2", "Param 2", -100.0f, 100.0f, 0.0f);
        addParam ("p3", "Param 3", -100.0f, 100.0f, 0.0f);
        addParam ("p4", "Param 4", -100.0f, 100.0f, 0.0f);

        // Default expression: pass-through with gain
        setExpression ("out = in * p1");
    }

    void prepare (double sampleRate, int bs) override
    {
        DSPNode::prepare (sampleRate, bs);
        vm.vars[ExpressionVM::VAR_SR] = (float) sampleRate;
        vm.vars[ExpressionVM::VAR_DT] = 1.0f / (float) sampleRate;
        vm.vars[ExpressionVM::VAR_T]  = 0.0f;
    }

    void process (const float** in, int numIn, float** out, int, int n) override
    {
        if (! vm.isCompiled()) {
            for (int i = 0; i < n; ++i) out[0][i] = 0.0f;
            return;
        }

        // Load user params
        vm.vars[ExpressionVM::VAR_P1] = getParam("p1")->get();
        vm.vars[ExpressionVM::VAR_P2] = getParam("p2")->get();
        vm.vars[ExpressionVM::VAR_P3] = getParam("p3")->get();
        vm.vars[ExpressionVM::VAR_P4] = getParam("p4")->get();

        float dt = vm.vars[ExpressionVM::VAR_DT];

        for (int i = 0; i < n; ++i)
        {
            vm.vars[ExpressionVM::VAR_IN]  = in[0][i];
            vm.vars[ExpressionVM::VAR_IN2] = (numIn > 1) ? in[1][i] : 0.0f;
            vm.vars[ExpressionVM::VAR_OUT] = 0.0f;

            float result = vm.evaluate();

            // Use explicit out= assignment if set, otherwise last expression value
            out[0][i] = (vm.vars[ExpressionVM::VAR_OUT] != 0.0f)
                      ? vm.vars[ExpressionVM::VAR_OUT]
                      : result;

            vm.vars[ExpressionVM::VAR_T] += dt;
        }
    }

    void reset() override { vm.resetState(); }

    //==========================================================================
    /** Set the expression source code. Called from the UI. */
    bool setExpression (const juce::String& expr)
    {
        expressionSource = expr;
        return vm.compile_and_store (expr);
    }

    const juce::String& getExpression() const { return expressionSource; }
    const juce::String& getCompileError() const { return vm.getError(); }

    //==========================================================================
    /** Override serialization to include expression source. */
    juce::var toJSON() const
    {
        auto base = DSPNode::toJSON();
        if (auto* obj = base.getDynamicObject())
            obj->setProperty ("expression", expressionSource);
        return base;
    }

    void fromJSON (const juce::var& json)
    {
        DSPNode::fromJSON (json);
        if (auto* obj = json.getDynamicObject())
        {
            auto expr = obj->getProperty ("expression").toString();
            if (expr.isNotEmpty())
                setExpression (expr);
        }
    }

private:
    ExpressionVM vm;
    juce::String expressionSource;
};

//==============================================================================
// ─── MIDI NODES ──────────────────────────────────────────────────────────────
// Connect MIDI controllers into the Wiremod signal flow.

/**
 * MIDI Note — Outputs the last-received note as control signals.
 * Outputs: note (0-127 normalized to 0-1), velocity (0-1), gate (0 or 1).
 */
class MidiNoteNode : public DSPNode
{
public:
    MidiNoteNode() : DSPNode ("midi_note", "MIDI Note")
    {
        addOutput ("note", NodePort::Control);
        addOutput ("velocity", NodePort::Control);
        addOutput ("gate", NodePort::Control);
        addParam ("channel", "Channel", 0.0f, 16.0f, 0.0f); // 0 = omni
    }

    void process (const float**, int, float** out, int, int n) override
    {
        int ch = (int) getParam("channel")->get();

        // Scan MIDI for note events
        if (midiBuffer)
        {
            for (const auto metadata : *midiBuffer)
            {
                auto msg = metadata.getMessage();
                if (ch > 0 && msg.getChannel() != ch) continue;

                if (msg.isNoteOn())
                {
                    currentNote = msg.getNoteNumber() / 127.0f;
                    currentVelocity = msg.getFloatVelocity();
                    gate = 1.0f;
                }
                else if (msg.isNoteOff())
                {
                    if (msg.getNoteNumber() / 127.0f == currentNote)
                    {
                        gate = 0.0f;
                        currentVelocity = 0.0f;
                    }
                }
            }
        }

        for (int i = 0; i < n; ++i)
        {
            out[0][i] = currentNote;
            out[1][i] = currentVelocity;
            out[2][i] = gate;
        }
    }

    void reset() override { currentNote = 0; currentVelocity = 0; gate = 0; }

private:
    float currentNote = 0.0f, currentVelocity = 0.0f, gate = 0.0f;
};

/**
 * MIDI CC — Extracts a specific CC number and outputs as 0-1 control signal.
 * Great for mapping knobs/faders on MIDI controllers to effect parameters.
 */
class MidiCCNode : public DSPNode
{
public:
    MidiCCNode() : DSPNode ("midi_cc", "MIDI CC")
    {
        addOutput ("value", NodePort::Control);
        addParam ("cc", "CC Number", 0.0f, 127.0f, 1.0f);
        addParam ("channel", "Channel", 0.0f, 16.0f, 0.0f); // 0 = omni
    }

    void process (const float**, int, float** out, int, int n) override
    {
        int targetCC = (int) getParam("cc")->get();
        int ch = (int) getParam("channel")->get();

        if (midiBuffer)
        {
            for (const auto metadata : *midiBuffer)
            {
                auto msg = metadata.getMessage();
                if (ch > 0 && msg.getChannel() != ch) continue;

                if (msg.isController() && msg.getControllerNumber() == targetCC)
                    currentValue = msg.getControllerValue() / 127.0f;
            }
        }

        for (int i = 0; i < n; ++i)
            out[0][i] = currentValue;
    }

    void reset() override { currentValue = 0.0f; }

private:
    float currentValue = 0.0f;
};

/**
 * MIDI Pitch Bend — Outputs pitch bend as a bipolar control signal (-1 to +1).
 */
class MidiPitchBendNode : public DSPNode
{
public:
    MidiPitchBendNode() : DSPNode ("midi_pitchbend", "MIDI Pitch Bend")
    {
        addOutput ("bend", NodePort::Control);
        addParam ("channel", "Channel", 0.0f, 16.0f, 0.0f);
    }

    void process (const float**, int, float** out, int, int n) override
    {
        int ch = (int) getParam("channel")->get();

        if (midiBuffer)
        {
            for (const auto metadata : *midiBuffer)
            {
                auto msg = metadata.getMessage();
                if (ch > 0 && msg.getChannel() != ch) continue;

                if (msg.isPitchWheel())
                    currentBend = (msg.getPitchWheelValue() - 8192) / 8192.0f;
            }
        }

        for (int i = 0; i < n; ++i)
            out[0][i] = currentBend;
    }

    void reset() override { currentBend = 0.0f; }

private:
    float currentBend = 0.0f;
};

/**
 * MIDI Clock — Syncs to external MIDI clock.
 * Outputs: pulse (1.0 on tick, 0 otherwise), beat position (0-1 within beat).
 * MIDI clock sends 24 ticks per quarter note.
 */
class MidiClockNode : public DSPNode
{
public:
    MidiClockNode() : DSPNode ("midi_clock", "MIDI Clock")
    {
        addOutput ("pulse", NodePort::Control);
        addOutput ("beat", NodePort::Control);
        addOutput ("running", NodePort::Gate);
    }

    void process (const float**, int, float** out, int, int n) override
    {
        // Process MIDI clock messages
        int tickSample = -1; // sample position of last tick in this block

        if (midiBuffer)
        {
            for (const auto metadata : *midiBuffer)
            {
                auto msg = metadata.getMessage();

                if (msg.isMidiClock())
                {
                    tickSample = metadata.samplePosition;
                    tickCount++;
                    if (tickCount >= 24) tickCount = 0; // 24 ppqn
                }
                else if (msg.isMidiStart())
                {
                    isRunning = true;
                    tickCount = 0;
                }
                else if (msg.isMidiStop())
                {
                    isRunning = false;
                }
                else if (msg.isMidiContinue())
                {
                    isRunning = true;
                }
            }
        }

        float beatPos = tickCount / 24.0f;
        float running = isRunning ? 1.0f : 0.0f;

        for (int i = 0; i < n; ++i)
        {
            out[0][i] = (i == tickSample) ? 1.0f : 0.0f;
            out[1][i] = beatPos;
            out[2][i] = running;
        }
    }

    void reset() override { tickCount = 0; isRunning = false; }

private:
    int tickCount = 0;
    bool isRunning = false;
};

#include "SynthNodeLibrary.h"
#include "MemoryNodeLibrary.h"
#include "MathNodeLibrary.h"
#include "LogicNodeLibrary.h"
#include "MidiNodeLibrary.h"
#include "ControlNodeLibrary.h"
