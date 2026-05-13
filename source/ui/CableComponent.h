#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
 * Visual cable connecting two pedals.
 * Renders as a Bézier curve with droop and optional signal flow animation.
 */
class CableComponent : public juce::Component,
                       public juce::Timer
{
public:
    CableComponent (juce::Colour colour = juce::Colour (0xFFCBD5E1));

    void paint (juce::Graphics& g) override;
    bool hitTest (int x, int y) override;

    void timerCallback() override;

    /** Update the cable endpoints (in parent coordinates). */
    void setEndpoints (juce::Point<float> start, juce::Point<float> end);

    /** Set cable colour. */
    void setCableColour (juce::Colour c) { cableColour = c; repaint(); }

    /** Enable/disable signal flow animation. */
    void setAnimated (bool shouldAnimate);

private:
    juce::Point<float> startPoint, endPoint;
    juce::Colour cableColour;

    float animationPhase = 0.0f;
    bool animated = true;

    juce::Path getCablePath() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CableComponent)
};
