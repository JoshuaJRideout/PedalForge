#include "PedalComponent.h"
#include "PedalboardGrid.h"
#include "BoardComponent.h"
#include "PedalPainter.h"
#include "LookAndFeel.h"
#include "../dsp/PedalDesign.h"
#include "../dsp/GraphPedalProcessor.h"

//==============================================================================
PedalComponent::PedalComponent (PedalInstance& inst, AudioGraphEngine& eng, MidiLearnManager& midiMgr)
    : instance (inst), engine (eng), midiLearn (midiMgr)
{
}

//==============================================================================
void PedalComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // ── Drag tint ──
    float alpha = 1.0f;
    if (dragging) alpha = 0.65f;

    // Use PedalPainter::paintDesign (which handles null designs by drawing a fallback outline)
    PedalPainter::paintDesign (g, bounds, instance.design.get(),
                               instance.controlValues, instance.controlTexts, instance.bypassed, alpha);

    // ── Selection / drag overlays ──
    if (dragging)
    {
        auto borderColour = dragValid ? PedalForgeLookAndFeel::success
                                      : PedalForgeLookAndFeel::danger;
        g.setColour (borderColour.withAlpha (0.6f));
        float cornerR = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.08f;
        g.drawRoundedRectangle (bounds.reduced (bounds.getWidth() * 0.04f), cornerR, 2.0f);
    }
    else if (selected)
    {
        g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.6f));
        float cornerR = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.08f;
        g.drawRoundedRectangle (bounds.reduced (bounds.getWidth() * 0.04f), cornerR, 2.5f);
    }
}

//==============================================================================
void PedalComponent::mouseDown (const juce::MouseEvent& e)
{

    if (parentBoard != nullptr)
        parentBoard->grabKeyboardFocus();

    // 1. Check if we clicked on a control
    if (instance.design != nullptr)
    {
        auto bounds = getLocalBounds().toFloat();
        float margin = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.04f;
        auto body = bounds.reduced (margin);
        float scaleX = body.getWidth() / instance.design->chassisW;
        float scaleY = body.getHeight() / instance.design->chassisH;
        float sc = juce::jmin (scaleX, scaleY);
        float drawW = instance.design->chassisW * sc;
        float drawH = instance.design->chassisH * sc;
        float offX = body.getX() + (body.getWidth() - drawW) * 0.5f;
        float offY = body.getY() + (body.getHeight() - drawH) * 0.5f;

        float mx = (e.x - offX) / sc;
        float my = (e.y - offY) / sc;

        for (const auto& ctrl : instance.design->controls)
        {
            if (mx >= ctrl.x && mx <= ctrl.x + ctrl.width &&
                my >= ctrl.y && my <= ctrl.y + ctrl.height)
            {
                // If it's a right click, show MIDI Learn menu
                if (e.mods.isRightButtonDown() || e.mods.isCtrlDown())
                {
                    juce::String mappedParamID;
                    for (const auto& m : instance.design->mappings)
                        if (m.controlID == ctrl.controlID) { mappedParamID = m.nodeParam; break; }
                        
                    if (mappedParamID.isNotEmpty())
                    {
                        // To make the mapping instance-specific, we prefix the parameter ID with the node ID
                        juce::String uniqueParamID = juce::String(instance.nodeID.uid) + ":" + mappedParamID;
                        
                        juce::PopupMenu menu;
                        bool isLearning = midiLearn.getLearningParamId() == uniqueParamID;
                        int currentCC = midiLearn.getMappedCC (uniqueParamID);
                        
                        juce::String learnText = isLearning ? "Cancel MIDI Learn" : "Learn MIDI CC";
                        menu.addItem (1, learnText);
                        
                        if (currentCC >= 0)
                        {
                            menu.addSeparator();
                            menu.addItem (2, "Clear MIDI Mapping (CC " + juce::String(currentCC) + ")");
                        }
                        
                        juce::Component::SafePointer<PedalComponent> sp (this);
                        menu.showMenuAsync (juce::PopupMenu::Options(), [sp, uniqueParamID] (int result)
                        {
                            if (sp == nullptr) return;
                            if (result == 1)
                            {
                                if (sp->midiLearn.getLearningParamId() == uniqueParamID)
                                    sp->midiLearn.cancelLearning();
                                else
                                    sp->midiLearn.startLearning (uniqueParamID);
                            }
                            else if (result == 2)
                            {
                                sp->midiLearn.removeMapping (uniqueParamID);
                            }
                        });
                        return; // Handled right click on control
                    }
                }
                else
                {
                    if (ctrl.type == "footswitch" || ctrl.type == "switch" || ctrl.controlID.containsIgnoreCase("bypass"))
                    {
                        instance.bypassed = !instance.bypassed;
                        
                        if (auto* node = engine.getGraph().getNodeForId(instance.nodeID))
                        {
                            node->setBypassed(instance.bypassed);

                            auto* proc = node->getProcessor();
                            juce::String mappedParamID;
                            for (const auto& m : instance.design->mappings)
                                if (m.controlID == ctrl.controlID) { mappedParamID = m.nodeParam; break; }

                            if (mappedParamID.isNotEmpty())
                            {
                                for (auto* p : proc->getParameters())
                                {
                                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p))
                                    {
                                        if (ranged->getParameterID() == mappedParamID)
                                        {
                                            ranged->setValueNotifyingHost(instance.bypassed ? 1.0f : 0.0f);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        
                        repaint();
                        dragging = false;
                        return;
                    }
                    else if (ctrl.type == "knob" || ctrl.type == "slider")
                    {
                        draggedKnobID = ctrl.controlID;
                        
                        auto it = instance.controlValues.find (ctrl.controlID);
                        if (it != instance.controlValues.end())
                            draggedKnobStartValue = it->second;
                        else
                            draggedKnobStartValue = ctrl.defaultValue;
                            
                        dragging = false;
                        return;
                    }
                    else if (ctrl.type == "file_loader" || ctrl.type == "file_browser")
                    {
                        // Launch the file chooser
                        fileChooser = std::make_unique<juce::FileChooser> ("Select File", juce::File{}, "*.nam;*.wav;*.mp3;*.aif;*.flac");
                        auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
                        
                        juce::String mappedParamID;
                        for (const auto& m : instance.design->mappings)
                            if (m.controlID == ctrl.controlID) { mappedParamID = m.nodeParam; break; }
                            
                        int targetNodeID = -1;
                        if (mappedParamID.isNotEmpty())
                            targetNodeID = mappedParamID.upToFirstOccurrenceOf("_", false, false).getIntValue();
                            
                        juce::Component::SafePointer<PedalComponent> sp(this);
                        fileChooser->launchAsync(chooserFlags, [sp, targetNodeID](const juce::FileChooser& fc) {
                            if (fc.getResult().existsAsFile() && sp != nullptr && targetNodeID >= 0) {
                                if (auto* node = sp->engine.getGraph().getNodeForId(sp->instance.nodeID)) {
                                    if (auto* graphProc = dynamic_cast<GraphPedalProcessor*>(node->getProcessor())) {
                                        graphProc->setNodeFilePath(targetNodeID, fc.getResult().getFullPathName());
                                    }
                                }
                            }
                        });
                        dragging = false;
                        return;
                    }
                    else if (ctrl.type == "library_loader")
                    {
                        // Open the Library tab filtered to the appropriate category
                        juce::String mappedParamID;
                        for (const auto& m : instance.design->mappings)
                            if (m.controlID == ctrl.controlID) { mappedParamID = m.nodeParam; break; }

                        int targetNodeID = -1;
                        if (mappedParamID.isNotEmpty())
                            targetNodeID = mappedParamID.upToFirstOccurrenceOf("_", false, false).getIntValue();

                        // Determine library category from context
                        juce::String category = "NAM"; // Default; could be derived from node type
                        if (onOpenLibrary)
                            onOpenLibrary (category, targetNodeID);

                        dragging = false;
                        return;
                    }
                }
            }
        }
    }

    // 2. If it was a right-click but NOT on a control, show the pedal context menu
    if (e.mods.isRightButtonDown() || e.mods.isCtrlDown())
    {
        juce::PopupMenu menu;
        menu.addItem (1, instance.bypassed ? "Enable" : "Bypass");
        menu.addSeparator();
        
        if (parentBoard != nullptr)
        {
            menu.addItem (3, "Remove from Board");
            menu.addItem (2, "Delete");
        }
        else
        {
            menu.addItem (4, "Remove");
        }

        juce::Component::SafePointer<PedalComponent> sp (this);
        menu.showMenuAsync (juce::PopupMenu::Options(),
                            [sp] (int result)
                            {
                                if (sp == nullptr) return;
                                
                                if (result == 1)
                                {
                                    sp->instance.bypassed = ! sp->instance.bypassed;
                                    if (auto* node = sp->engine.getGraph().getNodeForId(sp->instance.nodeID))
                                        node->setBypassed(sp->instance.bypassed);
                                    sp->repaint();
                                }
                                else if (result == 2 && sp->parentBoard != nullptr && sp->parentBoard->getParentGrid() != nullptr)
                                {
                                    sp->parentBoard->getParentGrid()->removePedal (sp->instance.nodeID);
                                }
                                else if (result == 3 && sp->parentBoard != nullptr && sp->parentBoard->getParentGrid() != nullptr)
                                {
                                    sp->instance.onBoard = false;
                                    sp->parentBoard->getParentGrid()->rebuildFromEngine();
                                }
                                else if (result == 4 && sp->parentBoard == nullptr)
                                {
                                    sp->engine.removePedal (sp->instance.nodeID);
                                }
                            });
        return;
    }

    // Select this pedal (notify grid)
    if (parentBoard != nullptr && parentBoard->getParentGrid() != nullptr)
        parentBoard->getParentGrid()->selectPedal (this);

    if (e.mods.isAltDown() && parentBoard != nullptr && parentBoard->getParentGrid() != nullptr)
    {
        parentBoard->getParentGrid()->addPedalCopy (instance, instance.gridX + 1, instance.gridY + 1);
        dragging = false;
        return;
    }

    // If we're on a board, prepare to drag the pedal
    if (parentBoard != nullptr)
    {
        dragOffset = e.getPosition();
        dragSnappedGrid = { instance.gridX, instance.gridY };
        dragValid = true;
        dragging = true;
        toFront (true);
        repaint();
    }
}

void PedalComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedKnobID.isNotEmpty())
    {
        // Simple vertical drag logic
        float delta = -(float)e.getDistanceFromDragStartY() / 150.0f;
        float newVal = juce::jlimit (0.0f, 1.0f, draggedKnobStartValue + delta);
        
        instance.controlValues[draggedKnobID] = newVal;
        
        // Update audio parameter
        if (auto* node = engine.getGraph().getNodeForId(instance.nodeID))
        {
            auto* proc = node->getProcessor();
            juce::String mappedParamID;
            for (const auto& m : instance.design->mappings)
                if (m.controlID == draggedKnobID) { mappedParamID = m.nodeParam; break; }

            if (mappedParamID.isNotEmpty())
            {
                for (auto* p : proc->getParameters())
                {
                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p))
                    {
                        if (ranged->getParameterID() == mappedParamID)
                        {
                            ranged->setValueNotifyingHost (newVal);
                            break;
                        }
                    }
                }
            }
        }
        
        repaint();
        return;
    }

    if (! dragging || parentBoard == nullptr)
        return;

    // Compute where the mouse is in the grid's coordinate space
    auto mouseInParent = e.getEventRelativeTo (parentBoard).getPosition();

    // Offset so we snap relative to where the user grabbed the pedal
    int px = mouseInParent.x - dragOffset.x;
    int py = mouseInParent.y - dragOffset.y;

    // Snap to the nearest grid cell
    auto snapped = parentBoard->pixelToGrid (px + parentBoard->getCellSize() / 2,
                                             py + parentBoard->getCellSize() / 2);

    // Check if the pedal is being dragged outside the grid bounds
    bool offGrid = snapped.x < 0 || snapped.y < 0
                || snapped.x + instance.gridW > parentBoard->getGridCols()
                || snapped.y + instance.gridH > parentBoard->getGridRows();

    if (offGrid)
    {
        // Let the component follow the mouse freely (no grid snapping)
        dragValid = false;
        int cellSize = parentBoard->getCellSize();
        setBounds (px, py, instance.gridW * cellSize, instance.gridH * cellSize);
    }
    else
    {
        dragSnappedGrid = snapped;

        // Check if this position is free (ignoring our own footprint)
        dragValid = parentBoard->isGridRectFree (snapped.x, snapped.y,
                                                 instance.gridW, instance.gridH,
                                                 instance.nodeID);

        // Move component to the snapped pixel position
        auto pos = parentBoard->gridToPixel (snapped.x, snapped.y);
        int cellSize = parentBoard->getCellSize();
        setBounds (pos.x, pos.y, instance.gridW * cellSize, instance.gridH * cellSize);
    }

    repaint();
}

void PedalComponent::mouseUp (const juce::MouseEvent& e)
{
    if (draggedKnobID.isNotEmpty())
    {
        draggedKnobID = "";
        return;
    }

    if (! dragging)
        return;

    dragging = false;

    if (parentBoard != nullptr)
    {
        // Check if dropped outside the grid
        auto mouseInParent = e.getEventRelativeTo (parentBoard).getPosition();
        int px = mouseInParent.x - dragOffset.x;
        int py = mouseInParent.y - dragOffset.y;
        auto snapped = parentBoard->pixelToGrid (px + parentBoard->getCellSize() / 2,
                                                 py + parentBoard->getCellSize() / 2);

        bool offGrid = snapped.x < 0 || snapped.y < 0
                    || snapped.x + instance.gridW > parentBoard->getGridCols()
                    || snapped.y + instance.gridH > parentBoard->getGridRows();

        if (offGrid)
        {
            // Dragged off the board!
            if (engine.hasConnections (instance.nodeID))
            {
                // Has routing connections → remove from board but keep in engine
                instance.onBoard = false;
                parentBoard->rebuildFromEngine();
            }
            else
            {
                // No connections → delete entirely
                parentBoard->removePedal (instance.nodeID);
            }
            return;
        }

        if (dragValid)
        {
            // Commit the new position
            instance.gridX = dragSnappedGrid.x;
            instance.gridY = dragSnappedGrid.y;

            auto pos = parentBoard->gridToPixel (instance.gridX, instance.gridY);
            int cellSize = parentBoard->getCellSize();
            setBounds (pos.x, pos.y,
                       instance.gridW * cellSize, instance.gridH * cellSize);
        }
        else
        {
            // Invalid position — snap back to original
            auto pos = parentBoard->gridToPixel (instance.gridX, instance.gridY);
            int cellSize = parentBoard->getCellSize();
            setBounds (pos.x, pos.y,
                       instance.gridW * cellSize, instance.gridH * cellSize);
        }
    }

    repaint();
}
