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
#include "peripherals/displays/DisplayManager.h"
#include "peripherals/displays/TuringDisplay.h"
#include "ui/ToastOverlay.h"
#include "ui/AudioStatusBar.h"
#include "ui/AiAssistantPanel.h"
#include "ui/ScriptingTabComponent.h"
#include "ui/WikiTabComponent.h"
#include "ai/ToolHost.h"

class PedalForgeProcessor;

//==============================================================================
class PedalForgeEditor : public juce::AudioProcessorEditor,
                         public juce::DragAndDropContainer,
                         public juce::FileDragAndDropTarget,
                         public juce::Button::Listener,
                         public juce::KeyListener,
                         public pf::ai::ToolHost
{
public:
    explicit PedalForgeEditor (PedalForgeProcessor& processor);
    ~PedalForgeEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void buttonClicked (juce::Button* button) override;

    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    // FileDragAndDropTarget — accept .pfpedal files dragged from Finder.
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    void triggerUndo();
    void triggerRedo();
    void commitActiveTabState();
    void refreshAfterUndoRedo();

    //==========================================================================
    // pf::ai::ToolHost — lets the in-app AI assistant read/modify app state.
    juce::String readActiveTab() override;
    juce::String listPedals() override;
    juce::String readPedalDesign (const juce::String& uuid) override;
    bool writePedalDesign (const juce::String& uuid, const juce::String& json, juce::String& errorOut) override;
    juce::String readFxGraph (const juce::String& pedalUuid) override;
    bool writeFxGraph (const juce::String& pedalUuid, const juce::String& json, juce::String& errorOut) override;
    void showToast (const juce::String& message) override;
    juce::String listFactoryPedals() override;
    juce::String addPedalToBoard (const juce::String& pedalId, juce::String& errorOut) override;
    juce::String createBlankPedal (const juce::String& name, juce::String& errorOut) override;
    juce::String getScriptApiReference() override;
    juce::String runBoardScript (const juce::String& source) override;
    juce::String runPedalScript (const juce::String& pedalUuid, const juce::String& source) override;
    juce::String runFxScript (const juce::String& pedalUuid, const juce::String& source) override;
    juce::String runDspScript (const juce::String& pedalUuid, const juce::String& source) override;
    juce::String readBoardAsScript() override;
    juce::String readPedalAsScript (const juce::String& pedalUuid) override;
    juce::String readFxAsScript (const juce::String& pedalUuid) override;
    juce::String verifyPedal (const juce::String& pedalUuid) override;
    juce::String probePedal (const juce::String& pedalUuid) override;
    juce::String captureView (const juce::String& target) override;
    juce::String listPlayPresets() override;
    juce::String loadPlayPreset (const juce::String& name) override;
    juce::String readPlayChain() override;
    juce::String playAddPedal (const juce::String& pedalName) override;
    juce::String playClear() override;
    juce::String readRouting() override;
    juce::String connectPedals (const juce::String& fromUuid, const juce::String& toUuid) override;
    juce::String disconnectPedals (const juce::String& fromUuid, const juce::String& toUuid) override;
    juce::String listMidiMappings() override;
    juce::String listPedalParams (const juce::String& pedalUuid) override;
    juce::String mapMidiCc (const juce::String& param, int cc, int channel) override;
    juce::String removeMidiMapping (const juce::String& param) override;
    juce::String clearMidiMappings() override;
    juce::String switchTab (const juce::String& tabName) override;
    juce::String listAssets (const juce::String& category) override;
    juce::String listWikiPages() override;
    juce::String readWikiPage (const juce::String& pageId) override;
    juce::String openWikiPage (const juce::String& pageId) override;


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
    juce::TextButton tabScript  { "Script" };
    juce::TextButton tabWiki    { "Wiki" };
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
    ScriptingTabComponent scriptingTab;
    WikiTabComponent wikiTab;

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

    // New secondary-display subsystem. Drives the Turing 3.5" V2 via real
    // USB-serial protocol; future displays attach to the same manager.
    std::unique_ptr<DisplayManager> displayManager;

    // Non-modal toasts overlay (lower-right). Singleton-routed via
    // pf::toastInfo / toastWarn / toastError from anywhere in the app.
    ToastOverlay toastOverlay;

    // Bottom status bar — I/O meters, master volume, mute, device info.
    std::unique_ptr<AudioStatusBar> audioStatusBar;
    static constexpr int audioStatusBarHeight = 30;

    // AI assistant panel — sits just above the audio status bar; collapsed
    // it's a single input line, expanded it's a chat surface (#64).
    AiAssistantPanel aiPanel { *this };

    // Find a live pedal instance by its design uuid (for the AI ToolHost).
    PedalInstance* findInstanceByUuid (const juce::String& uuid);

    // Resolve a routing endpoint (board-pedal uuid, or "input"/"output") to a
    // graph node id for the Route tools. Returns false if not on the Board.
    bool resolveRoutingNode (const juce::String& token,
                             juce::AudioProcessorGraph::NodeID& outId,
                             juce::String& outName);

    static constexpr int toolbarHeight = 44;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalForgeEditor)
};
