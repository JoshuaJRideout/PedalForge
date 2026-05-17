#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../library/AssetLibrary.h"
#include <functional>
#include <set>

//==============================================================================
/**
 * Library browser component.
 * Left sidebar: categories. Right: grid of assets.
 * Supports import and selection callbacks.
 */
class LibraryComponent : public juce::Component
{
public:
    LibraryComponent();
    ~LibraryComponent() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setSidebarVisible (bool shouldBeVisible)
    {
        sidebarVisible = shouldBeVisible;
        categoryTree.setVisible (sidebarVisible);
        resized();
    }

    /** Refresh the asset grid for the current category. */
    void refreshAssets();

    /** Select a specific category by name (e.g. "NAM"). */
    void selectCategory (const juce::String& category);

    void setCategory(const juce::String& display, const juce::String& id)
    {
        currentCategoryDisplay = display;
        currentCategoryID = id;
        refreshAssets();
    }

    /** Called when the user selects an asset. Provides the full file path. */
    std::function<void (const juce::File&)> onAssetSelected;

    AssetLibrary& getLibrary() { return library; }

private:
    AssetLibrary library;
    bool sidebarVisible = true;

    juce::TreeView categoryTree;

    juce::String currentCategoryDisplay = "Pedals";
    juce::String currentCategoryID      = "Pedals";

    juce::TextEditor searchBox;
    juce::TextButton importBtn  { "Import" };

    //==========================================================================
    // Asset grid
    std::vector<AssetLibrary::AssetItem> currentAssets;
    std::vector<AssetLibrary::AssetItem> filteredAssets;

    void applyFilter();

    class AssetGrid : public juce::Component
    {
    public:
        AssetGrid (LibraryComponent& owner) : parent (owner)
        {
            setWantsKeyboardFocus (true);
        }

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        bool keyPressed (const juce::KeyPress& key) override;
        void resized() override { repaint(); }

        int getItemAtPosition (juce::Point<int> pos) const;

        /** Recalculate the component height based on the number of items. */
        void updateSize (int viewportWidth)
        {
            int cols = juce::jmax (1, (viewportWidth - padding) / (itemW + padding));
            int rows = ((int) parent.filteredAssets.size() + cols - 1) / juce::jmax (1, cols);
            int totalH = juce::jmax (100, padding + rows * (itemH + padding));
            setSize (viewportWidth, totalH);
        }

        std::set<int> selectedIndices;

    private:
        LibraryComponent& parent;

        static constexpr int itemW = 200;
        static constexpr int itemH = 56;
        static constexpr int padding = 8;
    };

    AssetGrid assetGrid { *this };
    juce::Viewport gridViewport;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryComponent)
};
