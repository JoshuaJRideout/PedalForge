// PedalForge — Reverb
// Plate-style reverb using Freeverb algorithm.
// Parameters: Size (0-1), Decay (0-0.99), Damping (0-1), Mix (0-1)

import("stdfaust.lib");

size    = hslider("Size", 0.5, 0, 1, 0.01);
decay   = hslider("Decay", 0.6, 0, 0.99, 0.01);
damping = hslider("Damping", 0.5, 0, 1, 0.01);
mix     = hslider("Mix", 0.3, 0, 1, 0.01);

// Use the standard library reverb
reverb_algo = re.mono_freeverb(size, decay, damping);

// Stereo reverb: process each channel, then cross-mix for width
channel_l(l, r) = reverb_algo(l * 0.7 + r * 0.3);
channel_r(l, r) = reverb_algo(l * 0.3 + r * 0.7);

process(l, r) = l_out, r_out
with {
    wet_l = channel_l(l, r);
    wet_r = channel_r(l, r);
    l_out = l * (1 - mix) + wet_l * mix;
    r_out = r * (1 - mix) + wet_r * mix;
};
