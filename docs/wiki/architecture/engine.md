# AudioGraphEngine

`AudioGraphEngine` is the central audio processing engine. It wraps a `juce::AudioProcessorGraph` to manage a chain of effect pedals with full routing, undo/redo, and serialization.

PedalForge runs **two instances**: one for editing (Board/Route/Pedal/FX/Script tabs) and one for live performance (Play tab).

## Lifecycle

```cpp
// Created in PluginProcessor
AudioGraphEngine editEngine;
AudioGraphEngine playGraphEngine;

// Prepared when host starts playback
engine.prepare(sampleRate, samplesPerBlock, numInputChannels, numOutputChannels);

// Called every audio block
engine.processBlock(audioBuffer, midiBuffer);

// Cleanup
engine.releaseResources();
```

## Pedal Management

### Adding Pedals
```cpp
// Add a pedal to a board at a specific grid position
NodeID id = engine.addPedal(
    std::move(processor),  // unique_ptr<AudioProcessor>
    boardId,               // which board ("" = default)
    pageIndex,             // page within board
    boardX, boardY,        // grid position
    boardW, boardH,        // size
    customNodeId           // optional specific ID
);

// Add a pedal without placing it on any board
NodeID id = engine.addPedalOffBoard(std::move(processor));

// Auto-route: splice into chain based on physical position
engine.autoRoutePedal(nodeId);

// Remove a pedal
engine.removePedal(nodeId);

// Hot-swap processor (preserves connections)
engine.updatePedalProcessor(nodeId, std::move(newProcessor));
```

### Accessing Pedals
```cpp
// Get all pedal instances
const std::list<PedalInstance>& pedals = engine.getPedalInstances();

// Lookup by ID
PedalInstance* inst = engine.getPedalInstance(nodeId);

// Fixed I/O nodes
NodeID audioIn  = engine.getAudioInputNodeID();
NodeID audioOut = engine.getAudioOutputNodeID();
NodeID midiIn   = engine.getMidiInputNodeID();
NodeID midiOut  = engine.getMidiOutputNodeID();
```

## Audio Routing

Connections are at the `juce::AudioProcessorGraph` level — channel-based audio/MIDI routing between pedal processors.

```cpp
// Connect source output channel to destination input channel
bool ok = engine.connect(srcNodeId, srcChannel, dstNodeId, dstChannel);

// Disconnect
engine.disconnect(srcNodeId, srcChannel, dstNodeId, dstChannel);

// Remove all connections to/from a node
engine.disconnectAll(nodeId);

// Check if a node has any connections
bool connected = engine.hasConnections(nodeId);
```

## Board Routing Connections

Separate from audio routing, these handle MIDI and Expression pedal routing between pedals on the board.

```cpp
struct BoardRoutingConnection {
    NodeID srcNodeId;
    String srcPortId;
    NodeID dstNodeId;
    String dstPortId;
};

engine.addBoardConnection(connection);
engine.removeBoardConnection(src, srcPort, dst, dstPort);
const vector<BoardRoutingConnection>& conns = engine.getBoardConnections();
```

## Board Management

Pedalboards can have multiple pages and custom grid sizes.

```cpp
std::list<BoardConfig>& boards = engine.getBoards();
BoardConfig* board = engine.getBoard(boardId);
engine.addBoard(boardConfig);
engine.removeBoard(boardId);
```

### BoardConfig Fields
| Field | Type | Description |
|-------|------|-------------|
| `id` | String | Unique identifier |
| `name` | String | Display name |
| `cols`, `rows` | int | Grid dimensions (default 8×4) |
| `numPages` | int | Number of pages |
| `activePage` | int | Current page |
| `snapGridSize` | float | Snap grid (0 = off) |
| `displayIndex` | int | -1 = main window, ≥0 = external monitor |
| `assignToTuring` | bool | Turing display assignment |

## Undo/Redo

```cpp
engine.saveUndoState();    // Snapshot current state
engine.undo();             // Restore previous
engine.redo();             // Restore next
engine.canUndo();          // Check availability
engine.canRedo();
engine.clearUndoHistory();
```

## Serialization

The entire engine state (pedals, connections, boards, MIDI config) can be serialized to/from JSON.

```cpp
String json = engine.serialise();
engine.deserialise(jsonString);
```

## Navigation & Focus

```cpp
engine.setFocusedPedal(nodeId);   // Set auto-map target
engine.cyclePage(+1);             // Navigate pages
engine.cycleTrack(-1);            // Navigate pedals
engine.cycleTuringPedal(+1);     // Turing display cycling
```

## Hardware MIDI Devices

```cpp
struct HardwareMidiDevice {
    NodeID engineNodeId;
    String deviceName;
    bool isInput;
    float routeX, routeY;
    shared_ptr<atomic<bool>> activity;
};

engine.refreshHardwareMidiDevices();
engine.injectHardwareMidi(deviceName, midiMessage);
engine.extractHardwareMidi(deviceName, destBuffer);
```

## MIDI Monitor

```cpp
engine.logMidiMessage(msg, sourceName);
auto events = engine.getMidiMonitorEvents();  // MidiMonitorEvent array
engine.clearMidiMonitor();
```

## Level Meters

```cpp
// Atomic RMS values updated each audio block
float inL  = engine.mainInRMS[0].load();
float inR  = engine.mainInRMS[1].load();
float outL = engine.mainOutRMS[0].load();
float outR = engine.mainOutRMS[1].load();
bool midi  = engine.mainMidiIn.load();
```
