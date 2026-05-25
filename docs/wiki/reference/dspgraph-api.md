# DSPGraph API Reference

The `DSPGraph` class is the internal audio processing graph used by each pedal's `GraphPedalProcessor`. It manages DSP nodes, connections, topological sorting, and buffer routing.

## Node Management

### addNode

```cpp
int addNode (std::unique_ptr<DSPNode> node);
int addNode (std::unique_ptr<DSPNode> node, int id);
```

Add a node to the graph. Returns the assigned node ID. The second overload forces a specific ID (used during deserialization).

### removeNode

```cpp
void removeNode (int nodeID);
```

Remove a node and all its connections.

### getNode

```cpp
DSPNode* getNode (int nodeID);
```

Get a node by ID. Returns `nullptr` if not found.

### getNodes

```cpp
const std::map<int, std::unique_ptr<DSPNode>>& getNodes() const;
```

Get all nodes as a map of ID → node.

### clear

```cpp
void clear();
```

Clear all nodes and connections.

---

## Connection Management

### connect

```cpp
bool connect (int srcID, int srcPort, int dstID, int dstPort);
```

Connect source node's output port to dest node's input port. Returns false if:
- Nodes don't exist
- Port indices are out of range
- Port types are incompatible
- Connection already exists

### disconnect

```cpp
bool disconnect (int srcID, int srcPort, int dstID, int dstPort);
```

Disconnect a specific connection.

### getConnections

```cpp
const std::vector<NodeConnection>& getConnections() const;
```

Get all connections in the graph.

---

## NodeConnection struct

```cpp
struct NodeConnection {
    int sourceNodeID;
    int sourcePort;
    int destNodeID;
    int destPort;
};
```

---

## Audio Processing

### prepare

```cpp
void prepare (double sampleRate, int maxBlockSize);
```

Prepare all nodes for playback. Allocates internal buffers and performs topological sorting.

### processBlock (stereo)

```cpp
void processBlock (juce::AudioBuffer<float>& buffer, int numSamples,
                   juce::MidiBuffer* midi = nullptr);
```

Process audio through the graph. Nodes are processed in topological order. Special handling for:
- `audio_input` — reads from host buffer channels
- `audio_output` — writes to host buffer channels
- `aux_input` — reads from channels 2-3 (sidechain)
- `aux_output` — writes to channels 2-3 (FX send)

### processBlock (mono)

```cpp
void processBlock (const float* input, float* output, int numSamples,
                   juce::MidiBuffer* midi = nullptr);
```

Convenience mono wrapper for backward compatibility.

### reset

```cpp
void reset();
```

Reset all nodes to their initial state.

---

## Serialization

### toJSON

```cpp
juce::var toJSON() const;
```

Serialize the graph (nodes + connections) to JSON. Each node serializes its type, ID, parameters, and position.

### fromJSON

```cpp
void fromJSON (const juce::var& json);
```

Restore the graph from JSON. Uses `createNodeByType()` factory to instantiate nodes.

---

## Node Factory

```cpp
std::unique_ptr<DSPNode> createNodeByType (const juce::String& type);
```

Factory function that creates DSP nodes by type string. Supports 128+ node types across all categories. Returns `nullptr` for unknown types.

---

## Internal Architecture

### Topological Sort

The graph uses **Kahn's algorithm** for topological sorting. The sort is performed lazily — only when the graph topology changes (`sortDirty` flag).

### Buffer Management

- Each output port gets assigned a buffer from a shared pool
- Buffers are mapped via `portBufferMap` (built during topological sort)
- Pre-allocated scratch arrays avoid audio-thread allocations:
  - `scratchInPtrs` — input pointer array
  - `scratchOutPtrs` — output pointer array
  - `scratchSilence` — zero-filled buffer for unconnected inputs
  - `scratchDevNull` — discard buffer for unconnected outputs

### Port Type Compatibility

Connections enforce type compatibility via `NodePort::areCompatible()`:

| Source Type | Compatible Destinations |
|------------|------------------------|
| Audio | Audio |
| Control | Control, Audio (scaled) |
| Gate | Gate, Control |
| MIDI | MIDI |

### Live Debugging

Each node records its last input/output values (`lastInputValues`, `lastOutputValues`) after processing, enabling real-time value inspection in the FX tab node inspector.
