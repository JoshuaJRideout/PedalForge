# Filter Nodes

Filter nodes shape the frequency content of audio signals.

## lowpass — Low Pass Filter

Simple 1-pole low-pass filter.

| | |
|---|---|
| **Type** | `lowpass` |
| **Inputs** | In (Audio), Cutoff (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `cutoff` (20–20000 Hz, default 1000) |

## highpass — High Pass Filter

Simple 1-pole high-pass filter.

| | |
|---|---|
| **Type** | `highpass` |
| **Inputs** | In (Audio), Cutoff (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `cutoff` (20–20000 Hz, default 200) |

## allpass — All Pass Filter

All-pass filter for phase manipulation.

| | |
|---|---|
| **Type** | `allpass` |
| **Inputs** | In (Audio), Frequency (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `frequency` (20–20000 Hz, default 1000) |

## svf — State Variable Filter

Versatile multi-mode filter with simultaneous lowpass, highpass, and bandpass outputs.

| | |
|---|---|
| **Type** | `svf` |
| **Inputs** | In (Audio), Cutoff (Control), Resonance (Control) |
| **Outputs** | LP (Audio), HP (Audio), BP (Audio) |
| **Parameters** | `cutoff` (20–20000 Hz, default 1000), `resonance` (0–1, default 0.5), `mode` (0=LP, 1=HP, 2=BP) |

The SVF is one of the most commonly used filters. Its three simultaneous outputs make it ideal for creating crossover networks or blending filter characters.

## ladder_filter — Ladder Filter

Classic Moog-style 4-pole ladder filter with self-oscillation capability.

| | |
|---|---|
| **Type** | `ladder_filter` |
| **Inputs** | In (Audio), Cutoff (Control), Resonance (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `cutoff` (20–20000 Hz, default 1000), `resonance` (0–1, default 0.5) |

At high resonance values the filter self-oscillates, producing a sine-like tone at the cutoff frequency.

## tonestack — Tone Stack

Guitar amplifier-style 3-band tone control.

| | |
|---|---|
| **Type** | `tonestack` |
| **Inputs** | In (Audio), Bass (Control), Mid (Control), Treble (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `bass` (0–1, default 0.5), `mid` (0–1, default 0.5), `treble` (0–1, default 0.5) |

## peq — Parametric EQ

Full parametric equalizer with configurable frequency, gain, and Q.

| | |
|---|---|
| **Type** | `peq` |
| **Inputs** | In (Audio), Freq (Control), Gain (Control), Q (Control) |
| **Outputs** | Out (Audio) |
| **Parameters** | `frequency` (20–20000 Hz, default 1000), `gain` (-24–24 dB, default 0), `q` (0.1–10, default 0.707) |

## Usage: Building an EQ Chain

```
in  = audio_input
eq1 = peq       -- Low shelf at 100 Hz
eq2 = peq       -- Mid boost at 1 kHz
eq3 = peq       -- High cut at 8 kHz
out = audio_output

setParam(eq1, "frequency", 100)
setParam(eq1, "gain", 3.0)
setParam(eq2, "frequency", 1000)
setParam(eq2, "gain", 2.0)
setParam(eq3, "frequency", 8000)
setParam(eq3, "gain", -6.0)

connect(in, 0, eq1, 0)
connect(eq1, 0, eq2, 0)
connect(eq2, 0, eq3, 0)
connect(eq3, 0, out, 0)
```
