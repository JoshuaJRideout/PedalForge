#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FaustTypes.h"
#include <functional>
#include <map>
#include <string>

//==============================================================================
/**
 * Wraps a FAUST-generated dsp class into a juce::AudioProcessor.
 *
 * This adapter:
 *   - Instantiates a FAUST dsp object
 *   - Discovers parameters via buildUserInterface
 *   - Exposes them as JUCE AudioProcessorParameters
 *   - Delegates processBlock → faust compute()
 *
 * Usage:
 *   auto pedal = std::make_unique<FaustPedal>("Overdrive",
 *       []() { return new overdrive(); });
 */
class FaustPedal : public juce::AudioProcessor
{
public:
    using DspFactory = std::function<dsp*()>;

    FaustPedal (const juce::String& name, DspFactory factory);
    ~FaustPedal() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    //==========================================================================
    const juce::String getName() const override { return pedalName; }

    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==========================================================================
    int   getNumPrograms() override { return 1; }
    int   getCurrentProgram() override { return 0; }
    void  setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void  changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    /** Get the parameter zones mapped by name (for external control). */
    const std::map<std::string, float*>& getParameterZones() const { return parameterZones; }

private:
    //==========================================================================
    /**
     * Custom UI class that captures FAUST parameter declarations
     * and maps them to JUCE AudioParameters.
     */
    class ParameterMapper;

    juce::String pedalName;
    DspFactory dspFactory;
    std::unique_ptr<dsp> faustDsp;

    // Maps parameter label → FAUST zone pointer
    std::map<std::string, float*> parameterZones;

    // Maps parameter label → JUCE parameter
    std::map<std::string, juce::AudioParameterFloat*> juceParams;

    int numFaustInputs  = 0;
    int numFaustOutputs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FaustPedal)
};
