// PedalForge — Delay
// Stereo delay with modulation and feedback.
// Parameters: Time (10-2000 ms), Feedback (0-0.95), Mix (0-1), Mod Depth (0-10 ms)

import("stdfaust.lib");

time_ms   = hslider("Time", 400, 10, 2000, 1);
feedback  = hslider("Feedback", 0.4, 0, 0.95, 0.01);
mix       = hslider("Mix", 0.3, 0, 1, 0.01);
mod_depth = hslider("Mod Depth", 2, 0, 10, 0.1);

// Convert ms to samples
time_samps = time_ms * ma.SR / 1000.0;

// LFO for modulation
lfo = os.osc(0.5) * mod_depth * ma.SR / 1000.0;

// Modulated delay with feedback (using FAUST's tilde feedback operator)
delay_channel = + ~ (de.fdelay(192000, max(1, time_samps + lfo)) : *(feedback));

// Wet/dry mix for each channel
process = par(i, 2, _ <: _*(1-mix), delay_channel*mix :> _);
