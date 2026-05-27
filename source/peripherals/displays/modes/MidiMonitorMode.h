#pragma once

#include "../DisplayMode.h"
#include "../../../engine/AudioGraphEngine.h"

//==============================================================================
/**
 * Scrolling live feed of recent MIDI events. Subscribes to
 * AudioGraphEngine::getMidiMonitorEvents() at render time.
 *
 * Layout: a header band at the top with title + event count, then a
 * stack of recent events (newest at top), type-coloured.
 *
 * Designed primarily for landscape orientation but works at any aspect.
 */
class MidiMonitorMode : public DisplayMode
{
public:
    juce::String getModeID()          const override { return "midi_monitor"; }
    juce::String getModeDisplayName() const override { return "MIDI Monitor"; }
    int          getPreferredFPS()    const override { return 15; }
    AspectHint   getPreferredAspect() const override { return AspectHint::Any; }

    void paint (juce::Graphics& g) override;

private:
    static juce::Colour colourForMessage (const juce::MidiMessage& m);
    static juce::String describeMessage  (const juce::MidiMessage& m);
};
