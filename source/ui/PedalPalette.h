#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../pedals/PedalRegistry.h"
#include <memory>

struct PedalDesign;

//==============================================================================
/**
 * Left sidebar showing available pedals to drag onto the board.
 * Each entry shows the pedal name, category, and grid footprint.
 * Wraps items in a Viewport for scrolling when the list is long.
 *
 * NOTE: The parent PedalForgeEditor is the DragAndDropContainer.
 * PaletteItems just initiate drags via findParentDragContainerFor.
 */
class PedalPalette : public juce::Component
{
public:
    PedalPalette();

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Reload user designs from the designs directory. */
    void loadUserDesigns();

private:
    //==========================================================================
    class PaletteItem : public juce::Component
    {
    public:
        PaletteItem (const PedalInfo& info);

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;

    private:
        PedalInfo info;
        bool dragStarted = false;
    };

    /** Container component that holds all palette items for the Viewport. */
    class PaletteContent : public juce::Component
    {
    public:
        void layoutItems (int width);
        std::vector<std::unique_ptr<PaletteItem>> items;
    };

    juce::Viewport viewport;
    PaletteContent content;

    // Storage for loaded user designs (kept alive while in the palette)
    std::vector<std::shared_ptr<PedalDesign>> loadedDesigns;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalPalette)
};
