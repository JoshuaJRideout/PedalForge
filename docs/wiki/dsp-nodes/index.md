# DSP Node Catalog

PedalForge includes **128 built-in DSP node types** across 12 categories. Each node can be instantiated via the `createNodeByType(typeString)` factory function.

## Categories at a Glance

| Category | Count | Type Strings |
|----------|-------|-------------|
| **Audio I/O** | 6 | `audio_input`, `audio_output`, `aux_input`, `aux_output`, `midi_input`, `midi_output` |
| **Utility** | 5 | `gain`, `mix`, `split`, `stereo_mixer`, `matrix_mixer`, `matrix_mixer_xl` |
| **Filters** | 7 | `lowpass`, `highpass`, `allpass`, `svf`, `ladder_filter`, `tonestack`, `peq` |
| **Drive / Nonlinear** | 3 | `softclip`, `hardclip`, `fuzz` |
| **Modulation / Delay** | 5 | `lfo`, `delay`, `mod_delay`, `phaser`, `flanger` |
| **Dynamics** | 2 | `compressor`, `noisegate` |
| **Reverb / Cabinet** | 2 | `reverb`, `cabinet` |
| **Synthesis** | 7 | `oscillator`, `noise`, `adsr`, `ar_env`, `vca`, `voice_alloc`, `glide` |
| **Logic Gates** | 11 | `and_gate`, `or_gate`, `not_gate`, `nand_gate`, `nor_gate`, `xor_gate`, `xnor_gate`, `buffer`, `pulse`, `gate_buffer` |
| **Flip-Flops / Latches** | 7 | `sr_latch`, `d_latch`, `d_ff`, `t_ff`, `jk_ff`, `latch`, `comparator` |
| **Routing** | 3 | `mux`, `demux`, `priority` |
| **Comparison** | 6 | `cmp_eq`, `cmp_neq`, `cmp_gt`, `cmp_lt`, `cmp_gte`, `cmp_lte` |
| **Edge / Change** | 4 | `edge_rising`, `edge_falling`, `change_det`, `delta` |
| **Time / Triggers** | 7 | `logic_delay`, `pulse_width`, `one_shot`, `debounce`, `blink`, `ramp`, `array` |
| **Math (Basic)** | 5 | `add`, `subtract`, `multiply`, `divide`, `modulo` |
| **Math (Rounding)** | 3 | `round`, `floor`, `ceiling` |
| **Math (Functions)** | 16 | `sqrt`, `power`, `min`, `max`, `sign`, `reciprocal`, `increment`, `decrement`, `average`, `math_sin`, `math_cos`, `math_tan`, `math_sinh`, `math_cosh`, `math_tanh`, `math_lerp`, `math_exp`, `math_log` |
| **Math (Advanced)** | 3 | `math_smoothstep`, `accumulator`, `constant` |
| **Bitwise** | 6 | `bit_and`, `bit_or`, `bit_xor`, `bit_not`, `bit_shl`, `bit_shr` |
| **Control Mapping** | 5 | `ranger`, `smooth`, `clamp`, `abs`, `negate` |
| **Sensors / Timing** | 5 | `env_follower`, `sample_hold`, `clock`, `counter`, `sequencer` |
| **Audio Analysis** | 4 | `pitch_det`, `transient_det`, `zero_cross`, `pid_ctrl` |
| **Sequencing** | 3 | `grid_sequencer`, `midi_editor`, `expression` |
| **Control Surface** | 6 | `ctrl_knob`, `ctrl_fader`, `ctrl_button`, `ctrl_toggle`, `ctrl_selector`, `ctrl_xy` |
| **MIDI Input** | 10 | `midi_note`, `midi_cc`, `midi_pitchbend`, `midi_clock`, `midi_program`, `midi_pressure`, `midi_poly_pressure`, `midi_cc14`, `midi_song_pos`, `midi_transport` |
| **MIDI Output** | 7 | `midi_note_gen`, `midi_cc_gen`, `midi_program_gen`, `midi_pressure_gen`, `midi_poly_pressure_gen`, `midi_pitchbend_gen`, `midi_transport_gen` |
| **Display** | 13 | `disp_led`, `disp_rgb_led`, `disp_display`, `disp_vu`, `disp_tuner`, `disp_7seg`, `disp_text`, `disp_console`, `disp_scope`, `disp_pixel`, `disp_shader`, `disp_indicator`, `disp_sound` |
| **I/O Peripherals** | 4 | `io_expression`, `io_footswitch`, `io_cv_in`, `io_cv_out` |
| **External** | 4 | `nam`, `ir`, `sampler`, `ram`, `plugin_host` |

## Node Anatomy

Every node has:
- **Type string** — unique identifier for the factory (e.g., `"svf"`)
- **Display name** — human-readable name (e.g., `"SVF Filter"`)
- **Input ports** — typed inputs (Audio, Control, MIDI, Gate)
- **Output ports** — typed outputs
- **Parameters** — adjustable values with id, name, min, max, default
- **CV auto-exposure** — parameters automatically generate Control input ports for modulation

## Using Nodes in Scripts

In the **FX Graph Builder** script mode, nodes are created by type string:

```
-- Create a simple effects chain
in  = addNode("audio_input")
flt = addNode("svf")
dly = addNode("delay")
out = addNode("audio_output")

-- Wire them up
connect(in, 0, flt, 0)    -- audio_input out → svf in
connect(flt, 0, dly, 0)   -- svf out → delay in
connect(dly, 0, out, 0)   -- delay out → audio_output in

-- Set parameters
setParam(flt, "cutoff", 2000)
setParam(flt, "resonance", 0.7)
setParam(dly, "time", 0.35)
setParam(dly, "feedback", 0.4)
```

Browse the category pages for detailed documentation of each node type.
