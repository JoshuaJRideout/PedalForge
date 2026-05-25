# Control Surface Nodes

Control surface nodes create UI controls that appear on the pedal chassis and canvas overlays. They bridge user interaction with the DSP graph.

## ctrl_knob — Knob

Rotary potentiometer control.

| | |
|---|---|
| **Type** | `ctrl_knob` |
| **Inputs** | CV In (Control) — optional external modulation |
| **Outputs** | Value (Control) — 0 to 1 |
| **Parameters** | `value` (0–1, default 0.5) |

When placed on a pedal chassis, renders as a rotary knob with configurable arc range (default 270°) and drag sensitivity.

## ctrl_fader — Fader

Linear slider control.

| | |
|---|---|
| **Type** | `ctrl_fader` |
| **Inputs** | CV In (Control) |
| **Outputs** | Value (Control) — 0 to 1 |
| **Parameters** | `value` (0–1, default 0.5) |

## ctrl_button — Button

Momentary pushbutton. Outputs 1.0 while pressed, 0.0 when released.

| | |
|---|---|
| **Type** | `ctrl_button` |
| **Inputs** | None |
| **Outputs** | Out (Gate) |
| **Parameters** | None |

## ctrl_toggle — Toggle

Latching on/off switch. Click to toggle state.

| | |
|---|---|
| **Type** | `ctrl_toggle` |
| **Inputs** | None |
| **Outputs** | Out (Gate) |
| **Parameters** | `state` (0 or 1, default 0) |

## ctrl_selector — Selector

Multi-position selector switch.

| | |
|---|---|
| **Type** | `ctrl_selector` |
| **Inputs** | None |
| **Outputs** | Value (Control) — 0 to 1 (normalized position) |
| **Parameters** | `positions` (2–16, default 3), `value` (0–1, default 0) |

## ctrl_xy — XY Pad

Two-dimensional touch pad.

| | |
|---|---|
| **Type** | `ctrl_xy` |
| **Inputs** | None |
| **Outputs** | X (Control), Y (Control) — each 0 to 1 |
| **Parameters** | `x` (0–1, default 0.5), `y` (0–1, default 0.5) |

## Usage with PedalDesign Mappings

Control nodes become useful when mapped to DSP parameters through the PedalDesign mapping system:

```
-- In a pedal design, a knob controls filter cutoff:
-- Mapping: controlID "cutoff_knob" → nodeParam "3_cutoff"
-- Where 3 is the node ID of the SVF filter

-- The knob's 0-1 output is mapped through the parameter's
-- min/max range (e.g. 20-20000 Hz for cutoff)
```
