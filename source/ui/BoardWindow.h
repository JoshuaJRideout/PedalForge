#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BoardComponent.h"

class BoardCanvas;

class BoardWindow : public juce::DocumentWindow
{
public:
    BoardWindow(juce::String name, BoardComponent* boardToWrap, BoardCanvas* canvas);
    ~BoardWindow() override;

    void closeButtonPressed() override;

    BoardComponent* getBoard() const { return board; }

private:
    BoardComponent* board;
    BoardCanvas* parentCanvas;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoardWindow)
};
