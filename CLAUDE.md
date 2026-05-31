# PedalForge Handoff Document

## 1. Project overview
PedalForge is an open-source, community-driven virtual guitar pedalboard available as a VST3, AU, and Standalone plugin. It allows users to drag effect pedals onto a virtual board, chain them together, and process audio in real-time. Pedals are powered by a custom DSP node graph, integrating FAUST (a functional DSP language) for safe and auditable effects, alongside custom C++ nodes and upcoming Neural Amp Modeler (NAM) support. The project is currently in active pre-launch development.

## 2. Architecture
The project is built in C++20 using the JUCE 8 framework for plugin management, audio routing, and the user interface. 
- **Audio Routing**: The top-level audio engine uses `juce::AudioProcessorGraph` wrapped inside `AudioGraphEngine`. This handles routing between different pedals and hardware I/O.
- **Pedal Internals**: Individual pedals execute a local node-based graph (`DSPGraph` running inside `GraphPedalProcessor`). Users can build their own pedals using an in-app node editor (`NodeGraphEditor`).
- **Dependencies**: JUCE 8 (via FetchContent), NAMCore (via FetchContent), and FAUST (compiled to C++ via a custom CMake script at build time).

### Repository Structure
- `/cmake/` — Custom CMake scripts (e.g., `FaustCompile.cmake`).
- `/faust/` — FAUST `.dsp` files and architectures. Compiled automatically into C++ headers.
- `/source/dsp/` — Inner node graph engine: DSP nodes, Logic, Math, and MIDI nodes.
- `/source/engine/` — Outer audio engine: `AudioGraphEngine` managing `PedalInstance`s.
- `/source/ui/` — Substantial custom GUI code, including editors, overlays, and hardware rendering.
- `/source/pedals/` — Factory pedal definitions and registry.

## 3. Key files and entry points
- `source/PluginProcessor.h/cpp`: The main JUCE plugin entry point. Manages the `AudioGraphEngine` and state persistence.
- `source/engine/AudioGraphEngine.h/cpp`: The core application engine. Manages the outer graph (pedal-to-pedal routing), undo/redo stacks, and MIDI hardware monitors.
- `source/dsp/GraphPedalProcessor.h/cpp`: The internal engine for a single pedal. Executes a `DSPGraph` of connected `DSPNode`s.
- `source/dsp/DSPNodeLibrary.h`: Massive repository of standard DSP nodes available in the graph editor.
- `source/ui/BoardComponent.cpp`: The main "Pedalboard" view where users arrange their pedals.
- `source/ui/PedalDesignerComponent.cpp`: The UI for designing a custom pedal (UI layout and hardware controls).
- `source/ui/NodeGraphEditor.cpp`: The UI for wiring the internal logic/DSP of a pedal.
- `source/ui/InventoryOverlay.cpp`: The "Wiremod-style" Q-menu overlay used for browsing pedals, parts, and nodes.

## 4. Data model
- `BoardConfig`: Defines the layout of pedals on the visual board.
- `PedalInstance`: Represents a live, instantiated pedal on the board. Contains a processor (usually `GraphPedalProcessor`) and its UI state.
- `PedalDesign`: A JSON-serializable definition of a custom pedal. Includes its DSP graph, visual chassis layout, and hardware controls.
- `DSPGraph`: A directed graph of `DSPNode` objects. Nodes process audio/MIDI blocks.
- **Serialization**: The entire application state (and undo/redo history) is serialized to JSON.

## 5. Conventions and house style
- **C++ Standard**: C++20. Use modern C++ features (smart pointers, lambdas, concepts where appropriate).
- **JUCE Conventions**: Follow JUCE class naming (PascalCase for classes, camelCase for methods). Use `juce::String`, `juce::Array`, etc., when interfacing with JUCE APIs, but standard library containers (`std::vector`, `std::map`) are heavily used in the DSP engine.
- **UI Drawing**: Use JUCE's `Graphics` context. Custom hardware UI elements (knobs, switches) are drawn programmatically in `HardwareDrawing.h` rather than using bitmap strips.
- **State Changes**: Any change to the AudioGraphEngine must be captured by calling `engine.saveUndoState()` beforehand.

## 6. Current state
- **Completed**: Core audio graph, FAUST integration, strict typed connections in the effects builder, and complete MIDI specification (CC, Pitch Bend, Aftertouch, etc.).
- **In Progress (Active Focus)**: The `InventoryOverlay` (Wiremod-style Q-menu) is being finalized. It has a 3-column layout, drag-and-drop support, and live search.
- **Stubbed/Pending**: 
  - VST3/AUv3 Plugin Host Node (`source/dsp/PluginHostNode.h`) is stubbed out.
  - NAM Model support (`source/dsp/NAMNode.h`) exists in the project but lacks full UI integration.
  - C/C++ custom nodes are planned to replace the factory FAUST effects.

## 7. Active plans
The immediate roadmap entails:
1. **Finishing the Inventory Overlay**: Hooking up the remaining tabs (Nodes, IRs, Images, Audio) and tagging system.
2. **Displays & Meters**: Adding visual feedback nodes (LEDs, scopes, meters) to the `NodeGraphEditor` and `PedalDesignerComponent`.
3. **Control Surface Nodes**: Creating special DSP nodes (Knob Node, Fader Node, Button Node) that expose internal DSP parameters to the outer Pedal UI faceplate.
4. **C/C++ Nodes**: Migrating away from FAUST factory pedals towards native C++ nodes (Tube Screamer, Delays, Reverbs).

## 8. Known issues, gotchas, and landmines
- **Audio Thread Safety**: `AudioGraphEngine` rebuilds its internal `juce::AudioProcessorGraph` when pedals are added/removed. This requires careful locking to avoid interrupting the audio thread or causing glitches.
- **MIDI Routing vs Audio Routing**: While audio routing is handled by the `juce::AudioProcessorGraph`, board-level MIDI and Expression routing is handled manually via `BoardRoutingConnection` in `AudioGraphEngine::processBlock`. Do not confuse the two graphs.
- **Auto-Mapping**: There is a dynamic parameter auto-mapping system in `PluginProcessor` for MIDI controllers. It depends on `focusedPedalNodeID`. If the UI doesn't correctly update the focused pedal, MIDI learn will target the wrong plugin.
- **Undo/Redo Bloat**: The undo stack works by serializing the *entire* graph state to JSON. While simple, it's memory intensive and can be slow if the graph gets massive. Limit the stack size.
- **FAUST Recompilation**: Modifying a `.dsp` file in `/faust/` triggers a CMake custom command to regenerate the C++ header. If you change a FAUST file, you must run the build system for it to take effect.

## 9. External dependencies and accounts
- **JUCE 8**: Pulled directly from GitHub via `FetchContent`.
- **NAMCore**: (Neural Amp Modeler) pulled via `FetchContent`.
- **FAUST**: Must be installed locally on the system (`brew install faust` on macOS).
- **CMake 3.22+**: Required for building.

## 10. How to run, test, and deploy
Assuming a fresh clone:
```bash
# 1. Configure the project
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 2. Build the project
cmake --build build --config Release

# 3. Run the standalone app (macOS)
open build/PedalForge_artefacts/Release/Standalone/PedalForge.app
```
The VST3 and AU plugins are automatically copied to `/Library/Audio/Plug-Ins/` (or `~/Library/...`) during the build process.

## 11. Open questions for the user
- When implementing the VST3/AU Host Node, should the hosted plugin GUI be a floating detachable window, or embedded inside the node editor?
- For the NAM integration, should we prioritize the dedicated "NAM Node" or focus on the full out-of-the-box factory NAM Pedal first?
- How far along is the `InventoryOverlay` drag-and-drop implementation? Are there specific edge cases remaining to handle?
