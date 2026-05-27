#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/**
 * No-op AudioProcessor wrapper for Companion pedals (displays, MIDI foot
 * controllers, etc.).
 *
 * Companion pedals don't process audio — they exist on the board only as
 * UI + control surfaces. AudioGraphEngine excludes them from the audio
 * routing pass, but every PedalInstance still needs *some* AudioProcessor
 * by JUCE's contract, so we hand it this one.
 *
 * Has zero audio buses (no inputs, no outputs). processBlock is a no-op.
 * Accepts/produces MIDI so script-driven Companion pedals (e.g. a foot
 * controller wrapper) can still route MIDI through it later if needed.
 */
class CompanionPedalProcessor : public juce::AudioProcessor
{
public:
    explicit CompanionPedalProcessor (const juce::String& name)
        : AudioProcessor (BusesProperties()),   // no inputs, no outputs
          pedalName (name) {}

    ~CompanionPedalProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override
    {
        // No audio. MIDI is left untouched — anything upstream that routes
        // MIDI through a Companion pedal sees it pass through verbatim.
    }

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    const juce::String getName() const override { return pedalName; }
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }   // signal to host: no audio
    double getTailLengthSeconds() const override { return 0.0; }

    int   getNumPrograms() override                       { return 1; }
    int   getCurrentProgram() override                    { return 0; }
    void  setCurrentProgram (int) override                {}
    const juce::String getProgramName (int) override      { return {}; }
    void  changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override   {}

private:
    juce::String pedalName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompanionPedalProcessor)
};
