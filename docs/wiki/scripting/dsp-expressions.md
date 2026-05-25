# DSP Expressions

DSP expressions allow you to create custom audio processing using the ExpressionVM inside `expression` DSP nodes.

## Expression Node

The `expression` node type accepts a code string that runs per-sample.

| | |
|---|---|
| **Type** | `expression` |
| **Inputs** | In0..In3 (Audio/Control) |
| **Outputs** | Out0..Out3 (Audio/Control) |
| **Parameters** | `code` (string — the expression script) |

## Per-Sample Variables

| Variable | Description |
|----------|-------------|
| `in0`..`in3` | Input port values for current sample |
| `out0`..`out3` | Output port values (assign these) |
| `sr` | Sample rate (e.g. 44100) |
| `t` | Time in seconds since start |
| `dt` | Time between samples (1/sr) |
| `pi` | π constant |
| `phase` | Persistent phase accumulator (0-1, wraps) |

## State Variables

Expression nodes can maintain persistent state between samples using the `state` prefix:

```
-- state variables persist across samples
state.prev = in0
state.sum = state.sum + in0
```

## Math Functions

All standard math functions are available:

| Function | Description |
|----------|-------------|
| `sin(x)` | Sine |
| `cos(x)` | Cosine |
| `tan(x)` | Tangent |
| `abs(x)` | Absolute value |
| `floor(x)` | Floor |
| `ceil(x)` | Ceiling |
| `round(x)` | Round |
| `sqrt(x)` | Square root |
| `pow(x, y)` | Power |
| `exp(x)` | e^x |
| `log(x)` | Natural log |
| `log2(x)` | Log base 2 |
| `min(a, b)` | Minimum |
| `max(a, b)` | Maximum |
| `clamp(x, lo, hi)` | Clamp to range |
| `lerp(a, b, t)` | Linear interpolation |
| `tanh(x)` | Hyperbolic tangent (soft clipping) |
| `sign(x)` | Sign (-1, 0, 1) |
| `mod(x, y)` | Modulo |
| `wrap(x, lo, hi)` | Wrap to range |

## Example: Custom Waveshaper

```
-- Asymmetric soft clipper with bias
drive = 5.0
bias = 0.1

x = in0 * drive + bias
out0 = tanh(x) * 0.8
```

## Example: Ring Modulator

```
-- Ring modulation: multiply two signals
out0 = in0 * in1
```

## Example: Simple Low-Pass Filter

```
-- One-pole low-pass filter
-- in0 = audio, in1 = cutoff (0-1)

cutoff = clamp(in1, 0.001, 0.999)
coeff = exp(-2.0 * pi * cutoff * 0.5)

state.y = state.y * coeff + in0 * (1.0 - coeff)
out0 = state.y
```

## Example: Bit Crusher

```
-- Bit depth reduction
bits = floor(in1 * 14 + 2)  -- 2-16 bits
steps = pow(2, bits)
out0 = floor(in0 * steps) / steps
```

## Example: Sample Rate Reducer

```
-- Downsample effect
-- in1 controls reduction factor (0-1)

rate = floor(lerp(1, 32, in1))

state.counter = state.counter + 1
if state.counter >= rate then
  state.counter = 0
  state.held = in0
end

out0 = state.held
```

## Example: Stereo Tremolo

```
-- Auto-panning tremolo
-- in0 = left, in1 = right
-- Rate and depth controlled by expression parameters

rate = 4.0     -- Hz
depth = 0.7

lfo = sin(t * rate * 2 * pi) * depth * 0.5 + 0.5

out0 = in0 * lfo            -- Left modulated
out1 = in1 * (1.0 - lfo)    -- Right inverse
```

## Example: Envelope Follower

```
-- Simple envelope follower
-- in0 = audio input

attack = 0.001   -- fast attack
release = 0.05   -- slower release

input_abs = abs(in0)

if input_abs > state.env then
  coeff = exp(-1.0 / (attack * sr))
else
  coeff = exp(-1.0 / (release * sr))
end

state.env = state.env * coeff + input_abs * (1.0 - coeff)
out0 = state.env
```

## Performance Notes

- Expression nodes run per-sample — keep code efficient
- Avoid heavy branching or deep nesting
- State variables are initialized to 0 on first use
- Division by zero returns 0 (safe division)
- The VM has a stack depth limit of 256
