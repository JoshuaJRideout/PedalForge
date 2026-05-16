#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PedalInstance.h"
#include "BoardConfig.h"
#include "AppMidiConfig.h"

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
    /** Board management */
    std::vector<BoardConfig>& getBoards() { return boards; }
    const std::vector<BoardConfig>& getBoards() const { return boards; }
    BoardConfig* getBoard (const juce::String& boardId);
    void addBoard (const BoardConfig& board);
    void removeBoard (const juce::String& boardId);

    //==========================================================================
    /** Prepare the graph for playback. */
    void prepare (double sampleRate, int samplesPerBlock,
                  int numInputChannels, int numOutputChannels);

    /** Release resources. */
    void releaseResources();

    /** Process a block of audio through the graph. */
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    //==========================================================================
    /** Add a pedal processor to the graph at a board position. Returns the new node's ID. */
    NodeID addPedal (std::unique_ptr<juce::AudioProcessor> processor,
                     const juce::String& boardId, int pageIndex,
                     int gridX, int gridY, int gridW, int gridH);

    /** Add a pedal processor to the graph WITHOUT placing it on the board.
        The pedal exists in the engine for routing but onBoard is false. */
    NodeID addPedalOffBoard (std::unique_ptr<juce::AudioProcessor> processor);

    /** Safely splices a newly added pedal into the signal chain based on its physical left-neighbor. */
    void autoRoutePedal (NodeID newNodeId);

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

    /** Cycle the Turing display pedal via MIDI */
    void cycleTuringPedal(int dir);

    /** Check if a node has any connections (input or output). */
    bool hasConnections (NodeID nodeId) const;

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

    // Routing-tab positions for the fixed I/O nodes (public for RoutingGraphEditor)
    float audioInRouteX  = 80.0f,  audioInRouteY  = 200.0f;
    float audioOutRouteX = 800.0f, audioOutRouteY = 200.0f;
    
    AppMidiConfig appMidiConfig;

private:
    juce::AudioProcessorGraph graph;
    std::vector<PedalInstance> instances;
    std::vector<BoardConfig> boards;

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
