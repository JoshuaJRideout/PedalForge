# PedalForge — TODO / Roadmap

> [!NOTE]
> Ideas captured 2026-05-13. Items are roughly prioritized by dependency order, not importance.

---

## 1. 🔌 Typed Connections & Colored Wires (Effects Builder) ✅ DONE

- [x] **Color-coded wires** by connection type (blue/green/yellow/red)
- [x] **Enforce port compatibility** — prevents incompatible connections
- [x] Visual feedback: incompatible ports dim during wire drag
- [ ] Allow "adapter" nodes for intentional type conversion (e.g. MIDI-to-CV)

---

## 2. 🎹 Complete MIDI to Full Spec ✅ DONE

- [x] Program Change (receiver + generator)
- [x] Channel Pressure / Aftertouch (receiver + generator)
- [x] Polyphonic Key Pressure (receiver + generator)
- [x] 14-bit CC support (MSB/LSB pairs)
- [x] Song Position, Transport (Start/Stop/Continue)
- [x] Pitch Bend generator, Note generator, CC generator
- [ ] System Exclusive (SysEx) messages
- [ ] MIDI 2.0 consideration (future)

---

## 3. 📊 Displays, Meters & Gauges

Add visual feedback components to both Pedal Builder and Effects Builder:

### Pedal Builder (visual components)
- [ ] LED display (numeric / text)
- [ ] VU meter / level meter
- [ ] Tuner display
- [ ] Custom gauge (needle-style)
- [ ] Waveform display (mini scope)

### Effects Builder (node graph)
- [ ] Meter node (shows live signal level)
- [ ] Display node (shows numeric value of control signal)
- [ ] Scope node (waveform viewer)
- [ ] Spectrum analyzer node
- [ ] Probe node (tooltip-on-hover for any wire)

---

## 4. 🧩 C/C++ Node Support

Allow users to write custom DSP nodes in C/C++:

- [ ] Define a clean C API for custom nodes (`process()`, `prepare()`, `reset()`, param registration)
- [ ] Hot-reload or JIT compilation workflow
- [ ] Template/scaffold generator for new nodes
- [ ] Documentation & examples
- [ ] Sandboxing / safety considerations

---

## 5. 🎸 Factory C/C++ Effects Library (Replace Current Pedals)

Remove the current FAUST-based factory pedals and replace with native C/C++ effect nodes that serve as both starting points for users and building blocks:

### Distortion / Drive
- [ ] Tube Screamer-style overdrive
- [ ] RAT-style distortion
- [ ] Big Muff-style fuzz
- [ ] Clean boost
- [ ] Amp-in-a-box (preamp sim)

### Delay
- [ ] Digital delay
- [ ] Tape delay (with wow/flutter)
- [ ] Analog delay (BBD emulation)
- [ ] Ping-pong delay
- [ ] Multi-tap delay

### Reverb
- [ ] Spring reverb
- [ ] Plate reverb
- [ ] Hall reverb
- [ ] Room reverb
- [ ] Shimmer reverb

### Modulation
- [ ] Chorus
- [ ] Flanger
- [ ] Phaser
- [ ] Tremolo
- [ ] Vibrato
- [ ] Rotary speaker (Leslie)
- [ ] Uni-Vibe

### Utility
- [ ] Tuner
- [ ] Noise gate
- [ ] Compressor
- [ ] EQ (parametric, graphic)
- [ ] Volume pedal
- [ ] Looper

---

## 6. 💰 Store / Charity System ("Puppies vs Kids")

A pay-what-you-want store with a charitable twist:

- [ ] **Store infrastructure** — browse, preview, download pedals/packs
- [ ] **PWYW pricing** — minimum $0, suggested price shown
- [ ] **Revenue split:**
  - 10% → Developer (server/hosting costs)
  - 90% → Charity split (user-controlled slider)
- [ ] **Two competing charities** — "Puppies vs Kids"
  - Boxing promotion-style poster art for each
  - Real-time leaderboard on the store front page
  - Annual "winner" announcement
- [ ] **Tracking dashboard** — total raised, per-charity breakdown, lifetime stats
- [ ] **Payment integration** — LemonSqueezy or Stripe
- [ ] **Content pipeline** — how creators submit pedals/packs for the store

---

## 7. ⚡ Analog Circuit Simulation (Effects Builder)

Schematic-style analog component nodes that process audio, letting users build pseudo-analog circuits and learn electronics:

### Passive Components
- [ ] Resistor
- [ ] Capacitor
- [ ] Inductor
- [ ] Potentiometer

### Semiconductors
- [ ] Diode (Si, Ge, LED, Zener)
- [ ] NPN / PNP Transistor (BJT)
- [ ] JFET / MOSFET
- [ ] Op-Amp (ideal + real models like TL072, LM386, etc.)

### IC / Complex
- [ ] 555 Timer
- [ ] BBD chip (bucket brigade delay)
- [ ] Voltage regulator

### Infrastructure
- [ ] Ground / VCC / V+ / V- power rails
- [ ] Wire junctions / bus
- [ ] Schematic-style rendering (component symbols, not boxes)
- [ ] SPICE-inspired simulation engine (nodal analysis)
- [ ] Dedicated "Analog" color theme in the graph

---

## 8. 🔬 Measurement Tools / Scopes (Effects Builder)

Professional debug and analysis tools for the node graph:

- [ ] **Oscilloscope node** — time-domain waveform viewer with triggering
- [ ] **Spectrum analyzer node** — FFT-based frequency display
- [ ] **Signal generator node** — test tones (sine, square, saw, noise)
- [ ] **Multimeter node** — RMS level, peak, DC offset, frequency
- [ ] **Phase correlation meter**
- [ ] **Latency measurement tool**

---

## 9. 🎛️ Dynamic I/O, Variables & Control Surface Nodes

### Expression / Code Node Enhancements
Make expression nodes (and future C/C++ / FAUST nodes) fully dynamic like Wiremod:

- [ ] **Declare custom inputs/outputs** with names and types (Audio, Control, MIDI, Gate)
- [ ] **Variables** — persistent named values that survive between process calls
- [ ] **Constants** — named immutable values declared at the top of the expression
- [ ] Inputs/outputs dynamically add/remove ports on the node visual
- [ ] Apply same I/O declaration system to future C/C++ and FAUST custom nodes

### Control Surface Nodes (Parameter Export)
Special nodes that bridge the effects graph to the pedal UI. Only things wired to these are exposed on the pedal:

- [ ] **Knob Node** — continuous rotary control (0-1), maps to a knob on the pedal face
- [ ] **Fader Node** — linear slider control (0-1)
- [ ] **Button Node** — momentary gate (high while pressed)
- [ ] **Toggle Node** — latching on/off switch
- [ ] **Selector Node** — multi-position switch (dropdown / rotary selector)
- [ ] Each control node has editable: name, min, max, default, curve (linear/log/exp)
- [ ] Control nodes appear in the Pedal Designer as mappable hardware components
- [ ] Only control nodes are exposed as user-facing parameters — internal graph wiring stays hidden

---

## 🗓️ Priority Notes

| Phase | Items | Notes |
|-------|-------|-------|
| **Done** | 1, 2 | Typed wires, MIDI spec complete |
| **Now** | 3, 9 | Displays/meters + control surface nodes |
| **Soon** | 4, 5 | C/C++ nodes + factory effects library |
| **Next** | 7, 8 | Analog circuits + measurement tools |
| **Ongoing** | 6 | Store can launch whenever there's content to sell |

---

## 10. 📱 Multi-Device Pedalboard & Routing Architecture

Separate the concept of "Routing" from "Layout" to allow for highly flexible, multi-monitor and multi-device performance setups:

- [ ] **Tab Separation:** Split "Pedalboard" and "Routing" into entirely separate tabs. Routing dictates signal flow, Pedalboard dictates visual layout.
- [ ] **Multi-Board Support:** Allow creating multiple different visual pedalboards simultaneously that share the same underlying effects engine and routing graph.
- [ ] **Adaptive Layouts:** Design layouts specific to different screen aspect ratios/sizes:
  - **Ultrawide Monitor (480x1920):** 4 pedals at a time in a single row, switchable via MIDI footswitches or swiping.
  - **Mini Display (3.5 inch):** Single pedal view, switchable via swiping or touch gestures.
  - **iPad / Standard Monitor:** Multi-row grid layout.
- [ ] **Companion Apps:** Develop iOS and Android companion apps that connect to the desktop instance to display and interact with the pedals remotely.

---

## 11. 📁 Wiremod-Style Library & Asset Manager ✅ IN PROGRESS

Q-menu style overlay (Tab key to toggle) with 3-column layout: categories + search (left), item grid (center), preview/properties (right).

- [x] **Q-Menu Overlay:** Full-screen dimmed overlay toggled with Tab key, dismissed with Escape or clicking backdrop.
- [x] **3-Column Layout:** Category tree + search bar | scrollable item grid with card cells | item preview panel.
- [x] **Category Tree:** Auto-built from item database with main categories (Pedals, Parts) and sub-categories (Drive, Modulation, Controls, Lights, etc.)
- [x] **Search Bar:** Live filtering across item names, categories, and descriptions.
- [x] **Factory Protection:** Factory pedals and parts cannot be deleted. Only custom user items show the delete option.
- [x] **Drag & Drop:** Items can be dragged from the grid directly onto the workspace (pedalboard or designer canvas).
- [ ] **Nodes Tab:** Browse and drag DSP nodes for the effects builder.
- [ ] **IRs Tab:** Browse and manage impulse response files.
- [ ] **Images Tab:** Browse chassis graphics, knob textures, and image packs.
- [ ] **Audio/Samples Tab:** Test sounds, looper buffers.
- [ ] **Tagging System:** User-defined tags for organizing custom content.

---

## 12. 🔧 Custom Parts / Hardware Components (Advanced)

Allow advanced users to create entirely new hardware control types for the Pedal Builder using C/C++:

- [ ] **Custom Part API:** Define a C/C++ API for user-created controls (`draw()`, `processInput()`, `getControlValue()`, param registration).
- [ ] **Examples of exotic controls:**
  - Mod wheel (synth-style)
  - Webcam theremin (video input → pitch control)
  - Accelerometer/gyro input (phone tilt)
  - Touch strips / XY pads
  - Ribbon controllers
  - Breath controller input
- [ ] **Part Editor:** Visual tool for defining the control's appearance and hit zones.
- [ ] **Hot-reload workflow:** Edit C/C++ code, recompile, and see changes live.
- [ ] **Template/scaffold generator:** CLI or in-app wizard to bootstrap a new custom part project.
- [ ] **Community sharing:** Custom parts can be packaged and shared via the Store.

---

## 13. 🔌 VST / AUv3 Plugin Host Node (Effects Builder)

Allow users to load third-party VST3 and AUv3 plugins as nodes inside the effects builder graph, turning PedalForge into a modular plugin host:

- [ ] **Plugin Host Node:** A new node type in the effects builder that wraps an external plugin.
  - Audio I/O ports matching the plugin's channel configuration.
  - MIDI I/O ports if the plugin supports MIDI input/output.
  - Parameter automation ports for key plugin parameters.
- [ ] **Plugin Scanner:** Scan system plugin directories for available VST3/AU plugins.
  - Show plugins in the Inventory Overlay under a new "Plugins" category.
  - Cache scan results for fast startup.
- [ ] **Plugin Window Display:**
  - Option A: Dedicated "Plugin" tab that shows the active plugin's GUI.
  - Option B: Floating detachable window (like a DAW's plugin editor).
  - Option C: Embedded inline within a resizable node in the graph (for simple UIs).
- [ ] **DAW-style Controls:** Bypass, preset management, parameter list for automation.
- [ ] **State Persistence:** Plugin state (chunk data) saved/restored with presets and projects.
- [ ] **Sandboxing Considerations:** Crash-proof plugin hosting (out-of-process if feasible).
- [ ] **Latency Compensation:** Report and compensate for plugin-induced latency in the graph.

---

## 14. 🧠 Neural Amp Modeler (NAM) Support

Integrate the Neural Amp Modeler inference engine alongside IR support, giving users access to the massive library of community-trained amp/drive/tone models:

- [ ] **NAM Node (Effects Builder):** A DSP node that loads and runs `.nam` model files in real-time.
  - Audio input/output ports.
  - File selector for `.nam` model files.
  - Gain/level controls on the node itself.
- [ ] **Default NAM Pedal:** A factory pedal pre-wired with a NAM node, file browser, and gain/tone controls — ready to use out of the box (similar to how the Cabinet Sim pedal wraps the IR convolution node).
- [ ] **NAM Model Browser:** Integrate `.nam` files into the Inventory Overlay and Library tab under an "Amp Models" or "NAM" category.
- [ ] **Model Management:** Import, organize, tag, and preview NAM models in the Library.
- [ ] **Community Packs:** NAM models can be bundled into community packs alongside IRs, pedals, and images.
- [ ] **Performance:** Leverage SIMD/vectorized inference for low-latency real-time processing.
- [ ] **Compatibility:** Support both NAM and AIDA-X `.aidax` model formats (if feasible).

---

## 15. 🎸 Play Tab — Simplified "Just Play" Mode

A beginner-friendly mode for users who just want to plug in and play — no routing graphs, no node editors, no complicated setup. Inspired by Fender Tone Master, Positive Grid Spark, Line 6 HX Stomp, and other modeling amps.

- [ ] **Linear Signal Chain:** A horizontal strip of pedal slots (4–8). Drag pedals in, reorder by dragging, signal flows left → right automatically.
- [ ] **Full-Size Pedal View:** Each pedal in the chain shows its full skeuomorphic face — knobs are interactive, stomp switches work, LEDs light up.
- [ ] **No Routing Required:** Wiring is automatic — output of pedal N feeds input of pedal N+1. No Board/Route/FX tabs needed.
- [ ] **Preset System:** Save/load complete chain snapshots (which pedals, in what order, with what settings).
- [ ] **Quick Swap:** Click an empty slot to open a simplified pedal picker (subset of the Q-menu, no categories needed — just a grid of pedals).
- [ ] **First-Run Default:** New users see this tab first with a sensible default chain (e.g., Tuner → Drive → Amp → Cab → Delay → Reverb).
- [ ] **Visual Style:** Clean, dark, stage-ready UI. Large pedal graphics. Minimal chrome. Designed to look good on a tablet or touchscreen during a gig.
- [ ] **MIDI Foot Control:** Full MIDI mapping for bypass/engage per slot, preset switching, and expression pedal assignment.
- [ ] **Tap Tempo:** Global tap tempo button that feeds into any tempo-synced pedals in the chain (delays, tremolos, etc.).
- [ ] **Tuner Mode:** Built-in chromatic tuner accessible from the Play tab header — tap to mute output and tune.

> [!TIP]
> The Play tab can reuse the same `AudioGraphEngine` internally — it just auto-wires a linear chain instead of exposing the full graph. Power users can "Explode to Board" to take their Play chain into the advanced tabs.


---

## 16. 🎨 App Skins (Winamp-Style)

Add a "Skins" section in the library to allow deep customization of the app interface:
- [ ] **Skin JSON format**: Define a specification for overriding `PedalForgeLookAndFeel` colors, fonts, and background images.
- [ ] **Skin Browser**: UI in the Library tab to browse, preview, and apply skins dynamically without restarting.
- [ ] **Community Sharing**: Allow creators to upload and share custom UI skins on the community store.

---

## 17. 📦 Standalone Plugin Export

Create a method for users to export individual pedals as standalone plugins:
- [ ] **Export Wizard**: A UI flow to select a PedalInstance for export.
- [ ] **CMake Template**: A standalone JUCE AudioPlugin template that wraps the PedalInstance and its Faust DSP graph.
- [ ] **Background Build Process**: Automate CMake/Ninja in the background to compile a ready-to-use VST3/AUv3 binary.
- [ ] **Zero Shell Overhead**: The exported plugin functions entirely on its own in any host DAW without requiring the full PedalForge application shell.
