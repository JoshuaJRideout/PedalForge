#include "PresetManager.h"
#include "../PluginProcessor.h"
#include "../util/AppPaths.h"

//==============================================================================
PresetManager::PresetManager (PedalForgeProcessor& proc)
    : processor (proc)
{
    // Presets now live under the app data root (~/Library/.../PedalForge).
    // Older builds used ~/Documents/PedalForge/Presets, but touching
    // ~/Documents triggers a "would like to access your Documents folder"
    // TCC prompt (task #66 / #63). We deliberately do NOT scan the old
    // location at launch — that would re-introduce the very prompt we're
    // removing, even for fresh installs. Any pre-existing presets stay on
    // disk in Documents; relocate them manually if needed.
}

//==============================================================================
juce::File PresetManager::getPresetsDirectory() const
{
    return pf::paths::getPresetsDir();   // ~/Library/.../PedalForge/Presets
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
