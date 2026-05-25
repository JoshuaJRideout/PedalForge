# Graph Builder Commands

The Graph Builder is a text-based scripting interface in the **Script tab** that lets you build and modify DSP graphs programmatically.

## Overview

Instead of visually connecting nodes in the FX tab, you can write scripts that create nodes, set parameters, and wire connections. This is especially useful for:

- **Templating** — Create reusable pedal architectures
- **Batch operations** — Set up complex routing in a few lines
- **Precision** — Exact parameter values without mouse dragging
- **Sharing** — Copy-paste graph definitions as text

## Commands

### Node Management

```
-- Add a new node (returns node ID)
id = addNode("node_type")
id = addNode("node_type", "Custom Name")

-- Remove a node
removeNode(id)

-- List all nodes
nodes = listNodes()
```

### Connections

```
-- Connect output port to input port
connect(sourceId, outputPort, destId, inputPort)

-- Disconnect a specific connection
disconnect(sourceId, outputPort, destId, inputPort)

-- Disconnect all connections from/to a node
disconnectAll(nodeId)
```

### Parameters

```
-- Set a node parameter
setParam(nodeId, "paramName", value)

-- Get a node parameter
value = getParam(nodeId, "paramName")
```

### Graph Operations

```
-- Clear the entire graph
clearGraph()

-- Get current graph as JSON
json = exportGraph()

-- Load a graph from JSON
importGraph(json)
```

## Example: Classic Overdrive Pedal

```
-- Build a tube screamer-style overdrive
clearGraph()

-- I/O
inp = addNode("audio_input")
out = addNode("audio_output")

-- Signal chain
hp  = addNode("highpass", "Input Filter")
drv = addNode("softclip", "Drive")
ts  = addNode("tonestack", "Tone")
vol = addNode("gain", "Volume")

-- Set parameters
setParam(hp, "cutoff", 720)
setParam(drv, "drive", 12)
setParam(ts, "bass", 0.3)
setParam(ts, "mid", 0.7)
setParam(ts, "treble", 0.5)
setParam(vol, "gain", -3)

-- Wire it up
connect(inp, 0, hp, 0)
connect(hp, 0, drv, 0)
connect(drv, 0, ts, 0)
connect(ts, 0, vol, 0)
connect(vol, 0, out, 0)
```

## Example: Parallel FX Chain

```
-- Two parallel effects mixed together
clearGraph()

inp   = addNode("audio_input")
out   = addNode("audio_output")
split = addNode("split", "Split")
mixer = addNode("mix", "Mixer")

-- Path A: Delay
dly = addNode("delay", "Delay")
setParam(dly, "time", 0.35)
setParam(dly, "feedback", 0.4)

-- Path B: Reverb
rev = addNode("reverb", "Reverb")
setParam(rev, "decay", 0.6)

-- Split input to both paths
connect(inp, 0, split, 0)
connect(split, 0, dly, 0)   -- Path A
connect(split, 1, rev, 0)   -- Path B

-- Mix and output
connect(dly, 0, mixer, 0)
connect(rev, 0, mixer, 1)
setParam(mixer, "mix", 0.5)
connect(mixer, 0, out, 0)
```

## Example: MIDI Synth Voice

```
-- Mono synth with filter and envelope
clearGraph()

-- MIDI input
mi = addNode("midi_input")
mn = addNode("midi_note", "Note")

-- Oscillator
osc = addNode("oscillator", "VCO")
setParam(osc, "shape", 1)  -- Sawtooth

-- Filter
flt = addNode("svf", "VCF")
setParam(flt, "cutoff", 2000)
setParam(flt, "resonance", 0.6)

-- Envelope
env = addNode("adsr", "Env")
setParam(env, "attack", 0.01)
setParam(env, "decay", 0.2)
setParam(env, "sustain", 0.5)
setParam(env, "release", 0.4)

-- VCA
vca = addNode("vca", "VCA")

-- Output
out = addNode("audio_output")

-- MIDI → Note
connect(mi, 0, mn, 0)

-- Note → Oscillator frequency
connect(mn, 0, osc, 0)

-- Note gate → Envelope
connect(mn, 1, env, 0)

-- Oscillator → Filter → VCA → Output
connect(osc, 0, flt, 0)
connect(flt, 0, vca, 0)    -- LP output
connect(env, 0, vca, 1)     -- Envelope → VCA gain
connect(vca, 0, out, 0)
```

## Example: Adding Controls

```
-- Create knobs that map to DSP parameters

-- Add a cutoff knob
knob = addNode("ctrl_knob", "Cutoff")

-- The knob outputs 0-1, use a ranger to map to frequency range
rng = addNode("ranger", "Freq Range")
setParam(rng, "min", 200)
setParam(rng, "max", 8000)

-- Connect: knob → ranger → filter cutoff CV
connect(knob, 0, rng, 0)
connect(rng, 0, flt, 1)    -- Cutoff CV input of SVF
```

## Script Tab Integration

When you run a graph builder script in the Script tab:

1. The script modifies the active pedal's effects graph
2. Changes are immediately reflected in the FX tab
3. The pedal processor is rebuilt for audio
4. Changes are saved in the undo history

The `onGraphChanged` callback in the Script tab notifies the editor to refresh the FX view after script execution.

## Tips

- Use `clearGraph()` at the start of templates to start fresh
- Node IDs are integers — store them in variables for readability
- Port indices start at 0
- Use descriptive node names for easier identification in the FX tab
- The graph builder script runs in the same [[expression-vm|scripting/expression-vm]] environment
