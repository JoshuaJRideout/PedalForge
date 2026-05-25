# Synthesis Nodes

Synthesis nodes generate and shape audio signals for building synth voices.

## oscillator — Oscillator

Multi-waveform audio oscillator.

| | |
|---|---|
| **Type** | `oscillator` |
| **Inputs** | Frequency (Control), Shape (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `frequency` (20–20000 Hz, default 440), `shape` (0=Sine, 1=Saw, 2=Square, 3=Triangle), `detune` (0–1, default 0) |

## noise — Noise Generator

White/pink noise source.

| | |
|---|---|
| **Type** | `noise` |
| **Inputs** | None |
| **Outputs** | Out (Audio) |
| **Parameters** | `type` (0=White, 1=Pink) |

## adsr — ADSR Envelope

Attack-Decay-Sustain-Release envelope generator.

| | |
|---|---|
| **Type** | `adsr` |
| **Inputs** | Gate (Gate) |
| **Outputs** | Out (Control) |
| **Parameters** | `attack` (0–5 sec, default 0.01), `decay` (0–5 sec, default 0.1), `sustain` (0–1, default 0.7), `release` (0–10 sec, default 0.3) |

The ADSR output is 0–1. Connect a Gate input to trigger the envelope. The attack phase starts on gate-on, release on gate-off.

## ar_env — AR Envelope

Simplified Attack-Release envelope (no sustain phase).

| | |
|---|---|
| **Type** | `ar_env` |
| **Inputs** | Gate (Gate) |
| **Outputs** | Out (Control) |
| **Parameters** | `attack` (0–5 sec, default 0.01), `release` (0–10 sec, default 0.3) |

## vca — Voltage Controlled Amplifier

Multiplies an audio signal by a control signal.

| | |
|---|---|
| **Type** | `vca` |
| **Inputs** | In (Audio), CV (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | None |

`Out = In × CV`. Essential for connecting envelopes to oscillators.

## voice_alloc — Voice Allocator

Polyphonic voice allocation from MIDI.

| | |
|---|---|
| **Type** | `voice_alloc` |
| **Inputs** | MIDI (MIDI) |
| **Outputs** | Frequency (Control), Gate (Gate), Velocity (Control) |
| **Parameters** | `voices` (1–16, default 4) |

Routes incoming MIDI notes to voice outputs. Manages note stealing and voice priority.

## glide — Glide / Portamento

Smoothly slides between frequency values.

| | |
|---|---|
| **Type** | `glide` |
| **Inputs** | In (Control) |
| **Outputs** | Out (Control) |
| **Parameters** | `time` (0–2 sec, default 0.1) |

## Usage: Basic Mono Synth

```
midi_in  = midi_input
note     = midi_note
osc      = oscillator
env      = adsr
vca_node = vca
flt      = svf
out      = audio_output

-- MIDI → pitch + gate
connect(midi_in, 0, note, 0)     -- MIDI stream
connect(note, 0, osc, 0)         -- frequency → osc
connect(note, 1, env, 0)         -- gate → envelope

-- Oscillator → filter → VCA → output
connect(osc, 0, flt, 0)
setParam(flt, "cutoff", 2000)
setParam(flt, "resonance", 0.6)

connect(flt, 0, vca_node, 0)     -- filtered audio → VCA
connect(env, 0, vca_node, 1)      -- envelope → VCA gain

connect(vca_node, 0, out, 0)
```
