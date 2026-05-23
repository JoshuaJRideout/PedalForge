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
    
    juce::Point<float> snapToGrid (float px, float py) const;
    bool isGridRectFree (float bx, float by, float bw, float bh, AudioGraphEngine::NodeID ignoreNodeId = {}) const;

    float getBoardWidth() const { return config.cols * 100.0f; }
    float getBoardHeight() const { return config.rows * 100.0f; }
    
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
    float dragHoverBoardX = 0.0f;
    float dragHoverBoardY = 0.0f;
    float dragHoverBoardW = 100.0f;
    float dragHoverBoardH = 200.0f;
    bool dragHoverValid = false;
    
    int getHeaderHeight() const;

    void updateHeader();
    void timerCallback() override;

    int lastActivePage = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoardComponent)
};
