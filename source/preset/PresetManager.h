#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class PedalForgeProcessor;

//==============================================================================
/**
 * Manages save/load of full pedalboard presets.
 * Presets are stored as JSON files in ~/Documents/PedalForge/Presets/
 */
class PresetManager
{
public:
    PresetManager (PedalForgeProcessor& processor);

    /** Get the preset directory. */
    juce::File getPresetsDirectory() const;

    /** Get a list of available preset names. */
    juce::StringArray getPresetNames() const;

    /** Save the current state to a named preset. */
    void savePreset (const juce::String& name);

    /** Load a preset by name. */
    void loadPreset (const juce::String& name);

    /** Delete a preset by name. */
    void deletePreset (const juce::String& name);

    /** Serialise the current full state to a JSON string. */
    juce::String serializeState() const;

    /** Restore full state from a JSON string. */
    void restoreState (const juce::String& jsonState);

    /** Get/set the current preset name. */
    juce::String getCurrentPresetName() const { return currentPresetName; }
    void setCurrentPresetName (const juce::String& name) { currentPresetName = name; }

private:
    PedalForgeProcessor& processor;
    juce::String currentPresetName { "Init" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
