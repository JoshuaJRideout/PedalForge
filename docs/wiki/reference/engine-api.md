# AudioGraphEngine API Reference

The `AudioGraphEngine` is the top-level audio processing engine. It wraps a `juce::AudioProcessorGraph` to manage a chain of effect pedals with routing, undo/redo, and serialization.

## Type Aliases

```cpp
using NodeID = juce::AudioProcessorGraph::NodeID;
```

## Audio Lifecycle

### prepare

```cpp
void prepare (double sampleRate, int samplesPerBlock,
              int numInputChannels, int numOutputChannels);
```

Prepare the graph for playback. Called when the host sets up audio processing.

### releaseResources

```cpp
void releaseResources();
```

Release audio resources when playback stops.

### processBlock

```cpp
void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);
```

Process a block of audio through the entire pedal graph.

---

## Pedal Management

### addPedal

```cpp
NodeID addPedal (std::unique_ptr<juce::AudioProcessor> processor,
                 const juce::String& boardId, int pageIndex,
                 float boardX, float boardY, float boardW, float boardH,
                 NodeID customNodeId = {});
```

Add a pedal processor to the graph at a board position. Returns the new node's ID.

| Parameter | Description |
|-----------|-------------|
| `processor` | The audio processor (typically a `GraphPedalProcessor`) |
| `boardId` | Which board page to place on |
| `pageIndex` | Page index within the board |
| `boardX/Y` | Position on the board grid |
| `boardW/H` | Size on the board grid |
| `customNodeId` | Optional: force a specific NodeID (used during deserialization) |

### addPedalOffBoard

```cpp
NodeID addPedalOffBoard (std::unique_ptr<juce::AudioProcessor> processor,
                         NodeID customNodeId = {});
```

Add a pedal processor WITHOUT placing it on the board. The pedal exists in the engine for routing but `onBoard` is false. Useful for hidden utility pedals.

### removePedal

```cpp
void removePedal (NodeID nodeId);
```

Remove a pedal from the graph. Also removes all connections to/from it.

### updatePedalProcessor

```cpp
void updatePedalProcessor (NodeID nodeId, std::unique_ptr<juce::AudioProcessor> newProcessor);
```

Replace the processor for an existing pedal node, maintaining its connections. Used when rebuilding a pedal's DSP graph after editing.

### autoRoutePedal

```cpp
void autoRoutePedal (NodeID newNodeId);
```

Automatically splice a newly added pedal into the signal chain based on its physical position (left-neighbor routing).

### getPedalInstances

```cpp
const std::list<PedalInstance>& getPedalInstances() const;
```

Get the list of all active pedal instances.

### getPedalInstance

```cpp
PedalInstance* getPedalInstance (NodeID nodeId);
```

Get a mutable reference to a specific pedal instance by NodeID.

---

## Routing / Connections

### connect

```cpp
bool connect (NodeID sourceNode, int sourceChannel,
              NodeID destNode, int destChannel);
```

Connect two pedal nodes (output of source → input of dest). Returns false if connection fails.

### disconnect

```cpp
bool disconnect (NodeID sourceNode, int sourceChannel,
                 NodeID destNode, int destChannel);
```

Disconnect a specific connection.

### disconnectAll

```cpp
void disconnectAll (NodeID nodeId);
```

Remove all connections from/to a node.

### hasConnections

```cpp
bool hasConnections (NodeID nodeId) const;
```

Check if a node has any connections (input or output).

### I/O Node IDs

```cpp
NodeID getAudioInputNodeID() const;
NodeID getAudioOutputNodeID() const;
NodeID getMidiInputNodeID() const;
NodeID getMidiOutputNodeID() const;
```

Get the fixed I/O node IDs for the graph endpoints.

---

## Board-Level Routing

These connections handle MIDI and expression routing between pedals (outside the audio graph).

### addBoardConnection

```cpp
void addBoardConnection (const BoardRoutingConnection& conn);
```

### removeBoardConnection

```cpp
void removeBoardConnection (NodeID srcNodeId, const juce::String& srcPortId,
                            NodeID dstNodeId, const juce::String& dstPortId);
```

### getBoardConnections

```cpp
const std::vector<BoardRoutingConnection>& getBoardConnections() const;
```

### BoardRoutingConnection struct

```cpp
struct BoardRoutingConnection {
    NodeID srcNodeId {};
    juce::String srcPortId;   // e.g. "midi_out", "expr_out_1"
    NodeID dstNodeId {};
    juce::String dstPortId;   // e.g. "midi_in", "expr_in_1"
};
```

---

## Undo / Redo

### saveUndoState

```cpp
void saveUndoState();
```

Push the current graph state onto the undo stack.

### undo / redo

```cpp
bool undo();
bool redo();
bool canUndo() const;
bool canRedo() const;
```

### clearUndoHistory

```cpp
void clearUndoHistory();
```

---

## Board Management

### getBoards

```cpp
std::list<BoardConfig>& getBoards();
const std::list<BoardConfig>& getBoards() const;
```

### getBoard / addBoard / removeBoard

```cpp
BoardConfig* getBoard (const juce::String& boardId);
void addBoard (const BoardConfig& board);
void removeBoard (const juce::String& boardId);
```

---

## Serialization

### serialise

```cpp
juce::String serialise() const;
```

Serialize the entire graph state (pedals, connections, positions, boards) to a JSON string.

### deserialise

```cpp
void deserialise (const juce::String& jsonState);
```

Restore graph state from a JSON string.

---

## Navigation

### cyclePage

```cpp
void cyclePage (int dir);
```

Navigate pedalboard pages. `dir`: -1 = left, +1 = right.

### cycleTrack

```cpp
void cycleTrack (int dir);
```

Navigate between pedals on the current page. Sets the focused pedal for auto-map.

### setFocusedPedal / getFocusedPedal

```cpp
void setFocusedPedal (NodeID id);
NodeID getFocusedPedal() const;
```

---

## Hardware MIDI

### refreshHardwareMidiDevices

```cpp
bool refreshHardwareMidiDevices();
```

Re-enumerate system MIDI devices. Returns true if the list changed.

### injectHardwareMidi

```cpp
void injectHardwareMidi (const juce::String& deviceName, const juce::MidiMessage& msg);
```

### extractHardwareMidi

```cpp
void extractHardwareMidi (const juce::String& deviceName, juce::MidiBuffer& dest);
```

---

## MIDI Monitor

```cpp
void logMidiMessage (const juce::MidiMessage& msg, const juce::String& source);
juce::Array<MidiMonitorEvent> getMidiMonitorEvents() const;
void clearMidiMonitor();
bool hasNewMidiMonitorEvents() const;
void resetMidiMonitorTrigger();
```

---

## Metering

```cpp
std::atomic<float> mainInRMS[2];    // Left/Right input RMS
std::atomic<float> mainOutRMS[2];   // Left/Right output RMS
std::atomic<bool>  mainMidiIn;      // MIDI activity indicator
```
