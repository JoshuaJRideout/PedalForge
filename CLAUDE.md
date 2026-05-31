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
- `source/ui/ScriptingTabComponent.h`: The Script tab. Per-mode compilers (UI/DSP/FX-GraphBuilder/Board/Pedal) build boards/pedals/FX-graphs from code; `getApiReference()` is the canonical script-API doc; `runScriptHeadless()` is the entry the AI agent's run_* tools call.
- `source/ai/`: The in-app AI assistant -- `ClaudeCodeProvider` (shells out to `claude`), `AiAgent` (tool-use loop), `AiTools` (tool defs + dispatch), `ToolHost` (interface the editor implements), `AudioProbe.h` (probe_pedal), `AiAssistantPanel` (UI + the `/tmp` insider test bridge).
- `source/dsp/ControlNodeLibrary.h` / `PeripheralNodeLibrary.h`: control-surface input nodes / display + IO nodes.

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
- **Non-ASCII string literals**: `juce::String(const char*)` decodes as ASCII/Latin-1, so a bare UTF-8 literal (e.g. `"…"`) renders as mojibake. Wrap ANY non-ASCII literal in `juce::CharPointer_UTF8("…")` (prefer `\xNN` byte escapes). A build guard (`cmake/check_utf8_literals.py`) FAILS the build on an unwrapped one; use a trailing `// utf8-ok` to suppress a deliberate case.

## 6. Current state
- **Completed**: Core audio graph; FAUST integration; strict typed connections in the effects builder; full MIDI spec (CC, Pitch Bend, Aftertouch, etc.); the `InventoryOverlay` Q-menu (3-column, drag-and-drop, live search); control-surface nodes (Knob/Fader/Button/Toggle/Selector + Encoder/Pan/Wheel/Trim, each with rich physical-widget settings) that auto-spawn faceplate controls; Companion pedals + display framework; Snapshots (.pfsnapshot); autosave/crash recovery; data-root abstraction (`pf::paths`).
- **In-app AI assistant (#64, working v1)**: a bottom-bar agent that drives the user's LOCALLY-INSTALLED Claude Code on their subscription (NOT the metered API) via a JSON tool protocol. It can operate every tab (build boards/pedals/FX graphs, run scripts, manual audio routing, MIDI mapping, library, wiki), and has perception: `verify_pedal` (graph topology), `probe_pedal` ("ears" -- offline audio analysis), `screenshot` ("eyes" -- inline image via stream-json). Code in `source/ai/`. See also the insider testing bridge in section 10.
- **In progress**: Display subsystem (#44 "easy" text-grid menu display + "advanced" Canvas-2D display) -- see `docs/handoff/display-subsystem.md`. First polished factory pedal (Clean Boost, #32).
- **Stubbed/Pending**:
  - VST3/AUv3 Plugin Host Node (`source/dsp/PluginHostNode.h`) is stubbed (factory registration is lazy; not scanned at launch).
  - NAM Model support (`source/dsp/NAMNode.h`) exists but lacks full UI integration.
  - Factory pedals are being rebuilt as honest, user-reproducible declared-graph + control-node twins (retiring the C++ GraphPedalFactory), one pedal at a time.

## 7. Active plans
The immediate roadmap entails:
1. **Display subsystem** (`docs/handoff/display-subsystem.md`): finish the Easy Display (faceplate menu renderer is done; nav, then the Advanced Canvas-2D display, then a visual editor adapted from the Pedal Designer).
2. **Controls "feel" UI** (#44): the faceplate widget honouring the physical settings (encoder rotation, sensitivity, detents) -- the interaction layer beyond the now-stored properties.
3. **Honest factory pedals**: rebuild each factory pedal as a declared graph + control-node twins (retiring the C++ GraphPedalFactory), one at a time -- starting with Clean Boost (#32).
4. **AI assistant v2**: native MCP tool-use (retire the text JSON protocol), streaming, model picker.
5. **Distribution signing** (#68): Developer ID + hardened runtime + notarization (the dev-loop mic re-prompt is already fixed -- see section 8).

## 8. Known issues, gotchas, and landmines
- **Audio Thread Safety**: `AudioGraphEngine` rebuilds its internal `juce::AudioProcessorGraph` when pedals are added/removed. This requires careful locking to avoid interrupting the audio thread or causing glitches.
- **MIDI Routing vs Audio Routing**: While audio routing is handled by the `juce::AudioProcessorGraph`, board-level MIDI and Expression routing is handled manually via `BoardRoutingConnection` in `AudioGraphEngine::processBlock`. Do not confuse the two graphs.
- **Auto-Mapping**: There is a dynamic parameter auto-mapping system in `PluginProcessor` for MIDI controllers. It depends on `focusedPedalNodeID`. If the UI doesn't correctly update the focused pedal, MIDI learn will target the wrong plugin.
- **Undo/Redo Bloat**: The undo stack works by serializing the *entire* graph state to JSON. While simple, it's memory intensive and can be slow if the graph gets massive. Limit the stack size.
- **FAUST Recompilation**: Modifying a `.dsp` file in `/faust/` triggers a CMake custom command to regenerate the C++ header. If you change a FAUST file, you must run the build system for it to take effect.
- **Build target**: build `--target PedalForge_All` (or no target). `--target PedalForge` only builds the static lib and leaves the .app / VST3 / AU bundles STALE.
- **Two engines**: there are TWO `AudioGraphEngine`s -- the Board's `getGraphEngine()` and the Play tab's `getPlayGraphEngine()` (live performance rig, own pedals + tone presets + `playMidiLearn`). Don't conflate them; agent Play tools target the play engine, everything else the board.
- **App Nap**: disabled at standalone launch (`source/util/AppNap.mm`) so real-time audio and the automation-bridge timer aren't throttled when the window is unfocused. Don't re-enable.

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

# 2. Build the project (use PedalForge_All, NOT PedalForge -- see gotchas)
cmake --build build --config Release --target PedalForge_All

# 3. Run the standalone app (macOS)
open build/PedalForge_artefacts/Release/Standalone/PedalForge.app
```
The VST3 and AU plugins are automatically copied to `/Library/Audio/Plug-Ins/` (or `~/Library/...`) during the build process.

**Dev code-signing**: CMake auto-signs the Standalone post-build with a detected code-signing identity (prefers "Apple Development"), giving a stable TCC requirement so the microphone grant persists across rebuilds (no re-prompt each compile). Override with `-DPEDALFORGE_CODESIGN_IDENTITY="…"` or `""` for ad-hoc.

**Insider AI test bridge (autonomous, no clicking)**: with the standalone running, write a prompt to `/tmp/pedalforge_ai_cmd.txt`; the in-app AI panel's timer runs it through the real agent and writes `/tmp/pedalforge_ai_response.txt` (ends with `[turn complete]`). Tool-call audit log: `~/Library/PedalForge/logs/ai_audit.log`. The agent's `screenshot` tool renders the editor offscreen, so you can drive a feature and visually verify it from a headless test (then `Read` the PNG). App Nap is off so the timer is reliable even backgrounded.

## 11. Open questions for the user
- When implementing the VST3/AU Host Node, should the hosted plugin GUI be a floating detachable window, or embedded inside the node editor?
- For the NAM integration, should we prioritize the dedicated "NAM Node" or the full out-of-the-box factory NAM Pedal first?
- Advanced display (#44): which scripting engine should drive the Canvas-2D `draw()` -- extend `ExpressionVM`, or a small dedicated interpreter?
