#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/LookAndFeel.h"
#include "ui/PedalboardGrid.h"
#include "ui/InventoryOverlay.h"
#include "ui/PedalDesignerComponent.h"
#include "ui/NodeGraphEditor.h"
#include "ui/RoutingGraphEditor.h"
#include "ui/LibraryComponent.h"
#include "ui/TuringRenderer.h"
#include "ui/MidiSettingsPanel.h"

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

    // Toolbar Tabs
    juce::Label titleLabel;
    juce::TextButton tabPlay    { "Play" };
    juce::TextButton tabBoard   { "Board" };
    juce::TextButton tabRoute   { "Route" };
    juce::TextButton tabPedal   { "Pedal" };
    juce::TextButton tabFX      { "FX" };
    juce::TextButton tabLibrary { "Library" };
    juce::TextButton tabStore   { "Store" };
    juce::TextButton tabMidi    { "MIDI" };

    class PlayTabComponent* playTab = nullptr;
    PedalDesignerComponent pedalDesigner;
    NodeGraphEditor nodeGraphEditor;
    RoutingGraphEditor* routingEditor = nullptr;  // created in constructor (needs engine ref)
    LibraryComponent libraryView;    // Will become the full asset manager
    MidiSettingsPanel midiSettingsPanel;

    // Q-menu style inventory overlay
    InventoryOverlay inventory;

    // Cross-tab state: currently selected pedal from the pedalboard
    PedalInstance* activePedal = nullptr;

    // Library selection target: which DSP node ID inside the pedal should receive the file
    int libraryTargetNodeID = -1;
    
    std::unique_ptr<TuringRenderer> turingRenderer;

    static constexpr int toolbarHeight = 44;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalForgeEditor)
};
