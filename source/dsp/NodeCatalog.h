#pragma once

#include <juce_core/juce_core.h>
#include <vector>

//==============================================================================
/**
 * Single source of truth for the set of DSP node types the user can add to a
 * pedal's FX graph.
 *
 * Consumers (must NOT maintain their own parallel list):
 *   - source/ui/NodeGraphEditor.cpp — right-click "Add Node" popup menu
 *   - source/ui/InventoryOverlay.cpp — Tab key Q-menu node browser
 *   - source/ui/AutocompletePanelComponent.cpp — FX Graph Builder script DB
 *
 * Adding a new node type:
 *   1. Implement the node class + register it in DSPGraph::createNodeByType
 *   2. Add a NodeCatalogEntry below — that's it; menus update automatically.
 *
 * Menu path: "Group/Subgroup[/Sub-subgroup]". Slash-separated; each segment
 * becomes a level of nested submenu in the right-click menu. The first segment
 * is also used as the InventoryOverlay's main category (currently "Effects" or
 * "Nodes").
 */
struct NodeCatalogEntry
{
    juce::String type;          // DSPGraph::createNodeByType key
    juce::String displayName;   // Shown in menus
    juce::String menuPath;      // e.g. "Effects/Filters", "Nodes/MIDI/Receive"
    juce::String description;   // Tooltip + autocomplete docs

    bool inAddNodeMenu = true;  // Show in NodeGraphEditor right-click menu
    bool inInventory   = true;  // Show in InventoryOverlay Q-menu
};

class NodeCatalog
{
public:
    static const std::vector<NodeCatalogEntry>& getEntries()
    {
        static const std::vector<NodeCatalogEntry> entries =
        {
            // ── Effects ──────────────────────────────────────────────
            { "lowpass",        "Low Pass",          "Effects/Filters",     "Cuts high frequencies." },
            { "highpass",       "High Pass",         "Effects/Filters",     "Cuts low frequencies." },
            { "allpass",        "All Pass",          "Effects/Filters",     "Changes phase without affecting amplitude." },
            { "tonestack",      "Tone Stack",        "Effects/Filters",     "Classic guitar amp 3-band EQ." },
            { "peq",            "Parametric EQ",     "Effects/Filters",     "Precise multi-band equalization." },

            { "softclip",       "Soft Clip",         "Effects/Drive",       "Smooth overdrive." },
            { "hardclip",       "Hard Clip",         "Effects/Drive",       "Harsh distortion." },
            { "fuzz",           "Fuzz",              "Effects/Drive",       "Classic transistor fuzz." },

            { "lfo",            "LFO",               "Effects/Modulation",  "Low frequency oscillator for CV." },
            { "phaser",         "Phaser",            "Effects/Modulation",  "Phase shifting effect." },
            { "flanger",        "Flanger",           "Effects/Modulation",  "Classic flanging effect." },

            { "delay",          "Delay",             "Effects/Delay",       "Standard digital delay." },
            { "mod_delay",      "Mod Delay",         "Effects/Delay",       "Delay with modulated time." },

            { "compressor",     "Compressor",        "Effects/Dynamics",    "Reduces dynamic range." },
            { "noisegate",      "Noise Gate",        "Effects/Dynamics",    "Mutes signal below threshold." },

            { "reverb",         "Reverb",            "Effects/Reverb",      "Algorithmic room/hall simulation." },
            { "ir",             "IR Convolution",    "Effects/Reverb",      "Loads impulse responses." },

            { "cabinet",        "Cabinet Sim",       "Effects/Guitar Utility", "Speaker cabinet simulation." },

            // ── Nodes / I/O ──────────────────────────────────────────
            // Audio Input/Output are auto-added with each pedal — keep them
            // out of the add-node menu but discoverable in the inventory.
            { "audio_input",    "Audio Input",       "Nodes/I/O",           "Receives audio from the pedalboard input.",  false, true },
            { "audio_output",   "Audio Output",      "Nodes/I/O",           "Sends audio to the pedalboard output.",      false, true },
            { "midi_input",     "MIDI Input",        "Nodes/I/O",           "Receives MIDI events." },
            { "midi_output",    "MIDI Output",       "Nodes/I/O",           "Sends MIDI events out." },
            { "io_expression",  "Expression Pedal",  "Nodes/I/O",           "Reads expression pedal input." },
            { "io_footswitch",  "Footswitch (I/O)",  "Nodes/I/O",           "Hardware footswitch input." },
            { "io_cv_in",       "CV Input",          "Nodes/I/O",           "Control voltage input." },
            { "io_cv_out",      "CV Output",         "Nodes/I/O",           "Control voltage output." },

            // ── Nodes / Utility ──────────────────────────────────────
            { "gain",           "Gain",              "Nodes/Utility",       "Simple volume adjustment." },
            { "mix",            "Mix",               "Nodes/Utility",       "Crossfades between two signals." },
            { "split",          "Split",             "Nodes/Utility",       "Splits one signal into two." },

            // ── Nodes / Controls (auto-spawn pedal-face controls) ─────
            { "ctrl_knob",      "Knob",              "Nodes/Controls",      "Rotary control. Auto-spawns a knob on the pedal face bound to its value." },
            { "ctrl_fader",     "Fader",             "Nodes/Controls",      "Linear slider. Auto-spawns a fader on the pedal face." },
            { "ctrl_button",    "Button",            "Nodes/Controls",      "Momentary gate. Auto-spawns a footswitch (high while pressed)." },
            { "ctrl_toggle",    "Toggle",            "Nodes/Controls",      "Latching on/off. Auto-spawns a switch on the pedal face." },
            { "ctrl_selector",  "Selector",          "Nodes/Controls",      "Multi-position switch. Auto-spawns a switch on the pedal face." },
            { "ctrl_encoder",   "Encoder",           "Nodes/Controls",      "Endless rotary that counts in steps (integer or value). Renders as a knob." },
            { "ctrl_pan",       "Pan / Bipolar",     "Nodes/Controls",      "Centre-detented knob over a +/- range (pan, balance, bias)." },
            { "ctrl_modwheel",  "Wheel",             "Nodes/Controls",      "Mod/pitch wheel fader with optional spring-return rest position." },
            { "ctrl_trim",      "Trim",              "Nodes/Controls",      "Fine-adjust knob (low sensitivity) for set-and-forget calibration." },
            { "footswitch",     "Footswitch",        "Nodes/Controls",      "Latching foot-controlled stomp switch." },

            // ── Nodes / Memory / Files ───────────────────────────────
            { "ram",            "RAM / Delay Line",  "Nodes/Memory / Files", "Raw memory buffer." },
            { "sampler",        "File Sampler",      "Nodes/Memory / Files", "Plays back audio files." },

            // ── Nodes / Synthesizer ──────────────────────────────────
            { "oscillator",     "Oscillator (VCO)",  "Nodes/Synthesizer",   "Standard waveform generator." },
            { "noise",          "Noise Gen",         "Nodes/Synthesizer",   "White/pink noise source." },
            { "adsr",           "ADSR Envelope",     "Nodes/Synthesizer",   "4-stage envelope generator." },
            { "ar_env",         "AR Envelope",       "Nodes/Synthesizer",   "2-stage attack/release envelope." },
            { "svf",            "State Variable Filter", "Nodes/Synthesizer", "Multi-mode resonant filter." },
            { "ladder_filter",  "Ladder Filter",     "Nodes/Synthesizer",   "Moog-style resonant lowpass." },
            { "vca",            "VCA",               "Nodes/Synthesizer",   "Voltage controlled amplifier." },
            { "glide",          "Glide (Portamento)","Nodes/Synthesizer",   "Smooths pitch transitions." },
            { "voice_alloc",    "Voice Allocator",   "Nodes/Synthesizer",   "Distributes notes across multiple voices." },

            // ── Nodes / Logic ────────────────────────────────────────
            { "and_gate",       "AND Gate",          "Nodes/Logic",         "Outputs 1 if all inputs are 1." },
            { "or_gate",        "OR Gate",           "Nodes/Logic",         "Outputs 1 if any input is 1." },
            { "not_gate",       "NOT Gate",          "Nodes/Logic",         "Inverts a binary signal." },
            { "nand_gate",      "NAND Gate",         "Nodes/Logic",         "Inverted AND." },
            { "nor_gate",       "NOR Gate",          "Nodes/Logic",         "Inverted OR." },
            { "xor_gate",       "XOR Gate",          "Nodes/Logic",         "Exclusive OR." },
            { "xnor_gate",      "XNOR Gate",         "Nodes/Logic",         "Inverted XOR." },
            { "buffer",         "Buffer",            "Nodes/Logic",         "Passes signal through (clean copy)." },
            { "pulse",          "Pulse",             "Nodes/Logic",         "Generates a single pulse on trigger." },
            { "gate_buffer",    "Gate (Buffer)",     "Nodes/Logic",         "Latches a gate signal." },
            { "sr_latch",       "SR Latch",          "Nodes/Logic",         "Set/Reset latch." },
            { "d_latch",        "D Latch",           "Nodes/Logic",         "Data latch." },
            { "d_ff",           "D Flip-Flop",       "Nodes/Logic",         "Edge-triggered D flip-flop." },
            { "t_ff",           "T Flip-Flop",       "Nodes/Logic",         "Toggle flip-flop." },
            { "jk_ff",          "JK Flip-Flop",      "Nodes/Logic",         "JK flip-flop." },
            { "comparator",     "Comparator",        "Nodes/Logic",         "Compares two signals." },
            { "latch",          "Latch / Toggle",    "Nodes/Logic",         "Holds a value on trigger." },
            { "constant",       "Constant",          "Nodes/Logic",         "Outputs a fixed value." },
            { "mux",            "Mux / A|B Switch",  "Nodes/Logic",         "Selects between multiple inputs." },
            { "demux",          "Demux",             "Nodes/Logic",         "Routes one input to one of many outputs." },
            { "priority",       "Priority",          "Nodes/Logic",         "Outputs highest-priority active input." },

            // ── Nodes / Math ─────────────────────────────────────────
            { "add",            "Add",               "Nodes/Math",          "A + B" },
            { "subtract",       "Subtract",          "Nodes/Math",          "A - B" },
            { "multiply",       "Multiply",          "Nodes/Math",          "A * B" },
            { "divide",         "Divide",            "Nodes/Math",          "A / B" },
            { "modulo",         "Modulo",            "Nodes/Math",          "A % B" },
            { "round",          "Round",             "Nodes/Math",          "Round to nearest integer." },
            { "floor",          "Floor",             "Nodes/Math",          "Largest integer <= input." },
            { "ceiling",        "Ceiling",           "Nodes/Math",          "Smallest integer >= input." },
            { "sqrt",           "Square Root",       "Nodes/Math",          "Square root." },
            { "power",          "Power",             "Nodes/Math",          "A raised to B." },
            { "min",            "Min",               "Nodes/Math",          "Minimum of two inputs." },
            { "max",            "Max",               "Nodes/Math",          "Maximum of two inputs." },
            { "sign",           "Sign",              "Nodes/Math",          "-1, 0, or +1 based on input." },
            { "reciprocal",     "Reciprocal",        "Nodes/Math",          "1 / A." },
            { "increment",      "Increment",         "Nodes/Math",          "Add 1 each trigger." },
            { "decrement",      "Decrement",         "Nodes/Math",          "Subtract 1 each trigger." },
            { "average",        "Average",           "Nodes/Math",          "Running average." },
            { "ranger",         "Ranger / Remap",    "Nodes/Math",          "Remaps a value from one range to another." },
            { "smooth",         "Smooth / Slew",     "Nodes/Math",          "Slews a signal to prevent clicks." },
            { "clamp",          "Clamp",             "Nodes/Math",          "Constrains a signal between min/max." },
            { "abs",            "Abs (Rectify)",     "Nodes/Math",          "Absolute value." },
            { "negate",         "Negate (Invert)",   "Nodes/Math",          "Multiplies by -1." },

            // ── Nodes / Timing & Sensors ─────────────────────────────
            { "clock",          "Clock / Timer",     "Nodes/Timing / Sensors", "Generates steady trigger pulses." },
            { "counter",        "Counter",           "Nodes/Timing / Sensors", "Counts trigger pulses." },
            { "sequencer",      "Sequencer (8-step)","Nodes/Timing / Sensors", "8-step CV sequencer." },
            { "grid_sequencer", "Grid Sequencer (8-track)", "Nodes/Timing / Sensors", "8-track, 32-step rhythmic sequencer supporting MIDI and CV outputs." },
            { "env_follower",   "Envelope Follower", "Nodes/Timing / Sensors", "Tracks the amplitude of an audio signal." },
            { "sample_hold",    "Sample & Hold",     "Nodes/Timing / Sensors", "Samples a value on trigger." },

            // ── Nodes / Scripting ────────────────────────────────────
            { "expression",     "Expression",        "Nodes/Scripting",     "ExpressionVM script node (custom DSP / logic)." },

            // ── Nodes / MIDI / Receive ───────────────────────────────
            { "midi_note",          "Note",            "Nodes/MIDI/Receive",  "Receives MIDI notes as pitch/gate." },
            { "midi_cc",            "CC",              "Nodes/MIDI/Receive",  "Receives MIDI CC values." },
            { "midi_cc14",          "CC 14-bit",       "Nodes/MIDI/Receive",  "Receives 14-bit MIDI CC values." },
            { "midi_pitchbend",     "Pitch Bend",      "Nodes/MIDI/Receive",  "Receives MIDI pitch bend." },
            { "midi_clock",         "Clock",           "Nodes/MIDI/Receive",  "Receives MIDI clock." },
            { "midi_program",       "Program Change",  "Nodes/MIDI/Receive",  "Receives MIDI program change." },
            { "midi_pressure",      "Channel Pressure","Nodes/MIDI/Receive",  "Receives MIDI channel aftertouch." },
            { "midi_poly_pressure", "Poly Pressure",   "Nodes/MIDI/Receive",  "Receives MIDI polyphonic aftertouch." },
            { "midi_song_pos",      "Song Position",   "Nodes/MIDI/Receive",  "Receives MIDI song position pointer." },
            { "midi_transport",     "Transport",       "Nodes/MIDI/Receive",  "Receives MIDI transport (start/stop/continue)." },

            // ── Nodes / MIDI / Generate ──────────────────────────────
            { "midi_note_gen",          "Note Gen",          "Nodes/MIDI/Generate", "Generates MIDI notes from CV." },
            { "midi_cc_gen",            "CC Gen",            "Nodes/MIDI/Generate", "Generates MIDI CC from CV." },
            { "midi_pitchbend_gen",     "Pitch Bend Gen",    "Nodes/MIDI/Generate", "Generates MIDI pitch bend from CV." },
            { "midi_program_gen",       "Program Change Gen","Nodes/MIDI/Generate", "Generates MIDI program change." },
            { "midi_pressure_gen",      "Pressure Gen",      "Nodes/MIDI/Generate", "Generates MIDI channel pressure." },
            { "midi_poly_pressure_gen", "Poly Pressure Gen", "Nodes/MIDI/Generate", "Generates MIDI poly pressure." },
            { "midi_transport_gen",     "Transport Gen",     "Nodes/MIDI/Generate", "Generates MIDI transport messages." },

            // ── Nodes / Displays & Gadgets ───────────────────────────
            { "disp_led",       "LED",                "Nodes/Displays/Lights",      "Single-colour LED on the pedal face." },
            { "disp_rgb_led",   "RGB LED",            "Nodes/Displays/Lights",      "RGB LED on the pedal face." },
            { "disp_indicator", "Indicator Light",    "Nodes/Displays/Lights",      "Large status indicator." },

            { "disp_display",   "Numeric Display",    "Nodes/Displays/Screens",     "Numeric value readout." },
            { "disp_7seg",      "7-Segment Display",  "Nodes/Displays/Screens",     "Digital numeric readout." },
            { "disp_text",      "Text Screen",        "Nodes/Displays/Screens",     "Text-based screen." },
            { "disp_console",   "Console Screen",     "Nodes/Displays/Screens",     "Text log output." },
            { "disp_pixel",     "Pixel Display (32x16)", "Nodes/Displays/Screens",  "Custom 32x16 addressable pixel grid." },
            { "disp_shader",    "Shader Display",     "Nodes/Displays/Screens",     "Code-driven math visualization grid." },

            { "disp_vu",        "VU Meter",           "Nodes/Displays/Instruments", "Analog-style VU level meter." },
            { "disp_scope",     "Oscilloscope",       "Nodes/Displays/Instruments", "Mini oscilloscope waveform display." },
            { "disp_tuner",     "Tuner",              "Nodes/Displays/Instruments", "Guitar tuner display." },

            { "disp_sound",     "Sound Emitter",      "Nodes/Displays",             "Emits an audio cue on trigger." },
        };
        return entries;
    }

    /** Look up an entry by its DSPGraph type string. Returns nullptr if not found. */
    static const NodeCatalogEntry* findByType (const juce::String& type)
    {
        for (const auto& e : getEntries())
            if (e.type == type) return &e;
        return nullptr;
    }
};
