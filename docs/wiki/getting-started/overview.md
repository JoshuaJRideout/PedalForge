# PedalForge Overview

PedalForge is a modular guitar effects workstation built as a JUCE audio plugin (VST3/AU/Standalone). It combines a visual pedalboard with a node-based DSP engine, allowing users to design, wire, and perform with custom effects chains.

## Architecture at a Glance

```
┌─────────────────────────────────────────────────────┐
│                   PluginEditor                       │
│  ┌──────────────────────────────────────────────┐   │
│  │ Toolbar: Play Board Route Pedal FX Script ... │   │
│  └──────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────┐   │
│  │              Active Tab View                  │   │
│  │  (PedalboardGrid / NodeGraphEditor / etc.)    │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
         │                          │
         ▼                          ▼
┌─────────────────┐    ┌──────────────────────┐
│ AudioGraphEngine │    │  PlayGraphEngine     │
│ (Edit Mode)      │    │  (Play Mode)         │
│                  │    │  (same class)        │
│ ┌──────────────┐ │    └──────────────────────┘
│ │ JUCE Audio-  │ │
│ │ Processor-   │ │
│ │ Graph        │ │
│ │  ┌────────┐  │ │
│ │  │ Pedal  │──┤ │    Each pedal wraps a
│ │  │Instance│  │ │    GraphPedalProcessor
│ │  └────────┘  │ │    containing a DSPGraph
│ │      │       │ │
│ │  ┌───▼────┐  │ │
│ │  │DSPGraph│  │ │    ← Node-based FX engine
│ │  │┌──┐┌──┐│  │ │      (filter → delay → etc.)
│ │  ││N1││N2││  │ │
│ │  │└──┘└──┘│  │ │
│ │  └────────┘  │ │
│ └──────────────┘ │
└─────────────────┘
```

## Key Concepts

### Dual Engine Architecture
PedalForge runs **two independent AudioGraphEngines**:
- **Edit Engine** — used in Board/Route/Pedal/FX/Script tabs for designing
- **Play Engine** — used in Play tab for live performance

Both are instances of the same `AudioGraphEngine` class. The processor switches between them based on which tab is active.

### Pedal = Processor + Design
Every pedal on the board is a `PedalInstance` containing:
- A `GraphPedalProcessor` (JUCE AudioProcessor wrapping a DSPGraph)
- A `PedalDesign` (visual layout, controls, parameter mappings)
- Runtime state (control values, bypass, position)

### DSPGraph = Wiremod-style Node Network
Inside each pedal, a `DSPGraph` holds interconnected `DSPNode` objects. Nodes are processed in topological order. There are **128+ node types** spanning audio effects, synthesis, logic gates, math, MIDI, and display peripherals.

### ExpressionVM = Scriptable Everything
The `ExpressionVM` is a bytecode virtual machine that powers:
- Custom DSP (via `ExpressionNode`)
- Custom UI drawing (via canvas overlays)
- Script tab automation

## Technology Stack
- **Framework**: JUCE 8
- **Language**: C++20
- **Plugin Formats**: VST3, AU, Standalone
- **Dependencies**: NAM (Neural Amp Modeler), Eigen
- **Build System**: CMake

## File Structure
```
source/
├── PluginProcessor.h/cpp    ← Audio processing entry point
├── PluginEditor.h/cpp       ← Main UI with tab system
├── dsp/
│   ├── DSPNode.h            ← Base class for all DSP nodes
│   ├── DSPGraph.h           ← Node graph engine
│   ├── DSPNodeLibrary.h     ← Core node implementations
│   ├── ExpressionVM.h       ← Bytecode virtual machine
│   ├── PedalDesign.h        ← Pedal blueprint schema
│   ├── GraphPedalProcessor.h← JUCE processor wrapping DSPGraph
│   ├── LogicNodeLibrary.h   ← Logic gate nodes
│   ├── MathNodeLibrary.h    ← Math operation nodes
│   ├── MemoryNodeLibrary.h  ← Memory/timing nodes
│   ├── ControlNodeLibrary.h ← Control surface nodes
│   ├── SynthNodeLibrary.h   ← Synthesis nodes
│   ├── MidiNodeLibrary.h    ← MIDI processing nodes
│   ├── PeripheralNodeLibrary.h ← Display/I/O nodes
│   └── NAMNode.h            ← Neural Amp Modeler integration
├── engine/
│   ├── AudioGraphEngine.h   ← Main audio graph engine
│   ├── PedalInstance.h      ← Runtime pedal state
│   ├── BoardConfig.h        ← Pedalboard configuration
│   └── MidiRoutingNodes.h   ← Hardware MIDI routing
├── midi/
│   └── MidiLearn.h          ← MIDI learn/mapping manager
├── pedals/
│   ├── PedalRegistry.h      ← Factory pedal registration
│   └── FactoryDesigns.h     ← Built-in pedal designs
└── ui/
    ├── PedalboardGrid.h/cpp ← Board tab grid
    ├── NodeGraphEditor.h/cpp← FX tab node editor
    ├── RoutingGraphEditor.h ← Route tab graph editor
    ├── PedalDesignerComponent.h ← Pedal tab designer
    ├── CanvasOverlay.h      ← Modular pedal overlay
    ├── ScriptingTabComponent.h ← Script tab
    ├── LookAndFeel.h/cpp    ← App theming
    └── ...
```
