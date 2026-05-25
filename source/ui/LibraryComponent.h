#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "../library/AssetLibrary.h"
#include "../network/Tone3000Client.h"
#include <functional>
#include <set>
#include <map>

//==============================================================================
/**
 * Library browser component.
 * Left sidebar: categories. Right: grid of assets.
 * Supports import and selection callbacks.
 */
class LibraryComponent : public juce::Component,
                         private juce::Timer
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
    juce::String currentSubcategory;     // empty = show all

    juce::TextEditor searchBox;
    juce::TextButton importBtn  { "Import" };
    juce::ComboBox subcategoryCombo;
    juce::TextButton newCategoryBtn { "+ Category" };

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
            int iw = currentItemW(), ih = currentItemH();
            int cols = juce::jmax (1, (viewportWidth - padding) / (iw + padding));
            int numItems = parent.showingCloud ? (int) parent.cloudResults.size() : (int) parent.filteredAssets.size();
            int rows = (numItems + cols - 1) / juce::jmax (1, cols);
            int totalH = juce::jmax (100, padding + rows * (ih + padding));
            setSize (viewportWidth, totalH);
        }

        std::set<int> selectedIndices;

    private:
        LibraryComponent& parent;

        static constexpr int itemW = 200;
        static constexpr int itemH = 56;
        static constexpr int imgItemW = 160;
        static constexpr int imgItemH = 140;
        static constexpr int padding = 8;

        bool isImageMode() const { return parent.currentCategoryID == "Images"; }

        int currentItemW() const { return isImageMode() ? imgItemW : itemW; }
        int currentItemH() const { return isImageMode() ? imgItemH : itemH; }
    };

    AssetGrid assetGrid { *this };
    juce::Viewport gridViewport;

    std::unique_ptr<juce::FileChooser> fileChooser;

    // Image thumbnail cache
    std::map<juce::String, juce::Image> thumbnailCache;
    juce::Image getThumbnail (const juce::File& file);
    void clearThumbnailCache() { thumbnailCache.clear(); }

    void rebuildSubcategoryCombo();

    //==========================================================================
    // TONE3000 Cloud Integration
    Tone3000Client cloudClient;
    bool showingCloud = false;

    juce::TextButton cloudToggle { juce::String(juce::CharPointer_UTF8("\xe2\x98\x81")) + " TONE3000" };
    juce::TextButton localToggle { juce::String(juce::CharPointer_UTF8("\xf0\x9f\x93\x81")) + " Local" };

    // Cloud filter buttons
    juce::TextButton filterAll { "All" };
    juce::TextButton filterAmps { "Amps" };
    juce::TextButton filterPedals { "Pedals" };
    juce::TextButton filterRigs { "Full Rigs" };
    juce::TextButton filterIRs { "IRs" };
    juce::String activeGearFilter; // e.g. "amp", "pedal", "full-rig", "ir"

    std::vector<ToneResult> cloudResults;
    int cloudTotalPages = 0;
    int cloudCurrentPage = 1;
    juce::TextButton prevPageBtn { juce::String(juce::CharPointer_UTF8("\xe2\x97\x80")) };
    juce::TextButton nextPageBtn { juce::String(juce::CharPointer_UTF8("\xe2\x96\xb6")) };
    juce::Label pageLabel;
    
    // Downloading state
    std::set<juce::String> downloadingIds;  // tone IDs currently downloading

    void performCloudSearch();
    void triggerCloudDownload (const ToneResult& tone);
    void switchToCloudMode (bool cloud);
    void setGearFilter (const juce::String& filter);

    // Search button visible in cloud mode
    juce::TextButton searchBtn { juce::String(juce::CharPointer_UTF8("\xf0\x9f\x94\x8d")) + " Search" };
    juce::TextButton apiKeyBtn { "Cloud Key" };

    // API Key settings load/save helpers
    juce::String loadApiKey();
    void saveApiKey (const juce::String& key);
    void promptForApiKey (std::function<void()> onSuccessCallback);

    // Debounce timer for cloud auto-search
    void timerCallback() override;
    void scheduleCloudSearch();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryComponent)
};
