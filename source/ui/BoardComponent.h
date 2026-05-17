#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/AudioGraphEngine.h"
#include "PedalComponent.h"

class PedalboardGrid;

class BoardComponent : public juce::Component,
                       public juce::DragAndDropTarget,
                       public juce::Timer
{
public:
    BoardComponent (BoardConfig& config, AudioGraphEngine& engine, MidiLearnManager& midiLearn, PedalboardGrid* grid);
    ~BoardComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    // DragAndDropTarget
    bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDragEnter (const SourceDetails& details) override;
    void itemDragMove  (const SourceDetails& details) override;
    void itemDragExit  (const SourceDetails& details) override;
    void itemDropped   (const SourceDetails& details) override;

    void rebuildPedals();
    
    BoardConfig& getConfig() { return config; }
    PedalboardGrid* getParentGrid() { return parentGrid; }
    
    juce::Point<int> gridToPixel (int gx, int gy) const;
    juce::Point<int> pixelToGrid (int px, int py) const;
    bool isGridRectFree (int gx, int gy, int gw, int gh, AudioGraphEngine::NodeID ignoreNodeId = {}) const;

    int getCellSize() const { return cellSize; }
    int getGridCols() const { return config.cols; }
    int getGridRows() const { return config.rows; }
    void rebuildFromEngine();
    void removePedal (AudioGraphEngine::NodeID nodeId);

private:
    BoardConfig& config;
    AudioGraphEngine& engine;
    MidiLearnManager& midiLearn;
    PedalboardGrid* parentGrid;

    std::vector<std::unique_ptr<PedalComponent>> pedalComponents;

    // Header controls
    juce::Label titleLabel;
    juce::TextButton btnMenu { u8"\u2630" }; // Unicode hamburger icon ☰

    bool isDraggingBoard = false;
    bool isResizingBoard = false;
    juce::Point<int> dragStartPos;
    juce::Point<int> boardStartPos;
    int startCols = 0;
    int startRows = 0;
    
    // Drag and drop preview
    bool isDragHovering = false;
    int dragHoverGridX = 0;
    int dragHoverGridY = 0;
    int dragHoverGridW = 1;
    int dragHoverGridH = 2;
    bool dragHoverValid = false;
    
    static constexpr int cellSize = 100;
    
    int getHeaderHeight() const;

    void updateHeader();
    void timerCallback() override;

    int lastActivePage = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoardComponent)
};
