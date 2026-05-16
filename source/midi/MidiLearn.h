#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>

class AudioGraphEngine;

//==============================================================================
/**
 * MIDI Learn manager — allows mapping MIDI CC messages to pedal parameters.
 *
 * Usage flow:
 *   1. User right-clicks a parameter → calls startLearning(paramId)
 *   2. User moves a CC on their controller
 *   3. processMidi() captures the CC and creates the mapping
 *   4. Subsequent CC messages update the mapped parameter
 */
class MidiLearnManager
{
public:
    MidiLearnManager (AudioGraphEngine& engine);

    /** Start learning mode for a parameter. */
    void startLearning (const juce::String& paramId);

    /** Cancel learning mode. */
    void cancelLearning();

    /** Is currently in learning mode? */
    bool isLearning() const { return learning; }

    /** The parameter currently being learned. */
    juce::String getLearningParamId() const { return learningParamId; }

    /** Remove a MIDI mapping for a parameter. */
    void removeMapping (const juce::String& paramId);

    /** Clear all MIDI mappings. */
    void clearAllMappings();

    /** Process incoming MIDI (called from processBlock). */
    void processMidi (const juce::MidiBuffer& midiBuffer);

    /** Get the CC number mapped to a parameter, or -1 if none. */
    int getMappedCC (const juce::String& paramId) const;

    //==========================================================================
    struct MidiMapping
    {
        int ccNumber;
        int channel; // 0 = omni
        juce::String paramId;

        // MIDI Pickup (Catch-up) mode state
        bool isLatched = false;
        float lastPhysicalValue = -1.0f;
        float lastVirtualValue = -1.0f;
    };

    const std::map<juce::String, MidiMapping>& getMappings() const { return mappings; }

private:
    AudioGraphEngine& targetEngine;

    bool learning = false;
    juce::String learningParamId;

    // paramId → mapping
    std::map<juce::String, MidiMapping> mappings;

    // CC+channel → paramId (reverse lookup for fast dispatch)
    std::map<int, juce::String> ccToParam;

    void applyCC (int ccNumber, int channel, float value);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnManager)
};
