#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PedalInstance.h"

//==============================================================================
/**
 * Wraps juce::AudioProcessorGraph to manage a chain of effect pedals.
 * Provides a clean API for adding/removing/connecting pedals.
 */
class AudioGraphEngine
{
public:
    using NodeID = juce::AudioProcessorGraph::NodeID;

    AudioGraphEngine();
    ~AudioGraphEngine();

    //==========================================================================
    /** Prepare the graph for playback. */
    void prepare (double sampleRate, int samplesPerBlock,
                  int numInputChannels, int numOutputChannels);

    /** Release resources. */
    void releaseResources();

    /** Process a block of audio through the graph. */
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    //==========================================================================
    /** Add a pedal processor to the graph. Returns the new node's ID. */
    NodeID addPedal (std::unique_ptr<juce::AudioProcessor> processor,
                     int gridX, int gridY, int gridW, int gridH);

    /** Remove a pedal from the graph. */
    void removePedal (NodeID nodeId);

    /** Replace the processor for an existing pedal node, maintaining its connections. */
    void updatePedalProcessor (NodeID nodeId, std::unique_ptr<juce::AudioProcessor> newProcessor);

    /** Connect two pedal nodes (output of source → input of dest). */
    bool connect (NodeID sourceNode, int sourceChannel,
                  NodeID destNode,   int destChannel);

    /** Disconnect a specific connection. */
    bool disconnect (NodeID sourceNode, int sourceChannel,
                     NodeID destNode,   int destChannel);

    /** Remove all connections from/to a node. */
    void disconnectAll (NodeID nodeId);

    //==========================================================================
    /** Get the list of active pedal instances. */
    const std::vector<PedalInstance>& getPedalInstances() const { return instances; }

    /** Get a mutable reference to a pedal instance. */
    PedalInstance* getPedalInstance (NodeID nodeId);

    /** Get the underlying graph (for direct access if needed). */
    juce::AudioProcessorGraph& getGraph() { return graph; }

    /** Get the audio I/O node IDs. */
    NodeID getAudioInputNodeID()  const { return audioInputNodeID; }
    NodeID getAudioOutputNodeID() const { return audioOutputNodeID; }

    //==========================================================================
    /** Serialise the graph state to a JSON string. */
    juce::String serialise() const;

    /** Restore graph state from a JSON string. */
    void deserialise (const juce::String& jsonState);

private:
    juce::AudioProcessorGraph graph;
    std::vector<PedalInstance> instances;

    NodeID audioInputNodeID;
    NodeID audioOutputNodeID;
    NodeID midiInputNodeID;
    NodeID midiOutputNodeID;

    uint32_t nextNodeIndex = 1000; // Start IDs above the I/O nodes

    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;
    int    currentNumChannels = 2;

    void setupIONodes();
    void connectPassthrough();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGraphEngine)
};
