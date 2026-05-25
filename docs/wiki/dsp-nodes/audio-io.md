# Audio I/O Nodes

These nodes handle audio signal flow into and out of the pedal's DSP graph, including auxiliary (sidechain) buses.

## audio_input — Audio Input

The entry point for audio into the graph. Reads from the host audio buffer.

| | |
|---|---|
| **Type** | `audio_input` |
| **Display** | Audio Input |
| **Inputs** | None |
| **Outputs** | Left (Audio), Right (Audio) |
| **Parameters** | None |

Each output port reads from the corresponding host audio channel. In a stereo configuration, port 0 = Left, port 1 = Right.

## audio_output — Audio Output

The exit point for audio from the graph. Writes back to the host audio buffer.

| | |
|---|---|
| **Type** | `audio_output` |
| **Display** | Audio Output |
| **Inputs** | Left (Audio), Right (Audio) |
| **Outputs** | None |
| **Parameters** | None |

Multiple Audio Output nodes are supported — their signals are summed into the host buffer.

## midi_input — MIDI Input

Receives MIDI from the host or hardware MIDI devices.

| | |
|---|---|
| **Type** | `midi_input` |
| **Display** | MIDI Input |
| **Inputs** | None |
| **Outputs** | MIDI Out (MIDI) |

## midi_output — MIDI Output

Sends MIDI back to the host or hardware MIDI devices.

| | |
|---|---|
| **Type** | `midi_output` |
| **Display** | MIDI Output |
| **Inputs** | MIDI In (MIDI) |
| **Outputs** | None |

## aux_input — Auxiliary Input (Sidechain)

Reads from the secondary input bus (channels 2-3). Used for sidechain processing or FX return.

| | |
|---|---|
| **Type** | `aux_input` |
| **Display** | Aux Input |
| **Inputs** | None |
| **Outputs** | Left (Audio), Right (Audio) |

## aux_output — Auxiliary Output (FX Send)

Writes to the secondary output bus (channels 2-3). Used for FX send.

| | |
|---|---|
| **Type** | `aux_output` |
| **Display** | Aux Output |
| **Inputs** | Left (Audio), Right (Audio) |
| **Outputs** | None |

## Usage Example

A basic stereo pass-through:

```
in  = audio_input
out = audio_output
connect(in, 0, out, 0)   -- Left channel
connect(in, 1, out, 1)   -- Right channel
```

A sidechain compressor setup:

```
in   = audio_input
aux  = aux_input       -- sidechain source
comp = compressor
out  = audio_output

connect(in, 0, comp, 0)    -- main audio
connect(aux, 0, comp, 1)   -- sidechain key
connect(comp, 0, out, 0)
```
