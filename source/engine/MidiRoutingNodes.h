#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "BoardConfig.h"

//==============================================================================
/**
 * A dummy processor representing a physical hardware MIDI input.
 * The PluginProcessor pushes incoming hardware MIDI events into this node,
 * and the AudioProcessorGraph ferries them to whatever pedals the user has connected.
 */
class HardwareMidiInputNode : public juce::AudioProcessor
{
public:
    HardwareMidiInputNode (const juce::String& name)
        : AudioProcessor (BusesProperties()), deviceName(name)
    {}

    const juce::String getName() const override { return deviceName; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer& midiMessages) override 
    {
        const juce::ScopedLock sl (lock);
        midiMessages.addEvents (injectedMidi, 0, -1, 0);
        injectedMidi.clear();
    }
    
    void pushMidiMessage (const juce::MidiMessage& msg)
    {
        const juce::ScopedLock sl (lock);
        injectedMidi.addEvent (msg, 0);
    }

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    juce::String deviceName;
    juce::MidiBuffer injectedMidi;
    juce::CriticalSection lock;
};

//==============================================================================
/**
 * A dummy processor representing a physical hardware MIDI output.
 * The AudioProcessorGraph ferries MIDI to this node. The PluginProcessor
 * reads the captured MIDI after processBlock and sends it to the actual hardware.
 */
class HardwareMidiOutputNode : public juce::AudioProcessor
{
public:
    HardwareMidiOutputNode (const juce::String& name)
        : AudioProcessor (BusesProperties()), deviceName(name)
    {}

    const juce::String getName() const override { return deviceName; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer& midiMessages) override 
    {
        const juce::ScopedLock sl (lock);
        capturedMidi.addEvents (midiMessages, 0, -1, 0);
        midiMessages.clear();
    }
    
    void popCapturedMidi (juce::MidiBuffer& dest)
    {
        const juce::ScopedLock sl (lock);
        dest.addEvents (capturedMidi, 0, -1, 0);
        capturedMidi.clear();
    }

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    juce::String deviceName;
    juce::MidiBuffer capturedMidi;
    juce::CriticalSection lock;
};

//==============================================================================
/**
 * A dummy processor representing a Pedalboard container.
 * When MIDI is routed to this node on the Routing Canvas, it evaluates CCs
 * to change its assigned BoardConfig's active page.
 */
class AudioGraphEngine;

class BoardMidiReceiverNode : public juce::AudioProcessor
{
public:
    BoardMidiReceiverNode (AudioGraphEngine& eng, const juce::String& bId)
        : AudioProcessor (BusesProperties()), engine(eng), boardId(bId)
    {}

    const juce::String getName() const override { return boardId + " Receiver"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer& midiMessages) override;

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    AudioGraphEngine& engine;
    juce::String boardId;
};
