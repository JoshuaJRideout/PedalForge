#pragma once

#include "../engine/FaustPedal.h"
#include "../../faust/generated/delay.h"

/**
 * Factory wrapper for the Delay pedal.
 */
struct DelayPedalInfo
{
    static constexpr const char* name     = "Delay";
    static constexpr const char* category = "Time";
    static constexpr int gridW = 2;
    static constexpr int gridH = 3;
    static inline juce::Colour colour { 0xFF3B82F6 }; // Blue

    static std::unique_ptr<FaustPedal> create()
    {
        return std::make_unique<FaustPedal> (name, []() { return new delay(); });
    }
};
