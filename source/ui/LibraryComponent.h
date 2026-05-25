#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "../library/AssetLibrary.h"
#include "../dsp/PedalDesign.h"
#include "../network/Tone3000Client.h"
#include "../network/Tone3000OAuth.h"
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
        static constexpr int pedalItemW = 120;
        static constexpr int pedalItemH = 200;
        static constexpr int boardItemW = 200;
        static constexpr int boardItemH = 120;
        static constexpr int padding = 8;

        bool isImageMode() const { return parent.currentCategoryID == "Images"; }
        bool isPedalMode() const { return parent.currentCategoryID == "Pedals"; }
        bool isBoardMode() const { return parent.currentCategoryID == "Boards"; }

        int currentItemW() const
        {
            if (isImageMode()) return imgItemW;
            if (isPedalMode()) return pedalItemW;
            if (isBoardMode()) return boardItemW;
            return itemW;
        }
        int currentItemH() const
        {
            if (isImageMode()) return imgItemH;
            if (isPedalMode()) return pedalItemH;
            if (isBoardMode()) return boardItemH;
            return itemH;
        }
    };

    AssetGrid assetGrid { *this };
    juce::Viewport gridViewport;

    //==========================================================================
    // Table view — used for everything except Images (which stays as a
    // thumbnail grid). Supports sortable, resizable, reorderable columns.
    class AssetTable : public juce::Component, public juce::TableListBoxModel
    {
    public:
        AssetTable (LibraryComponent& owner);

        /** Re-read parent.filteredAssets and refresh the table. Call after
            refreshAssets() / applyFilter(). */
        void refresh();

        // TableListBoxModel
        int  getNumRows() override;
        void paintRowBackground (juce::Graphics&, int rowNumber, int width, int height, bool rowIsSelected) override;
        void paintCell (juce::Graphics&, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
        void cellClicked (int rowNumber, int columnId, const juce::MouseEvent& e) override;
        void cellDoubleClicked (int rowNumber, int columnId, const juce::MouseEvent& e) override;
        void sortOrderChanged (int newSortColumnId, bool isForwards) override;
        void backgroundClicked (const juce::MouseEvent&) override;
        juce::String getCellTooltip (int rowNumber, int columnId) override;

        void resized() override { table.setBounds (getLocalBounds()); }

    private:
        LibraryComponent& parent;
        juce::TableListBox table { "AssetTable", this };

        // displayOrder[row] → index in parent.filteredAssets
        std::vector<int> displayOrder;
        int  sortColumnId = 1;   // 1 = Name
        bool sortAscending = true;

        void recomputeOrder();
        void showRowContextMenu (int rowNumber);
    };

    AssetTable assetTable { *this };

    //==========================================================================
    // View mode toggle — visual categories (Pedals/Boards/Images) default to
    // icon view; file-list categories (NAM/IR/Presets) default to table.
    // The user can flip the active category via the toolbar Grid/Table buttons;
    // we remember each category's preference for the rest of the session.
    std::map<juce::String, bool> categoryShowsIcons;

    bool categorySupportsBothViews (const juce::String& cat) const
    {
        return cat == "Pedals" || cat == "Boards" || cat == "Images";
    }

    bool currentCategoryShowsIcons() const
    {
        if (! categorySupportsBothViews (currentCategoryID)) return false;
        auto it = categoryShowsIcons.find (currentCategoryID);
        return it == categoryShowsIcons.end() ? true : it->second;
    }

    juce::TextButton btnIconView  { "Icons" };
    juce::TextButton btnTableView { "Table" };

    void loadViewModePrefs();
    void saveViewModePrefs() const;

    // Cached PedalDesigns for the Pedals icon view — re-parsing every paint
    // would be expensive. Refilled by refreshAssets().
    std::map<juce::String, std::shared_ptr<PedalDesign>> pedalDesignCache;

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::FileChooser> exportChooser;

    // Image thumbnail cache
    std::map<juce::String, juce::Image> thumbnailCache;
    juce::Image getThumbnail (const juce::File& file);
    void clearThumbnailCache() { thumbnailCache.clear(); }

    void rebuildSubcategoryCombo();

    //==========================================================================
    // TONE3000 Cloud Integration
    Tone3000OAuth   cloudAuth;
    Tone3000Client  cloudClient { cloudAuth };
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
    juce::TextButton signInBtn { "Sign in" };

    // Sign-in driver — invokes OAuth then calls the continuation on success.
    void startSignIn (std::function<void()> onSuccess);
    void refreshSignInButton();

    // Debounce timer for cloud auto-search
    void timerCallback() override;
    void scheduleCloudSearch();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryComponent)
};
