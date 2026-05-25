# Memory & Timing Nodes

These nodes provide time-based operations, sequencing, and data storage.

## Timing

### logic_delay — Logic Delay

Delays a control signal by a configurable number of samples.

| | |
|---|---|
| **Type** | `logic_delay` |
| **Inputs** | In (Control) |
| **Outputs** | Out (Control) |
| **Parameters** | `samples` (1–44100, default 1) |

### pulse_width — Pulse Width

Extends a gate pulse to a configurable duration.

| | |
|---|---|
| **Type** | `pulse_width` |
| **Inputs** | In (Gate) |
| **Outputs** | Out (Gate) |
| **Parameters** | `width` (0.001–5.0 sec, default 0.1) |

### one_shot — One Shot

Produces a single pulse of fixed duration on each trigger.

| | |
|---|---|
| **Type** | `one_shot` |
| **Inputs** | Trigger (Gate) |
| **Outputs** | Out (Gate) |
| **Parameters** | `duration` (0.001–5.0 sec, default 0.1) |

### debounce — Debounce

Filters rapid transitions from a gate signal.

| | |
|---|---|
| **Type** | `debounce` |
| **Inputs** | In (Gate) |
| **Outputs** | Out (Gate) |
| **Parameters** | `time` (0.001–1.0 sec, default 0.05) |

### blink — Blink

Generates a periodic on/off square wave.

| | |
|---|---|
| **Type** | `blink` |
| **Inputs** | Enable (Gate) |
| **Outputs** | Out (Gate) |
| **Parameters** | `rate` (0.1–20 Hz, default 2.0) |

### ramp — Ramp

Generates a rising ramp signal from 0 to 1 over a configurable duration.

| | |
|---|---|
| **Type** | `ramp` |
| **Inputs** | Trigger (Gate) |
| **Outputs** | Out (Control) |
| **Parameters** | `duration` (0.01–10 sec, default 1.0) |

## Sensors

### env_follower — Envelope Follower

Tracks the amplitude envelope of an audio signal.

| | |
|---|---|
| **Type** | `env_follower` |
| **Inputs** | In (Audio) |
| **Outputs** | Out (Control) — 0 to 1 |
| **Parameters** | `attack` (0.1–100 ms, default 5), `release` (10–1000 ms, default 100) |

### sample_hold — Sample & Hold

Captures the input value when triggered.

| | |
|---|---|
| **Type** | `sample_hold` |
| **Inputs** | In (Control), Trigger (Gate) |
| **Outputs** | Out (Control) |

### pitch_det — Pitch Detector

Detects the fundamental frequency of an audio signal.

| | |
|---|---|
| **Type** | `pitch_det` |
| **Inputs** | In (Audio) |
| **Outputs** | Frequency (Control), Confidence (Control) |

### transient_det — Transient Detector

Detects transients (attacks) in an audio signal.

| | |
|---|---|
| **Type** | `transient_det` |
| **Inputs** | In (Audio) |
| **Outputs** | Out (Gate) |
| **Parameters** | `sensitivity` (0–1, default 0.5) |

### zero_cross — Zero Crossing Detector

Detects zero crossings in an audio signal.

| | |
|---|---|
| **Type** | `zero_cross` |
| **Inputs** | In (Audio) |
| **Outputs** | Rate (Control), Trigger (Gate) |

### pid_ctrl — PID Controller

Proportional-Integral-Derivative controller for feedback control loops.

| | |
|---|---|
| **Type** | `pid_ctrl` |
| **Inputs** | Input (Control), Setpoint (Control) |
| **Outputs** | Out (Control) |
| **Parameters** | `kp` (0–10, default 1), `ki` (0–10, default 0), `kd` (0–10, default 0) |

## Sequencing

### clock — Clock

Generates a periodic trigger at a configurable BPM.

| | |
|---|---|
| **Type** | `clock` |
| **Inputs** | None |
| **Outputs** | Trigger (Gate), Beat (Control) |
| **Parameters** | `bpm` (20–300, default 120), `division` (1–16, default 4) |

### counter — Counter

Counts trigger pulses.

| | |
|---|---|
| **Type** | `counter` |
| **Inputs** | Clock (Gate), Reset (Gate) |
| **Outputs** | Count (Control) |
| **Parameters** | `max` (1–1024, default 16) |

### sequencer — Step Sequencer

Multi-step sequencer with configurable values per step.

| | |
|---|---|
| **Type** | `sequencer` |
| **Inputs** | Clock (Gate), Reset (Gate) |
| **Outputs** | Value (Control), Gate (Gate), Step (Control) |
| **Parameters** | `steps` (1–64, default 8) |

Step values are stored in `PedalInstance::controlData`.

### grid_sequencer — Grid Sequencer

2D grid-based sequencer for drum patterns and melodies.

| | |
|---|---|
| **Type** | `grid_sequencer` |
| **Inputs** | Clock (Gate), Reset (Gate) |
| **Outputs** | Row0..Row7 (Gate), Step (Control) |
| **Parameters** | `steps` (1–64, default 16), `rows` (1–8, default 4) |

### midi_editor — MIDI Editor

Piano roll style MIDI pattern editor and playback.

| | |
|---|---|
| **Type** | `midi_editor` |
| **Inputs** | Clock (Gate), Reset (Gate) |
| **Outputs** | MIDI (MIDI) |

## Data Storage

### array — Array

Indexed data storage with read/write access.

| | |
|---|---|
| **Type** | `array` |
| **Inputs** | Index (Control), Value (Control), Write (Gate) |
| **Outputs** | Out (Control) |
| **Parameters** | `size` (1–256, default 16) |
