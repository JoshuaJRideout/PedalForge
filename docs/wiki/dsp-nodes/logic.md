# Logic Gate Nodes

Logic nodes operate on gate/control signals using boolean logic. Values > 0.5 are treated as TRUE (1.0), values ≤ 0.5 as FALSE (0.0).

## Basic Gates

| Type | Display | Inputs | Output | Function |
|------|---------|--------|--------|----------|
| `and_gate` | AND Gate | A (Control), B (Control) | Out (Control) | A AND B |
| `or_gate` | OR Gate | A (Control), B (Control) | Out (Control) | A OR B |
| `not_gate` | NOT Gate | A (Control) | Out (Control) | NOT A |
| `nand_gate` | NAND Gate | A (Control), B (Control) | Out (Control) | NOT (A AND B) |
| `nor_gate` | NOR Gate | A (Control), B (Control) | Out (Control) | NOT (A OR B) |
| `xor_gate` | XOR Gate | A (Control), B (Control) | Out (Control) | A XOR B |
| `xnor_gate` | XNOR Gate | A (Control), B (Control) | Out (Control) | NOT (A XOR B) |

## Signal Flow

| Type | Display | Inputs | Outputs | Description |
|------|---------|--------|---------|-------------|
| `buffer` | Buffer | In (Control) | Out (Control) | Signal buffer (1-sample delay) |
| `pulse` | Pulse | Trigger (Gate) | Out (Gate) | Converts rising edge to single-sample pulse |
| `gate_buffer` | Gate Buffer | In (Gate) | Out (Gate) | Buffers gate signal |

## Flip-Flops & Latches

| Type | Display | Inputs | Outputs | Description |
|------|---------|--------|---------|-------------|
| `sr_latch` | SR Latch | Set (Gate), Reset (Gate) | Q (Control), Q̄ (Control) | Set-Reset latch |
| `d_latch` | D Latch | D (Control), Enable (Gate) | Q (Control) | Data latch (transparent when enabled) |
| `d_ff` | D Flip-Flop | D (Control), Clock (Gate) | Q (Control) | Edge-triggered D flip-flop |
| `t_ff` | T Flip-Flop | T (Gate), Clock (Gate) | Q (Control) | Toggle flip-flop |
| `jk_ff` | JK Flip-Flop | J (Gate), K (Gate), Clock (Gate) | Q (Control) | JK flip-flop |
| `latch` | Latch | In (Control), Trigger (Gate) | Out (Control) | Captures input on trigger |

## Routing

| Type | Display | Inputs | Outputs | Description |
|------|---------|--------|---------|-------------|
| `mux` | Multiplexer | A (Control), B (Control), Select (Gate) | Out (Control) | Selects A or B based on Select |
| `demux` | Demultiplexer | In (Control), Select (Gate) | A (Control), B (Control) | Routes input to A or B |
| `priority` | Priority | In0..In3 (Control) | Out (Control), Index (Control) | Outputs highest-priority active input |

## Comparison

| Type | Display | Inputs | Outputs | Description |
|------|---------|--------|---------|-------------|
| `cmp_eq` | Equal | A, B (Control) | Out (Control) | 1.0 if A == B |
| `cmp_neq` | Not Equal | A, B (Control) | Out (Control) | 1.0 if A != B |
| `cmp_gt` | Greater Than | A, B (Control) | Out (Control) | 1.0 if A > B |
| `cmp_lt` | Less Than | A, B (Control) | Out (Control) | 1.0 if A < B |
| `cmp_gte` | Greater/Equal | A, B (Control) | Out (Control) | 1.0 if A >= B |
| `cmp_lte` | Less/Equal | A, B (Control) | Out (Control) | 1.0 if A <= B |
| `comparator` | Comparator | In (Control), Threshold (Control) | Out (Control) | 1.0 if In > Threshold |

## Edge Detection

| Type | Display | Inputs | Outputs | Description |
|------|---------|--------|---------|-------------|
| `edge_rising` | Rising Edge | In (Control) | Out (Gate) | Pulse on transition from low to high |
| `edge_falling` | Falling Edge | In (Control) | Out (Gate) | Pulse on transition from high to low |
| `change_det` | Change Detector | In (Control) | Out (Gate) | Pulse on any value change |
| `delta` | Delta | In (Control) | Out (Control) | Outputs difference from previous sample |

## Constants

| Type | Display | Outputs | Description |
|------|---------|---------|-------------|
| `constant` | Constant | Out (Control) | Outputs a fixed value |

**Parameters**: `value` (any float, default 0)

## Usage: Toggle Bypass with Footswitch

```
foot  = io_footswitch
toggle = t_ff
mux_  = mux
in    = audio_input
fx    = softclip
out   = audio_output

-- Footswitch → toggle flip-flop
connect(foot, 0, toggle, 0)

-- Mux: dry (A) or wet (B) based on toggle
connect(in, 0, fx, 0)
connect(in, 0, mux_, 0)     -- dry
connect(fx, 0, mux_, 1)     -- wet
connect(toggle, 0, mux_, 2) -- select
connect(mux_, 0, out, 0)
```
