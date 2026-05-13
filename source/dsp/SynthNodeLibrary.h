#pragma once

#include "DSPNode.h"
#include <cmath>
#include <random>

//==============================================================================
// ─── OSCILLATORS ─────────────────────────────────────────────────────────────

class OscillatorNode : public DSPNode
{
public:
    OscillatorNode() : DSPNode ("oscillator", "Oscillator")
    {
        addInput ("pitch", NodePort::Control);
        addInput ("fm", NodePort::Audio);
        addInput ("pwm", NodePort::Control);
        addOutput ("out", NodePort::Audio);
        
        addParam ("waveform", "Waveform", 0.0f, 3.0f, 1.0f); // 0=Sine, 1=Saw, 2=Square, 3=Triangle
        addParam ("octave", "Octave", -4.0f, 4.0f, 0.0f);
        addParam ("semitone", "Semitone", -12.0f, 12.0f, 0.0f);
        addParam ("fine", "Fine", -100.0f, 100.0f, 0.0f);
    }

    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;

        float wave = getParam("waveform")->get();
        float oct = getParam("octave")->get();
        float semi = getParam("semitone")->get();
        float fine = getParam("fine")->get();

        for (int i = 0; i < n; ++i)
        {
            // Pitch input is 0..1 representing MIDI notes 0..127
            float pitchCV = (numIn > 0 && in[0] != nullptr) ? in[0][i] : 0.5f; // default to center
            float midiNote = pitchCV * 127.0f + (oct * 12.0f) + semi + (fine / 100.0f);
            
            // Linear FM
            float fm = (numIn > 1 && in[1] != nullptr) ? in[1][i] * 1000.0f : 0.0f;
            
            float freq = 440.0f * std::pow (2.0f, (midiNote - 69.0f) / 12.0f) + fm;
            freq = juce::jlimit (1.0f, 20000.0f, freq);
            
            float phaseInc = freq / (float)sr;
            
            float pwm = (numIn > 2 && in[2] != nullptr) ? in[2][i] : 0.5f;
            pwm = juce::jlimit (0.05f, 0.95f, pwm);

            float val = 0.0f;
            if (wave < 0.5f) // Sine
            {
                val = std::sin (phase * 2.0f * juce::MathConstants<float>::pi);
            }
            else if (wave < 1.5f) // Saw
            {
                val = 2.0f * phase - 1.0f;
            }
            else if (wave < 2.5f) // Square
            {
                val = (phase < pwm) ? 1.0f : -1.0f;
            }
            else // Triangle
            {
                val = 4.0f * std::abs(phase - 0.5f) - 1.0f;
            }

            out[0][i] = val;

            phase += phaseInc;
            if (phase >= 1.0f) phase -= 1.0f;
        }
    }
private:
    float phase = 0.0f;
};

class NoiseNode : public DSPNode
{
public:
    NoiseNode() : DSPNode ("noise", "Noise")
    {
        addOutput ("out", NodePort::Audio);
        addParam ("type", "Type", 0.0f, 2.0f, 0.0f); // 0=White, 1=Pink, 2=Brown
    }

    void process (const float**, int, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        float type = getParam("type")->get();

        for (int i = 0; i < n; ++i)
        {
            float white = dist(rng);
            float val = white;

            if (type > 0.5f && type < 1.5f) // Pink (Paul Kellet's method)
            {
                b0 = 0.99886f * b0 + white * 0.0555179f;
                b1 = 0.99332f * b1 + white * 0.0750759f;
                b2 = 0.96900f * b2 + white * 0.1538520f;
                b3 = 0.86650f * b3 + white * 0.3104856f;
                b4 = 0.55000f * b4 + white * 0.5329522f;
                b5 = -0.7616f * b5 - white * 0.0168980f;
                val = b0 + b1 + b2 + b3 + b4 + b5 + white * 0.5362f;
                val *= 0.11f; // compensate gain
            }
            else if (type >= 1.5f) // Brown
            {
                val = (lastBrown + (0.02f * white)) / 1.02f;
                lastBrown = val;
                val *= 3.5f; // compensate gain
            }

            out[0][i] = val;
        }
    }
private:
    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> dist{-1.0f, 1.0f};
    float b0=0, b1=0, b2=0, b3=0, b4=0, b5=0;
    float lastBrown = 0.0f;
};

//==============================================================================
// ─── ENVELOPES ───────────────────────────────────────────────────────────────

class ADSRNode : public DSPNode
{
public:
    ADSRNode() : DSPNode ("adsr", "ADSR")
    {
        addInput ("gate", NodePort::Control);
        addOutput ("out", NodePort::Control);
        
        addParam ("attack", "Attack (s)", 0.001f, 10.0f, 0.1f);
        addParam ("decay", "Decay (s)", 0.001f, 10.0f, 0.1f);
        addParam ("sustain", "Sustain", 0.0f, 1.0f, 0.5f);
        addParam ("release", "Release (s)", 0.001f, 10.0f, 0.5f);
    }

    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        
        float a = getParam("attack")->get();
        float d = getParam("decay")->get();
        float s = getParam("sustain")->get();
        float r = getParam("release")->get();

        float aRate = 1.0f / (a * sr);
        float dRate = 1.0f / (d * sr);
        float rRate = 1.0f / (r * sr);

        for (int i = 0; i < n; ++i)
        {
            bool gate = (numIn > 0 && in[0] != nullptr) && in[0][i] > 0.5f;
            
            if (gate && !lastGate) state = 1; // Attack
            if (!gate && lastGate) state = 4; // Release
            lastGate = gate;

            switch (state)
            {
                case 1: // Attack
                    val += aRate;
                    if (val >= 1.0f) { val = 1.0f; state = 2; }
                    break;
                case 2: // Decay
                    val -= dRate;
                    if (val <= s) { val = s; state = 3; }
                    break;
                case 3: // Sustain
                    val = s;
                    break;
                case 4: // Release
                    val -= rRate;
                    if (val <= 0.0f) { val = 0.0f; state = 0; }
                    break;
            }

            out[0][i] = val;
        }
    }
private:
    int state = 0; // 0=Idle, 1=A, 2=D, 3=S, 4=R
    float val = 0.0f;
    bool lastGate = false;
};

class ARNode : public DSPNode
{
public:
    ARNode() : DSPNode ("ar_env", "AR Env")
    {
        addInput ("gate", NodePort::Control);
        addOutput ("out", NodePort::Control);
        addParam ("attack", "Attack (s)", 0.001f, 10.0f, 0.01f);
        addParam ("release", "Release (s)", 0.001f, 10.0f, 0.5f);
    }

    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        float aRate = 1.0f / (getParam("attack")->get() * sr);
        float rRate = 1.0f / (getParam("release")->get() * sr);

        for (int i = 0; i < n; ++i)
        {
            bool gate = (numIn > 0 && in[0] != nullptr) && in[0][i] > 0.5f;
            if (gate)
            {
                val += aRate;
                if (val > 1.0f) val = 1.0f;
            }
            else
            {
                val -= rRate;
                if (val < 0.0f) val = 0.0f;
            }
            out[0][i] = val;
        }
    }
private:
    float val = 0.0f;
};

//==============================================================================
// ─── FILTERS ─────────────────────────────────────────────────────────────────

class SVFNode : public DSPNode
{
public:
    SVFNode() : DSPNode ("svf", "SVF Filter")
    {
        addInput ("in", NodePort::Audio);
        addInput ("cutoff_cv", NodePort::Control);
        addInput ("res_cv", NodePort::Control);
        
        addOutput ("lp", NodePort::Audio);
        addOutput ("hp", NodePort::Audio);
        addOutput ("bp", NodePort::Audio);
        addOutput ("notch", NodePort::Audio);
        
        addParam ("cutoff", "Cutoff (Hz)", 20.0f, 20000.0f, 1000.0f);
        addParam ("res", "Resonance", 0.0f, 0.99f, 0.1f);
    }

    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        float baseCutoff = getParam("cutoff")->get();
        float baseRes = getParam("res")->get();

        for (int i = 0; i < n; ++i)
        {
            float input = (numIn > 0 && in[0] != nullptr) ? in[0][i] : 0.0f;
            float cutCV = (numIn > 1 && in[1] != nullptr) ? in[1][i] : 0.0f;
            float resCV = (numIn > 2 && in[2] != nullptr) ? in[2][i] : 0.0f;

            // Exponential FM for cutoff (1V/oct roughly)
            float freq = baseCutoff * std::pow(2.0f, cutCV * 10.0f);
            freq = juce::jlimit (20.0f, 20000.0f, freq);
            
            float res = juce::jlimit (0.0f, 0.99f, baseRes + resCV);
            
            float q = 1.0f - res;
            float p = freq * juce::MathConstants<float>::pi / sr;
            float f = 2.0f * std::sin(p);

            hp = input - lp - q * bp;
            bp += f * hp;
            lp += f * bp;
            float notch = hp + lp;

            if (numOut > 0 && out[0]) out[0][i] = lp;
            if (numOut > 1 && out[1]) out[1][i] = hp;
            if (numOut > 2 && out[2]) out[2][i] = bp;
            if (numOut > 3 && out[3]) out[3][i] = notch;
        }
    }
private:
    float lp=0.0f, hp=0.0f, bp=0.0f;
};

class LadderFilterNode : public DSPNode
{
public:
    LadderFilterNode() : DSPNode ("ladder_filter", "Ladder Filter")
    {
        addInput ("in", NodePort::Audio);
        addInput ("cutoff_cv", NodePort::Control);
        addInput ("res_cv", NodePort::Control);
        addOutput ("out", NodePort::Audio);
        
        addParam ("cutoff", "Cutoff (Hz)", 20.0f, 20000.0f, 1000.0f);
        addParam ("res", "Resonance", 0.0f, 4.0f, 0.5f);
        addParam ("drive", "Drive", 1.0f, 10.0f, 1.0f);
    }

    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;

        float baseCutoff = getParam("cutoff")->get();
        float baseRes = getParam("res")->get();
        float drive = getParam("drive")->get();

        for (int i = 0; i < n; ++i)
        {
            float input = (numIn > 0 && in[0] != nullptr) ? in[0][i] : 0.0f;
            float cutCV = (numIn > 1 && in[1] != nullptr) ? in[1][i] : 0.0f;
            float resCV = (numIn > 2 && in[2] != nullptr) ? in[2][i] : 0.0f;

            float freq = baseCutoff * std::pow(2.0f, cutCV * 10.0f);
            freq = juce::jlimit (20.0f, 20000.0f, freq);
            float res = juce::jlimit (0.0f, 4.0f, baseRes + resCV * 4.0f);
            
            float wc = 2.0f * juce::MathConstants<float>::pi * freq / sr;
            float g = std::tan(wc / 2.0f);
            float g1 = g / (1.0f + g);

            // Simple 4-pole approximation
            float sig = std::tanh (input * drive) - res * s[3];
            
            for (int k=0; k<4; ++k)
            {
                float v = (sig - s[k]) * g1;
                float y = v + s[k];
                s[k] = y + v;
                sig = y;
            }

            out[0][i] = sig;
        }
    }
private:
    float s[4] = {0,0,0,0};
};

//==============================================================================
// ─── AMPLIFIERS ──────────────────────────────────────────────────────────────

class VCANode : public DSPNode
{
public:
    VCANode() : DSPNode ("vca", "VCA")
    {
        addInput ("in", NodePort::Audio);
        addInput ("cv", NodePort::Control);
        addOutput ("out", NodePort::Audio);
        addParam ("response", "Response", 0.0f, 1.0f, 0.0f); // 0=Linear, 1=Exponential
        addParam ("gain", "Base Gain", 0.0f, 2.0f, 0.0f);
    }

    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        float isExp = getParam("response")->get() > 0.5f;
        float baseGain = getParam("gain")->get();

        for (int i = 0; i < n; ++i)
        {
            float input = (numIn > 0 && in[0] != nullptr) ? in[0][i] : 0.0f;
            float cv = (numIn > 1 && in[1] != nullptr) ? in[1][i] : 0.0f;
            
            float g = baseGain + cv;
            if (isExp)
                g = std::pow (10.0f, 2.0f * (g - 1.0f)); // rough expo mapping
                
            out[0][i] = input * juce::jlimit(0.0f, 10.0f, g);
        }
    }
};

//==============================================================================
// ─── PERFORMANCE TOOLS ───────────────────────────────────────────────────────

class GlideNode : public DSPNode
{
public:
    GlideNode() : DSPNode ("glide", "Glide (Porta)")
    {
        addInput ("in", NodePort::Control);
        addInput ("gate", NodePort::Control);
        addOutput ("out", NodePort::Control);
        addParam ("time", "Time (s)", 0.0f, 5.0f, 0.1f);
        addParam ("legato", "Legato Only", 0.0f, 1.0f, 0.0f);
    }

    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        
        float time = getParam("time")->get();
        float coeff = time > 0.001f ? std::exp (-1.0f / (time * sr)) : 0.0f;
        bool legatoOnly = getParam("legato")->get() > 0.5f;

        for (int i = 0; i < n; ++i)
        {
            float input = (numIn > 0 && in[0] != nullptr) ? in[0][i] : 0.0f;
            bool gate = (numIn > 1 && in[1] != nullptr) && in[1][i] > 0.5f;
            
            if (!lastGate && gate && !legatoOnly)
                current = input; // snap on new note if not legato
            else
                current = input + coeff * (current - input);

            lastGate = gate;
            out[0][i] = current;
        }
    }
private:
    float current = 0.0f;
    bool lastGate = false;
};

class VoiceAllocatorNode : public DSPNode
{
public:
    VoiceAllocatorNode() : DSPNode ("voice_alloc", "Voice Allocator")
    {
        addOutput ("p1", NodePort::Control); addOutput ("g1", NodePort::Control);
        addOutput ("p2", NodePort::Control); addOutput ("g2", NodePort::Control);
        addOutput ("p3", NodePort::Control); addOutput ("g3", NodePort::Control);
        addOutput ("p4", NodePort::Control); addOutput ("g4", NodePort::Control);
        
        addParam ("polyphony", "Voices", 1.0f, 4.0f, 4.0f);
    }

    void process (const float**, int, float** out, int numOut, int n) override
    {
        if (midiBuffer)
        {
            for (const auto metadata : *midiBuffer)
            {
                auto msg = metadata.getMessage();
                if (msg.isNoteOn())
                {
                    // Find free voice
                    int bestVoice = 0;
                    for (int v=0; v<4; ++v) {
                        if (!gates[v]) { bestVoice = v; break; }
                    }
                    pitches[bestVoice] = msg.getNoteNumber() / 127.0f;
                    gates[bestVoice] = true;
                    notes[bestVoice] = msg.getNoteNumber();
                }
                else if (msg.isNoteOff())
                {
                    for (int v=0; v<4; ++v) {
                        if (notes[v] == msg.getNoteNumber()) {
                            gates[v] = false;
                        }
                    }
                }
            }
        }
        
        for (int v=0; v<4; ++v)
        {
            if (numOut > v*2+1)
            {
                for (int i = 0; i < n; ++i)
                {
                    if (out[v*2]) out[v*2][i] = pitches[v];
                    if (out[v*2+1]) out[v*2+1][i] = gates[v] ? 1.0f : 0.0f;
                }
            }
        }
    }
private:
    float pitches[4] = {0,0,0,0};
    bool gates[4] = {false,false,false,false};
    int notes[4] = {-1,-1,-1,-1};
};
