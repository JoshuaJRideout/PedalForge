// PedalForge — Overdrive
// Tube-style overdrive with soft clipping, tone stack, and output level.
// Parameters: Drive (1-100), Tone (20-20000 Hz), Level (0-1)

import("stdfaust.lib");

drive = hslider("Drive", 50, 1, 100, 0.1);
tone  = hslider("Tone", 4000, 200, 20000, 1);
level = hslider("Level", 0.5, 0, 1, 0.01);

// Soft-clip transfer function (tanh-based tube saturation)
softclip(x) = ma.tanh(x * gain)
with {
    gain = 1.0 + (drive / 10.0);
};

// One-pole lowpass for tone control
toneLPF = fi.lowpass(2, tone);

// Per-channel processing
channel = softclip : toneLPF : *(level);

process = channel, channel;
