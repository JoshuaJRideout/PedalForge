#include "PedalboardGrid.h"
#include "LookAndFeel.h"
#include "../pedals/PedalRegistry.h"
#include "../dsp/GraphPedalProcessor.h"
#include "../dsp/PedalDesign.h"

//==============================================================================
PedalboardGrid::PedalboardGrid (AudioGraphEngine& eng)
    : engine (eng), routeOverlay (eng)
{
    addAndMakeVisible (routeOverlay);
    addAndMakeVisible (detailPanel);
    detailPanel.addListener (&detailListener);
    
    routeToggle.setColour (juce::ToggleButton::textColourId, PedalForgeLookAndFeel::textSecondary);
    routeToggle.setColour (juce::ToggleButton::tickColourId, PedalForgeLookAndFeel::accent);
    routeToggle.addListener (this);
    addAndMakeVisible (routeToggle);

    // Apply default board preset
    setBoardPreset (currentPresetIndex);

    startTimerHz (30);
}

PedalboardGrid::~PedalboardGrid()
{
}

//==============================================================================
void PedalboardGrid::paint (juce::Graphics& g)
{
    // Fill entire background
    int gridAreaWidth = detailPanel.hasSelection()
                            ? getWidth() - detailPanelWidth
                            : getWidth();
    auto fullArea = juce::Rectangle<int> (0, 0, gridAreaWidth, getHeight());
    g.setColour (PedalForgeLookAndFeel::bgDark);
    g.fillRect (fullArea);

    // The fixed-size board rect (centred)
    auto boardRect = juce::Rectangle<int> (
        gridOriginX, gridOriginY,
        gridCols * cellSize, gridRows * cellSize);

    // Board background — slightly lighter than the surround
    g.setColour (PedalForgeLookAndFeel::bgMid.withAlpha (0.3f));
    g.fillRoundedRectangle (boardRect.toFloat(), 6.0f);

    // Dot pattern
    g.setColour (PedalForgeLookAndFeel::gridLine.withAlpha (0.2f));
    for (int col = 0; col <= gridCols; ++col)
    {
        for (int row = 0; row <= gridRows; ++row)
        {
            float x = (float) (gridOriginX + col * cellSize);
            float y = (float) (gridOriginY + row * cellSize);
            float dotSize = (col % 4 == 0 && row % 4 == 0) ? 3.0f : 1.5f;
            g.fillEllipse (x - dotSize * 0.5f, y - dotSize * 0.5f,
                           dotSize, dotSize);
        }
    }

    // Faint grid lines
    g.setColour (PedalForgeLookAndFeel::gridLine.withAlpha (0.06f));
    for (int col = 0; col <= gridCols; ++col)
    {
        float x = (float) (gridOriginX + col * cellSize);
        g.drawVerticalLine ((int) x, (float) gridOriginY,
                            (float) (gridOriginY + gridRows * cellSize));
    }
    for (int row = 0; row <= gridRows; ++row)
    {
        float y = (float) (gridOriginY + row * cellSize);
        g.drawHorizontalLine ((int) y, (float) gridOriginX,
                              (float) (gridOriginX + gridCols * cellSize));
    }

    // Grid border
    g.setColour (PedalForgeLookAndFeel::gridLine.withAlpha (0.2f));
    g.drawRoundedRectangle (boardRect.toFloat(), 6.0f, 1.5f);

    // Board preset label (bottom-right corner of the board)
    auto& presets = getBoardPresets();
    if (currentPresetIndex >= 0 && currentPresetIndex < (int) presets.size())
    {
        auto& preset = presets[(size_t) currentPresetIndex];
        juce::String label = juce::String (preset.name) + " (" +
                              juce::String (preset.cols) + "×" +
                              juce::String (preset.rows) + ")";
        g.setColour (PedalForgeLookAndFeel::textMuted.withAlpha (0.3f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (label,
                    boardRect.getRight() - 100, boardRect.getBottom() + 4,
                    100, 14,
                    juce::Justification::centredRight);
    }

    // Drop preview
    if (showDropPreview)
    {
        auto pos = gridToPixel (dropPreviewX, dropPreviewY);
        auto previewRect = juce::Rectangle<float> (
            (float) pos.x, (float) pos.y,
            (float) (dropPreviewW * cellSize),
            (float) (dropPreviewH * cellSize));

        if (dropPreviewValid)
        {
            g.setColour (PedalForgeLookAndFeel::success.withAlpha (0.15f));
            g.fillRoundedRectangle (previewRect, 6.0f);
            g.setColour (PedalForgeLookAndFeel::success.withAlpha (0.4f));
            g.drawRoundedRectangle (previewRect, 6.0f, 2.0f);
        }
        else
        {
            g.setColour (PedalForgeLookAndFeel::danger.withAlpha (0.12f));
            g.fillRoundedRectangle (previewRect, 6.0f);
            g.setColour (PedalForgeLookAndFeel::danger.withAlpha (0.35f));
            g.drawRoundedRectangle (previewRect, 6.0f, 2.0f);
        }
    }
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
    // Detail panel on the right when a pedal is selected
    auto bounds = getLocalBounds();

    if (detailPanel.hasSelection())
    {
        detailPanel.setBounds (bounds.removeFromRight (detailPanelWidth));
        detailPanel.setVisible (true);
    }
    else
    {
        detailPanel.setVisible (false);
    }
    
    // Put routeToggle in the bottom left
    routeToggle.setBounds (bounds.getX() + 16, bounds.getBottom() - 40, 100, 24);

    // Centre the fixed-size board in the remaining area
    int boardW = gridCols * cellSize;
    int boardH = gridRows * cellSize;
    gridOriginX = bounds.getX() + (bounds.getWidth()  - boardW) / 2;
    gridOriginY = bounds.getY() + (bounds.getHeight() - boardH) / 2;

    // Clamp so it doesn't go negative
    gridOriginX = juce::jmax (gridOriginX, bounds.getX() + 8);
    gridOriginY = juce::jmax (gridOriginY, bounds.getY() + 8);

    routeOverlay.setBounds (bounds);

    // Reposition pedals at fixed cell size
    for (auto& comp : pedalComponents)
    {
        auto& inst = comp->getInstance();
        auto pos = gridToPixel (inst.gridX, inst.gridY);
        comp->setBounds (pos.x, pos.y,
                         inst.gridW * cellSize, inst.gridH * cellSize);
    }

    updateRoutes();
}

//==============================================================================
void PedalboardGrid::mouseDown (const juce::MouseEvent& /*e*/)
{
    grabKeyboardFocus();
    deselectAll();
}

//==============================================================================
bool PedalboardGrid::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.toString().startsWith ("pedal:");
}

void PedalboardGrid::itemDragEnter (const SourceDetails& details)
{
    auto desc = details.description.toString();
    auto parts = juce::StringArray::fromTokens (desc, ":", "");
    if (parts.size() < 2 || parts[0] != "pedal") return;

    auto pedalName = parts[1];
    for (auto& info : getFactoryPedals())
    {
        if (info.name == pedalName)
        {
            dropPreviewW = info.gridW;
            dropPreviewH = info.gridH;
            break;
        }
    }

    showDropPreview = true;
    routeOverlay.setDragMode (true);
    repaint();
}

void PedalboardGrid::itemDragMove (const SourceDetails& details)
{
    auto desc = details.description.toString();
    auto parts = juce::StringArray::fromTokens (desc, ":", "");
    float ratioX = 0.5f;
    float ratioY = 0.5f;
    if (parts.size() >= 4)
    {
        ratioX = parts[2].getFloatValue();
        ratioY = parts[3].getFloatValue();
    }

    int px = details.localPosition.x - (int) (ratioX * dropPreviewW * cellSize);
    int py = details.localPosition.y - (int) (ratioY * dropPreviewH * cellSize);
    auto gp = pixelToGrid (px, py);
    dropPreviewX = gp.x;
    dropPreviewY = gp.y;
    dropPreviewValid = isGridRectFree (dropPreviewX, dropPreviewY,
                                        dropPreviewW, dropPreviewH);
    repaint();
}

void PedalboardGrid::itemDragExit (const SourceDetails& /*details*/)
{
    showDropPreview = false;
    routeOverlay.setDragMode (false);
    repaint();
}

void PedalboardGrid::itemDropped (const SourceDetails& details)
{
    showDropPreview = false;
    routeOverlay.setDragMode (false);

    auto desc = details.description.toString();
    auto parts = juce::StringArray::fromTokens (desc, ":", "");
    if (parts.size() < 2 || parts[0] != "pedal") return;

    auto pedalName = parts[1];
    float ratioX = 0.5f;
    float ratioY = 0.5f;
    if (parts.size() >= 4)
    {
        ratioX = parts[2].getFloatValue();
        ratioY = parts[3].getFloatValue();
    }
    
    int px = details.localPosition.x - (int) (ratioX * dropPreviewW * cellSize);
    int py = details.localPosition.y - (int) (ratioY * dropPreviewH * cellSize);
    auto gridPos = pixelToGrid (px, py);

    addPedalAtGrid (pedalName, gridPos.x, gridPos.y);
}

//==============================================================================
void PedalboardGrid::addPedalAtGrid (const juce::String& pedalName,
                                      int gridX, int gridY)
{
    for (auto& info : getFactoryPedals())
    {
        if (info.name == pedalName)
        {
            if (! isGridRectFree (gridX, gridY, info.gridW, info.gridH))
            {
                bool placed = false;
                for (int radius = 1; radius < gridCols && ! placed; ++radius)
                {
                    for (int dx = -radius; dx <= radius && ! placed; ++dx)
                    {
                        for (int dy = -radius; dy <= radius && ! placed; ++dy)
                        {
                            int nx = gridX + dx, ny = gridY + dy;
                            if (isGridRectFree (nx, ny, info.gridW, info.gridH))
                            {
                                gridX = nx; gridY = ny;
                                placed = true;
                            }
                        }
                    }
                }
                if (! placed) return;
            }

            auto processor = info.factory();
            auto nodeId = engine.addPedal (std::move (processor),
                                            gridX, gridY,
                                            info.gridW, info.gridH);

            if (auto* inst = engine.getPedalInstance (nodeId))
            {
                inst->colour = info.colour;
                inst->category = info.category;
                inst->numKnobs = info.numKnobs;

                if (info.designFactory)
                {
                    inst->design = info.designFactory();
                    if (auto* proc = dynamic_cast<GraphPedalProcessor*>(engine.getGraph().getNodeForId(nodeId)->getProcessor()))
                        inst->design->effectsGraph = juce::JSON::parse (proc->saveGraph());
                }
                else
                {
                    inst->design = std::make_shared<PedalDesign>();
                    inst->design->name = inst->name;
                    inst->design->category = inst->category;
                    inst->design->chassisColour = inst->colour;
                    
                    // Fetch DSP graph for JSON and create knobs for parameters
                    if (auto* proc = dynamic_cast<GraphPedalProcessor*>(engine.getGraph().getNodeForId(nodeId)->getProcessor()))
                    {
                        inst->design->effectsGraph = juce::JSON::parse (proc->saveGraph());
                        
                        float x = 20, y = 40;
                        for (auto* param : proc->getParameters())
                        {
                            if (auto* pf = dynamic_cast<juce::AudioParameterFloat*> (param))
                            {
                                PedalDesign::Control ctrl;
                                ctrl.type = "knob";
                                ctrl.label = pf->name;
                                ctrl.controlID = "knob_" + juce::String (inst->design->controls.size() + 1);
                                ctrl.x = x;
                                ctrl.y = y;
                                ctrl.width = 40;
                                ctrl.height = 40;
                                inst->design->controls.push_back (ctrl);

                                PedalDesign::Mapping m;
                                m.controlID = ctrl.controlID;
                                m.nodeParam = pf->paramID;
                                inst->design->mappings.push_back (m);

                                x += 50;
                                if (x > 150) { x = 20; y += 60; }
                            }
                        }
                    }
                }
            }

            rebuildFromEngine();
            return;
        }
    }
}

void PedalboardGrid::addPedalCopy (const PedalInstance& srcInst, int gridX, int gridY)
{
    if (! isGridRectFree (gridX, gridY, srcInst.gridW, srcInst.gridH))
    {
        bool placed = false;
        for (int radius = 1; radius < gridCols && ! placed; ++radius)
        {
            for (int dx = -radius; dx <= radius && ! placed; ++dx)
            {
                for (int dy = -radius; dy <= radius && ! placed; ++dy)
                {
                    int nx = gridX + dx, ny = gridY + dy;
                    if (isGridRectFree (nx, ny, srcInst.gridW, srcInst.gridH))
                    {
                        gridX = nx; gridY = ny;
                        placed = true;
                    }
                }
            }
        }
        if (! placed) return;
    }

    for (auto& info : getFactoryPedals())
    {
        if (info.name == srcInst.name)
        {
            auto processor = info.factory();
            // TODO: Ideally we would copy internal processor state too
            auto nodeId = engine.addPedal (std::move (processor),
                                            gridX, gridY,
                                            info.gridW, info.gridH);

            if (auto* inst = engine.getPedalInstance (nodeId))
            {
                inst->colour = info.colour;
                inst->category = info.category;
                inst->numKnobs = info.numKnobs;
                if (srcInst.design != nullptr)
                    inst->design = std::make_shared<PedalDesign> (*srcInst.design);
            }

            rebuildFromEngine();
            
            // Select the new one
            for (auto& c : pedalComponents)
            {
                if (c->getInstance().nodeID == nodeId)
                {
                    selectPedal(c.get());
                    break;
                }
            }
            return;
        }
    }
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
    
    if (key.getModifiers().isCommandDown())
    {
        if (key.getKeyCode() == 'C' || key.getKeyCode() == 'c')
        {
            if (auto* inst = getSelectedInstance())
            {
                clipboardPedal = std::make_unique<PedalInstance>(*inst);
            }
            return true;
        }
        if (key.getKeyCode() == 'V' || key.getKeyCode() == 'v')
        {
            if (clipboardPedal)
            {
                auto pos = getMouseXYRelative();
                auto gridPos = pixelToGrid (pos.getX(), pos.getY());
                if (selectedPedal)
                {
                    gridPos.x = clipboardPedal->gridX + 1;
                    gridPos.y = clipboardPedal->gridY + 1;
                }
                
                addPedalCopy (*clipboardPedal, gridPos.x, gridPos.y);
            }
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
    // Save selection by nodeID so we can restore after rebuild
    // (the old PedalInstance pointers will be invalid after vector reallocation)
    auto previousSelectedNodeId = selectedComponent != nullptr
                                      ? selectedComponent->getInstance().nodeID
                                      : juce::AudioProcessorGraph::NodeID {};

    selectedComponent = nullptr;
    detailPanel.clearSelection();

    pedalComponents.clear();

    for (auto& inst : engine.getPedalInstances())
    {
        auto comp = std::make_unique<PedalComponent> (
            const_cast<PedalInstance&> (inst), engine);
        comp->setGrid (this);

        auto pos = gridToPixel (inst.gridX, inst.gridY);
        comp->setBounds (pos.x, pos.y,
                         inst.gridW * cellSize, inst.gridH * cellSize);

        addAndMakeVisible (*comp);
        pedalComponents.push_back (std::move (comp));
    }

    // Z-order: route overlay behind pedals, detail panel on top
    routeOverlay.toBack();
    for (auto& comp : pedalComponents)
        comp->toFront (false);
    detailPanel.toFront (false);

    // Restore selection if the previously-selected pedal still exists
    if (previousSelectedNodeId != juce::AudioProcessorGraph::NodeID {})
    {
        for (auto& comp : pedalComponents)
        {
            if (comp->getInstance().nodeID == previousSelectedNodeId)
            {
                selectPedal (comp.get());
                break;
            }
        }
    }

    updateRoutes();
    repaint();
}

//==============================================================================
void PedalboardGrid::selectPedal (PedalComponent* comp)
{
    // Deselect previous
    for (auto& c : pedalComponents)
        c->setSelected (false);

    selectedComponent = comp;

    if (comp != nullptr)
    {
        comp->setSelected (true);
        detailPanel.showPedal (comp->getInstance(), engine);
    }
    else
    {
        detailPanel.clearSelection();
    }

    if (onPedalSelected)
        onPedalSelected (comp ? &comp->getInstance() : nullptr);

    resized(); // Re-layout to show/hide detail panel
    repaint();
}

void PedalboardGrid::deselectAll()
{
    for (auto& c : pedalComponents)
        c->setSelected (false);

    selectedComponent = nullptr;
    detailPanel.clearSelection();
    resized();
    repaint();
}

void PedalboardGrid::refreshSelectedPedal()
{
    if (selectedComponent != nullptr)
    {
        // Re-assign size from design footprint in case it was modified
        selectedComponent->getInstance().gridW = (int)std::ceil (selectedComponent->getInstance().design->chassisW / 50.0f);
        selectedComponent->getInstance().gridH = (int)std::ceil (selectedComponent->getInstance().design->chassisH / 50.0f);
        
        // Sync visual appearance
        selectedComponent->repaint();
        detailPanel.showPedal (selectedComponent->getInstance(), engine);
        resized();
        repaint();
    }
}

//==============================================================================
void PedalboardGrid::snapPedalToGrid (PedalComponent& comp)
{
    auto& inst = comp.getInstance();
    auto gp = pixelToGrid (comp.getX(), comp.getY());

    gp.x = juce::jlimit (0, gridCols - inst.gridW, gp.x);
    gp.y = juce::jlimit (0, gridRows - inst.gridH, gp.y);

    if (! isGridRectFree (gp.x, gp.y, inst.gridW, inst.gridH, inst.nodeID))
    {
        gp.x = inst.gridX;
        gp.y = inst.gridY;
    }

    inst.gridX = gp.x;
    inst.gridY = gp.y;

    auto pos = gridToPixel (gp.x, gp.y);
    comp.setBounds (pos.x, pos.y,
                    inst.gridW * cellSize, inst.gridH * cellSize);

    updateRoutes();
}

void PedalboardGrid::updateRoutes()
{
    routeOverlay.rebuild (cellSize, gridOriginX, gridOriginY, gridCols, gridRows);
}

//==============================================================================
void PedalboardGrid::setBoardPreset (int presetIndex)
{
    auto& presets = getBoardPresets();
    if (presetIndex < 0 || presetIndex >= (int) presets.size())
        return;

    currentPresetIndex = presetIndex;
    gridCols = presets[(size_t) presetIndex].cols;
    gridRows = presets[(size_t) presetIndex].rows;

    // TODO: Remove pedals that are now outside the board bounds
    resized();
    repaint();
}

//==============================================================================
juce::Point<int> PedalboardGrid::gridToPixel (int gx, int gy) const
{
    return { gridOriginX + gx * cellSize,
             gridOriginY + gy * cellSize };
}

juce::Point<int> PedalboardGrid::pixelToGrid (int px, int py) const
{
    int gx = (px - gridOriginX + cellSize / 2) / cellSize;
    int gy = (py - gridOriginY + cellSize / 2) / cellSize;
    gx = juce::jlimit (0, gridCols - 1, gx);
    gy = juce::jlimit (0, gridRows - 1, gy);
    return { gx, gy };
}

bool PedalboardGrid::isGridRectFree (int gx, int gy, int gw, int gh,
                                      AudioGraphEngine::NodeID ignoreNodeId) const
{
    if (gx < 0 || gy < 0 || gx + gw > gridCols || gy + gh > gridRows)
        return false;

    for (auto& inst : engine.getPedalInstances())
    {
        if (inst.nodeID == ignoreNodeId) continue;
        if (gx < inst.gridX + inst.gridW && gx + gw > inst.gridX &&
            gy < inst.gridY + inst.gridH && gy + gh > inst.gridY)
            return false;
    }

    return true;
}

//==============================================================================
void PedalboardGrid::buttonClicked (juce::Button* button)
{
    if (button == &routeToggle)
    {
        setRoutingMode (routeToggle.getToggleState());
    }
}
