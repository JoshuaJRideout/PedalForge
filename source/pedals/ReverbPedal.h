#pragma once

#include "../engine/FaustPedal.h"
#include "../../faust/generated/reverb.h"

/**
 * Factory wrapper for the Reverb pedal.
 */
struct ReverbPedalInfo
{
    static constexpr const char* name     = "Reverb";
    static constexpr const char* category = "Ambience";
    static constexpr int gridW = 2;
    static constexpr int gridH = 3;
    static inline juce::Colour colour { 0xFF8B5CF6 }; // Purple

    static std::unique_ptr<FaustPedal> create()
    {
        return std::make_unique<FaustPedal> (name, []() { return new reverb(); });
    }
};
