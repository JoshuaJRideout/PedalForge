# Math Nodes

Math nodes perform arithmetic and mathematical operations on control signals.

## Basic Arithmetic

| Type | Display | Inputs | Output | Formula |
|------|---------|--------|--------|---------|
| `add` | Add | A, B (Control) | Out | A + B |
| `subtract` | Subtract | A, B (Control) | Out | A - B |
| `multiply` | Multiply | A, B (Control) | Out | A × B |
| `divide` | Divide | A, B (Control) | Out | A / B (safe: 0 if B≈0) |
| `modulo` | Modulo | A, B (Control) | Out | A mod B |

## Rounding

| Type | Display | Input | Output | Formula |
|------|---------|-------|--------|---------|
| `round` | Round | In (Control) | Out | round(In) |
| `floor` | Floor | In (Control) | Out | floor(In) |
| `ceiling` | Ceiling | In (Control) | Out | ceil(In) |

## Power & Roots

| Type | Display | Inputs | Output | Formula |
|------|---------|--------|--------|---------|
| `sqrt` | Square Root | In (Control) | Out | √|In| |
| `power` | Power | Base, Exp (Control) | Out | Base^Exp |

## Min / Max / Clamp

| Type | Display | Inputs | Output | Formula |
|------|---------|--------|--------|---------|
| `min` | Min | A, B (Control) | Out | min(A, B) |
| `max` | Max | A, B (Control) | Out | max(A, B) |
| `clamp` | Clamp | In, Lo, Hi (Control) | Out | clamp(In, Lo, Hi) |
| `abs` | Absolute | In (Control) | Out | |In| |
| `negate` | Negate | In (Control) | Out | -In |
| `sign` | Sign | In (Control) | Out | -1, 0, or 1 |
| `reciprocal` | Reciprocal | In (Control) | Out | 1/In |

## Counters

| Type | Display | Inputs | Output | Formula |
|------|---------|--------|--------|---------|
| `increment` | Increment | In (Control) | Out | In + 1 |
| `decrement` | Decrement | In (Control) | Out | In - 1 |
| `accumulator` | Accumulator | In (Control), Reset (Gate) | Out | Running sum of In |

## Averaging

| Type | Display | Inputs | Output | Formula |
|------|---------|--------|--------|---------|
| `average` | Average | A, B (Control) | Out | (A + B) / 2 |

## Trigonometric

| Type | Display | Input | Output | Formula |
|------|---------|-------|--------|---------|
| `math_sin` | Sine | In (Control) | Out | sin(In) |
| `math_cos` | Cosine | In (Control) | Out | cos(In) |
| `math_tan` | Tangent | In (Control) | Out | tan(In) |
| `math_sinh` | Sinh | In (Control) | Out | sinh(In) |
| `math_cosh` | Cosh | In (Control) | Out | cosh(In) |
| `math_tanh` | Tanh | In (Control) | Out | tanh(In) |

## Exponential & Logarithmic

| Type | Display | Input | Output | Formula |
|------|---------|-------|--------|---------|
| `math_exp` | Exp | In (Control) | Out | e^In |
| `math_log` | Log | In (Control) | Out | ln(|In|) |

## Interpolation & Mapping

| Type | Display | Inputs | Output | Formula |
|------|---------|--------|--------|---------|
| `math_lerp` | Lerp | A, B, T (Control) | Out | A + (B-A) × T |
| `math_smoothstep` | Smooth Step | In (Control) | Out | Hermite smooth |
| `ranger` | Ranger | In, Min, Max (Control) | Out | Maps 0–1 to Min–Max |
| `smooth` | Smooth | In (Control) | Out | Low-pass smoothing |

## Bitwise

| Type | Display | Inputs | Output | Formula |
|------|---------|--------|--------|---------|
| `bit_and` | Bit AND | A, B (Control) | Out | A & B |
| `bit_or` | Bit OR | A, B (Control) | Out | A \| B |
| `bit_xor` | Bit XOR | A, B (Control) | Out | A ^ B |
| `bit_not` | Bit NOT | In (Control) | Out | ~In |
| `bit_shl` | Shift Left | In, Amount (Control) | Out | In << Amount |
| `bit_shr` | Shift Right | In, Amount (Control) | Out | In >> Amount |

These operate on the integer representation of the float values.
