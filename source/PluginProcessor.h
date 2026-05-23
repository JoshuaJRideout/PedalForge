#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_dsp/juce_dsp.h>
#include "engine/AudioGraphEngine.h"
#include "preset/PresetManager.h"
#include "midi/MidiLearn.h"

#include "dsp/TestSoundGenerator.h"

//==============================================================================
class PedalForgeProcessor : public juce::AudioProcessor,
                             public juce::MidiInputCallback
{
public:
    PedalForgeProcessor();
    ~PedalForgeProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi()  const override { return true; }
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
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    AudioGraphEngine& getGraphEngine() { return graphEngine; }
    AudioGraphEngine& getPlayGraphEngine() { return playGraphEngine; }
    PresetManager& getPresetManager() { return presetManager; }
    MidiLearnManager& getMidiLearn() { return midiLearn; }

    void setPlayMode(bool isActive) { isPlayModeActive = isActive; }

    AudioGraphEngine graphEngine;
    AudioGraphEngine playGraphEngine;
    PresetManager presetManager;
    MidiLearnManager midiLearn;
    MidiLearnManager playMidiLearn;

    bool isPlayModeActive = false;

    // Test Sound state
    TestSoundGenerator testSoundGen;
    bool testSoundActive = false;
    bool isTestSoundActive() const { return testSoundActive; }
    void setTestSoundActive (bool active) { testSoundActive = active; const_cast<TestSoundGenerator&>(testSoundGen).setActive (active); }

    //==========================================================================
    // Hardware MIDI I/O — opened when the user enables devices in the Routing Tab
    std::vector<std::unique_ptr<juce::MidiInput>>  openMidiInputs;
    std::vector<std::unique_ptr<juce::MidiOutput>> openMidiOutputs;
    juce::MidiBuffer    hardwareMidiInBuffer;  // filled by callback thread
    juce::CriticalSection midiInputLock;
    juce::CriticalSection midiOutputLock;

    /** Called on the MIDI callback thread by each open MidiInput device. */
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& msg) override;

    //==========================================================================
    // Auto-Map state — dynamically assigns CC numbers to parameter slots
    std::map<int, int> autoMapCCSlots;     // CC number → parameter index
    juce::uint32 autoMapLastFocusedNode = 0; // reset slots when focus changes
    int autoMapNextSlot = 0;

    /** Opens/closes hardware MIDI devices based on what the engine has enabled. */
    void refreshHardwareMidiConnections();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalForgeProcessor)
};
