// PedalForge — Compressor
// Optical-style compressor with smooth envelope follower.
// Parameters: Threshold, Ratio, Attack, Release, Makeup Gain

import("stdfaust.lib");

threshold = hslider("Threshold", -20, -60, 0, 0.1);     // dB
ratio     = hslider("Ratio", 4, 1, 20, 0.1);
attack    = hslider("Attack", 10, 0.1, 100, 0.1);        // ms
release   = hslider("Release", 100, 10, 1000, 1);        // ms
makeup    = hslider("Makeup", 0, 0, 30, 0.1);            // dB

// Use the standard library compressor
comp = co.compressor_mono(ratio, threshold, attack / 1000.0, release / 1000.0)
       : *(ba.db2linear(makeup));

process = comp, comp;
