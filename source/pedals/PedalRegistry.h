#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/GraphPedalProcessor.h"
#include "FactoryDesigns.h"
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
    std::function<std::shared_ptr<PedalDesign>()> designFactory;
};

inline std::vector<PedalInfo> getFactoryPedals()
{
    std::vector<PedalInfo> pedals = {
        // ─── MIDI & CV ─────────────────────────────────────────────────────
        { "Step Sequencer", "MIDI & CV", 2, 2, 6,
          juce::Colour (0xFF1A0533),    // deep indigo
          [] { return GraphPedalFactory::createStepSequencer(); },
          [] { return FactoryDesigns::createStepSequencer(); } },

        // ─── DRIVE ─────────────────────────────────────────────────────────
        { "Clean Boost",  "Drive",      1, 2, 1,
          juce::Colour (0xFF4ADE80),    // green
          [] { return GraphPedalFactory::createCleanBoost(); },
          [] { return FactoryDesigns::createCleanBoost(); } },

        { "Overdrive",    "Drive",      1, 2, 3,
          juce::Colour (0xFFFBBF24),    // yellow/orange
          [] { return GraphPedalFactory::createOverdrive(); },
          [] { return FactoryDesigns::createOverdrive(); } },

        { "Distortion",   "Drive",      1, 2, 3,
          juce::Colour (0xFFF97316),    // orange
          [] { return GraphPedalFactory::createDistortion(); },
          [] { return FactoryDesigns::createDistortion(); } },

        { "Fuzz",         "Drive",      1, 2, 3,
          juce::Colour (0xFFDC2626),    // red
          [] { return GraphPedalFactory::createFuzz(); },
          [] { return FactoryDesigns::createFuzz(); } },

        // ─── MODULATION ────────────────────────────────────────────────────
        { "Chorus",       "Modulation", 1, 2, 4,
          juce::Colour (0xFFA78BFA),    // purple
          [] { return GraphPedalFactory::createChorus(); },
          [] { return FactoryDesigns::createChorus(); } },

        { "Phaser",       "Modulation", 1, 2, 3,
          juce::Colour (0xFFD946EF),    // fuchsia
          [] { return GraphPedalFactory::createPhaser(); },
          [] { return FactoryDesigns::createPhaser(); } },

        { "Flanger",      "Modulation", 1, 2, 4,
          juce::Colour (0xFFEC4899),    // pink
          [] { return GraphPedalFactory::createFlanger(); },
          [] { return FactoryDesigns::createFlanger(); } },

        { "Tremolo",      "Modulation", 1, 2, 2,
          juce::Colour (0xFFFB7185),    // light pink
          [] { return GraphPedalFactory::createTremolo(); },
          [] { return FactoryDesigns::createTremolo(); } },

        // ─── TIME & DYNAMICS ───────────────────────────────────────────────
        { "Delay",        "Time",       1, 2, 3,
          juce::Colour (0xFF38BDF8),    // blue
          [] { return GraphPedalFactory::createDelay(); },
          [] { return FactoryDesigns::createDelay(); } },

        { "Reverb",       "Time",       1, 2, 3,
          juce::Colour (0xFF22D3EE),    // cyan
          [] { return GraphPedalFactory::createReverb(); },
          [] { return FactoryDesigns::createReverb(); } },

        { "Compressor",   "Dynamics",   1, 2, 2,
          juce::Colour (0xFF34D399),    // emerald
          [] { return GraphPedalFactory::createCompressor(); },
          [] { return FactoryDesigns::createCompressor(); } },

        { "Noise Gate",   "Dynamics",   1, 2, 1,
          juce::Colour (0xFF9CA3AF),    // gray
          [] { return GraphPedalFactory::createNoiseGate(); },
          [] { return FactoryDesigns::createNoiseGate(); } },

        // ─── EQ / FILTER / UTILITY ─────────────────────────────────────────
        { "Parametric EQ","EQ",         2, 2, 9,
          juce::Colour (0xFF60A5FA),    // light blue
          [] { return GraphPedalFactory::createParametricEQ(); },
          [] { return FactoryDesigns::createParametricEQ(); } },

        { "Tone Control", "EQ",         1, 2, 1,
          juce::Colour (0xFF818CF8),    // indigo
          [] { return GraphPedalFactory::createToneControl(); },
          [] { return FactoryDesigns::createToneControl(); } },

        { "Cabinet Sim",  "Utility",    1, 2, 2,
          juce::Colour (0xFFFCD34D),    // amber
          [] { return GraphPedalFactory::createCabinetSim(); },
          [] { return FactoryDesigns::createCabinetSim(); } },

        { "NAM Amp",      "Amp Sim",    1, 2, 2,
          juce::Colour (0xFF333333),    // dark grey
          [] { return GraphPedalFactory::createNAMAmp(); },
          [] { return FactoryDesigns::createNAMAmp(); } },

        { "IR Cabinet",   "Amp/Cab",    1, 2, 2,
          juce::Colour (0xff555555),
          [] { return GraphPedalFactory::createIRLoader(); },
          [] { return FactoryDesigns::createIRLoader(); } },

        { "IR Reverb",    "Reverb",     1, 2, 2,
          juce::Colour (0xff4a5d85),
          [] { return GraphPedalFactory::createIRReverb(); },
          [] { return FactoryDesigns::createIRReverb(); } }
    };
    return pedals;
}
