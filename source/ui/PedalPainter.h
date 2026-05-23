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
    /** Paint a pedal using its PedalDesign layout. If design is null, draws a fallback "NO DESIGN" box. */
    void paintDesign (juce::Graphics& g, juce::Rectangle<float> bounds,
                      const PedalDesign* design,
                      const std::map<juce::String, float>& controlValues,
                      const std::map<juce::String, juce::String>& controlTexts,
                      const std::map<juce::String, std::vector<float>>& controlData,
                      bool bypassed = false, float alpha = 1.0f);
}
