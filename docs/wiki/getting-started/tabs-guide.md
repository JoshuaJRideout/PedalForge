# Tabs Guide

PedalForge has 10 tabs across the toolbar, each providing a different view into your effects system.

## Play
**Component**: `PlayTabComponent`

The performance view. Shows your pedals in a simplified, touch-friendly layout optimized for live use. Uses a separate `PlayGraphEngine` so edits in other tabs don't interrupt your live sound.

- Pedals displayed as slots with knobs and bypass toggles
- Tap a slot to open the pedal's canvas overlay
- File loaders (NAM/IR) accessible via the overlay
- MIDI learn works independently from the edit engine

## Board
**Component**: `PedalboardGrid`

The main pedalboard workspace. Drag-and-drop pedals onto a grid to build your signal chain.

- Add pedals via the Q-menu (press Q or click the + button)
- Drag to reposition, resize handles on corners
- Right-click for context menu (bypass, delete, duplicate, rotate)
- Double-click to open the canvas overlay for that pedal
- Auto-routing: new pedals splice into the chain based on position

## Route
**Component**: `RoutingGraphEditor`

A node-graph style routing view showing how pedals are connected at the audio/MIDI level.

- Pedals appear as nodes with typed ports (Audio Stereo, MIDI, Expression)
- Drag wires between ports to create connections
- Pan and zoom the canvas
- Hardware MIDI devices appear as special nodes
- Board routing connections (MIDI/Expression) managed separately from audio

## Pedal
**Component**: `PedalDesignerComponent`

The pedal design workbench. Design the visual layout and control surface for your custom pedals.

- Add controls: knobs, switches, footswitches, LEDs, faders, screens, labels
- Position and resize controls on the chassis
- Set control properties: label, ID, default value, colour, font
- Map controls to DSP parameters via the mapping system
- Add canvas pages for overlay screens (step sequencer, XY pad, etc.)
- Import/export pedal designs as JSON

## FX
**Component**: `NodeGraphEditor`

The effects forge. Build the DSP processing chain inside a pedal using a visual node graph.

- Add nodes from the palette (128+ types)
- Connect ports by dragging wires
- Nodes process in topological order
- Parameter editing via clicking on nodes
- Copy/paste nodes, sticky notes
- Changes auto-sync back to the pedal design

## Script
**Component**: `ScriptingTabComponent`

The code workspace. Three scripting modes for programmatic control:

- **UI Script** — Draw custom UI overlays with ExpressionVM graphics commands. Live preview at 30fps.
- **DSP Expression** — Write math expressions for the ExpressionNode in your pedal's FX graph.
- **FX Graph Builder** — Programmatically create node chains with `addNode()`, `connect()`, `setParam()`.

See the [[Scripting]] section for full documentation.

## Wiki
**Component**: `WikiTabComponent`

You're reading it! The in-app documentation browser.

- Browse all PedalForge documentation
- Searchable, with sidebar navigation
- Covers architecture, DSP nodes, scripting, and API reference

## Library
**Component**: `LibraryComponent`

Asset manager for NAM models, IR files, and other resources.

- Browse and organize NAM amp models
- Browse and organize IR cabinet files
- Preview and load files into pedals
- Drag-and-drop file loading

## Store
*(Coming soon)* — Online marketplace for sharing pedal designs, presets, and assets.

## MIDI
**Component**: `MidiSettingsPanel`

Global MIDI configuration and monitoring.

- MIDI learn assignments (CC → parameter mappings)
- MIDI device configuration
- MIDI monitor (live message log)
- Novation controller modes (AutoMap, Passthrough, Preset Recall)
- Global MIDI CC assignments (page navigation, Turing display, play mode toggle)
