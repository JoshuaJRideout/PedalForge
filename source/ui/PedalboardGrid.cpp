#include "PedalboardGrid.h"
#include "BoardCanvas.h"
#include "LookAndFeel.h"
#include "../pedals/PedalRegistry.h"
#include "../dsp/GraphPedalProcessor.h"
#include "../dsp/PedalDesign.h"

//==============================================================================
PedalboardGrid::PedalboardGrid (AudioGraphEngine& eng, MidiLearnManager& midiMgr)
    : engine (eng), midiLearn (midiMgr)
{
    addAndMakeVisible (detailPanel);
    detailPanel.addListener (&detailListener);
    detailPanel.onOpenLibrary = [this] (const juce::String& category, int targetNodeID)
    {
        if (onOpenLibrary) onOpenLibrary (category, targetNodeID);
    };

    addAndMakeVisible (btnInventory);
    btnInventory.onClick = [this] { if (onOpenInventory) onOpenInventory(); };

    addAndMakeVisible (activePedalsList);
    activePedalsList.onPedalClicked = [this] (PedalInstance* inst)
    {
        if (onPedalSelected)
            onPedalSelected (inst);
    };

    boardCanvas = std::make_unique<BoardCanvas> (engine, this);
    addAndMakeVisible (boardCanvas.get());

    addAndMakeVisible (btnAddBoard);
    btnAddBoard.onClick = [this] {
        BoardConfig newCfg;
        newCfg.id = "board_" + juce::String(juce::Time::getMillisecondCounter());
        newCfg.name = "New Board";
        newCfg.cols = 10;
        newCfg.rows = 6;
        newCfg.numPages = 1;
        newCfg.canvasX = 100;
        newCfg.canvasY = 100;
        engine.addBoard (newCfg);
        boardCanvas->rebuildBoards();
    };

    startTimerHz (30);
}

PedalboardGrid::~PedalboardGrid()
{
}

//==============================================================================
void PedalboardGrid::paint (juce::Graphics& g)
{
    // Fill entire background
    auto fullArea = getLocalBounds();
    fullArea.removeFromTop (36); // Skip toolbar area
    if (detailPanel.hasSelection())
        fullArea.removeFromRight (detailPanelWidth);
    
    g.setColour (PedalForgeLookAndFeel::bgDark);
    g.fillRect (fullArea);

    // Tab Toolbar Background
    auto toolbarArea = getLocalBounds().removeFromTop (36);
    g.setColour (PedalForgeLookAndFeel::bgMid.darker(0.2f));
    g.fillRect (toolbarArea);
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawHorizontalLine (35, 0.0f, (float)getWidth());
}

void PedalboardGrid::timerCallback()
{
    bool needsRepaint = false;

    // Iterate over all pedals
    for (auto* comp : getChildren())
    {
        if (auto* pedalComp = dynamic_cast<PedalComponent*> (comp))
        {
            auto& instance = pedalComp->getInstance();
            if (instance.design != nullptr)
            {
                auto* node = engine.getGraph().getNodeForId (instance.nodeID);
                if (node != nullptr)
                {
                    if (auto* proc = node->getProcessor())
                    {
                        auto params = proc->getParameters();
                        for (const auto& mapping : instance.design->mappings)
                        {
                            for (auto* p : params)
                            {
                                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                                {
                                    if (ranged->getParameterID() == mapping.nodeParam)
                                    {
                                        float val = ranged->convertFrom0to1 (ranged->getValue());
                                        juce::String text = ranged->getText (ranged->getValue(), 32);

                                        // Only repaint if changed
                                        if (instance.controlValues[mapping.controlID] != val ||
                                            instance.controlTexts[mapping.controlID] != text)
                                        {
                                            instance.controlValues[mapping.controlID] = val;
                                            instance.controlTexts[mapping.controlID] = text;
                                            needsRepaint = true;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (needsRepaint)
    {
        repaint(); // Repaint the board to update components
        if (detailPanel.hasSelection())
            detailPanel.repaint(); // Update the detail panel too!
    }
}

//==============================================================================
void PedalboardGrid::resized()
{
    auto area = getLocalBounds();

    // Tab-specific toolbar area
    auto toolbar = area.removeFromTop (36);
    btnInventory.setBounds (toolbar.removeFromLeft (140).reduced (8, 6));

    // Board / Page navigation controls on the right of the toolbar
    auto tbRight = toolbar.removeFromRight (150);
    btnAddBoard.setBounds (tbRight.removeFromLeft (100).reduced (4, 6));

    if (detailPanel.hasSelection())
    {
        detailPanel.setBounds (area.removeFromRight (detailPanelWidth));
        detailPanel.setVisible (true);
    }
    else
    {
        detailPanel.setVisible (false);
    }

    // Active pedals sidebar on the left
    activePedalsList.setBounds (area.removeFromLeft (170));

    // Centre the grid in the remaining space
    if (boardCanvas)
        boardCanvas->setBounds (area);
}

//==============================================================================
void PedalboardGrid::mouseDown (const juce::MouseEvent& /*e*/)
{
    grabKeyboardFocus();
    deselectAll();
}
void PedalboardGrid::selectPedal (PedalComponent* comp)
{
    if (selectedComponent == comp)
        return;

    selectedComponent = comp;

    if (selectedComponent != nullptr)
    {
        selectedComponent->toFront (false);
        detailPanel.showPedal (selectedComponent->getInstance(), engine);
    }
    else
    {
        detailPanel.clearSelection();
    }

    if (onPedalSelected)
        onPedalSelected (getSelectedInstance());

    resized();
    repaint();
}

void PedalboardGrid::deselectAll()
{
    selectedComponent = nullptr;
    detailPanel.clearSelection();

    if (onPedalSelected)
        onPedalSelected (nullptr);

    resized();
    repaint();
}

void PedalboardGrid::refreshSelectedPedal()
{
    if (selectedComponent != nullptr)
    {
        selectedComponent->getInstance().gridW = std::max(1, (int)std::round (selectedComponent->getInstance().design->chassisW / 100.0f));
        selectedComponent->getInstance().gridH = std::max(1, (int)std::round (selectedComponent->getInstance().design->chassisH / 100.0f));
        
        selectedComponent->repaint();
        detailPanel.showPedal (selectedComponent->getInstance(), engine);
        resized();
        repaint();
    }
}

void PedalboardGrid::addPedalAtGrid (const juce::String& pedalName, int gridX, int gridY)
{
    auto& boards = engine.getBoards();
    if (boards.empty()) return;
    auto& config = boards.front(); // Use the first board for now

    for (auto& info : getFactoryPedals())
    {
        if (info.name == pedalName)
        {
            int placeX = gridX;
            int placeY = gridY;
            
            // Auto-place if coords are -1
            if (placeX < 0 || placeY < 0)
            {
                placeX = 0; placeY = 0;
                bool found = false;
                for (int y = 0; y <= config.rows - info.gridH && !found; ++y)
                {
                    for (int x = 0; x <= config.cols - info.gridW && !found; ++x)
                    {
                        // Check if rect is free
                        bool free = true;
                        for (auto& inst : engine.getPedalInstances())
                        {
                            if (!inst.onBoard || inst.boardId != config.id || inst.pageIndex != config.activePage)
                                continue;
                            if (x < inst.gridX + inst.gridW && x + info.gridW > inst.gridX &&
                                y < inst.gridY + inst.gridH && y + info.gridH > inst.gridY)
                            {
                                free = false;
                                break;
                            }
                        }
                        if (free)
                        {
                            placeX = x;
                            placeY = y;
                            found = true;
                        }
                    }
                }
            }
            
            auto processor = info.factory();
            auto nodeId = engine.addPedal (std::move (processor),
                                           config.id, config.activePage,
                                           placeX, placeY,
                                           info.gridW, info.gridH);
                                            
            if (auto* inst = engine.getPedalInstance (nodeId))
            {
                inst->colour = info.colour;
                inst->category = info.category;
                inst->numKnobs = info.numKnobs;

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

                        inst->design->effectsGraph = juce::JSON::parse (proc->saveGraph());
                    }
                }
            }
            engine.autoRoutePedal(nodeId);
            rebuildFromEngine();
            break;
        }
    }
}

void PedalboardGrid::addPedalCopy (const PedalInstance& srcInst, int gridX, int gridY)
{
    // Implementation not supported yet
}

bool PedalboardGrid::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (auto* inst = getSelectedInstance())
        {
            removePedal (inst->nodeID);
            return true;
        }
    }
    
    return false;
}

void PedalboardGrid::removePedal (AudioGraphEngine::NodeID nodeId)
{
    engine.removePedal (nodeId);
    deselectAll();
    rebuildFromEngine();
}

//==============================================================================
void PedalboardGrid::rebuildFromEngine()
{
    activePedalsList.refresh();
    if (boardCanvas)
        boardCanvas->rebuildBoards();
}

//==============================================================================
void PedalboardGrid::ActivePedalsList::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgMid);

    // Right border
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float)getHeight());

    // Title
    g.setColour (PedalForgeLookAndFeel::textPrimary);
    g.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
    g.drawText ("Active Pedals", 8, 6, getWidth() - 16, 20, juce::Justification::centredLeft);
}

void PedalboardGrid::ActivePedalsList::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (30); // title
    viewport.setBounds (area);

    int y = 0;
    int w = area.getWidth() - viewport.getScrollBarThickness() - 2;
    for (auto* row : rows)
    {
        row->setBounds (0, y, w, 44);
        y += 46;
    }
    content.setSize (w, y + 4);
}

void PedalboardGrid::ActivePedalsList::refresh()
{
    rows.clear();
    content.removeAllChildren();

    for (auto& inst : grid.engine.getPedalInstances())
    {
        auto* row = rows.add (new PedalRow (inst, grid));
        content.addAndMakeVisible (row);
    }

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (6);
    addAndMakeVisible (viewport);
    resized();
}

//==============================================================================
// PedalRow
//==============================================================================
PedalboardGrid::ActivePedalsList::PedalRow::PedalRow (const PedalInstance& inst, PedalboardGrid& g)
    : nodeID (inst.nodeID),
      name (inst.name),
      category (inst.category),
      colour (inst.colour),
      onBoard (inst.onBoard),
      gridW (inst.gridW),
      gridH (inst.gridH),
      grid (g) {}

void PedalboardGrid::ActivePedalsList::PedalRow::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);

    // Background
    auto bg = PedalForgeLookAndFeel::bgLight.withAlpha (0.5f);
    if (isMouseOver()) bg = PedalForgeLookAndFeel::accent.withAlpha (0.15f);
    g.setColour (bg);
    g.fillRoundedRectangle (bounds, 6.0f);

    // Border
    g.setColour (PedalForgeLookAndFeel::gridLine.withAlpha (0.4f));
    g.drawRoundedRectangle (bounds, 6.0f, 0.8f);

    auto inner = bounds.reduced (6.0f);

    // Colour dot
    g.setColour (colour);
    g.fillEllipse (inner.getX(), inner.getCentreY() - 5.0f, 10.0f, 10.0f);

    auto textArea = inner.withTrimmedLeft (16.0f);

    // Name
    g.setColour (PedalForgeLookAndFeel::textPrimary);
    g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
    g.drawText (name, textArea.removeFromTop (16.0f), juce::Justification::centredLeft);

    // Category + on-board badge
    g.setFont (juce::FontOptions (10.0f));
    g.setColour (PedalForgeLookAndFeel::textMuted);
    juce::String label = category;
    if (onBoard)
        label += "  •  ON BOARD";
    else
        label += "  •  drag to place";
    g.drawText (label, textArea, juce::Justification::centredLeft);
}

void PedalboardGrid::ActivePedalsList::PedalRow::mouseDown (const juce::MouseEvent&)
{
    dragStarted = false;

    // Click → select this pedal (look up live instance by NodeID)
    if (auto* list = dynamic_cast<ActivePedalsList*> (getParentComponent()->getParentComponent()->getParentComponent()))
    {
        if (list->onPedalClicked)
        {
            auto* inst = grid.engine.getPedalInstance (nodeID);
            list->onPedalClicked (inst);
        }
    }
}

void PedalboardGrid::ActivePedalsList::PedalRow::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragStarted && e.getDistanceFromDragStart() > 5 && ! onBoard)
    {
        dragStarted = true;
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            juce::String desc = "active_pedal:" + juce::String (nodeID.uid);
            juce::Image emptyImage (juce::Image::ARGB, 1, 1, true);
            container->startDragging (desc, this, emptyImage, false);
        }
    }
}
