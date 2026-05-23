#include "PedalComponent.h"
#include "PedalboardGrid.h"
#include "BoardComponent.h"
#include "PedalPainter.h"
#include "LookAndFeel.h"
#include "PluginBrowserWindow.h"
#include "../dsp/PedalDesign.h"
#include "../dsp/GraphPedalProcessor.h"

//==============================================================================
PedalComponent::PedalComponent (PedalInstance& inst, AudioGraphEngine& eng, MidiLearnManager& midiMgr)
    : instance (inst), engine (eng), midiLearn (midiMgr)
{
}

//==============================================================================
juce::Rectangle<float> PedalComponent::getRenderBounds() const
{
    return getLocalBounds().toFloat();
}

//==============================================================================
void PedalComponent::paint (juce::Graphics& g)
{
    auto bounds = getRenderBounds();

    // ── Drag tint ──
    float alpha = 1.0f;
    if (dragging) alpha = 0.65f;

    // Use PedalPainter::paintDesign (which handles null designs by drawing a fallback outline)
    PedalPainter::paintDesign (g, bounds, instance.design.get(),
                               instance.controlValues, instance.controlTexts, instance.controlData, instance.bypassed, alpha);

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
        g.setColour (PedalForgeLookAndFeel::success.withAlpha (0.6f));
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
        auto bounds = getRenderBounds();
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
                    if (ctrl.type == "footswitch" || ctrl.type == "switch")
                    {
                        // WYSIWYG: only toggle the explicitly mapped parameter
                        juce::String mappedParamID;
                        for (const auto& m : instance.design->mappings)
                            if (m.controlID == ctrl.controlID) { mappedParamID = m.nodeParam; break; }

                        if (mappedParamID.isNotEmpty())
                        {
                            if (auto* node = engine.getGraph().getNodeForId(instance.nodeID))
                            {
                                auto* proc = node->getProcessor();
                                for (auto* p : proc->getParameters())
                                {
                                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(p))
                                    {
                                        if (ranged->getParameterID() == mappedParamID)
                                        {
                                            // Toggle: if > 0.5 → 0, else → 1
                                            float newVal = ranged->getValue() > 0.5f ? 0.0f : 1.0f;
                                            ranged->setValueNotifyingHost(newVal);
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
                    else if (ctrl.type == "plugin_browser")
                    {
                        juce::String mappedParamID;
                        for (const auto& m : instance.design->mappings)
                            if (m.controlID == ctrl.controlID) { mappedParamID = m.nodeParam; break; }
                            
                        int targetNodeID = -1;
                        if (mappedParamID.isNotEmpty())
                            targetNodeID = mappedParamID.upToFirstOccurrenceOf("_", false, false).getIntValue();
                            
                        new PluginBrowserWindow([this, targetNodeID, ctrl](const juce::PluginDescription& desc) {
                            if (targetNodeID >= 0) {
                                if (auto* node = engine.getGraph().getNodeForId(instance.nodeID)) {
                                    if (auto* graphProc = dynamic_cast<GraphPedalProcessor*>(node->getProcessor())) {
                                        graphProc->setNodeFilePath(targetNodeID, desc.fileOrIdentifier);
                                        instance.controlTexts[ctrl.controlID] = desc.name;
                                        repaint();
                                    }
                                }
                            }
                        });
                        
                        dragging = false;
                        return;
                    }
                    else if (ctrl.type == "file_loader" || ctrl.type == "file_browser")
                    {
                        // Launch the file chooser
                        fileChooser = std::make_unique<juce::FileChooser> ("Select File", juce::File{}, "*.nam;*.wav;*.mp3;*.aif;*.flac;*.vst3;*.component");
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

                        juce::String category = ctrl.libraryCategory.isNotEmpty() ? ctrl.libraryCategory : "NAM";
                        if (onOpenLibrary)
                        {
                            auto safeEngineRef = &engine;
                            auto safeNodeID = instance.nodeID;
                            onOpenLibrary (category, [safeEngineRef, safeNodeID, targetNodeID](const juce::File& file) {
                                if (auto* node = safeEngineRef->getGraph().getNodeForId (safeNodeID))
                                {
                                    if (auto* graphProc = dynamic_cast<GraphPedalProcessor*> (node->getProcessor()))
                                        graphProc->setNodeFilePath (targetNodeID, file.getFullPathName());
                                }
                            });
                        }

                        dragging = false;
                        return;
                    }
                    else if (ctrl.type == "overlay_launcher")
                    {
                        if (onOpenOverlay)
                        {
                            onOpenOverlay (ctrl.overlayPage);
                        }
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
                                    {
                                        if (auto* proc = node->getProcessor())
                                        {
                                            for (auto* p : proc->getParameters())
                                            {
                                                if (auto* bp = dynamic_cast<juce::AudioParameterBool*>(p))
                                                {
                                                    if (bp->getParameterID() == "bypass")
                                                    {
                                                        bp->setValueNotifyingHost(sp->instance.bypassed ? 1.0f : 0.0f);
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }
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
        parentBoard->getParentGrid()->addPedalCopy (instance, instance.boardX + 100.0f, instance.boardY + 100.0f);
        dragging = false;
        return;
    }


    if (parentBoard != nullptr)
    {
        // Check if there is an active text editor first
        if (auto* currentEditor = juce::Component::getCurrentlyFocusedComponent())
        {
            if (dynamic_cast<juce::TextEditor*>(currentEditor) != nullptr)
            {
                // Just clear focus so the user can continue playing
                currentEditor->unfocusAllComponents();
            }
        }
        
        parentBoard->getParentGrid()->selectPedal (this);
        dragging = true;
        
        auto mouseInParent = e.getEventRelativeTo (parentBoard).getPosition();
        dragOffset = mouseInParent - getBounds().getPosition();
        dragSnappedBoard = { instance.boardX, instance.boardY };
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

    // Snap to the nearest board coordinate based on the grid config
    auto snapped = parentBoard->snapToGrid (px, py - 40); // 40 = getHeaderHeight()

    // Check if the pedal is being dragged outside the grid bounds
    bool offGrid = snapped.x < 0.0f || snapped.y < 0.0f
                || snapped.x + instance.boardW > parentBoard->getBoardWidth()
                || snapped.y + instance.boardH > parentBoard->getBoardHeight();

    if (offGrid)
    {
        // Let the component follow the mouse freely (no grid snapping)
        dragValid = false;
        setBounds (px, py, instance.boardW, instance.boardH);
    }
    else
    {
        dragSnappedBoard = snapped;

        // Check if this position is free (ignoring our own footprint)
        dragValid = parentBoard->isGridRectFree (snapped.x, snapped.y,
                                                 instance.boardW, instance.boardH,
                                                 instance.nodeID);

        // Move component to the snapped position
        setBounds (snapped.x, 40 + snapped.y, instance.boardW, instance.boardH);
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
        auto snapped = parentBoard->snapToGrid (px, py - 40);

        bool offGrid = snapped.x < 0.0f || snapped.y < 0.0f
                    || snapped.x + instance.boardW > parentBoard->getBoardWidth()
                    || snapped.y + instance.boardH > parentBoard->getBoardHeight();

        if (offGrid)
        {
            // Just mark as invalid so it snaps back to its original valid position
            dragValid = false;
        }

        if (dragValid)
        {
            // Commit the new position
            instance.boardX = dragSnappedBoard.x;
            instance.boardY = dragSnappedBoard.y;

            setBounds (instance.boardX, 40 + instance.boardY,
                       instance.boardW, instance.boardH);
        }
        else
        {
            // Invalid position — snap back to original
            setBounds (instance.boardX, 40 + instance.boardY,
                       instance.boardW, instance.boardH);
        }
    }

    repaint();
}
