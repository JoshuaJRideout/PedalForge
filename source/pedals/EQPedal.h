#pragma once

#include "../engine/FaustPedal.h"
#include "../../faust/generated/eq.h"

/**
 * Factory wrapper for the Parametric EQ pedal.
 */
struct EQPedalInfo
{
    static constexpr const char* name     = "Parametric EQ";
    static constexpr const char* category = "EQ";
    static constexpr int gridW = 3;
    static constexpr int gridH = 3;
    static inline juce::Colour colour { 0xFF10B981 }; // Green

    static std::unique_ptr<FaustPedal> create()
    {
        return std::make_unique<FaustPedal> (name, []() { return new eq(); });
    }
};
