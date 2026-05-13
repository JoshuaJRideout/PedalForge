#include "PresetBrowser.h"
#include "../ui/LookAndFeel.h"

//==============================================================================
PresetBrowser::PresetBrowser (PresetManager& mgr)
    : manager (mgr)
{
    addAndMakeVisible (presetSelector);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (deleteButton);

    presetSelector.addListener (this);
    saveButton.addListener (this);
    deleteButton.addListener (this);

    presetSelector.setTextWhenNothingSelected ("No Preset");

    refreshList();
}

//==============================================================================
void PresetBrowser::paint (juce::Graphics& /*g*/)
{
}

void PresetBrowser::resized()
{
    auto bounds = getLocalBounds().reduced (4, 2);

    deleteButton.setBounds (bounds.removeFromRight (30));
    bounds.removeFromRight (4);
    saveButton.setBounds (bounds.removeFromRight (60));
    bounds.removeFromRight (4);
    presetSelector.setBounds (bounds);
}

//==============================================================================
void PresetBrowser::comboBoxChanged (juce::ComboBox* box)
{
    if (box == &presetSelector)
    {
        auto name = presetSelector.getText();
        if (name.isNotEmpty())
            manager.loadPreset (name);
    }
}

void PresetBrowser::buttonClicked (juce::Button* button)
{
    if (button == &saveButton)
    {
        // Show save dialog
        auto name = manager.getCurrentPresetName();

        auto* editor = new juce::AlertWindow ("Save Preset", "Enter preset name:",
                                               juce::MessageBoxIconType::NoIcon);
        editor->addTextEditor ("name", name, "Name:");
        editor->addButton ("Save", 1);
        editor->addButton ("Cancel", 0);

        editor->enterModalState (true,
            juce::ModalCallbackFunction::create ([this, editor] (int result)
            {
                if (result == 1)
                {
                    auto presetName = editor->getTextEditorContents ("name");
                    if (presetName.isNotEmpty())
                    {
                        manager.savePreset (presetName);
                        refreshList();
                    }
                }
                delete editor;
            }));
    }
    else if (button == &deleteButton)
    {
        auto name = manager.getCurrentPresetName();
        if (name.isNotEmpty() && name != "Init")
        {
            manager.deletePreset (name);
            refreshList();
        }
    }
}

//==============================================================================
void PresetBrowser::refreshList()
{
    presetSelector.clear (juce::dontSendNotification);

    auto names = manager.getPresetNames();
    int id = 1;
    for (auto& name : names)
        presetSelector.addItem (name, id++);

    auto current = manager.getCurrentPresetName();
    presetSelector.setText (current, juce::dontSendNotification);
}
