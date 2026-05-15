#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/LookAndFeel.h"
#include "ui/PedalboardGrid.h"
#include "ui/InventoryOverlay.h"
#include "ui/PedalDesignerComponent.h"
#include "ui/NodeGraphEditor.h"
#include "preset/PresetBrowser.h"

class PedalForgeProcessor;

//==============================================================================
class PedalForgeEditor : public juce::AudioProcessorEditor,
                         public juce::DragAndDropContainer,
                         public juce::Button::Listener
{
public:
    explicit PedalForgeEditor (PedalForgeProcessor& processor);
    ~PedalForgeEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void buttonClicked (juce::Button* button) override;

private:
    PedalForgeProcessor& processorRef;

    PedalForgeLookAndFeel lookAndFeel;
    PedalboardGrid grid;
    PresetBrowser presetBrowser;

    // Toolbar Tabs
    juce::Label titleLabel;
    juce::TextButton tabPedalboard { "Pedalboard" };
    juce::TextButton tabForge { "The Forge" };
    juce::TextButton tabEffects { "Effects" };
    juce::TextButton tabStore { "Pedal Store" };

    PedalDesignerComponent pedalDesigner;
    NodeGraphEditor nodeGraphEditor;

    // Q-menu style inventory overlay (replaces left sidebar)
    InventoryOverlay inventory;

    // Cross-tab state: currently selected pedal from the pedalboard
    PedalInstance* activePedal = nullptr;

    static constexpr int toolbarHeight = 44;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalForgeEditor)
};
