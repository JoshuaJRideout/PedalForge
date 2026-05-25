# Effect Nodes

Effect nodes provide standard audio processing like delay, reverb, modulation, and dynamics.

## delay — Delay

Simple delay line with feedback.

| | |
|---|---|
| **Type** | `delay` |
| **Inputs** | In (Audio), Time (Control), Feedback (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `time` (0–2.0 sec, default 0.3), `feedback` (0–1, default 0.4), `mix` (0–1, default 0.5) |

## mod_delay — Modulated Delay

Delay with built-in LFO modulating the delay time for chorus/flanger effects.

| | |
|---|---|
| **Type** | `mod_delay` |
| **Inputs** | In (Audio), Time (Control), Feedback (Control), Rate (Control), Depth (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `time` (0–2.0 sec, default 0.01), `feedback` (0–1, default 0.3), `rate` (0.1–10 Hz, default 1.0), `depth` (0–1, default 0.5), `mix` (0–1, default 0.5) |

## reverb — Schroeder Reverb

Classic Schroeder reverb algorithm.

| | |
|---|---|
| **Type** | `reverb` |
| **Inputs** | In (Audio), Decay (Control), Mix (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `decay` (0–1, default 0.5), `mix` (0–1, default 0.3), `damping` (0–1, default 0.5) |

## compressor — Compressor

Dynamic range compressor.

| | |
|---|---|
| **Type** | `compressor` |
| **Inputs** | In (Audio), Sidechain (Audio), Threshold (Control), Ratio (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `threshold` (-60–0 dB, default -20), `ratio` (1–20, default 4), `attack` (0.1–100 ms, default 5), `release` (10–1000 ms, default 100), `makeup` (0–24 dB, default 0) |

## noisegate — Noise Gate

Gate that silences the signal below a threshold.

| | |
|---|---|
| **Type** | `noisegate` |
| **Inputs** | In (Audio), Threshold (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `threshold` (-80–0 dB, default -40), `attack` (0.1–50 ms, default 1), `release` (10–500 ms, default 50) |

## lfo — Low Frequency Oscillator

Generates control-rate modulation signals.

| | |
|---|---|
| **Type** | `lfo` |
| **Inputs** | Rate (Control), Depth (Control) |
| **Outputs** | Out (Control) |
| **Parameters** | `rate` (0.01–20 Hz, default 1.0), `depth` (0–1, default 1.0), `shape` (0=Sine, 1=Triangle, 2=Square, 3=Saw) |

## phaser — Phaser

Multi-stage phaser effect.

| | |
|---|---|
| **Type** | `phaser` |
| **Inputs** | In (Audio), Rate (Control), Depth (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `rate` (0.1–10 Hz, default 0.5), `depth` (0–1, default 0.7), `stages` (2–12, default 4), `feedback` (0–1, default 0.3) |

## flanger — Flanger

Classic flanger effect.

| | |
|---|---|
| **Type** | `flanger` |
| **Inputs** | In (Audio), Rate (Control), Depth (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `rate` (0.1–10 Hz, default 0.3), `depth` (0–1, default 0.5), `feedback` (0–1, default 0.4) |

## softclip — Soft Clipper

Smooth saturation/overdrive.

| | |
|---|---|
| **Type** | `softclip` |
| **Inputs** | In (Audio), Drive (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `drive` (1–50, default 1) |

Applies `tanh()` waveshaping for warm overdrive character.

## hardclip — Hard Clipper

Aggressive hard clipping distortion.

| | |
|---|---|
| **Type** | `hardclip` |
| **Inputs** | In (Audio), Drive (Control), Threshold (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `drive` (1–50, default 1), `threshold` (0–1, default 0.5) |

## fuzz — Fuzz

Asymmetric fuzz effect.

| | |
|---|---|
| **Type** | `fuzz` |
| **Inputs** | In (Audio), Drive (Control), Tone (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `drive` (1–100, default 10), `tone` (0–1, default 0.5), `mix` (0–1, default 1.0) |

## cabinet — Cabinet Simulator

Speaker cabinet simulation.

| | |
|---|---|
| **Type** | `cabinet` |
| **Inputs** | In (Audio) |
| **Outputs** | Out (Audio) |
| **Parameters** | `type` (0–3, cabinet model) |

## Usage: Classic Guitar Pedal Chain

```
in   = audio_input
ts   = tonestack
od   = softclip
dly  = delay
rev  = reverb
cab  = cabinet
out  = audio_output

-- Drive stage
setParam(od, "drive", 8)
connect(in, 0, od, 0)

-- Tone shaping
setParam(ts, "bass", 0.6)
setParam(ts, "mid", 0.7)
setParam(ts, "treble", 0.4)
connect(od, 0, ts, 0)

-- Time effects
setParam(dly, "time", 0.35)
setParam(dly, "feedback", 0.3)
connect(ts, 0, dly, 0)

setParam(rev, "decay", 0.4)
setParam(rev, "mix", 0.2)
connect(dly, 0, rev, 0)

-- Cabinet
connect(rev, 0, cab, 0)
connect(cab, 0, out, 0)
```
