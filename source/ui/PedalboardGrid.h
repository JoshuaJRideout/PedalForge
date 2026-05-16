#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/AudioGraphEngine.h"
#include "PedalComponent.h"
#include "PedalDetailPanel.h"

//==============================================================================
/**
 * Board size presets — RE4-style attache cases.
 * Each preset has a name and grid dimensions.
 */
struct BoardPreset
{
    const char* name;
    int cols;
    int rows;
};

inline const std::vector<BoardPreset>& getBoardPresets()
{
    static const std::vector<BoardPreset> presets = {
        { "Nano",     6,  4 },   // Tiny board — 24 slots
        { "Mini",     8,  5 },   // Small board — 40 slots
        { "Standard", 10, 7 },   // Standard board — 70 slots
        { "Pro",      14, 8 },   // Large board — 112 slots
    };
    return presets;
}

//==============================================================================
/**
 * The main pedalboard grid — RE4-style inventory where pedals are placed,
 * dragged, and snapped. Uses RouteOverlay for routing visualization.
 *
 * The grid uses a FIXED cell size and is centred in the available space.
 * Board dimensions come from selectable presets (Nano/Mini/Standard/Pro).
 */
class PedalboardGrid : public juce::Component,
                       public juce::Timer
{
public:
    PedalboardGrid (AudioGraphEngine& engine, MidiLearnManager& midiLearn);
    ~PedalboardGrid() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void mouseDown (const juce::MouseEvent& e) override;

    //==========================================================================
    void addPedalAtGrid (const juce::String& pedalName, int gridX, int gridY);
    void addPedalCopy (const PedalInstance& srcInst, int gridX, int gridY);
    void removePedal (AudioGraphEngine::NodeID nodeId);
    void rebuildFromEngine();
    void snapPedalToGrid (PedalComponent& comp);
    bool keyPressed (const juce::KeyPress& key) override;

    class BoardCanvas* getCanvas() { return boardCanvas.get(); }

    //==========================================================================
    /** Select a pedal (highlights it and opens the detail panel). */
    void selectPedal (PedalComponent* comp);
    void deselectAll();

    /** Force the grid to refresh the detail panel for the selected pedal. */
    void refreshSelectedPedal();

    /** Callback fired when a pedal is selected/deselected. */
    std::function<void(PedalInstance*)> onPedalSelected;

    /** Callback fired when the Tab menu button is clicked. */
    std::function<void()> onOpenInventory;

    /** Callback fired when a pedal's library_loader control is clicked. */
    std::function<void (const juce::String& category, int targetNodeID)> onOpenLibrary;

    /** Get the currently selected pedal instance (or nullptr). */
    PedalInstance* getSelectedInstance()
    {
        return selectedComponent ? &selectedComponent->getInstance() : nullptr;
    }

    /** Get the detail panel. */
    PedalDetailPanel& getDetailPanel() { return detailPanel; }
    
    /** Get the MidiLearnManager. */
    MidiLearnManager& getMidiLearn() { return midiLearn; }

    //==========================================================================

private:
    AudioGraphEngine& engine;
    MidiLearnManager& midiLearn;
    
    std::unique_ptr<class BoardCanvas> boardCanvas;
    
    std::unique_ptr<PedalInstance> clipboardPedal;

    PedalComponent* selectedComponent = nullptr;

    PedalDetailPanel detailPanel;
    juce::TextButton btnInventory { "+ Add Pedal (Tab)" };

    // Multi-board / Page controls
    juce::TextButton btnAddBoard { "+ Add Board" };

    static constexpr int detailPanelWidth = 240;

    // Listener bridge from detail panel → grid
    struct DetailPanelListener : public PedalDetailPanel::Listener
    {
        PedalboardGrid& grid;
        DetailPanelListener (PedalboardGrid& g) : grid (g) {}
        void pedalRemoved (juce::AudioProcessorGraph::NodeID nodeId) override
        {
            grid.removePedal (nodeId);
        }
    };

    DetailPanelListener detailListener { *this };

    //==========================================================================
    // Active Pedals sidebar — shows all pedals in the engine
    //==========================================================================
    class ActivePedalsList : public juce::Component
    {
    public:
        ActivePedalsList (PedalboardGrid& g) : grid (g) {}

        void paint (juce::Graphics& g) override;
        void resized() override;

        /** Rebuild the list from the engine. */
        void refresh();

        /** Row click callback. */
        std::function<void(PedalInstance*)> onPedalClicked;

    private:
        PedalboardGrid& grid;

        //----------------------------------------------------------------------
        class PedalRow : public juce::Component
        {
        public:
            PedalRow (const PedalInstance& inst, PedalboardGrid& g);

            void paint (juce::Graphics& g) override;
            void mouseDown (const juce::MouseEvent& e) override;
            void mouseDrag (const juce::MouseEvent& e) override;

            // Snapshot of display data (safe even if vector reallocates)
            juce::AudioProcessorGraph::NodeID nodeID;
            juce::String name;
            juce::String category;
            juce::Colour colour;
            bool onBoard = true;
            int gridW = 1, gridH = 2;

            PedalboardGrid& grid;
            bool dragStarted = false;
        };

        juce::Viewport viewport;
        juce::Component content;
        juce::OwnedArray<PedalRow> rows;
    };

    ActivePedalsList activePedalsList { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalboardGrid)
};
