#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/AudioGraphEngine.h"
#include "BoardComponent.h"
#include "BoardWindow.h"

class PedalboardGrid;

class BoardCanvas : public juce::Component
{
public:
    BoardCanvas (AudioGraphEngine& engine, PedalboardGrid* grid);
    ~BoardCanvas() override;

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;
    
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    void rebuildBoards();
    
    void setBoardFullscreen (BoardComponent* board, bool isFullscreen);
    void restoreBoardFromWindow (BoardComponent* board);

private:
    AudioGraphEngine& engine;
    PedalboardGrid* parentGrid;

    std::vector<std::unique_ptr<BoardComponent>> boardComponents;
    std::vector<std::unique_ptr<BoardWindow>> activeWindows;

    float scale = 1.0f;
    juce::Point<float> dragStartPan;
    float panX = 0.0f;
    float panY = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoardCanvas)
};
