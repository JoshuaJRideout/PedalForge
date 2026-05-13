#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../preset/PresetManager.h"

//==============================================================================
/**
 * Toolbar preset browser — dropdown for selecting presets,
 * plus save/delete buttons.
 */
class PresetBrowser : public juce::Component,
                      public juce::ComboBox::Listener,
                      public juce::Button::Listener
{
public:
    PresetBrowser (PresetManager& manager);

    void paint (juce::Graphics& g) override;
    void resized() override;

    void comboBoxChanged (juce::ComboBox* box) override;
    void buttonClicked (juce::Button* button) override;

    /** Refresh the preset list. */
    void refreshList();

private:
    PresetManager& manager;

    juce::ComboBox presetSelector;
    juce::TextButton saveButton   { "Save" };
    juce::TextButton deleteButton { "×" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};
