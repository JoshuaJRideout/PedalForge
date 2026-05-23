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
    detailPanel.onOpenLibrary = [this] (const juce::String& category, std::function<void(const juce::File&)> cb)
    {
        if (onOpenLibrary) onOpenLibrary (category, cb);
    };
    detailPanel.onOpenOverlay = [this] (PedalInstance* inst, const juce::String& controlID)
    {
        if (onOpenOverlay) onOpenOverlay (inst, controlID);
    };

    addAndMakeVisible (btnInventory);
    btnInventory.onClick = [this] { if (onOpenInventory) onOpenInventory(); };

    addAndMakeVisible (activePedalsList);
    activePedalsList.onPedalClicked = [this] (PedalInstance* inst)
    {
        selectPedalByInstance (inst);
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

    addAndMakeVisible (btnToggleLeft);
    btnToggleLeft.setTooltip ("Toggle Left Panel");
    btnToggleLeft.onClick = [this] { showLeftPanel = !showLeftPanel; resized(); };

    addAndMakeVisible (btnToggleRight);
    btnToggleRight.setTooltip ("Toggle Right Panel");
    btnToggleRight.onClick = [this] { showRightPanel = !showRightPanel; resized(); };

    addAndMakeVisible (btnMaximizeRight);
    btnMaximizeRight.setTooltip ("Maximize Detail View");
    btnMaximizeRight.onClick = [this] { rightPanelMaximized = !rightPanelMaximized; resized(); };

    // Notes
    notesOverlay.setNotes (engine.boardNotes);
    addChildComponent (notesOverlay);
    addAndMakeVisible (btnNotes);
    btnNotes.setTooltip ("Toggle Notes");
    btnNotes.onClick = [this] {
        bool show = !notesOverlay.isNotesVisible();
        notesOverlay.setVisible (show);
        if (show && engine.boardNotes.empty())
            notesOverlay.addNote (120, 80);
    };

    // ── Toolbar: Grid combo ──
    gridCombo.setColour (juce::ComboBox::backgroundColourId, PedalForgeLookAndFeel::bgLight);
    gridCombo.setColour (juce::ComboBox::textColourId, PedalForgeLookAndFeel::textPrimary);
    gridCombo.setColour (juce::ComboBox::outlineColourId, PedalForgeLookAndFeel::gridLine);
    gridCombo.addItem ("Off", 1);
    gridCombo.addItem ("10mm", 2);
    gridCombo.addItem ("20mm", 3);
    gridCombo.addItem ("50mm", 4);
    gridCombo.addItem ("100mm", 5);
    
    float initialSnap = 0.0f;
    if (!engine.getBoards().empty()) initialSnap = engine.getBoards().front().snapGridSize;
    
    int s = 1;
    if (initialSnap == 10.0f) s = 2;
    else if (initialSnap == 20.0f) s = 3;
    else if (initialSnap == 50.0f) s = 4;
    else if (initialSnap == 100.0f) s = 5;
    gridCombo.setSelectedId (s, juce::dontSendNotification);
    
    gridCombo.onChange = [this] {
        int id = gridCombo.getSelectedId();
        float size = 0.0f;
        if (id == 1) size = 0.0f;
        else if (id == 2) size = 10.0f;
        else if (id == 3) size = 20.0f;
        else if (id == 4) size = 50.0f;
        else if (id == 5) size = 100.0f;
        
        for (auto& b : engine.getBoards())
            b.snapGridSize = size;
            
        repaint();
    };
    addAndMakeVisible (gridCombo);

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
    g.setGradientFill (juce::ColourGradient (
        PedalForgeLookAndFeel::bgMid.darker (0.1f), 0, (float)toolbarArea.getY(),
        PedalForgeLookAndFeel::bgMid.darker (0.35f), 0, (float)toolbarArea.getBottom(), false));
    g.fillRect (toolbarArea);
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawHorizontalLine (35, 0.0f, (float)getWidth());

    // Toolbar labels
    g.setColour (PedalForgeLookAndFeel::textMuted);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("GRID", gridCombo.getX() - 32, gridCombo.getY(), 30, 24, juce::Justification::centredRight);
}

void PedalboardGrid::timerCallback()
{
    bool needsRepaint = false;

    // Recursively collect all PedalComponent descendants
    std::vector<PedalComponent*> allPedals;
    std::function<void(juce::Component*)> collectPedals = [&](juce::Component* parent)
    {
        for (auto* child : parent->getChildren())
        {
            if (auto* pc = dynamic_cast<PedalComponent*> (child))
                allPedals.push_back (pc);
            else
                collectPedals (child);
        }
    };
    collectPedals (this);

    for (auto* pedalComp : allPedals)
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
                            if (mapping.nodeParam.endsWith("_filepath"))
                            {
                                int targetNodeID = mapping.nodeParam.upToFirstOccurrenceOf("_", false, false).getIntValue();
                                if (auto* graphProc = dynamic_cast<GraphPedalProcessor*>(proc))
                                {
                                    if (auto* targetNode = graphProc->getDSPGraph().getNode(targetNodeID))
                                    {
                                        juce::String path = targetNode->getFilePath();
                                        juce::String text = "Empty";
                                        if (path.isNotEmpty())
                                            text = juce::File(path).getFileNameWithoutExtension();
                                            
                                        juce::String baseControlID = mapping.controlID;
                                        int lineIndex = -1;
                                        if (baseControlID.containsChar(':'))
                                        {
                                            lineIndex = baseControlID.fromLastOccurrenceOf(":", false, false).getIntValue();
                                            baseControlID = baseControlID.upToFirstOccurrenceOf(":", false, false);
                                        }

                                        if (lineIndex >= 0)
                                        {
                                            juce::StringArray lines;
                                            lines.addLines (instance.controlTexts[baseControlID]);
                                            while (lines.size() <= lineIndex) lines.add ("");
                                            if (lines[lineIndex] != text)
                                            {
                                                lines.set (lineIndex, text);
                                                instance.controlTexts[baseControlID] = lines.joinIntoString ("\n");
                                                needsRepaint = true;
                                            }
                                        }
                                        else
                                        {
                                            if (instance.controlTexts[baseControlID] != text)
                                            {
                                                instance.controlTexts[baseControlID] = text;
                                                needsRepaint = true;
                                            }
                                        }
                                    }
                                }
                            }
                            else
                            {
                                for (auto* p : params)
                                {
                                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                                    {
                                        if (ranged->getParameterID() == mapping.nodeParam)
                                        {
                                            float val = ranged->getValue();
                                            juce::String text = ranged->getText (val, 32);

                                            juce::String baseControlID = mapping.controlID;
                                            int lineIndex = -1;
                                            if (baseControlID.containsChar(':'))
                                            {
                                                lineIndex = baseControlID.fromLastOccurrenceOf(":", false, false).getIntValue();
                                                baseControlID = baseControlID.upToFirstOccurrenceOf(":", false, false);
                                            }

                                            bool textChanged = false;
                                            if (lineIndex >= 0)
                                            {
                                                juce::StringArray lines;
                                                lines.addLines (instance.controlTexts[baseControlID]);
                                                while (lines.size() <= lineIndex) lines.add ("");
                                                if (lines[lineIndex] != text)
                                                {
                                                    lines.set (lineIndex, text);
                                                    instance.controlTexts[baseControlID] = lines.joinIntoString ("\n");
                                                    textChanged = true;
                                                }
                                            }
                                            else
                                            {
                                                if (instance.controlTexts[baseControlID] != text)
                                                {
                                                    instance.controlTexts[baseControlID] = text;
                                                    textChanged = true;
                                                }
                                            }

                                            if (instance.controlValues[baseControlID] != val || textChanged)
                                            {
                                                instance.controlValues[baseControlID] = val;
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

    // Sync focused pedal from engine → UI (for track button navigation)
    auto focusedId = engine.getFocusedPedal();
    if (focusedId.uid != 0 && focusedId.uid != lastFocusedNodeUID)
    {
        lastFocusedNodeUID = focusedId.uid;
        if (auto* inst = engine.getPedalInstance (focusedId))
            selectPedalByInstance (inst);
    }
}

//==============================================================================
void PedalboardGrid::resized()
{
    auto area = getLocalBounds();

    // Tab-specific toolbar area
    auto toolbar = area.removeFromTop (36);
    btnToggleLeft.setBounds (toolbar.removeFromLeft (60).reduced (4, 6));
    
    // Add grid combo layout like PedalDesigner
    toolbar.removeFromLeft (32); // Space for GRID label
    gridCombo.setBounds (toolbar.removeFromLeft (75).reduced (4, 6));
    
    btnInventory.setBounds (toolbar.removeFromLeft (140).reduced (8, 6));
    btnNotes.setBounds (toolbar.removeFromLeft (60).reduced (4, 6));

    btnToggleRight.setBounds (toolbar.removeFromRight (60).reduced (4, 6));
    btnMaximizeRight.setBounds (toolbar.removeFromRight (50).reduced (4, 6));
    btnAddBoard.setBounds (toolbar.removeFromRight (100).reduced (4, 6));

    // Active pedals sidebar on the left
    if (showLeftPanel)
    {
        activePedalsList.setBounds (area.removeFromLeft (170));
        activePedalsList.setVisible (true);
    }
    else
    {
        activePedalsList.setVisible (false);
    }

    if (detailPanel.hasSelection() && showRightPanel)
    {
        if (rightPanelMaximized)
        {
            detailPanel.setBounds (area);
            area.setWidth (0); // consumes all remaining area
        }
        else
        {
            detailPanel.setBounds (area.removeFromRight (detailPanelWidth));
        }
        detailPanel.setVisible (true);
    }
    else
    {
        detailPanel.setVisible (false);
    }

    // Centre the grid in the remaining space
    if (boardCanvas)
    {
        boardCanvas->setBounds (area);
        boardCanvas->setVisible (area.getWidth() > 0);
    }

    notesOverlay.setBounds (area);
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

    if (selectedComponent != nullptr)
        selectedComponent->setSelected (false);

    selectedComponent = comp;

    if (selectedComponent != nullptr)
    {
        selectedComponent->setSelected (true);
        selectedComponent->toFront (false);
        detailPanel.showPedal (selectedComponent->getInstance(), engine, &midiLearn);
        showRightPanel = true; // Auto-open right panel when a pedal is selected
        lastFocusedNodeUID = selectedComponent->getInstance().nodeID.uid;
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

void PedalboardGrid::selectPedalByInstance (PedalInstance* inst)
{
    if (boardCanvas == nullptr || inst == nullptr) return;
    
    std::function<PedalComponent*(juce::Component*)> findComp = [&](juce::Component* parent) -> PedalComponent* {
        for (auto* child : parent->getChildren())
        {
            if (auto* pc = dynamic_cast<PedalComponent*>(child))
            {
                if (&pc->getInstance() == inst) return pc;
            }
            if (auto* found = findComp (child)) return found;
        }
        return nullptr;
    };
    
    auto* comp = findComp (boardCanvas.get());
    selectPedal (comp);
}

void PedalboardGrid::deselectAll()
{
    if (selectedComponent != nullptr)
        selectedComponent->setSelected (false);

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
        if (selectedComponent->getInstance().design != nullptr)
        {
            selectedComponent->getInstance().boardW = std::max(100.0f, selectedComponent->getInstance().design->chassisW);
            selectedComponent->getInstance().boardH = std::max(100.0f, selectedComponent->getInstance().design->chassisH);
        }
        
        selectedComponent->repaint();
        detailPanel.showPedal (selectedComponent->getInstance(), engine, &midiLearn);
        resized();
        repaint();
    }
}

void PedalboardGrid::addPedalAtGrid (const juce::String& pedalName, float boardX, float boardY)
{
    auto& boards = engine.getBoards();
    if (boards.empty()) return;
    auto& config = boards.front(); // Use the first board for now

    bool loaded = false;
    for (auto& info : getFactoryPedals())
    {
        if (info.name == pedalName)
        {
            float placeX = boardX;
            float placeY = boardY;
            
            float bw = info.gridW * 100.0f;
            float bh = info.gridH * 100.0f;
            
            // Auto-place if coords are < 0
            if (placeX < 0.0f || placeY < 0.0f)
            {
                placeX = 0.0f; placeY = 0.0f;
                bool found = false;
                for (float y = 0.0f; y <= config.rows * 100.0f - bh && !found; y += 100.0f)
                {
                    for (float x = 0.0f; x <= config.cols * 100.0f - bw && !found; x += 100.0f)
                    {
                        // Check if rect is free
                        bool free = true;
                        juce::Rectangle<float> rect(x, y, bw, bh);
                        for (auto& inst : engine.getPedalInstances())
                        {
                            if (!inst.onBoard || inst.boardId != config.id || inst.pageIndex != config.activePage)
                                continue;
                            
                            juce::Rectangle<float> other(inst.boardX, inst.boardY, inst.boardW, inst.boardH);
                            if (rect.intersects(other))
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
                                           bw, bh);
                                            
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
            loaded = true;
            break;
        }
    }

    if (!loaded)
    {
        if (auto design = loadCustomPedalDesign (pedalName))
        {
            float bw = std::max(100.0f, design->chassisW);
            float bh = std::max(100.0f, design->chassisH);

            float placeX = boardX;
            float placeY = boardY;
            
            if (placeX < 0.0f || placeY < 0.0f)
            {
                placeX = 0.0f; placeY = 0.0f;
                bool found = false;
                for (float y = 0.0f; y <= config.rows * 100.0f - bh && !found; y += 100.0f)
                {
                    for (float x = 0.0f; x <= config.cols * 100.0f - bw && !found; x += 100.0f)
                    {
                        bool free = true;
                        juce::Rectangle<float> rect(x, y, bw, bh);
                        for (auto& inst : engine.getPedalInstances())
                        {
                            if (!inst.onBoard || inst.boardId != config.id || inst.pageIndex != config.activePage)
                                continue;
                            juce::Rectangle<float> other(inst.boardX, inst.boardY, inst.boardW, inst.boardH);
                            if (rect.intersects(other))
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

            juce::String jsonGraph;
            if (!design->effectsGraph.isVoid())
                jsonGraph = juce::JSON::toString(design->effectsGraph);
                
            auto processor = std::make_unique<GraphPedalProcessor>(design->name, jsonGraph);
            auto nodeId = engine.addPedal (std::move (processor),
                                           config.id, config.activePage,
                                           placeX, placeY,
                                           bw, bh);
                                            
            if (auto* inst = engine.getPedalInstance (nodeId))
            {
                inst->colour = design->chassisColour;
                inst->category = design->category;
                inst->design = design;

                int numKnobs = 0;
                for (const auto& c : design->controls)
                    if (c.type == "knob") numKnobs++;
                inst->numKnobs = numKnobs;
            }
            engine.autoRoutePedal(nodeId);
            rebuildFromEngine();
        }
    }
}

void PedalboardGrid::addPedalCopy (const PedalInstance& srcInst, float boardX, float boardY)
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
    AudioGraphEngine::NodeID selectedNode;
    if (selectedComponent != nullptr)
        selectedNode = selectedComponent->getInstance().nodeID;
        
    deselectAll();

    activePedalsList.refresh();
    notesOverlay.repaint();
    if (boardCanvas)
    {
        boardCanvas->rebuildBoards();
        
        if (selectedNode.uid != 0)
        {
            for (auto* c : boardCanvas->getChildren())
            {
                if (auto* board = dynamic_cast<BoardComponent*>(c))
                {
                    for (auto* cc : board->getChildren())
                    {
                        if (auto* pedal = dynamic_cast<PedalComponent*>(cc))
                        {
                            if (pedal->getInstance().nodeID == selectedNode)
                            {
                                selectPedal (pedal);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
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
      gridW ((int)std::max(1.0f, std::round(inst.boardW / 100.0f))),
      gridH ((int)std::max(1.0f, std::round(inst.boardH / 100.0f))),
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
}

void PedalboardGrid::ActivePedalsList::PedalRow::mouseUp (const juce::MouseEvent& e)
{
    if (! dragStarted && e.mouseWasClicked() && ! e.mods.isRightButtonDown() && ! e.mods.isCtrlDown())
    {
        if (grid.activePedalsList.onPedalClicked)
        {
            auto* inst = grid.engine.getPedalInstance (nodeID);
            grid.activePedalsList.onPedalClicked (inst);
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
