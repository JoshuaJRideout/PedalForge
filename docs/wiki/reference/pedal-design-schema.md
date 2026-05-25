# PedalDesign JSON Schema

The `PedalDesign` struct is the complete, unified definition of a pedal. It bundles the visual layout, DSP graph, and control-to-parameter mappings into a single JSON-serializable format.

## Top-Level Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `name` | string | `"My Pedal"` | Pedal display name |
| `author` | string | `"User"` | Creator name |
| `description` | string | `""` | Short description |
| `category` | string | `"Custom"` | Category for organization |
| `tags` | string[] | `[]` | Free-form tags (e.g. `["drive", "tutorial"]`) |
| `version` | int | `1` | Design version number |
| `isFactory` | bool | `false` | Read-only factory pedal flag |
| `chassisW` | float | `200.0` | Chassis width in pixels |
| `chassisH` | float | `340.0` | Chassis height in pixels |
| `chassisColour` | int (ARGB) | `0xFF8A8A94` | Chassis background colour |
| `chassisImage` | string | `""` | Custom background image path |
| `controls` | Control[] | `[]` | Chassis controls |
| `effectsGraph` | object | `{}` | DSPGraph JSON (see [[dspgraph-api|reference/dspgraph-api]]) |
| `mappings` | Mapping[] | `[]` | Control → DSP parameter mappings |
| `routingPorts` | RoutingPort[] | `[]` | Cross-pedal routing ports |
| `canvasPages` | CanvasPage[] | `[]` | Overlay canvas pages |
| `designNotes` | StickyNote[] | `[]` | Notes on the Pedal Builder tab |
| `fxNotes` | StickyNote[] | `[]` | Notes on the FX tab |

---

## Control

Visual/interactive elements placed on the chassis or canvas pages.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `type` | string | — | Control type (see below) |
| `x` | float | `0` | X position |
| `y` | float | `0` | Y position |
| `width` | float | `40` | Width |
| `height` | float | `40` | Height |
| `label` | string | `""` | Display label |
| `controlID` | string | — | Unique ID within this design |
| `defaultValue` | float | `0.5` | Initial value (0-1) |
| `libraryCategory` | string | `""` | For `file_loader`/`library_loader` |
| `overlayPage` | string | `""` | For `overlay_launcher` — which canvas page to open |
| `rotationRange` | float | `270.0` | Knob arc in degrees |
| `sensitivity` | float | `200.0` | Pixels of drag for full sweep |
| `imageMain` | string | `""` | Custom image path |
| `imageTrack` | string | `""` | Custom track/background image |
| `customColour` | int (ARGB) | `0xFFFF0000` | Control accent colour |
| `stretchImage` | bool | `true` | Stretch image to fill bounds |
| `fontFamily` | string | `"Sans"` | Font family (`"Sans"`, `"Serif"`, `"Monospace"`) |
| `fontStyle` | int | `1` | 0=Plain, 1=Bold, 2=Italic, 3=BoldItalic |
| `fontSize` | float | `0` | Font size (0 = auto) |
| `numLines` | int | `1` | For multi-line displays |
| `isLocked` | bool | `false` | Ignore canvas interaction |

### Control Types

| Type | Description |
|------|-------------|
| `knob` | Rotary potentiometer |
| `fader` | Linear slider |
| `switch` | Toggle on/off |
| `button` | Momentary pushbutton |
| `footswitch` | Foot stomp switch |
| `selector` | Multi-position selector |
| `xy_pad` | 2D touch pad |
| `led` | Status LED |
| `rgb_led` | RGB LED |
| `label` | Text label (static) |
| `text_screen` | Multi-line text display |
| `console` | Scrollable console output |
| `vu_meter` | Level meter |
| `tuner` | Chromatic tuner display |
| `7seg` | 7-segment numeric display |
| `scope` | Oscilloscope waveform |
| `pixel_display` | Programmable pixel grid |
| `shader` | Expression-driven display |
| `file_loader` | File browser button |
| `library_loader` | Library asset picker |
| `overlay_launcher` | Opens a canvas overlay page |
| `image` | Static image |

---

## Mapping

Links a UI control to a DSP node parameter.

| Field | Type | Description |
|-------|------|-------------|
| `controlID` | string | References a `Control.controlID` |
| `nodeParam` | string | `"nodeID_paramID"` format (e.g. `"3_cutoff"`) |

### How Mappings Work

When a user adjusts a knob (0-1 range), the mapping system:
1. Finds the target DSP node by parsing the `nodeID` from `nodeParam`
2. Maps the 0-1 control value to the parameter's actual range
3. Updates the DSP node's parameter in real-time

### Display Mappings

For `text_screen` and `console` controls, mappings can target specific display lines using the `"controlID:lineIndex"` convention:

```json
{
  "controlID": "display:1",
  "nodeParam": "4_filepath"
}
```

This updates line 1 of the display with the loaded file's name.

---

## RoutingPort

Cross-pedal routing ports visible in the Routing Tab.

| Field | Type | Description |
|-------|------|-------------|
| `kind` | int | 0=MidiIn, 1=MidiOut, 2=ExpressionIn, 3=ExpressionOut |
| `id` | string | Unique port ID (e.g. `"midi_in"`, `"expr_out_1"`) |
| `label` | string | Display label in routing tab |

---

## CanvasPage

Full-screen or pop-up overlay UI panels.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `pageName` | string | — | Display name |
| `width` | float | `800` | Canvas width |
| `height` | float | `600` | Canvas height |
| `backgroundColour` | int (ARGB) | `0xFF222222` | Background colour |
| `controls` | Control[] | `[]` | Controls on this canvas page |

---

## Example: Minimal Pedal Design JSON

```json
{
  "name": "Simple Drive",
  "author": "User",
  "category": "Drive",
  "chassisW": 200,
  "chassisH": 340,
  "chassisColour": 4287137940,
  "controls": [
    {
      "type": "knob",
      "x": 80, "y": 80,
      "width": 50, "height": 50,
      "label": "Drive",
      "controlID": "drive_knob",
      "defaultValue": 0.3
    },
    {
      "type": "knob",
      "x": 80, "y": 180,
      "width": 50, "height": 50,
      "label": "Volume",
      "controlID": "volume_knob",
      "defaultValue": 0.5
    },
    {
      "type": "footswitch",
      "x": 80, "y": 280,
      "width": 40, "height": 40,
      "label": "",
      "controlID": "bypass_fs"
    }
  ],
  "effectsGraph": {
    "nodes": [
      { "id": 0, "type": "audio_input" },
      { "id": 1, "type": "softclip", "params": { "drive": 5.0 } },
      { "id": 2, "type": "gain", "params": { "gain": 0.0 } },
      { "id": 3, "type": "audio_output" }
    ],
    "connections": [
      { "srcNode": 0, "srcPort": 0, "dstNode": 1, "dstPort": 0 },
      { "srcNode": 1, "srcPort": 0, "dstNode": 2, "dstPort": 0 },
      { "srcNode": 2, "srcPort": 0, "dstNode": 3, "dstPort": 0 }
    ]
  },
  "mappings": [
    { "controlID": "drive_knob", "nodeParam": "1_drive" },
    { "controlID": "volume_knob", "nodeParam": "2_gain" }
  ]
}
```
