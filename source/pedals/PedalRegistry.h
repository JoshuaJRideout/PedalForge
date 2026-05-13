#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../engine/FaustPedal.h"
#include "../dsp/GraphPedalProcessor.h"
#include "OverdrivePedal.h"
#include "DelayPedal.h"
#include "ReverbPedal.h"
#include "EQPedal.h"
#include "CompressorPedal.h"
#include <functional>
#include <vector>

//==============================================================================
/**
 * Registry of all available pedal types.
 * Used by the palette UI and serialisation to create pedals by name.
 *
 * Supports both FaustPedal (from .dsp files) and GraphPedalProcessor
 * (from the node graph builder) through a unified AudioProcessor factory.
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
        // ─── FAUST-based pedals ──────────────────────────────────────
        { OverdrivePedalInfo::name,   OverdrivePedalInfo::category,
          1, 2, 3,
          OverdrivePedalInfo::colour, &OverdrivePedalInfo::create },

        { DelayPedalInfo::name,   DelayPedalInfo::category,
          1, 2, 4,
          DelayPedalInfo::colour, &DelayPedalInfo::create },

        { ReverbPedalInfo::name,   ReverbPedalInfo::category,
          1, 2, 4,
          ReverbPedalInfo::colour, &ReverbPedalInfo::create },

        { EQPedalInfo::name,   EQPedalInfo::category,
          2, 2, 5,
          EQPedalInfo::colour, &EQPedalInfo::create },

        { CompressorPedalInfo::name,   CompressorPedalInfo::category,
          1, 2, 5,
          CompressorPedalInfo::colour, &CompressorPedalInfo::create },

        // ─── Graph-based pedals (node builder) ───────────────────────
        { "Clean Boost",  "Boost",      1, 2, 1,
          juce::Colour (0xFF4ADE80),    // green
          [] { return GraphPedalFactory::createCleanBoost(); } },

        { "Distortion",   "Drive",      1, 2, 5,
          juce::Colour (0xFFF97316),    // orange
          [] { return GraphPedalFactory::createDistortion(); } },

        { "Graph Delay",  "Delay",      1, 2, 3,
          juce::Colour (0xFF38BDF8),    // blue
          [] { return GraphPedalFactory::createDelay(); } },

        { "Chorus",       "Modulation", 1, 2, 4,
          juce::Colour (0xFFA78BFA),    // purple
          [] { return GraphPedalFactory::createChorus(); } },

        { "Graph Reverb", "Reverb",     1, 2, 3,
          juce::Colour (0xFF22D3EE),    // cyan
          [] { return GraphPedalFactory::createReverb(); } },

        { "Graph Comp",   "Dynamics",   1, 2, 4,
          juce::Colour (0xFFFBBF24),    // yellow
          [] { return GraphPedalFactory::createCompressor(); } },

        { "Noise Gate",   "Dynamics",   1, 2, 3,
          juce::Colour (0xFF6B7280),    // grey
          [] { return GraphPedalFactory::createNoiseGate(); } },

        { "Tremolo",      "Modulation", 1, 2, 2,
          juce::Colour (0xFFFB7185),    // pink
          [] { return GraphPedalFactory::createTremolo(); } },
    };
}
