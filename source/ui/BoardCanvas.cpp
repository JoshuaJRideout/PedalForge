#include "BoardCanvas.h"
#include "PedalboardGrid.h"
#include "LookAndFeel.h"

BoardCanvas::BoardCanvas (AudioGraphEngine& eng, PedalboardGrid* grid)
    : engine (eng), parentGrid (grid)
{
    setWantsKeyboardFocus (true);
    setSize (4000, 4000); // Very large virtual size
    rebuildBoards();
}

BoardCanvas::~BoardCanvas()
{
}

void BoardCanvas::rebuildBoards()
{
    boardComponents.clear();
    for (auto& cfg : engine.getBoards())
    {
        auto comp = std::make_unique<BoardComponent> (const_cast<BoardConfig&>(cfg), engine, parentGrid->getMidiLearn(), parentGrid);
        comp->setBounds (cfg.canvasX, cfg.canvasY, cfg.cols * comp->getCellSize(), 40 + cfg.rows * comp->getCellSize());
        addAndMakeVisible (comp.get());
        boardComponents.push_back (std::move (comp));
    }
}

void BoardCanvas::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgDark);

    g.saveState();
    
    // Apply panning and zooming transform
    juce::AffineTransform t = juce::AffineTransform::scale (scale)
                                  .translated (panX, panY);
    g.addTransform (t);

    // Faint canvas grid
    g.setColour (juce::Colour (0x0AFFFFFF));
    int gs = 60;
    for (int x = 0; x < getWidth(); x += gs)
        g.drawVerticalLine (x, 0, (float) getHeight());
    for (int y = 0; y < getHeight(); y += gs)
        g.drawHorizontalLine (y, 0, (float) getWidth());
        
    g.restoreState();
}

void BoardCanvas::paintOverChildren (juce::Graphics& g)
{
    g.saveState();
    
    juce::AffineTransform t = juce::AffineTransform::scale (scale)
                                  .translated (panX, panY);
    g.addTransform (t);
    
    g.setColour (PedalForgeLookAndFeel::textPrimary);
    g.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
    
    for (auto& comp : boardComponents)
    {
        if (comp->getConfig().cols < 3)
        {
            juce::Font font (14.0f, juce::Font::bold);
            float textW = font.getStringWidthFloat (comp->getConfig().name) + 16.0f; // add padding
            float drawW = juce::jmax (textW, (float)comp->getWidth());
            
            // Center the text above the board
            juce::Rectangle<float> titleRect (comp->getX() + (comp->getWidth() - drawW) * 0.5f, 
                                              comp->getY() - 25.0f, 
                                              drawW, 20.0f);
            
            // Draw a subtle background for readability if desired, or just a drop shadow
            g.setColour (PedalForgeLookAndFeel::bgDark.withAlpha(0.6f));
            g.fillRoundedRectangle (titleRect.expanded(4, 2), 4.0f);
            
            g.setColour (PedalForgeLookAndFeel::textPrimary);
            g.setFont (font);
            g.drawText (comp->getConfig().name, titleRect, juce::Justification::centred, true);
        }
    }
    
    g.restoreState();
}

void BoardCanvas::resized()
{
    // Apply transform to children
    juce::AffineTransform t = juce::AffineTransform::scale (scale)
                                  .translated (panX, panY);
                                  
    for (auto& b : boardComponents)
    {
        b->setTransform (t);
    }
}

void BoardCanvas::mouseDown (const juce::MouseEvent& e)
{
    dragStartPan = { panX, panY };
}

void BoardCanvas::mouseDrag (const juce::MouseEvent& e)
{
    float dx = (float) (e.getScreenPosition().x - e.getMouseDownScreenPosition().x);
    float dy = (float) (e.getScreenPosition().y - e.getMouseDownScreenPosition().y);
    panX = dragStartPan.x + dx;
    panY = dragStartPan.y + dy;
    resized();
    repaint();
}

void BoardCanvas::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    float z = 1.0f + w.deltaY * 2.0f;
    if (z > 0)
    {
        float oldScale = scale;
        float newScale = juce::jlimit (0.2f, 3.0f, scale * z);
        
        if (newScale != oldScale)
        {
            // Center zoom on mouse cursor
            float worldX = (e.position.x - panX) / oldScale;
            float worldY = (e.position.y - panY) / oldScale;
            
            panX = e.position.x - worldX * newScale;
            panY = e.position.y - worldY * newScale;
            
            scale = newScale;
            resized();
            repaint();
        }
    }
}

void BoardCanvas::setBoardFullscreen (BoardComponent* board, bool isFullscreen)
{
    if (isFullscreen)
    {
        // Check if already open
        auto it = std::find_if (activeWindows.begin(), activeWindows.end(),
            [board](const std::unique_ptr<BoardWindow>& w) { return w->getBoard() == board; });
        if (it == activeWindows.end())
        {
            auto win = std::make_unique<BoardWindow> (board->getConfig().name, board, this);
            activeWindows.push_back (std::move (win));
        }
    }
    else
    {
        restoreBoardFromWindow (board);
    }
}

void BoardCanvas::restoreBoardFromWindow (BoardComponent* board)
{
    auto it = std::find_if (activeWindows.begin(), activeWindows.end(),
        [board](const std::unique_ptr<BoardWindow>& w) { return w->getBoard() == board; });
        
    if (it != activeWindows.end())
    {
        board->getConfig().displayIndex = -1; // Reflect close
        activeWindows.erase (it);
        resized();
        repaint();
    }
}
