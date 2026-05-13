#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"
#include "../dsp/PedalDesign.h"

//==============================================================================
/**
 * Shared pedal painting utility.
 * Draws a realistic guitar-pedal enclosure at any size:
 *   - Metal body with gradient, edge bevel, and shadow
 *   - Decorative knobs (drawn, not interactive)
 *   - Stomp footswitch at the bottom
 *   - Bypass LED
 *   - Name plate
 *   - Input/output jacks on the sides
 *
 * Used by PedalPalette, PedalComponent, and PedalDetailPanel
 * so the pedal always looks the same — just scaled.
 */
namespace PedalPainter
{
    struct PedalVisual
    {
        juce::String name;
        juce::String category;
        juce::Colour colour;
        bool bypassed = false;
        int  numKnobs = 3;

        // Optional: normalised knob values [0..1] for drawing indicator dots
        std::vector<float> knobValues;
    };

    /** Paint a guitar pedal into the given bounds. */
    void paint (juce::Graphics& g, juce::Rectangle<float> bounds,
                const PedalVisual& visual, float alpha = 1.0f);

    /** Paint a pedal using its PedalDesign layout (custom designed pedal). */
    void paintDesign (juce::Graphics& g, juce::Rectangle<float> bounds,
                      const PedalDesign& design,
                      const std::map<juce::String, float>& controlValues,
                      bool bypassed, float alpha = 1.0f);
}
