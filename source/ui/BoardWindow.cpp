#include "BoardWindow.h"
#include "BoardCanvas.h"
#include "LookAndFeel.h"

class FullscreenWrapper : public juce::Component, public juce::Timer
{
public:
    FullscreenWrapper (BoardComponent* b) : board(b)
    {
        startTimerHz(30);
    }
    
    void timerCallback() override
    {
        repaint();
    }
    
    void paint (juce::Graphics& g) override
    {
        g.fillAll (PedalForgeLookAndFeel::bgDark.darker(0.2f));

        if (board == nullptr) return;

        int bw = board->getConfig().cols * 100;
        int bh = 40 + board->getConfig().rows * 100;
        
        int sw = getWidth();
        int sh = getHeight();
        
        float scaleX = (sw - 40.0f) / (float)bw;
        float scaleY = (sh - 40.0f) / (float)bh;
        float scale = juce::jmin (scaleX, scaleY);
        
        if (scale > 3.0f) scale = 3.0f;
        
        auto transform = juce::AffineTransform::scale (scale)
                             .translated ((sw - bw * scale) * 0.5f, 
                                          (sh - bh * scale) * 0.5f);
                                          
        g.addTransform(transform);
        board->paintEntireComponent(g, true);
    }
    
    void resized() override
    {
    }
    
private:
    BoardComponent* board;
};

BoardWindow::BoardWindow (juce::String name, BoardComponent* boardToWrap, BoardCanvas* canvas)
    : juce::DocumentWindow (name, PedalForgeLookAndFeel::bgDark, juce::DocumentWindow::allButtons),
      board (boardToWrap),
      parentCanvas (canvas)
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    
    auto* wrapper = new FullscreenWrapper (board);
    setContentOwned (wrapper, true);
    
    auto displays = juce::Desktop::getInstance().getDisplays().displays;
    int targetIdx = board->getConfig().displayIndex;
    if (targetIdx >= 0 && targetIdx < displays.size())
    {
        auto rect = displays[targetIdx].userArea;
        setBounds (rect);
        setFullScreen (true);
    }
    else
    {
        centreWithSize (board->getConfig().cols * 100 + 40, 
                        40 + board->getConfig().rows * 100 + 40);
    }
    
    setVisible (true);
}

BoardWindow::~BoardWindow()
{
}

void BoardWindow::closeButtonPressed()
{
    if (parentCanvas != nullptr)
        parentCanvas->restoreBoardFromWindow (board);
}
