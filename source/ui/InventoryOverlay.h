#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PedalPalette.h"

//==============================================================================
/**
 * A full-screen, pause-menu-style inventory overlay (inspired by Wiremod's Q-menu).
 *
 * Layout (3-column):
 *   LEFT:   Category list + search bar
 *   CENTER: Scrollable grid of items matching the active category/search
 *   RIGHT:  Preview/properties for the hovered or selected item
 *
 * Toggle with Tab key, dismiss with Escape or clicking the backdrop.
 */
class InventoryOverlay : public juce::Component,
                         public juce::KeyListener
{
public:
    InventoryOverlay();
    ~InventoryOverlay() override;

    /** The active workspace determines which items the Q-menu shows. */
    enum class Context { Board, Route, Forge, FX };
    void setContext (Context ctx);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    void toggle();
    void show();
    void hide();

    bool isOpen() const { return visible; }

    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    PedalPalette& getPedalPalette() { return pedalPalette; }

private:
    //==========================================================================
    // Data model for items in the grid
    //==========================================================================
    struct InventoryItem
    {
        juce::String id;           // unique identifier (e.g. "knob", "Clean Boost")
        juce::String displayName;  // what shows in the grid
        juce::String category;     // "Controls", "Lights", "Screens", etc.
        juce::String mainCategory; // "Pedals" or "Parts"
        juce::String description;  // shown in the right panel
        bool isFactory = true;

        // For pedal items
        PedalInfo pedalInfo;
        std::shared_ptr<PedalDesign> pedalDesign;

        // For hardware items
        juce::String hardwareType; // "knob", "switch", etc.
    };

    //==========================================================================
    // Left panel — category tree + search
    //==========================================================================
    class CategoryPanel : public juce::Component
    {
    public:
        CategoryPanel();
        void paint (juce::Graphics& g) override;
        void resized() override;

        std::function<void (const juce::String& mainCat, const juce::String& subCat)> onCategorySelected;
        std::function<void (const juce::String& query)> onSearchChanged;

        void setCategories (const juce::StringArray& mainCategories,
                            const std::map<juce::String, juce::StringArray>& subCategories);
        void selectCategory (const juce::String& main, const juce::String& sub);

    private:
        juce::TextEditor searchBox;
        
        struct CatButton : public juce::TextButton
        {
            juce::String mainCat, subCat;
        };

        juce::Viewport viewport;
        juce::Component content;
        juce::OwnedArray<CatButton> buttons;
        CatButton* activeButton = nullptr;
    };

    //==========================================================================
    // Center panel — item grid
    //==========================================================================
    class ItemGrid : public juce::Component
    {
    public:
        ItemGrid();
        void paint (juce::Graphics& g) override;
        void resized() override;

        void setItems (const std::vector<InventoryItem*>& itemsToShow);

        std::function<void (InventoryItem*)> onItemHovered;
        std::function<void (InventoryItem*)> onItemSelected;

    private:
        struct GridCell : public juce::Component
        {
            GridCell (InventoryItem& item);
            void paint (juce::Graphics& g) override;
            void mouseEnter (const juce::MouseEvent&) override;
            void mouseDown (const juce::MouseEvent& e) override;
            void mouseDrag (const juce::MouseEvent& e) override;

            InventoryItem& item;
            bool dragStarted = false;

            std::function<void (InventoryItem*)> onHover;
            std::function<void (InventoryItem*)> onClick;
        };

        juce::Viewport viewport;
        juce::Component content;
        juce::OwnedArray<GridCell> cells;
    };

    //==========================================================================
    // Right panel — item preview/properties
    //==========================================================================
    class PreviewPanel : public juce::Component
    {
    public:
        void paint (juce::Graphics& g) override;
        void showItem (InventoryItem* item);

    private:
        InventoryItem* currentItem = nullptr;
    };

    //==========================================================================
    // State
    //==========================================================================
    bool visible = false;

    // Three columns
    CategoryPanel categoryPanel;
    ItemGrid itemGrid;
    PreviewPanel previewPanel;

    // Data
    std::vector<InventoryItem> allItems;
    std::vector<InventoryItem*> filteredItems;

    juce::String currentMainCategory;
    juce::String currentSubCategory;    // empty = "All"
    juce::String searchQuery;
    Context context = Context::Board;

    void buildItemDatabase();
    void filterItems();

    // The PedalPalette is kept for its data-loading logic but not displayed directly
    PedalPalette pedalPalette;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InventoryOverlay)
};
