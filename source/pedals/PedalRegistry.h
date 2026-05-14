#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/GraphPedalProcessor.h"
#include <functional>
#include <vector>

//==============================================================================
/**
 * Registry of all available factory pedal types.
 * Used by the palette UI to populate the pedalboard menu.
 */
struct PedalInfo
{
    juce::String name;
    juce::String category;
    int gridW, gridH;
    int numKnobs;
    juce::Colour colour;
    std::function<std::unique_ptr<juce::AudioProcessor>()> factory;
};

inline std::vector<PedalInfo> getFactoryPedals()
{
    return {
        // ─── DRIVE ─────────────────────────────────────────────────────────
        { "Clean Boost",  "Drive",      1, 2, 1,
          juce::Colour (0xFF4ADE80),    // green
          [] { return GraphPedalFactory::createCleanBoost(); } },

        { "Overdrive",    "Drive",      1, 2, 3,
          juce::Colour (0xFFFBBF24),    // yellow/orange
          [] { return GraphPedalFactory::createOverdrive(); } },

        { "Distortion",   "Drive",      1, 2, 3,
          juce::Colour (0xFFF97316),    // orange
          [] { return GraphPedalFactory::createDistortion(); } },

        { "Fuzz",         "Drive",      1, 2, 3,
          juce::Colour (0xFFDC2626),    // red
          [] { return GraphPedalFactory::createFuzz(); } },

        // ─── MODULATION ────────────────────────────────────────────────────
        { "Chorus",       "Modulation", 1, 2, 4,
          juce::Colour (0xFFA78BFA),    // purple
          [] { return GraphPedalFactory::createChorus(); } },

        { "Phaser",       "Modulation", 1, 2, 3,
          juce::Colour (0xFFD946EF),    // fuchsia
          [] { return GraphPedalFactory::createPhaser(); } },

        { "Flanger",      "Modulation", 1, 2, 4,
          juce::Colour (0xFFEC4899),    // pink
          [] { return GraphPedalFactory::createFlanger(); } },

        { "Tremolo",      "Modulation", 1, 2, 2,
          juce::Colour (0xFFFB7185),    // light pink
          [] { return GraphPedalFactory::createTremolo(); } },

        // ─── TIME & DYNAMICS ───────────────────────────────────────────────
        { "Delay",        "Time",       1, 2, 3,
          juce::Colour (0xFF38BDF8),    // blue
          [] { return GraphPedalFactory::createDelay(); } },

        { "Reverb",       "Time",       1, 2, 3,
          juce::Colour (0xFF22D3EE),    // cyan
          [] { return GraphPedalFactory::createReverb(); } },

        { "Compressor",   "Dynamics",   1, 2, 2,
          juce::Colour (0xFF34D399),    // emerald
          [] { return GraphPedalFactory::createCompressor(); } },

        { "Noise Gate",   "Dynamics",   1, 2, 1,
          juce::Colour (0xFF9CA3AF),    // gray
          [] { return GraphPedalFactory::createNoiseGate(); } },

        // ─── EQ / FILTER / UTILITY ─────────────────────────────────────────
        { "Parametric EQ","EQ",         2, 2, 9,
          juce::Colour (0xFF60A5FA),    // light blue
          [] { return GraphPedalFactory::createParametricEQ(); } },

        { "Tone Control", "EQ",         1, 2, 1,
          juce::Colour (0xFF818CF8),    // indigo
          [] { return GraphPedalFactory::createToneControl(); } },

        { "Cabinet Sim",  "Utility",    1, 2, 2,
          juce::Colour (0xFFFCD34D),    // amber
          [] { return GraphPedalFactory::createCabinetSim(); } },
    };
}
