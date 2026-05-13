// PedalForge — Parametric EQ
// 3-band parametric EQ with low shelf, parametric mid, high shelf.
// Parameters: Low Gain, Mid Gain, Mid Freq, Mid Q, High Gain (all in dB or Hz)

import("stdfaust.lib");

low_gain  = hslider("Low Gain", 0, -15, 15, 0.1);   // dB
mid_gain  = hslider("Mid Gain", 0, -15, 15, 0.1);   // dB
mid_freq  = hslider("Mid Freq", 1000, 100, 10000, 1); // Hz
mid_q     = hslider("Mid Q", 1.0, 0.1, 10, 0.01);
high_gain = hslider("High Gain", 0, -15, 15, 0.1);  // dB

// Shelving and peaking EQ filters from the standard library
low_shelf  = fi.low_shelf(low_gain, 300);
mid_peak   = fi.peak_eq(mid_gain, mid_freq, mid_freq / mid_q);
high_shelf = fi.high_shelf(high_gain, 3000);

// Chain the three bands
eq_chain = low_shelf : mid_peak : high_shelf;

process = eq_chain, eq_chain;
