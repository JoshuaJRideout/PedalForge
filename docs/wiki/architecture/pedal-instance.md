# PedalInstance

`PedalInstance` is a pure data struct representing the runtime state of a pedal placed on a pedalboard. It's managed by `AudioGraphEngine` and holds everything the UI needs to render and interact with the pedal.

## Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `nodeID` | NodeID | — | JUCE AudioProcessorGraph node identifier |
| `name` | String | — | Display name |
| `category` | String | — | e.g., "Overdrive", "Delay" |
| `colour` | Colour | — | UI colour for the pedal |
| `numKnobs` | int | 3 | Number of knobs to draw on chassis |
| `bypassed` | bool | false | Bypass state |

## Position & Layout

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `boardX`, `boardY` | float | — | Grid position in pixels |
| `boardW`, `boardH` | float | — | Size in pixels |
| `boardId` | String | "" | Which board (empty = default) |
| `pageIndex` | int | 0 | Page within the board |
| `rotation` | int | 0 | 0, 90, 180, or 270 degrees |
| `onBoard` | bool | true | False = engine-only (not on grid) |
| `routeX`, `routeY` | float | -1 | Position on routing canvas (-1 = auto) |

## Runtime State

| Field | Type | Description |
|-------|------|-------------|
| `controlValues` | `map<String, float>` | Live control values (controlID → 0..1) |
| `controlTexts` | `map<String, String>` | Text values for display controls |
| `controlData` | `map<String, vector<float>>` | Data arrays (sequencer grids, pixel displays) |
| `design` | `shared_ptr<PedalDesign>` | The pedal's design blueprint |
| `meters` | `shared_ptr<PedalMeters>` | Atomic output RMS & MIDI activity |

## PedalMeters

```cpp
struct PedalMeters {
    std::atomic<float> outRMS[2];   // Stereo output level
    std::atomic<bool> midiOut;       // MIDI output activity
};
```

These are updated atomically from the audio thread and read by the UI for level meters.

## Lifecycle

1. **Creation**: `AudioGraphEngine::addPedal()` creates a `PedalInstance` and adds it to the internal list
2. **Routing**: `autoRoutePedal()` splices it into the signal chain
3. **Interaction**: UI reads/writes `controlValues`, which get mapped to DSP parameters via `PedalDesign::mappings`
4. **Serialization**: Saved as part of `AudioGraphEngine::serialise()` → restored via `deserialise()`
5. **Removal**: `removePedal()` removes from graph and list
