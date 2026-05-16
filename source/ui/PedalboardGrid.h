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
                       public juce::DragAndDropTarget,
                       public juce::Timer
{
public:
    PedalboardGrid (AudioGraphEngine& engine);
    ~PedalboardGrid() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void mouseDown (const juce::MouseEvent& e) override;

    //==========================================================================
    // DragAndDropTarget
    bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDragEnter (const SourceDetails& details) override;
    void itemDragMove  (const SourceDetails& details) override;
    void itemDragExit  (const SourceDetails& details) override;
    void itemDropped   (const SourceDetails& details) override;

    //==========================================================================
    void addPedalAtGrid (const juce::String& pedalName, int gridX, int gridY);
    void addPedalCopy (const PedalInstance& srcInst, int gridX, int gridY);
    void removePedal (AudioGraphEngine::NodeID nodeId);
    void rebuildFromEngine();
    void snapPedalToGrid (PedalComponent& comp);
    bool keyPressed (const juce::KeyPress& key) override;

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

    /** Get the currently selected pedal instance (or nullptr). */
    PedalInstance* getSelectedInstance()
    {
        return selectedComponent ? &selectedComponent->getInstance() : nullptr;
    }

    /** Get the detail panel. */
    PedalDetailPanel& getDetailPanel() { return detailPanel; }

    int getCellSize() const { return cellSize; }
    int getGridCols() const { return gridCols; }
    int getGridRows() const { return gridRows; }

    juce::Point<int> gridToPixel (int gx, int gy) const;
    juce::Point<int> pixelToGrid (int px, int py) const;
    bool isGridRectFree (int gx, int gy, int gw, int gh,
                         AudioGraphEngine::NodeID ignoreNodeId = {}) const;

    //==========================================================================
    /** Set the board preset by index. */
    void setBoardPreset (int presetIndex);
    int  getBoardPresetIndex() const { return currentPresetIndex; }

private:
    AudioGraphEngine& engine;

    std::vector<std::unique_ptr<PedalComponent>> pedalComponents;
    PedalComponent* selectedPedal = nullptr;
    
    std::unique_ptr<PedalInstance> clipboardPedal;

    PedalComponent* selectedComponent = nullptr;

    PedalDetailPanel detailPanel;
    juce::TextButton btnInventory { "+ Add Pedal (Tab)" };

    // Fixed cell size — never changes with window resize
    static constexpr int cellSize = 60;

    int gridCols = 10;
    int gridRows = 7;
    int currentPresetIndex = 2; // Default: "Standard"

    // Grid origin (computed in resized to centre the board)
    int gridOriginX = 0;
    int gridOriginY = 0;

    static constexpr int detailPanelWidth = 240;

    // Drag-hover preview
    bool showDropPreview = false;
    int  dropPreviewX = 0, dropPreviewY = 0;
    int  dropPreviewW = 1, dropPreviewH = 2;
    bool dropPreviewValid = false;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalboardGrid)
};
