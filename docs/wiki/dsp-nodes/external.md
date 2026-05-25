# External Nodes

External nodes integrate third-party models, impulse responses, audio samples, and plugins into the PedalForge DSP graph.

## nam — Neural Amp Modeler

Loads and runs Neural Amp Modeler (.nam) neural network models for amp/pedal emulation.

| | |
|---|---|
| **Type** | `nam` |
| **Display** | NAM Amp |
| **Inputs** | In (Audio) |
| **Outputs** | Out (Audio) |
| **Parameters** | `input_level` (0–2, default 1.0), `output_level` (0–2, default 1.0) |

### Loading a Model
The NAM node uses a file path parameter (`_filepath`) to load `.nam` files. This is typically set through the Library overlay or file_loader control.

```
-- In a pedal design, use a file_loader control mapped to the NAM node:
-- Control: type="file_loader", controlID="nam_loader", libraryCategory="NAM"
-- Mapping: controlID="nam_loader" → nodeParam="0_filepath"
```

NAM models are loaded asynchronously from the audio thread. The node outputs silence until a model is loaded.

## ir — Impulse Response

Convolution-based impulse response processor for cabinet simulation.

| | |
|---|---|
| **Type** | `ir` |
| **Inputs** | In (Audio) |
| **Outputs** | Out (Audio) |
| **Parameters** | `mix` (0–1, default 1.0) |

Loads `.wav` files as impulse responses. Commonly used after NAM/amp nodes for cabinet simulation.

## sampler — Sampler

Audio sample playback triggered by gate.

| | |
|---|---|
| **Type** | `sampler` |
| **Inputs** | Trigger (Gate), Pitch (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `loop` (0 or 1, default 0), `start` (0–1, default 0), `end` (0–1, default 1) |

Loads `.wav` files for sample-based playback.

## ram — RAM Buffer

Audio buffer that can record and play back audio.

| | |
|---|---|
| **Type** | `ram` |
| **Inputs** | In (Audio), Record (Gate), Play (Gate) |
| **Outputs** | Out (Audio) |
| **Parameters** | `size` (0.1–30 sec, default 5.0) |

Useful for looper-style effects, audio freezing, and granular processing.

## plugin_host — Plugin Host

Hosts external VST3/AU plugins inside the PedalForge graph.

| | |
|---|---|
| **Type** | `plugin_host` |
| **Inputs** | In L (Audio), In R (Audio), MIDI (MIDI) |
| **Outputs** | Out L (Audio), Out R (Audio), MIDI (MIDI) |

### Loading a Plugin
The plugin host uses the system plugin scanner to find available plugins. The plugin path is set via the `_filepath` parameter.

### Notes
- Plugins are loaded in-process
- Plugin state is serialized as part of the node JSON
- Plugin UI can be opened from the FX tab node inspector
- Audio and MIDI pass through the hosted plugin

## Usage: Full Amp Rig

```
in   = audio_input
gate = noisegate
nam  = nam           -- Neural amp model
ir   = ir            -- Cabinet IR
out  = audio_output

-- Input → noise gate → amp → cab → output
setParam(gate, "threshold", -45)
connect(in, 0, gate, 0)
connect(gate, 0, nam, 0)
connect(nam, 0, ir, 0)
connect(ir, 0, out, 0)

-- Load files (done via Library overlay in UI)
-- nam._filepath = "/path/to/amp.nam"
-- ir._filepath = "/path/to/cabinet.wav"
```

## Utility Nodes

These are commonly used alongside external nodes:

### gain — Gain

| | |
|---|---|
| **Type** | `gain` |
| **Inputs** | In (Audio), Gain (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `gain` (-60–24 dB, default 0) |

### mix — Mix

| | |
|---|---|
| **Type** | `mix` |
| **Inputs** | A (Audio), B (Audio), Mix (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `mix` (0–1, default 0.5) |

### split — Split

| | |
|---|---|
| **Type** | `split` |
| **Inputs** | In (Audio) |
| **Outputs** | Out A (Audio), Out B (Audio) |

### stereo_mixer — Stereo Mixer

| | |
|---|---|
| **Type** | `stereo_mixer` |
| **Inputs** | In1 L/R, In2 L/R, In3 L/R, In4 L/R (Audio) |
| **Outputs** | Out L (Audio), Out R (Audio) |
| **Parameters** | `level_1` through `level_4` (0–1), `pan_1` through `pan_4` (0–1) |

### matrix_mixer — Matrix Mixer

| | |
|---|---|
| **Type** | `matrix_mixer` |
| **Inputs** | In1..In4 (Audio) |
| **Outputs** | Out1..Out4 (Audio) |

4×4 mixing matrix with per-crosspoint gain.

### matrix_mixer_xl — Matrix Mixer XL

| | |
|---|---|
| **Type** | `matrix_mixer_xl` |
| **Inputs** | In1..In8 (Audio) |
| **Outputs** | Out1..Out8 (Audio) |

8×8 mixing matrix.
