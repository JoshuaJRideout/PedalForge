#include "PedalComponent.h"
#include "PedalboardGrid.h"
#include "PedalPainter.h"
#include "LookAndFeel.h"
#include "../dsp/PedalDesign.h"

//==============================================================================
PedalComponent::PedalComponent (PedalInstance& inst, AudioGraphEngine& eng)
    : instance (inst), engine (eng)
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

    if (parentGrid != nullptr)
        parentGrid->grabKeyboardFocus();

    if (e.mods.isRightButtonDown() || e.mods.isCtrlDown())
    {
        juce::PopupMenu menu;
        menu.addItem (1, instance.bypassed ? "Enable" : "Bypass");
        menu.addSeparator();
        menu.addItem (2, "Remove");

        menu.showMenuAsync (juce::PopupMenu::Options(),
                            [this] (int result)
                            {
                                if (result == 1)
                                {
                                    instance.bypassed = ! instance.bypassed;
                                    if (auto* node = engine.getGraph().getNodeForId(instance.nodeID))
                                        node->setBypassed(instance.bypassed);
                                    repaint();
                                }
                                else if (result == 2 && parentGrid != nullptr)
                                {
                                    parentGrid->removePedal (instance.nodeID);
                                }
                            });
        return;
    }

    // Select this pedal (notify grid)
    if (parentGrid != nullptr)
        parentGrid->selectPedal (this);

    if (e.mods.isAltDown() && parentGrid != nullptr)
    {
        parentGrid->addPedalCopy (instance, instance.gridX + 1, instance.gridY + 1);
        dragging = false;
        return;
    }

    // Check if we clicked on a footswitch or toggle switch
    if (instance.design != nullptr)
    {
        float scaleX = getWidth() / instance.design->chassisW;
        float scaleY = getHeight() / instance.design->chassisH;
        float mx = e.x / scaleX;
        float my = e.y / scaleY;

        for (const auto& ctrl : instance.design->controls)
        {
            if (ctrl.type == "footswitch" || ctrl.type == "switch" || ctrl.controlID.containsIgnoreCase("bypass"))
            {
                if (mx >= ctrl.x && mx <= ctrl.x + ctrl.width &&
                    my >= ctrl.y && my <= ctrl.y + ctrl.height)
                {
                    instance.bypassed = !instance.bypassed;
                    
                    if (auto* node = engine.getGraph().getNodeForId(instance.nodeID))
                    {
                        // 1. Toggle node bypass state
                        node->setBypassed(instance.bypassed);

                        // 2. Also toggle the parameter mapped to this control, if any
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
                    // Don't drag if we just toggled a switch
                    dragging = false;
                    return;
                }
            }
        }
    }

    // Record the mouse offset within the component for snapping
    dragOffset = e.getPosition();
    dragSnappedGrid = { instance.gridX, instance.gridY };
    dragValid = true;
    dragging = true;
    toFront (true);
    repaint();
}

void PedalComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging || parentGrid == nullptr)
        return;

    // Compute where the mouse is in the grid's coordinate space
    auto mouseInParent = e.getEventRelativeTo (parentGrid).getPosition();

    // Offset so we snap relative to where the user grabbed the pedal
    int px = mouseInParent.x - dragOffset.x;
    int py = mouseInParent.y - dragOffset.y;

    // Snap to the nearest grid cell
    auto snapped = parentGrid->pixelToGrid (px + parentGrid->getCellSize() / 2,
                                             py + parentGrid->getCellSize() / 2);

    // Clamp to grid bounds
    snapped.x = juce::jlimit (0, parentGrid->getGridCols() - instance.gridW, snapped.x);
    snapped.y = juce::jlimit (0, parentGrid->getGridRows() - instance.gridH, snapped.y);

    dragSnappedGrid = snapped;

    // Check if this position is free (ignoring our own footprint)
    dragValid = parentGrid->isGridRectFree (snapped.x, snapped.y,
                                             instance.gridW, instance.gridH,
                                             instance.nodeID);

    // Move component to the snapped pixel position
    auto pos = parentGrid->gridToPixel (snapped.x, snapped.y);
    int cellSize = parentGrid->getCellSize();
    setBounds (pos.x, pos.y, instance.gridW * cellSize, instance.gridH * cellSize);
}

void PedalComponent::mouseUp (const juce::MouseEvent& /*e*/)
{
    if (! dragging)
        return;

    dragging = false;

    if (parentGrid != nullptr)
    {
        if (dragValid)
        {
            // Commit the new position
            instance.gridX = dragSnappedGrid.x;
            instance.gridY = dragSnappedGrid.y;

            auto pos = parentGrid->gridToPixel (instance.gridX, instance.gridY);
            int cellSize = parentGrid->getCellSize();
            setBounds (pos.x, pos.y,
                       instance.gridW * cellSize, instance.gridH * cellSize);
        }
        else
        {
            // Invalid position — snap back to original
            auto pos = parentGrid->gridToPixel (instance.gridX, instance.gridY);
            int cellSize = parentGrid->getCellSize();
            setBounds (pos.x, pos.y,
                       instance.gridW * cellSize, instance.gridH * cellSize);
        }
    }

    repaint();
}
