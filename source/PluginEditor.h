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
#include "ui/LibraryOverlay.h"
#include "ui/CanvasOverlay.h"
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

   #if JucePlugin_Build_Standalone
    juce::TextButton btnSettings { "Options" };
   #endif
    juce::TextButton btnTestSound { "Test Sound" };

    class PlayTabComponent* playTab = nullptr;
    PedalDesignerComponent pedalDesigner;
    NodeGraphEditor nodeGraphEditor;
    RoutingGraphEditor* routingEditor = nullptr;  // created in constructor (needs engine ref)
    LibraryComponent libraryView;    // Will become the full asset manager
    MidiSettingsPanel midiSettingsPanel;

    // Q-menu style inventory overlay
    InventoryOverlay inventory;
    
    // File picker overlay
    LibraryOverlay libraryOverlay;

    // Advanced modular pedal interface overlay
    CanvasOverlay canvasOverlay;

    // Cross-tab state: currently selected pedal from the pedalboard
    PedalInstance* activePedal = nullptr;

    // Library overlay: we store the current callback to execute when a file is chosen
    std::function<void(const juce::File&)> activeFileCallback;
    
    std::unique_ptr<TuringRenderer> turingRenderer;

    static constexpr int toolbarHeight = 44;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalForgeEditor)
};
