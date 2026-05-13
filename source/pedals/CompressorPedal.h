#pragma once

#include "../engine/FaustPedal.h"
#include "../../faust/generated/compressor.h"

/**
 * Factory wrapper for the Compressor pedal.
 */
struct CompressorPedalInfo
{
    static constexpr const char* name     = "Compressor";
    static constexpr const char* category = "Dynamics";
    static constexpr int gridW = 2;
    static constexpr int gridH = 3;
    static inline juce::Colour colour { 0xFFF59E0B }; // Amber

    static std::unique_ptr<FaustPedal> create()
    {
        return std::make_unique<FaustPedal> (name, []() { return new compressor(); });
    }
};
