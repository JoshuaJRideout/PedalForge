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
    juce::TextButton tabBoard   { "Board" };
    juce::TextButton tabRoute   { "Route" };
    juce::TextButton tabForge   { "Forge" };
    juce::TextButton tabFX      { "FX" };
    juce::TextButton tabLibrary { "Library" };
    juce::TextButton tabStore   { "Store" };

    PedalDesignerComponent pedalDesigner;
    NodeGraphEditor nodeGraphEditor;

    // Placeholder components for new tabs
    juce::Component routeView;      // Will become the routing graph
    juce::Component libraryView;    // Will become the full asset manager

    // Q-menu style inventory overlay
    InventoryOverlay inventory;

    // Cross-tab state: currently selected pedal from the pedalboard
    PedalInstance* activePedal = nullptr;

    static constexpr int toolbarHeight = 44;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalForgeEditor)
};
