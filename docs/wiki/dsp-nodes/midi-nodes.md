# MIDI Nodes

MIDI nodes receive and generate MIDI messages within the DSP graph.

## MIDI Input Nodes (Receivers)

These extract data from the MIDI stream flowing through the graph.

### midi_note — Note Receiver

| | |
|---|---|
| **Type** | `midi_note` |
| **Inputs** | MIDI (MIDI) |
| **Outputs** | Frequency (Control), Gate (Gate), Velocity (Control) |
| **Parameters** | `channel` (0=Omni, 1–16) |

Converts note-on/off to frequency (Hz), gate (0/1), and velocity (0–1).

### midi_cc — CC Receiver

| | |
|---|---|
| **Type** | `midi_cc` |
| **Inputs** | MIDI (MIDI) |
| **Outputs** | Value (Control) — 0 to 1 |
| **Parameters** | `cc` (0–127, default 1), `channel` (0=Omni) |

### midi_cc14 — 14-bit CC

| | |
|---|---|
| **Type** | `midi_cc14` |
| **Inputs** | MIDI (MIDI) |
| **Outputs** | Value (Control) — 0 to 1 (14-bit resolution) |
| **Parameters** | `cc_msb` (0–31, default 0), `channel` (0=Omni) |

Receives paired MSB/LSB CC messages for high-resolution control.

### midi_pitchbend — Pitch Bend Receiver

| | |
|---|---|
| **Type** | `midi_pitchbend` |
| **Inputs** | MIDI (MIDI) |
| **Outputs** | Value (Control) — -1 to +1 |
| **Parameters** | `channel` (0=Omni) |

### midi_clock — MIDI Clock

| | |
|---|---|
| **Type** | `midi_clock` |
| **Inputs** | MIDI (MIDI) |
| **Outputs** | Tempo (Control), Beat (Gate) |

Extracts BPM from MIDI clock messages.

### midi_program — Program Change

| | |
|---|---|
| **Type** | `midi_program` |
| **Inputs** | MIDI (MIDI) |
| **Outputs** | Program (Control) — 0 to 127 |
| **Parameters** | `channel` (0=Omni) |

### midi_pressure — Channel Pressure

| | |
|---|---|
| **Type** | `midi_pressure` |
| **Inputs** | MIDI (MIDI) |
| **Outputs** | Value (Control) — 0 to 1 |

### midi_poly_pressure — Polyphonic Key Pressure

| | |
|---|---|
| **Type** | `midi_poly_pressure` |
| **Inputs** | MIDI (MIDI) |
| **Outputs** | Value (Control) — 0 to 1 |
| **Parameters** | `note` (0–127) |

### midi_song_pos — Song Position

| | |
|---|---|
| **Type** | `midi_song_pos` |
| **Inputs** | MIDI (MIDI) |
| **Outputs** | Position (Control) |

### midi_transport — Transport

| | |
|---|---|
| **Type** | `midi_transport` |
| **Inputs** | MIDI (MIDI) |
| **Outputs** | Playing (Gate), Position (Control) |

---

## MIDI Output Nodes (Generators)

These create MIDI messages from control signals.

### midi_note_gen — Note Generator

| | |
|---|---|
| **Type** | `midi_note_gen` |
| **Inputs** | Note (Control), Velocity (Control), Trigger (Gate) |
| **Outputs** | MIDI (MIDI) |
| **Parameters** | `channel` (1–16, default 1) |

### midi_cc_gen — CC Generator

| | |
|---|---|
| **Type** | `midi_cc_gen` |
| **Inputs** | Value (Control) |
| **Outputs** | MIDI (MIDI) |
| **Parameters** | `cc` (0–127), `channel` (1–16) |

### midi_program_gen — Program Change Generator

| | |
|---|---|
| **Type** | `midi_program_gen` |
| **Inputs** | Program (Control), Trigger (Gate) |
| **Outputs** | MIDI (MIDI) |

### midi_pressure_gen — Channel Pressure Generator

| | |
|---|---|
| **Type** | `midi_pressure_gen` |
| **Inputs** | Value (Control) |
| **Outputs** | MIDI (MIDI) |

### midi_poly_pressure_gen — Poly Pressure Generator

| | |
|---|---|
| **Type** | `midi_poly_pressure_gen` |
| **Inputs** | Note (Control), Value (Control) |
| **Outputs** | MIDI (MIDI) |

### midi_pitchbend_gen — Pitch Bend Generator

| | |
|---|---|
| **Type** | `midi_pitchbend_gen` |
| **Inputs** | Value (Control) — -1 to +1 |
| **Outputs** | MIDI (MIDI) |

### midi_transport_gen — Transport Generator

| | |
|---|---|
| **Type** | `midi_transport_gen` |
| **Inputs** | Play (Gate), Stop (Gate) |
| **Outputs** | MIDI (MIDI) |

## Usage: MIDI-Controlled Filter

```
midi_in  = midi_input
cc_recv  = midi_cc
in       = audio_input
flt      = svf
out      = audio_output

-- CC 74 (brightness) controls cutoff
setParam(cc_recv, "cc", 74)
connect(midi_in, 0, cc_recv, 0)

-- Map CC output to filter cutoff via ranger
rng = ranger
setParam(rng, "min", 200)
setParam(rng, "max", 8000)
connect(cc_recv, 0, rng, 0)
connect(rng, 0, flt, 1)  -- cutoff CV input

connect(in, 0, flt, 0)
connect(flt, 0, out, 0)
```
