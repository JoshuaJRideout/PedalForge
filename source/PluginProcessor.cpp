#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PedalForgeProcessor::PedalForgeProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",    juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output",   juce::AudioChannelSet::stereo(), true)
                      .withInput  ("FX Return", juce::AudioChannelSet::stereo(), false)
                      .withOutput ("FX Send",   juce::AudioChannelSet::stereo(), false)),
      presetManager (*this),
      midiLearn (*this)
{
}

PedalForgeProcessor::~PedalForgeProcessor()
{
}

//==============================================================================
void PedalForgeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    graphEngine.prepare (sampleRate, samplesPerBlock,
                         getTotalNumInputChannels(),
                         getTotalNumOutputChannels());
}

void PedalForgeProcessor::releaseResources()
{
    graphEngine.releaseResources();
}

bool PedalForgeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // We support any layout up to 32 channels.
    // In standalone mode, this lets the user map all their hardware I/O freely.
    int totalIn = 0;
    for (auto& bus : layouts.inputBuses)
        totalIn += bus.size();

    int totalOut = 0;
    for (auto& bus : layouts.outputBuses)
        totalOut += bus.size();

    if (totalIn > 32 || totalOut > 32)
        return false;

    return true;
}

void PedalForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Process MIDI Learn assignments
    midiLearn.processMidi (midiMessages);

    // Process audio through the pedal graph
    graphEngine.processBlock (buffer, midiMessages);
}

//==============================================================================
juce::AudioProcessorEditor* PedalForgeProcessor::createEditor()
{
    return new PedalForgeEditor (*this);
}

//==============================================================================
void PedalForgeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = presetManager.serializeState();
    destData.replaceAll (state.toRawUTF8(),
                          state.getNumBytesAsUTF8());
}

void PedalForgeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto state = juce::String::fromUTF8 (static_cast<const char*> (data),
                                          sizeInBytes);
    presetManager.restoreState (state);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PedalForgeProcessor();
}
