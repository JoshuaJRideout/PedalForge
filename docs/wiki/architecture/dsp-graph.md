# DSPGraph

The `DSPGraph` is the node-based audio processing engine that lives inside each pedal. It manages a collection of `DSPNode` objects connected via typed ports, processes them in topological order, and handles buffer routing.

## Overview

```
┌──────────────────────────────────┐
│           DSPGraph               │
│                                  │
│  ┌─────────┐    ┌─────────┐     │
│  │AudioIn  │───▶│ SVF     │──┐  │
│  │(Node 0) │    │(Node 2) │  │  │
│  └─────────┘    └─────────┘  │  │
│                              ▼  │
│  ┌─────────┐    ┌─────────┐ │  │
│  │  LFO    │───▶│ Delay   │◀┘  │
│  │(Node 3) │    │(Node 4) │     │
│  └─────────┘    └─────────┘     │
│                      │          │
│                      ▼          │
│                ┌─────────┐      │
│                │AudioOut │      │
│                │(Node 1) │      │
│                └─────────┘      │
└──────────────────────────────────┘
```

## API Reference

### Node Management

```cpp
// Add a node (auto-assigns ID)
int id = graph.addNode(std::make_unique<GainNode>());

// Add with specific ID
graph.addNode(std::make_unique<GainNode>(), 42);

// Remove node and all its connections
graph.removeNode(nodeId);

// Get node pointer
DSPNode* node = graph.getNode(nodeId);

// Get all nodes
const std::map<int, std::unique_ptr<DSPNode>>& nodes = graph.getNodes();
```

### Connections

```cpp
// Connect source output port → dest input port
bool ok = graph.connect(srcNodeId, srcPort, dstNodeId, dstPort);

// Disconnect
graph.disconnect(srcNodeId, srcPort, dstNodeId, dstPort);

// Get all connections
const std::vector<NodeConnection>& conns = graph.getConnections();
```

#### NodeConnection
```cpp
struct NodeConnection {
    int sourceNodeID;
    int sourcePort;   // output port index on source
    int destNodeID;
    int destPort;     // input port index on destination
};
```

#### Port Type Compatibility
Connections enforce type compatibility via `NodePort::areCompatible()`. Port types:
- **Audio** — audio-rate signal (sample-by-sample)
- **Control** — control-rate value (per-block CV modulation)
- **MIDI** — MIDI message stream
- **Gate** — on/off gate signal

Audio ↔ Control connections are compatible (implicit conversion). MIDI and Gate are only compatible with their own type.

### Processing

```cpp
// Prepare all nodes (call once when audio settings change)
graph.prepare(sampleRate, maxBlockSize);

// Process audio (multi-channel)
graph.processBlock(audioBuffer, numSamples, &midiBuffer);

// Process audio (mono convenience)
graph.processBlock(inputPtr, outputPtr, numSamples, &midiBuffer);

// Reset all nodes
graph.reset();
```

### Processing Order
The graph uses **Kahn's algorithm** for topological sorting. Nodes are processed from sources (no incoming connections) to sinks (no outgoing connections). The sort runs lazily — only when the graph topology changes.

### Buffer Routing
Each output port gets assigned a buffer from a pre-allocated pool. During processing:
1. Input buffers are gathered from connected source output buffers
2. Unconnected inputs receive silence
3. The node processes its inputs → outputs
4. CV modulation is applied at block rate via `applyControlInputs()`

### Serialization

```cpp
// Serialize graph to JSON
juce::var json = graph.toJSON();

// Restore from JSON (uses createNodeByType factory)
graph.fromJSON(jsonVar);
```

The JSON format stores each node's type, ID, parameters, and file paths, plus all connections.

## Node Factory

The `createNodeByType(String)` free function is the authoritative registry of all node types. It maps type strings (e.g., `"svf"`, `"delay"`, `"midi_cc"`) to concrete `DSPNode` subclass instances.

See the [[DSP Node Catalog|dsp-nodes-index]] for the complete list of 128+ registered types.

## Special Node Types

### AudioInputNode / AudioOutputNode
Read from / write to the host audio buffer channels. These are the entry/exit points for audio flowing through the pedal.

### AuxInputNode / AuxOutputNode
Access secondary I/O buses (channels 2-3) for sidechain or FX send/return.

### ExpressionNode
A scriptable node that compiles and runs ExpressionVM bytecode for custom DSP. See [[ExpressionVM|expression-vm]].
