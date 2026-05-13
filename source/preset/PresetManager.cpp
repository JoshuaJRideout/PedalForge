#include "PresetManager.h"
#include "../PluginProcessor.h"

//==============================================================================
PresetManager::PresetManager (PedalForgeProcessor& proc)
    : processor (proc)
{
}

//==============================================================================
juce::File PresetManager::getPresetsDirectory() const
{
    auto dir = juce::File::getSpecialLocation (
                   juce::File::userDocumentsDirectory)
                   .getChildFile ("PedalForge")
                   .getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

juce::StringArray PresetManager::getPresetNames() const
{
    juce::StringArray names;
    auto dir = getPresetsDirectory();

    for (auto& file : dir.findChildFiles (juce::File::findFiles, false, "*.json"))
        names.add (file.getFileNameWithoutExtension());

    names.sort (true);
    return names;
}

//==============================================================================
void PresetManager::savePreset (const juce::String& name)
{
    auto file = getPresetsDirectory().getChildFile (name + ".json");
    auto state = serializeState();
    file.replaceWithText (state);
    currentPresetName = name;
}

void PresetManager::loadPreset (const juce::String& name)
{
    auto file = getPresetsDirectory().getChildFile (name + ".json");

    if (file.existsAsFile())
    {
        auto state = file.loadFileAsString();
        restoreState (state);
        currentPresetName = name;
    }
}

void PresetManager::deletePreset (const juce::String& name)
{
    auto file = getPresetsDirectory().getChildFile (name + ".json");
    file.deleteFile();
}

//==============================================================================
juce::String PresetManager::serializeState() const
{
    return processor.getGraphEngine().serialise();
}

void PresetManager::restoreState (const juce::String& jsonState)
{
    processor.getGraphEngine().deserialise (jsonState);
}
