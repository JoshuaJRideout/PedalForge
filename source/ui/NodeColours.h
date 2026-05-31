#pragma once

#include <juce_graphics/juce_graphics.h>

//==============================================================================
// Canonical colour-coding for DSP node types. Shared by the FX canvas node
// boxes and the inventory thumbnails so the colours match everywhere. (Real
// per-node icons are a separate art task; until then the colour + initials give
// each node a readable identity.)
//==============================================================================
namespace pf
{
    inline juce::Colour nodeColourForType (const juce::String& type)
    {
        if (type == "audio_input" || type == "audio_output") return juce::Colour (0xFF4ADE80);  // green
        if (type == "gain" || type == "mix" || type == "split") return juce::Colour (0xFF94A3B8);  // slate
        if (type == "lowpass" || type == "highpass" || type == "allpass" || type == "tonestack") return juce::Colour (0xFF38BDF8);  // sky blue
        if (type == "softclip" || type == "hardclip") return juce::Colour (0xFFF97316);  // orange
        if (type == "lfo") return juce::Colour (0xFFA78BFA);  // purple
        if (type == "delay" || type == "mod_delay") return juce::Colour (0xFF22D3EE);  // cyan
        if (type == "compressor" || type == "noisegate") return juce::Colour (0xFFFBBF24);  // yellow
        if (type == "reverb") return juce::Colour (0xFF818CF8);  // indigo
        if (type == "ir") return juce::Colour (0xFF6366F1);      // darker indigo
        if (type == "ram" || type == "sampler") return juce::Colour (0xFF14B8A6);  // teal
        if (type == "faust_custom") return juce::Colour (0xFFEC4899);  // pink

        // Synthesizer nodes
        if (type == "oscillator" || type == "noise" || type == "adsr" || type == "ar_env" ||
            type == "svf" || type == "ladder_filter" || type == "vca" || type == "glide" || type == "voice_alloc")
            return juce::Colour (0xFFD946EF);  // fuchsia
        // Logic — teal
        if (type == "and_gate" || type == "or_gate" || type == "not_gate" || type == "xor_gate"
            || type == "nand_gate" || type == "nor_gate" || type == "xnor_gate"
            || type == "buffer" || type == "pulse" || type == "gate_buffer"
            || type == "sr_latch" || type == "d_latch" || type == "d_ff" || type == "t_ff" || type == "jk_ff"
            || type == "comparator" || type == "latch" || type == "mux" || type == "demux" || type == "priority" || type == "constant")
            return juce::Colour (0xFF2DD4BF);
        // Math — rose/coral
        if (type == "add" || type == "subtract" || type == "multiply" || type == "divide" || type == "modulo" ||
            type == "ranger" || type == "smooth" || type == "clamp" || type == "abs" || type == "negate" ||
            type == "round" || type == "floor" || type == "ceiling" || type == "sqrt" || type == "power" ||
            type == "min" || type == "max" || type == "sign" || type == "reciprocal" ||
            type == "increment" || type == "decrement" || type == "average")
            return juce::Colour (0xFFFB7185);
        // Timing / Sensors — amber
        if (type == "clock" || type == "counter" || type == "sequencer"
            || type == "env_follower" || type == "sample_hold")
            return juce::Colour (0xFFF59E0B);
        // Scripting — hot pink / magenta
        if (type == "expression")
            return juce::Colour (0xFFE879F9);
        // MIDI — electric blue
        if (type == "midi_note" || type == "midi_cc" || type == "midi_pitchbend" || type == "midi_clock"
            || type == "midi_program" || type == "midi_pressure" || type == "midi_poly_pressure"
            || type == "midi_cc14" || type == "midi_song_pos" || type == "midi_transport"
            || type == "midi_note_gen" || type == "midi_cc_gen"
            || type == "midi_program_gen" || type == "midi_pressure_gen" || type == "midi_poly_pressure_gen"
            || type == "midi_pitchbend_gen" || type == "midi_transport_gen")
            return juce::Colour (0xFF3B82F6);
        // Control Surface — warm peach (these export to the pedal face)
        if (type == "ctrl_knob" || type == "ctrl_fader" || type == "ctrl_button"
            || type == "ctrl_toggle" || type == "ctrl_selector" || type == "ctrl_xy")
            return juce::Colour (0xFFE8A855);
        // Displays & Gadgets — soft cyan
        if (type == "disp_led" || type == "disp_rgb_led" || type == "disp_display"
            || type == "disp_vu" || type == "disp_tuner" || type == "disp_7seg"
            || type == "disp_text" || type == "disp_console" || type == "disp_scope"
            || type == "disp_pixel" || type == "disp_indicator" || type == "disp_sound")
            return juce::Colour (0xFF22D3EE);
        // I/O Peripherals — indigo (matches I/O category)
        if (type == "io_expression" || type == "io_footswitch" || type == "io_cv_in" || type == "io_cv_out")
            return juce::Colour (0xFF818CF8);
        return juce::Colour (0xFF6B7280);
    }
}
