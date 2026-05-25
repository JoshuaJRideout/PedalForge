#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <list>
#include <vector>
#include "PedalInstance.h"
#include "BoardConfig.h"
#include "AppMidiConfig.h"
#include "StickyNoteData.h"
#include "../dsp/PedalDesign.h"

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
    /** Undo/Redo support */
    void saveUndoState();
    bool undo();
    bool redo();
    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }
    void clearUndoHistory();

    //==========================================================================
    /** Board management */
    std::list<BoardConfig>& getBoards() { return boards; }
    const std::list<BoardConfig>& getBoards() const { return boards; }
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
                     float boardX, float boardY, float boardW, float boardH,
                     NodeID customNodeId = {});

    /** Add a pedal processor to the graph WITHOUT placing it on the board.
        The pedal exists in the engine for routing but onBoard is false. */
    NodeID addPedalOffBoard (std::unique_ptr<juce::AudioProcessor> processor,
                             NodeID customNodeId = {});

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
    const std::list<PedalInstance>& getPedalInstances() const { return instances; }

    /** Get a mutable reference to a pedal instance. */
    PedalInstance* getPedalInstance (NodeID nodeId);

    /** Get the underlying graph (for direct access if needed). */
    juce::AudioProcessorGraph& getGraph() { return graph; }

    /** Get the audio I/O node IDs. */
    NodeID getAudioInputNodeID()  const { return audioInputNodeID; }
    NodeID getAudioOutputNodeID() const { return audioOutputNodeID; }
    NodeID getMidiInputNodeID()   const { return midiInputNodeID; }
    NodeID getMidiOutputNodeID()  const { return midiOutputNodeID; }

    //==========================================================================
    /** Serialise the graph state to a JSON string. */
    juce::String serialise() const;

    /** Restore graph state from a JSON string. */
    void deserialise (const juce::String& jsonState);

    // Routing-tab positions for the fixed I/O nodes (public for RoutingGraphEditor)
    float audioInRouteX  = 80.0f,  audioInRouteY  = 200.0f;
    float audioOutRouteX = 800.0f, audioOutRouteY = 200.0f;
    float midiInRouteX   = 80.0f,  midiInRouteY   = 320.0f;
    float midiOutRouteX  = 800.0f, midiOutRouteY  = 320.0f;
    
    AppMidiConfig appMidiConfig;

    //==========================================================================
    // Focused Pedal — used by Auto-Map to target the selected pedal
    void setFocusedPedal (NodeID id)  { focusedPedalNodeID.store (id.uid); }
    NodeID getFocusedPedal() const    { return NodeID { focusedPedalNodeID.load() }; }
    std::atomic<juce::uint32> focusedPedalNodeID { 0 };

    /** Navigate pedalboard pages (dir: -1 = left, +1 = right). Affects all boards. */
    void cyclePage (int dir);

    /** Navigate between pedals on the current page (dir: -1 = prev, +1 = next).
        Sets the focused pedal for auto-map. */
    void cycleTrack (int dir);

    //==========================================================================
    // Live MIDI Monitor
    struct MidiMonitorEvent
    {
        juce::Time time;
        juce::MidiMessage message;
        juce::String source;
    };

    void logMidiMessage (const juce::MidiMessage& msg, const juce::String& source)
    {
        const juce::ScopedLock sl (midiMonitorLock);
        MidiMonitorEvent ev { juce::Time::getCurrentTime(), msg, source };
        midiMonitorQueue.add (ev);
        if (midiMonitorQueue.size() > 100)
            midiMonitorQueue.remove (0);
        midiMonitorTriggered.store (true);
    }

    juce::Array<MidiMonitorEvent> getMidiMonitorEvents() const
    {
        const juce::ScopedLock sl (midiMonitorLock);
        return midiMonitorQueue;
    }

    void clearMidiMonitor()
    {
        const juce::ScopedLock sl (midiMonitorLock);
        midiMonitorQueue.clear();
        midiMonitorTriggered.store (true);
    }

    bool hasNewMidiMonitorEvents() const
    {
        return midiMonitorTriggered.load();
    }

    void resetMidiMonitorTrigger()
    {
        midiMonitorTriggered.store (false);
    }

    //==========================================================================
    // Board-Level Routing Connections (MIDI and Expression — not audio graph connections)
    //
    // When a user wires a MIDI Out from pedal A to a MIDI In on pedal B in the
    // Routing Tab, we store it here rather than in the AudioProcessorGraph.
    // At processBlock time, MIDI messages and expression values are propagated
    // along these connections.
    struct BoardRoutingConnection
    {
        NodeID srcNodeId {};
        juce::String srcPortId;   // e.g. "midi_out", "expr_out_1"
        NodeID dstNodeId {};
        juce::String dstPortId;   // e.g. "midi_in", "expr_in_1"
    };

    void addBoardConnection    (const BoardRoutingConnection& conn);
    void removeBoardConnection (NodeID srcNodeId, const juce::String& srcPortId,
                                NodeID dstNodeId, const juce::String& dstPortId);
    const std::vector<BoardRoutingConnection>& getBoardConnections() const { return boardConnections; }

    //==========================================================================
    // Engine-scoped scripts — used by the Scripting tab's Pedalboard mode (mode 4).
    // Per-pedal scripts live on PedalDesign; board-wide scripts live here so they
    // survive across pedal swaps and project saves.
    std::vector<PedalDesign::Script> engineScripts;
    
    std::function<void()> onBoardConnectionsChanged;

    //==========================================================================
    // Hardware MIDI Devices — shown as nodes in the Routing Tab
    struct HardwareMidiDevice
    {
        NodeID engineNodeId {};
        juce::String deviceName;
        bool isInput;           // true = MIDI In device, false = MIDI Out device
        float routeX = 80.0f;
        float routeY = 400.0f;
        std::shared_ptr<std::atomic<bool>> activity { std::make_shared<std::atomic<bool>>(false) };
    };

    std::vector<HardwareMidiDevice>& getHardwareMidiDevices()       { return hwMidiDevices; }
    const std::vector<HardwareMidiDevice>& getHardwareMidiDevices() const { return hwMidiDevices; }
    bool refreshHardwareMidiDevices();  // re-enumerates system MIDI devices, returns true if changed
    
    /** Injects a MIDI message directly into the dummy node for the given hardware device */
    void injectHardwareMidi (const juce::String& deviceName, const juce::MidiMessage& msg);
    
    /** Retrieves MIDI generated by the graph that was routed to the given hardware output node */
    void extractHardwareMidi (const juce::String& deviceName, juce::MidiBuffer& dest);

    //==========================================================================
    // Main I/O Meters
    std::atomic<float> mainInRMS[2] { {0.0f}, {0.0f} };
    std::atomic<float> mainOutRMS[2] { {0.0f}, {0.0f} };
    std::atomic<bool> mainMidiIn { false };

    //==========================================================================
    // Session Notes
    std::vector<StickyNote> boardNotes;
    std::vector<StickyNote> routeNotes;
    std::vector<StickyNote> playNotes;

private:
    juce::AudioProcessorGraph graph;
    std::list<PedalInstance> instances;
    std::list<BoardConfig> boards;
    std::vector<BoardRoutingConnection> boardConnections;
    std::vector<HardwareMidiDevice> hwMidiDevices;

    NodeID audioInputNodeID;
    NodeID audioOutputNodeID;
    NodeID midiInputNodeID;
    NodeID midiOutputNodeID;

    uint32_t nextNodeIndex = 1000; // Start IDs above the I/O nodes

    // Undo/Redo stack
    std::vector<juce::String> undoStack;
    std::vector<juce::String> redoStack;
    static constexpr size_t maxUndoDepth = 50;
    bool isRestoringState = false;

    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;
    int    currentNumChannels = 2;


    void setupIONodes();
    void connectPassthrough();
    void autoRebuildIOConnections (int numInputs, int numOutputs);

    juce::CriticalSection midiMonitorLock;
    juce::Array<MidiMonitorEvent> midiMonitorQueue;
    std::atomic<bool> midiMonitorTriggered { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGraphEngine)
};
