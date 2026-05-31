#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "InventoryModel.h"

//==============================================================================
/**
 * Docked "Add" inventory pane — the always-visible left strip that replaces the
 * full-screen Q-menu overlay. Narrow vertical layout: a search box on top, then
 * a scrollable list of placeable items grouped under category headers. Items
 * drag onto the workspace using the same descriptor strings the canvases already
 * parse, so drops behave exactly like the old overlay.
 *
 * Items come from the shared pf::inv model (single source of truth). The pane is
 * context-scoped (Board pedals / Forge parts / FX nodes …).
 */
class InventoryPanel : public juce::Component
{
public:
    InventoryPanel();

    /** Which workspace this pane feeds (picks the visible top-level categories). */
    void setContext (pf::inv::Context ctx);

    /** Rebuild the item list from the model (call after importing a .pfpedal etc.). */
    void refresh();

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Fired when an item is clicked (not dragged). The host decides what to do
        (e.g. add a pedal to the board). Drag-to-place is handled internally. */
    std::function<void (const pf::inv::Item&)> onItemClicked;

private:
    class Cell;

    void rebuildRows();

    pf::inv::Context context = pf::inv::Context::Forge;
    juce::String searchQuery;
    std::vector<pf::inv::Item> allItems;   // context-agnostic; filtered in rebuildRows()

    juce::TextEditor searchBox;
    juce::Viewport   viewport;
    juce::Component  content;
    juce::OwnedArray<juce::Component> rows; // category headers + item cells

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InventoryPanel)
};
