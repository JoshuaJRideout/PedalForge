# PedalDesign

`PedalDesign` is the blueprint schema for a custom pedal. It bundles everything needed to define a pedal: visual layout, DSP graph, parameter mappings, and metadata.

## Schema Overview

```
PedalDesign
├── Metadata (name, author, category, tags, version)
├── Chassis (size, colour, background image)
├── Controls[] (knobs, switches, screens, labels, etc.)
├── Canvas Pages[] (overlay screens with their own controls)
├── Routing Ports[] (MIDI/Expression I/O declarations)
├── Mappings[] (control → DSP parameter bindings)
├── Effects Graph (serialized DSPGraph JSON)
└── Notes (design notes, FX notes)
```

## Metadata

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `name` | String | "My Pedal" | Display name |
| `author` | String | "User" | Creator |
| `description` | String | "" | Free text |
| `category` | String | "Custom" | e.g., "Overdrive", "Delay" |
| `tags` | StringArray | [] | Searchable tags |
| `version` | int | 1 | Schema version |
| `isFactory` | bool | false | Read-only if true |

## Chassis

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `chassisW` | float | 200 | Width in pixels |
| `chassisH` | float | 300 | Height in pixels |
| `chassisColour` | Colour | dark gray | Background colour |
| `chassisImage` | String | "" | Custom background image path |

## Controls

Each control in the `controls` vector is a `Control` struct:

| Field | Type | Description |
|-------|------|-------------|
| `type` | String | Control type (see list below) |
| `x`, `y` | float | Position on chassis |
| `width`, `height` | float | Size |
| `label` | String | Display label |
| `controlID` | String | Unique ID within design |
| `defaultValue` | float | Default value (0.5) |
| `rotationRange` | float | Arc range in degrees (default 270°) |
| `sensitivity` | float | Drag sensitivity in pixels (default 200) |
| `imageMain` | String | Custom knob/switch image |
| `imageTrack` | String | Custom track image |
| `customColour` | Colour | Custom colour |
| `fontFamily` | String | Font family for text controls |
| `fontSize` | float | Font size |
| `numLines` | int | Lines for multi-line displays |
| `isLocked` | bool | Ignores canvas interaction |
| `libraryCategory` | String | For `library_loader` type |
| `overlayPage` | String | For `overlay_launcher` type |

### Control Types

| Type | Description |
|------|-------------|
| `knob` | Rotary potentiometer |
| `switch` | Toggle/multi-position switch |
| `footswitch` | Stomp switch |
| `led` | Status LED |
| `fader` | Linear slider |
| `text_screen` | Text display |
| `console` | Scrollable text console |
| `label` | Static text label |
| `file_loader` | File picker (NAM/IR) |
| `library_loader` | Library browser launcher |
| `overlay_launcher` | Opens a canvas page overlay |

## Canvas Pages

`canvasPages` is a `vector<CanvasPage>` for overlay screens (e.g., step sequencer, XY pad).

```cpp
struct CanvasPage {
    String pageName;
    float width, height;
    Colour backgroundColour;
    std::vector<Control> controls;  // Same Control struct
};
```

## Routing Ports

`routingPorts` declares MIDI and Expression I/O that appears on the Route tab.

```cpp
struct RoutingPort {
    enum Kind { MidiIn, MidiOut, ExpressionIn, ExpressionOut };
    Kind kind;
    String id;
    String label;
};
```

## Mappings

`mappings` binds chassis controls to DSP graph parameters.

```cpp
struct Mapping {
    String controlID;   // References a Control's controlID
    String nodeParam;   // Format: "nodeID.paramID" in the DSP graph
};
```

When the user turns a knob on the chassis, the mapping translates the 0–1 control value to the target DSP parameter range.

## Effects Graph

`effectsGraph` stores the serialized DSPGraph as a `juce::var` (JSON). It's the output of `DSPGraph::toJSON()`.

## Serialization

```cpp
// To JSON
juce::var json = design.toJSON();

// From JSON
PedalDesign design = PedalDesign::fromJSON(jsonVar);

// File I/O
PedalDesign design = PedalDesign::loadFromFile(file);
design.saveToFile(file);
```
