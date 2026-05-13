#pragma once

#include "../engine/FaustPedal.h"
#include "../../faust/generated/overdrive.h"

/**
 * Factory wrapper for the Overdrive pedal.
 * Provides metadata and creates a FaustPedal from the generated FAUST code.
 */
struct OverdrivePedalInfo
{
    static constexpr const char* name     = "Overdrive";
    static constexpr const char* category = "Drive";
    static constexpr int gridW = 2;
    static constexpr int gridH = 3;
    static inline juce::Colour colour { 0xFFE85D2C }; // Warm orange

    static std::unique_ptr<FaustPedal> create()
    {
        return std::make_unique<FaustPedal> (name, []() { return new overdrive(); });
    }
};
