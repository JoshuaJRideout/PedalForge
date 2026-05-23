#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

class TestSoundGenerator
{
public:
    TestSoundGenerator() = default;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        phase = 0.0;
        envelope = 0.0f;
        triggerTime = 0.0f;
        noteIdx = 0;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! active) return;

        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        if (numChannels == 0) return;

        float* channelDataL = buffer.getWritePointer(0);
        float* channelDataR = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            // Trigger a note every 1.5 seconds
            triggerTime += 1.0f / (float)sr;
            if (triggerTime >= 1.5f)
            {
                triggerTime = 0.0f;
                float freqs[] = { 130.81f, 164.81f, 196.00f, 261.63f };
                freq = freqs[noteIdx];
                noteIdx = (noteIdx + 1) % 4;
                envelope = 1.0f;
            }

            // Simple guitar-like pluck: triangle + slight square + sub sine
            float tri = 4.0f * std::abs(std::fmod((float)phase + 0.25f, 1.0f) - 0.5f) - 1.0f;
            float sq  = std::fmod((float)phase, 1.0f) < 0.5f ? 1.0f : -1.0f;
            float sine = std::sin((float)phase * 2.0f * juce::MathConstants<float>::pi);
            float voice = (tri * 0.5f + sq * 0.1f + sine * 0.4f) * envelope;

            // Exponential pluck decay (decays in ~1.2s)
            envelope *= std::exp(-2.5f / (float)sr);

            // Advance phase
            phase += freq / sr;
            if (phase >= 1.0) phase -= 1.0;

            // Overwrite input buffer with synthesized test sound
            channelDataL[i] = voice * 0.4f;
            if (channelDataR) channelDataR[i] = voice * 0.4f;
        }
    }

    void setActive (bool act) { active = act; if (!active) envelope = 0.0f; }
    bool isActive() const { return active; }

private:
    double sr = 44100.0;
    double phase = 0.0;
    float freq = 220.0f;
    float envelope = 0.0f;
    float triggerTime = 0.0f;
    int noteIdx = 0;
    bool active = false;
};
