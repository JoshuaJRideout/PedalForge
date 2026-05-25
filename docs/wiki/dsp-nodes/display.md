# Display & Peripheral Nodes

Display nodes render visual output on the pedal chassis or canvas overlays. They read control values and display them graphically.

## LEDs

### disp_led — LED

Single-color status LED. Lights up when input > 0.5.

| | |
|---|---|
| **Type** | `disp_led` |
| **Inputs** | In (Control) |
| **Outputs** | None |
| **Parameters** | `colour` (hex colour, default red) |

### disp_rgb_led — RGB LED

Tri-color LED with separate R, G, B inputs.

| | |
|---|---|
| **Type** | `disp_rgb_led` |
| **Inputs** | R (Control), G (Control), B (Control) |
| **Outputs** | None |

### disp_indicator — Indicator

Multi-state indicator light.

| | |
|---|---|
| **Type** | `disp_indicator` |
| **Inputs** | Value (Control) |
| **Outputs** | None |

## Displays

### disp_display — Display

Generic numeric/text display.

| | |
|---|---|
| **Type** | `disp_display` |
| **Inputs** | Value (Control) |
| **Outputs** | None |
| **Parameters** | `format` (display format string) |

### disp_vu — VU Meter

Level meter display.

| | |
|---|---|
| **Type** | `disp_vu` |
| **Inputs** | Level (Audio or Control) |
| **Outputs** | None |
| **Parameters** | `range` (-60 to 0 dB) |

### disp_tuner — Tuner Display

Chromatic tuner display showing detected pitch.

| | |
|---|---|
| **Type** | `disp_tuner` |
| **Inputs** | Audio In (Audio) |
| **Outputs** | None |

### disp_7seg — Seven Segment Display

7-segment numeric display (0-9, A-F).

| | |
|---|---|
| **Type** | `disp_7seg` |
| **Inputs** | Value (Control) |
| **Outputs** | None |
| **Parameters** | `digits` (1–4, default 2) |

### disp_scope — Oscilloscope

Waveform oscilloscope display.

| | |
|---|---|
| **Type** | `disp_scope` |
| **Inputs** | In (Audio) |
| **Outputs** | None |
| **Parameters** | `timebase` (1–100 ms, default 10) |

## Screens

### disp_text — Text Screen

Multi-line text display.

| | |
|---|---|
| **Type** | `disp_text` |
| **Inputs** | None |
| **Outputs** | None |
| **Parameters** | `text` (string), `fontSize` (8–48, default 14) |

Text content is set via `PedalInstance::controlTexts`.

### disp_console — Console Screen

Scrollable console output.

| | |
|---|---|
| **Type** | `disp_console` |
| **Inputs** | None |
| **Outputs** | None |

### disp_pixel — Pixel Display

Programmable pixel grid display.

| | |
|---|---|
| **Type** | `disp_pixel` |
| **Inputs** | None |
| **Outputs** | None |
| **Parameters** | `width` (1–128, default 32), `height` (1–64, default 16) |

Pixel data is stored in `PedalInstance::controlData` as a flat float array.

### disp_shader — Shader Display

Expression-driven display using the ExpressionVM for custom rendering.

| | |
|---|---|
| **Type** | `disp_shader` |
| **Inputs** | In0..In3 (Control) |
| **Outputs** | None |

The shader code is compiled by ExpressionVM and can use drawing functions (`rect`, `circle`, `line`, `text`, `image`) along with the input values and mouse interaction.

## Audio Output

### disp_sound — Sound Emitter

Outputs audio through a secondary channel for sound effects or click tracks.

| | |
|---|---|
| **Type** | `disp_sound` |
| **Inputs** | In (Audio) |
| **Outputs** | None |

## I/O Peripherals

### io_expression — Expression Pedal

Expression pedal input.

| | |
|---|---|
| **Type** | `io_expression` |
| **Inputs** | None |
| **Outputs** | Value (Control) — 0 to 1 |
| **Parameters** | `min` (0–1, default 0), `max` (0–1, default 1) |

### io_footswitch — Footswitch

External footswitch input.

| | |
|---|---|
| **Type** | `io_footswitch` |
| **Inputs** | None |
| **Outputs** | Out (Gate) |
| **Parameters** | `mode` (0=Momentary, 1=Latching) |

### io_cv_in — CV Input

Control voltage input (for Eurorack/modular integration).

| | |
|---|---|
| **Type** | `io_cv_in` |
| **Inputs** | None |
| **Outputs** | Out (Control) |

### io_cv_out — CV Output

Control voltage output.

| | |
|---|---|
| **Type** | `io_cv_out` |
| **Inputs** | In (Control) |
| **Outputs** | None |
