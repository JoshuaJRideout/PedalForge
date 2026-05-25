# MIDI System

PedalForge's MIDI system spans three layers: hardware device routing, in-graph MIDI processing, and MIDI learn parameter mapping.

## Architecture

```
Physical MIDI Devices
    │
    ▼
┌──────────────────────────┐
│  HardwareMidiInputNode   │ ← injectHardwareMidi()
│  HardwareMidiOutputNode  │ → extractHardwareMidi()
└──────────┬───────────────┘
           │
           ▼
┌──────────────────────────┐
│   AudioGraphEngine       │
│   (AudioProcessorGraph)  │ ← MIDI flows through graph connections
│                          │
│   ┌────────────────────┐ │
│   │ BoardMidiReceiver  │ │ ← Evaluates page CCs
│   └────────────────────┘ │
│                          │
│   ┌────────────────────┐ │
│   │ GraphPedalProcessor│ │
│   │ ┌────────────────┐ │ │
│   │ │ DSPGraph       │ │ │
│   │ │ ┌────────────┐ │ │ │
│   │ │ │ MidiCCNode │ │ │ │ ← In-graph MIDI processing
│   │ │ │ MidiNoteGen│ │ │ │
│   │ │ └────────────┘ │ │ │
│   │ └────────────────┘ │ │
│   └────────────────────┘ │
└──────────────────────────┘
           │
           ▼
┌──────────────────────────┐
│   MidiLearnManager       │ ← CC → parameter mapping
└──────────────────────────┘
```

## Hardware MIDI Devices

The engine manages physical MIDI devices as special nodes in the `AudioProcessorGraph`:

```cpp
struct HardwareMidiDevice {
    NodeID engineNodeId;       // Graph node
    String deviceName;         // System device name
    bool isInput;              // Input or output
    float routeX, routeY;     // Position on routing canvas
    shared_ptr<atomic<bool>> activity;  // Activity indicator
};

// Refresh device list (call on startup, device change)
engine.refreshHardwareMidiDevices();

// Inject MIDI into an input device
engine.injectHardwareMidi("USB MIDI Controller", midiMessage);

// Extract MIDI from an output device
engine.extractHardwareMidi("DIN MIDI Out", destBuffer);
```

## MIDI Learn Manager

Located at `source/midi/MidiLearn.h`, the `MidiLearnManager` maps MIDI CC messages to pedal parameters.

### Usage

```cpp
MidiLearnManager midiLearn(engine);

// Start learning — the next CC received will be mapped
midiLearn.startLearning("pedal1.drive");

// Cancel learning
midiLearn.cancelLearning();

// Check state
midiLearn.isLearning();        // true/false
midiLearn.getLearningParamId(); // "pedal1.drive"

// Process incoming MIDI (called from audio thread)
midiLearn.processMidi(midiBuffer);

// Query mappings
int cc = midiLearn.getMappedCC("pedal1.drive");  // -1 if unmapped
const auto& mappings = midiLearn.getMappings();

// Remove mappings
midiLearn.removeMapping("pedal1.drive");
midiLearn.clearAllMappings();
```

### MidiMapping

```cpp
struct MidiMapping {
    int ccNumber;          // MIDI CC number (0-127)
    int channel;           // MIDI channel (0 = omni)
    String paramId;        // Target parameter
    bool isLatched;        // Pickup/catch-up mode
    float lastPhysicalValue;
    float lastVirtualValue;
};
```

## In-Graph MIDI Nodes

Inside a pedal's DSPGraph, MIDI is processed by specialized nodes:

### MIDI Input Nodes (Receivers)
| Type | Description |
|------|-------------|
| `midi_note` | Extracts note on/off → frequency, gate, velocity outputs |
| `midi_cc` | Extracts CC value (configurable CC number) |
| `midi_pitchbend` | Extracts pitch bend → -1..+1 |
| `midi_clock` | Extracts MIDI clock → tempo, beat outputs |
| `midi_program` | Extracts program change |
| `midi_pressure` | Channel pressure |
| `midi_poly_pressure` | Polyphonic key pressure |
| `midi_cc14` | 14-bit CC (MSB+LSB pair) |
| `midi_song_pos` | Song position pointer |
| `midi_transport` | Start/stop/continue |

### MIDI Output Nodes (Generators)
| Type | Description |
|------|-------------|
| `midi_note_gen` | Generates note on/off from trigger inputs |
| `midi_cc_gen` | Generates CC messages from value input |
| `midi_pitchbend_gen` | Generates pitch bend |
| `midi_program_gen` | Generates program change |
| `midi_pressure_gen` | Generates channel pressure |
| `midi_poly_pressure_gen` | Generates poly pressure |
| `midi_transport_gen` | Generates transport messages |

## Global MIDI Config

`AppMidiConfig` defines global MIDI CC assignments:

| Field | Type | Description |
|-------|------|-------------|
| `turingPrevCC` | int | CC for Turing display previous |
| `turingNextCC` | int | CC for Turing display next |
| `playModeToggleCC` | int | CC to toggle play mode |
| `pageLeftCC` | int | CC for page left |
| `pageRightCC` | int | CC for page right |
| `trackLeftCC` | int | CC for track left |
| `trackRightCC` | int | CC for track right |
| `NovationMode` | enum | AutoMap, Passthrough, or PresetRecall |

## MIDI Monitor

The engine logs all MIDI events for debugging:

```cpp
engine.logMidiMessage(msg, "USB Controller");
auto events = engine.getMidiMonitorEvents();
engine.clearMidiMonitor();
```
