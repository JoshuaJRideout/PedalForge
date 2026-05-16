#include "BoardComponent.h"
#include "PedalboardGrid.h"
#include "LookAndFeel.h"
#include "../pedals/PedalRegistry.h"
#include "../dsp/GraphPedalProcessor.h"
#include "../dsp/PedalDesign.h"
#include "BoardCanvas.h"

//==============================================================================
BoardComponent::BoardComponent (BoardConfig& cfg, AudioGraphEngine& eng, MidiLearnManager& midiMgr, PedalboardGrid* grid)
    : config (cfg), engine (eng), midiLearn (midiMgr), parentGrid (grid)
{
    addAndMakeVisible (titleLabel);
    titleLabel.setText (config.name, juce::dontSendNotification);
    
    lastActivePage = config.activePage;
    startTimerHz(10);
    titleLabel.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
    titleLabel.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textPrimary);
    titleLabel.setJustificationType (juce::Justification::centred);
    
    addAndMakeVisible (btnMenu);
    btnMenu.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    btnMenu.onClick = [this] {
        juce::PopupMenu menu;
        
        // Full screen
        menu.addItem ("Go Full Screen", [this] { 
            if (parentGrid)
                parentGrid->getCanvas()->setBoardFullscreen(this, true);
        });
        menu.addSeparator();
        
        // Pages
        juce::PopupMenu pageMenu;
        for (int i = 0; i < config.numPages; ++i)
        {
            pageMenu.addItem (juce::String("Page ") + juce::String(i+1), true, config.activePage == i, [this, i] {
                config.activePage = i;
                rebuildPedals();
            });
        }
        menu.addSubMenu ("Page", pageMenu);
        menu.addSeparator();
        
        // Displays
        juce::PopupMenu displayMenu;
        displayMenu.addItem ("Main Display", true, config.displayIndex == -1, [this] { 
            config.displayIndex = -1; 
            if (parentGrid) parentGrid->getCanvas()->setBoardFullscreen(this, false);
        });
        
        auto displays = juce::Desktop::getInstance().getDisplays().displays;
        for (int i = 0; i < displays.size(); ++i)
        {
            displayMenu.addItem ("Display " + juce::String(i+1), true, config.displayIndex == i, [this, i] { 
                config.displayIndex = i; 
                if (parentGrid) parentGrid->getCanvas()->setBoardFullscreen(this, true);
            });
        }
        menu.addSubMenu ("Assign Display", displayMenu);
        menu.addSeparator();
        
        // Turing
        juce::PopupMenu turingMenu;
        turingMenu.addItem ("Enable Output", true, config.assignToTuring, [this] {
            config.assignToTuring = !config.assignToTuring;
        });
        turingMenu.addItem ("Orientation: Horizontal", true, config.turingHorizontal, [this] {
            config.turingHorizontal = true;
        });
        turingMenu.addItem ("Orientation: Vertical", true, !config.turingHorizontal, [this] {
            config.turingHorizontal = false;
        });
        menu.addSubMenu ("Turing LCD", turingMenu);
        
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btnMenu));
    };
    
    rebuildPedals();
}

BoardComponent::~BoardComponent()
{
}

void BoardComponent::updateHeader()
{

}

void BoardComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Board background
    g.setColour (PedalForgeLookAndFeel::bgLight.withAlpha (0.6f));
    g.fillRoundedRectangle (bounds, 6.0f);
    
    // Header background
    g.setColour (PedalForgeLookAndFeel::bgMid.darker(0.2f));
    g.fillRoundedRectangle (bounds.withHeight (getHeaderHeight()), 6.0f);
    g.fillRect (0.0f, getHeaderHeight() - 6.0f, bounds.getWidth(), 6.0f); // square bottom corners of header
    
    // Grid dots
    g.setColour (PedalForgeLookAndFeel::gridLine.withAlpha (0.2f));
    for (int col = 0; col <= config.cols; ++col)
    {
        for (int row = 0; row <= config.rows; ++row)
        {
            float x = (float) (col * cellSize);
            float y = (float) (getHeaderHeight() + row * cellSize);
            float dotSize = (col % 4 == 0 && row % 4 == 0) ? 3.0f : 1.5f;
            g.fillEllipse (x - dotSize * 0.5f, y - dotSize * 0.5f, dotSize, dotSize);
        }
    }
    
    // Grid lines
    g.setColour (PedalForgeLookAndFeel::gridLine.withAlpha (0.06f));
    for (int col = 0; col <= config.cols; ++col)
        g.drawVerticalLine (col * cellSize, (float) getHeaderHeight(), (float) (getHeaderHeight() + config.rows * cellSize));
    for (int row = 0; row <= config.rows; ++row)
        g.drawHorizontalLine (getHeaderHeight() + row * cellSize, 0.0f, (float) (config.cols * cellSize));
        
    // Resize handle (bottom right)
    g.setColour (PedalForgeLookAndFeel::textMuted.withAlpha (0.5f));
    float rs = 10.0f;
    juce::Path resizeHandle;
    resizeHandle.addTriangle (bounds.getRight() - rs, bounds.getBottom(),
                              bounds.getRight(), bounds.getBottom(),
                              bounds.getRight(), bounds.getBottom() - rs);
    g.fillPath (resizeHandle);
                    
    // Drag preview
    if (isDragHovering)
    {
        auto hoverCol = dragHoverValid ? PedalForgeLookAndFeel::success : PedalForgeLookAndFeel::danger;
        g.setColour (hoverCol.withAlpha (0.2f));
        g.fillRect (dragHoverGridX * cellSize,
                    getHeaderHeight() + dragHoverGridY * cellSize,
                    dragHoverGridW * cellSize,
                    dragHoverGridH * cellSize);
        
        g.setColour (hoverCol.withAlpha (0.6f));
        g.drawRect (dragHoverGridX * cellSize,
                    getHeaderHeight() + dragHoverGridY * cellSize,
                    dragHoverGridW * cellSize,
                    dragHoverGridH * cellSize, 2);
    }

    // Border
    g.setColour (PedalForgeLookAndFeel::gridLine.withAlpha (0.2f));
    g.drawRoundedRectangle (bounds, 6.0f, 1.5f);
}

int BoardComponent::getHeaderHeight() const
{
    return 40;
}

void BoardComponent::resized()
{
    auto header = getLocalBounds().removeFromTop (getHeaderHeight());
    
    btnMenu.setBounds (header.removeFromLeft (40).reduced (4, 4));
    
    if (config.cols < 3)
    {
        titleLabel.setVisible (false);
    }
    else
    {
        titleLabel.setVisible (true);
        // Reduce right by 40 to perfectly center the text across the full width
        auto titleArea = header;
        titleArea.removeFromRight (40);
        titleLabel.setBounds (titleArea);
    }
    for (auto& comp : pedalComponents)
    {
        auto pos = gridToPixel (comp->getInstance().gridX, comp->getInstance().gridY);
        comp->setBounds (pos.x, pos.y, comp->getInstance().gridW * cellSize, comp->getInstance().gridH * cellSize);
    }
}

void BoardComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.y < getHeaderHeight())
    {
        isDraggingBoard = true;
        dragStartPos = e.getEventRelativeTo (getParentComponent()).getPosition();
        boardStartPos = getPosition();
    }
    else if (e.x > getWidth() - 20 && e.y > getHeight() - 20)
    {
        isResizingBoard = true;
        dragStartPos = e.getScreenPosition();
        startCols = config.cols;
        startRows = config.rows;
    }
}

void BoardComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (isDraggingBoard)
    {
        auto pos = e.getEventRelativeTo (getParentComponent()).getPosition();
        int dx = pos.x - dragStartPos.x;
        int dy = pos.y - dragStartPos.y;
        
        // Snap to parent canvas grid (e.g. 20px)
        int nx = std::round((boardStartPos.x + dx) / 20.0f) * 20;
        int ny = std::round((boardStartPos.y + dy) / 20.0f) * 20;
        
        config.canvasX = nx;
        config.canvasY = ny;
        setTopLeftPosition (nx, ny);
        
        // Repaint parent to clear any floating title trails
        if (getParentComponent() != nullptr)
            getParentComponent()->repaint();
    }
    else if (isResizingBoard)
    {
        float dx = (e.getScreenPosition().x - dragStartPos.x) / getTransform().mat00;
        float dy = (e.getScreenPosition().y - dragStartPos.y) / getTransform().mat11;
        
        int dCols = std::round (dx / (float)cellSize);
        int dRows = std::round (dy / (float)cellSize);
        
        int newCols = juce::jmax (1, startCols + dCols);
        int newRows = juce::jmax (1, startRows + dRows);
        
        if (newCols != config.cols || newRows != config.rows)
        {
            config.cols = newCols;
            config.rows = newRows;
            setSize (config.cols * cellSize, getHeaderHeight() + config.rows * cellSize);
            getParentComponent()->repaint(); // Update connections
        }
    }
}

void BoardComponent::mouseUp (const juce::MouseEvent&)
{
    isDraggingBoard = false;
    isResizingBoard = false;
}

//==============================================================================
juce::Point<int> BoardComponent::gridToPixel (int gx, int gy) const
{
    return { gx * cellSize, getHeaderHeight() + gy * cellSize };
}

juce::Point<int> BoardComponent::pixelToGrid (int px, int py) const
{
    int gx = (int)std::floor(px / (float)cellSize);
    int gy = (int)std::floor((py - getHeaderHeight()) / (float)cellSize);
    return { gx, gy };
}

bool BoardComponent::isGridRectFree (int gx, int gy, int gw, int gh, AudioGraphEngine::NodeID ignoreNodeId) const
{
    if (gx < 0 || gy < 0 || gx + gw > config.cols || gy + gh > config.rows)
        return false;

    for (auto& inst : engine.getPedalInstances())
    {
        if (! inst.onBoard || inst.boardId != config.id || inst.pageIndex != config.activePage)
            continue;
        if (inst.nodeID == ignoreNodeId) continue;
        if (gx < inst.gridX + inst.gridW && gx + gw > inst.gridX &&
            gy < inst.gridY + inst.gridH && gy + gh > inst.gridY)
            return false;
    }
    return true;
}

void BoardComponent::rebuildPedals()
{
    lastActivePage = config.activePage;
    pedalComponents.clear();

    for (auto& inst : engine.getPedalInstances())
    {
        if (! inst.onBoard || inst.boardId != config.id || inst.pageIndex != config.activePage)
            continue;

        auto comp = std::make_unique<PedalComponent> (const_cast<PedalInstance&> (inst), engine, midiLearn);
        comp->setGrid (this);

        // Wire library_loader callback through the grid
        if (parentGrid != nullptr)
        {
            comp->onOpenLibrary = [this] (const juce::String& category, int targetNodeID)
            {
                if (parentGrid && parentGrid->onOpenLibrary)
                    parentGrid->onOpenLibrary (category, targetNodeID);
            };
        }

        auto pos = gridToPixel (inst.gridX, inst.gridY);
        comp->setBounds (pos.x, pos.y, inst.gridW * cellSize, inst.gridH * cellSize);

        addAndMakeVisible (comp.get());
        pedalComponents.push_back (std::move (comp));
    }
    resized();
    repaint();
}

void BoardComponent::timerCallback()
{
    if (config.activePage != lastActivePage)
    {
        rebuildPedals();
    }
}

// Drag & Drop
bool BoardComponent::isInterestedInDragSource (const SourceDetails& details)
{
    auto desc = details.description.toString();
    return desc.startsWith ("pedal:") || desc.startsWith ("active_pedal:");
}

void BoardComponent::itemDragEnter (const SourceDetails& details)
{
    isDragHovering = true;
    itemDragMove (details);
}

void BoardComponent::itemDragMove (const SourceDetails& details)
{
    auto desc = details.description.toString();
    juce::StringArray tok;
    tok.addTokens (desc, ":", "");
    
    int gw = 1, gh = 2;
    if (tok.size() >= 2 && tok[0] == "pedal")
    {
        for (auto& info : getFactoryPedals())
        {
            if (info.name == tok[1])
            {
                gw = info.gridW;
                gh = info.gridH;
                break;
            }
        }
    }
    else if (tok.size() >= 2 && tok[0] == "active_pedal")
    {
        auto nodeId = AudioGraphEngine::NodeID { static_cast<juce::uint32> (tok[1].getLargeIntValue()) };
        if (auto* inst = engine.getPedalInstance(nodeId))
        {
            gw = inst->gridW;
            gh = inst->gridH;
        }
    }

    auto localPos = details.localPosition;
    auto gp = pixelToGrid (localPos.x, localPos.y);
    
    dragHoverGridX = gp.x;
    dragHoverGridY = gp.y;
    dragHoverGridW = gw;
    dragHoverGridH = gh;
    dragHoverValid = isGridRectFree (gp.x, gp.y, gw, gh);
    
    repaint();
}

void BoardComponent::itemDragExit (const SourceDetails& details)
{
    isDragHovering = false;
    repaint();
}

void BoardComponent::itemDropped (const SourceDetails& details)
{
    auto desc = details.description.toString();
    juce::StringArray tok;
    tok.addTokens (desc, ":", "");
    if (tok.size() < 2) return;

    auto localPos = details.localPosition;
    auto gp = pixelToGrid (localPos.x, localPos.y);

    if (tok[0] == "pedal")
    {
        auto type = tok[1];
        for (auto& info : getFactoryPedals())
        {
            if (info.name == type)
            {
                auto processor = info.factory();
                auto nodeId = engine.addPedal (std::move (processor),
                                                config.id, config.activePage,
                                                gp.x, gp.y,
                                                info.gridW, info.gridH);
                                                
                if (auto* inst = engine.getPedalInstance (nodeId))
                {
                    inst->colour = info.colour;
                    inst->category = info.category;
                    inst->numKnobs = info.numKnobs;

                    // If it's a DSP pedal (e.g. Memory Node), load its default chassis/wiring design
                    if (auto* node = engine.getGraph().getNodeForId (nodeId))
                    {
                        if (auto* proc = dynamic_cast<GraphPedalProcessor*> (node->getProcessor()))
                        {
                            if (info.designFactory)
                            {
                                inst->design = info.designFactory();
                            }
                            else
                            {
                                inst->design = std::make_shared<PedalDesign>();
                                inst->design->name = info.name;
                                inst->design->category = info.category;
                                inst->design->chassisW = info.gridW * 100.0f;
                                inst->design->chassisH = info.gridH * 100.0f;
                                inst->design->chassisColour = info.colour;
                            }

                            // Serialize the factory graph so we can edit it later
                            inst->design->effectsGraph = juce::JSON::parse (proc->saveGraph());
                        }
                    }
                }
                engine.autoRoutePedal(nodeId);
                break;
            }
        }
    }
    else if (tok[0] == "active_pedal")
    {
        juce::AudioProcessorGraph::NodeID nodeId { (juce::uint32) tok[1].getLargeIntValue() };
        if (auto* inst = engine.getPedalInstance (nodeId))
        {
            inst->onBoard = true;
            inst->boardId = config.id;
            inst->pageIndex = config.activePage;
            inst->gridX = gp.x;
            inst->gridY = gp.y;
        }
    }
    
    if (parentGrid)
        parentGrid->rebuildFromEngine();
    else
        rebuildPedals();
}

void BoardComponent::rebuildFromEngine()
{
    rebuildPedals();
}

void BoardComponent::removePedal (AudioGraphEngine::NodeID nodeId)
{
    engine.removePedal(nodeId);
    if (parentGrid)
        parentGrid->rebuildFromEngine();
    else
        rebuildPedals();
}

