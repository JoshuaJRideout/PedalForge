#include "LibraryComponent.h"
#include "LookAndFeel.h"
#include "PedalPainter.h"
#include "../dsp/PedalDesign.h"
#include "../pedals/PedalRegistry.h"

//==============================================================================
class CategoryTreeItem : public juce::TreeViewItem
{
public:
    CategoryTreeItem(const juce::String& name, const juce::String& id, LibraryComponent& lib, bool isHeader = false)
        : displayName(name), categoryID(id), library(lib), header(isHeader) {}

    bool mightContainSubItems() override { return getNumSubItems() > 0; }
    int getItemHeight() const override { return 36; }
    void paintItem(juce::Graphics& g, int width, int height) override
    {
        if (isSelected() && !header)
        {
            g.fillAll(PedalForgeLookAndFeel::bgMid);
            g.setColour(PedalForgeLookAndFeel::accent);
            g.fillRect(0, 0, 4, height);
        }

        g.setColour(isSelected() && !header ? juce::Colours::white : juce::Colour(0xffaaaaaa));
        g.setFont(header ? juce::FontOptions(13.0f).withStyle("Bold") : juce::FontOptions(16.0f));
        g.drawText(displayName, header ? 0 : 5, 0, width - (header ? 0 : 5), height, juce::Justification::centredLeft, true);
    }
    void itemSelectionChanged(bool isNowSelected) override
    {
        if (isNowSelected && !header)
        {
            library.setCategory(displayName, categoryID);
        }
    }
    bool canBeSelected() const override { return !header; }
    juce::String getUniqueName() const override { return displayName; }

    juce::String displayName;
    juce::String categoryID;
    LibraryComponent& library;
    bool header;
};

//==============================================================================
LibraryComponent::LibraryComponent()
{
    addAndMakeVisible (categoryTree);
    categoryTree.setColour(juce::TreeView::backgroundColourId, PedalForgeLookAndFeel::bgDark);
    categoryTree.setDefaultOpenness(true);

    auto* root = new CategoryTreeItem("Root", "", *this, true);
    categoryTree.setRootItem(root);
    categoryTree.setRootItemVisible(false);

    root->addSubItem(new CategoryTreeItem("Pedals", "Pedals", *this));
    root->addSubItem(new CategoryTreeItem("Boards", "Boards", *this));
    root->addSubItem(new CategoryTreeItem("NAM Models", "NAM", *this));

    auto* irNode = new CategoryTreeItem("IMPULSE RESPONSES", "", *this, true);
    irNode->addSubItem(new CategoryTreeItem("Cabinets", "IR_CAB", *this));
    irNode->addSubItem(new CategoryTreeItem("Reverbs", "IR_REV", *this));
    irNode->addSubItem(new CategoryTreeItem("Mics", "IR_MIC", *this));
    irNode->addSubItem(new CategoryTreeItem("Instruments", "IR_INST", *this));
    root->addSubItem(irNode);

    root->addSubItem(new CategoryTreeItem("Presets", "Presets", *this));
    root->addSubItem(new CategoryTreeItem("Images", "Images", *this));
    addAndMakeVisible (searchBox);
    searchBox.setTextToShowWhenEmpty ("Search library...", juce::Colours::grey);
    searchBox.setColour (juce::TextEditor::backgroundColourId, PedalForgeLookAndFeel::bgMid);
    searchBox.setColour (juce::TextEditor::outlineColourId, PedalForgeLookAndFeel::gridLine);
    searchBox.onTextChange = [this] { applyFilter(); };

    addAndMakeVisible (importBtn);
    importBtn.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::accent);
    importBtn.onClick = [this]
    {
        const bool isPedals = (currentCategoryID == "Pedals");
        const bool isBoards = (currentCategoryID == "Boards");
        juce::String filter =
              isPedals ? juce::String ("*.pfpedal;*.json")
            : isBoards ? juce::String ("*.pfboard")
            :            AssetLibrary::getFileFilterForCategory (currentCategoryID);

        fileChooser = std::make_unique<juce::FileChooser> ("Import to Library", juce::File{}, filter);
        auto flags = juce::FileBrowserComponent::openMode
                   | juce::FileBrowserComponent::canSelectFiles
                   | juce::FileBrowserComponent::canSelectMultipleItems;

        juce::Component::SafePointer<LibraryComponent> sp (this);
        fileChooser->launchAsync (flags, [sp, isPedals, isBoards] (const juce::FileChooser& fc)
        {
            if (sp == nullptr) return;
            auto results = fc.getResults();
            for (auto& f : results)
            {
                if (! f.existsAsFile()) continue;
                if      (isPedals) importPedalDesignFile (f);
                else if (isBoards) importBoardFile (f);
                else                sp->library.importFile (f, sp->currentCategoryID);
            }
            sp->refreshAssets();
        });
    };

    gridViewport.setViewedComponent (&assetGrid, false);
    gridViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (gridViewport);
    addChildComponent (assetTable);

    // Subcategory combo
    subcategoryCombo.setColour (juce::ComboBox::backgroundColourId, PedalForgeLookAndFeel::bgMid);
    subcategoryCombo.setColour (juce::ComboBox::outlineColourId, PedalForgeLookAndFeel::gridLine);
    subcategoryCombo.onChange = [this] {
        int sel = subcategoryCombo.getSelectedId();
        if (sel == 1) currentSubcategory = "";
        else currentSubcategory = subcategoryCombo.getText();
        clearThumbnailCache();
        refreshAssets();
    };
    addAndMakeVisible (subcategoryCombo);
    subcategoryCombo.setVisible (false);

    newCategoryBtn.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgLight);
    newCategoryBtn.onClick = [this] {
        auto* alert = new juce::AlertWindow ("New Category", "Enter category name:", juce::AlertWindow::NoIcon);
        alert->addTextEditor ("name", "");
        alert->addButton ("Create", 1, juce::KeyPress (juce::KeyPress::returnKey));
        alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<LibraryComponent> sp (this);
        alert->enterModalState (true, juce::ModalCallbackFunction::create ([sp, alert] (int r) {
            if (r == 1 && sp != nullptr) {
                auto name = alert->getTextEditorContents ("name").trim();
                if (name.isNotEmpty()) {
                    sp->library.createSubcategory (sp->currentCategoryID, name);
                    sp->rebuildSubcategoryCombo();
                    sp->refreshAssets();
                }
            }
        }));
    };
    addAndMakeVisible (newCategoryBtn);
    newCategoryBtn.setVisible (false);

    if (auto* first = root->getSubItem(0))
        first->setSelected(true, true);

    // ── TONE3000 Cloud Integration ──
    cloudAuth.onAuthStateChanged = [this] { refreshSignInButton(); };

    addChildComponent (cloudToggle);
    addChildComponent (localToggle);
    addChildComponent (signInBtn);
    cloudToggle.setVisible (true);
    localToggle.setVisible (true);

    cloudToggle.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgLight);
    localToggle.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::accent);
    signInBtn.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgLight);
    refreshSignInButton();

    cloudToggle.onClick = [this] { switchToCloudMode (true); };
    localToggle.onClick = [this] { switchToCloudMode (false); };

    // Icon/Table view toggles — only meaningful for Pedals/Boards/Images.
    loadViewModePrefs();

    addChildComponent (btnIconView);
    addChildComponent (btnTableView);
    btnIconView.setColour  (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgLight);
    btnTableView.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgLight);
    btnIconView.onClick  = [this] { categoryShowsIcons[currentCategoryID] = true;  saveViewModePrefs(); resized(); repaint(); };
    btnTableView.onClick = [this] { categoryShowsIcons[currentCategoryID] = false; saveViewModePrefs(); resized(); repaint(); };
    signInBtn.onClick = [this]
    {
        if (cloudAuth.isSignedIn())
        {
            cloudAuth.signOut();
        }
        else
        {
            startSignIn ([this] { performCloudSearch(); });
        }
    };

    // Gear filter buttons
    for (auto* btn : { &filterAll, &filterAmps, &filterPedals, &filterRigs, &filterIRs })
    {
        btn->setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgLight);
        addChildComponent (*btn);
    }
    filterAll.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::accent);

    filterAll.onClick    = [this] { setGearFilter (""); };
    filterAmps.onClick   = [this] { setGearFilter ("amp"); };
    filterPedals.onClick = [this] { setGearFilter ("pedal"); };
    filterRigs.onClick   = [this] { setGearFilter ("full-rig"); };
    filterIRs.onClick    = [this] { setGearFilter ("ir"); };

    // Pagination
    addChildComponent (prevPageBtn);
    addChildComponent (nextPageBtn);
    addChildComponent (pageLabel);
    pageLabel.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textMuted);
    pageLabel.setJustificationType (juce::Justification::centred);
    pageLabel.setFont (juce::FontOptions (11.0f));

    prevPageBtn.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgLight);
    nextPageBtn.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgLight);
    prevPageBtn.onClick = [this] { if (cloudCurrentPage > 1) { --cloudCurrentPage; performCloudSearch(); } };
    nextPageBtn.onClick = [this] { if (cloudCurrentPage < cloudTotalPages) { ++cloudCurrentPage; performCloudSearch(); } };

    // Search button (visible in cloud mode)
    searchBtn.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::accent);
    searchBtn.onClick = [this] { cloudCurrentPage = 1; performCloudSearch(); };
    addChildComponent (searchBtn);

    // Enter key support for the search box
    searchBox.onReturnKey = [this]
    {
        if (showingCloud)
        {
            stopTimer(); // Cancel any pending debounce
            cloudCurrentPage = 1;
            performCloudSearch();
        }
    };
}

//==============================================================================
void LibraryComponent::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgDark);

    // Toolbar gradient
    auto toolbarArea = getLocalBounds().removeFromTop (36);
    g.setGradientFill (juce::ColourGradient (
        PedalForgeLookAndFeel::bgMid.darker (0.1f), 0, (float)toolbarArea.getY(),
        PedalForgeLookAndFeel::bgMid.darker (0.35f), 0, (float)toolbarArea.getBottom(), false));
    g.fillRect (toolbarArea);
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawHorizontalLine (35, 0.0f, (float) getWidth());

    if (sidebarVisible)
    {
        g.setColour (PedalForgeLookAndFeel::gridLine);
        g.drawVerticalLine (200, 36.0f, (float) getHeight());

        g.setColour (PedalForgeLookAndFeel::textMuted);
        g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
        g.drawText ("  CATEGORIES", 0, 38, 200, 20, juce::Justification::centredLeft);
    }
}

void LibraryComponent::resized()
{
    auto bounds = getLocalBounds();
    auto toolbar = bounds.removeFromTop (36);

    if (sidebarVisible)
    {
        auto sidebar = bounds.removeFromLeft (200);
        sidebar.removeFromTop (24); // header space
        categoryTree.setBounds (sidebar);
    }

    // Toolbar layout
    toolbar.reduce (12, 4);
    searchBox.setBounds (toolbar.removeFromLeft (200).withSizeKeepingCentre (200, 24));
    toolbar.removeFromLeft (4);

    // Show search button next to the search box in cloud mode
    if (showingCloud)
    {
        searchBtn.setVisible (true);
        searchBtn.setBounds (toolbar.removeFromLeft (70).withSizeKeepingCentre (70, 24));
    }
    else
    {
        searchBtn.setVisible (false);
    }
    toolbar.removeFromLeft (4);

    bool showSubcat = (currentCategoryID == "Images");
    subcategoryCombo.setVisible (showSubcat);
    newCategoryBtn.setVisible (showSubcat);
    if (showSubcat)
    {
        subcategoryCombo.setBounds (toolbar.removeFromLeft (130).withSizeKeepingCentre (130, 24));
        toolbar.removeFromLeft (4);
        newCategoryBtn.setBounds (toolbar.removeFromLeft (85).withSizeKeepingCentre (85, 24));
    }

    importBtn.setBounds (toolbar.removeFromRight (80).withSizeKeepingCentre (80, 24));

    // Cloud / Local toggle buttons + API Key button
    int toggleW = showingCloud ? 280 : 170;
    auto toggleArea = toolbar.removeFromRight (toggleW);

    if (showingCloud)
    {
        signInBtn.setVisible (true);
        signInBtn.setBounds (toggleArea.removeFromLeft (80).withSizeKeepingCentre (80, 24));
        toggleArea.removeFromLeft (4);
    }
    else
    {
        signInBtn.setVisible (false);
    }

    cloudToggle.setBounds (toggleArea.removeFromRight (90).withSizeKeepingCentre (90, 24));
    toggleArea.removeFromRight (4);
    localToggle.setBounds (toggleArea.removeFromRight (70).withSizeKeepingCentre (70, 24));

    // Cloud-specific: filter row + pagination
    if (showingCloud)
    {
        auto filterRow = bounds.removeFromTop (32);
        filterRow.reduce (8, 4);
        int fbw = 60;
        for (auto* btn : { &filterAll, &filterAmps, &filterPedals, &filterRigs, &filterIRs })
        {
            btn->setVisible (true);
            btn->setBounds (filterRow.removeFromLeft (fbw).withSizeKeepingCentre (fbw, 22));
            filterRow.removeFromLeft (2);
        }

        // Pagination at the right of the filter row
        prevPageBtn.setVisible (true);
        nextPageBtn.setVisible (true);
        pageLabel.setVisible (true);
        nextPageBtn.setBounds (filterRow.removeFromRight (30).withSizeKeepingCentre (30, 22));
        filterRow.removeFromRight (2);
        pageLabel.setBounds (filterRow.removeFromRight (80).withSizeKeepingCentre (80, 22));
        filterRow.removeFromRight (2);
        prevPageBtn.setBounds (filterRow.removeFromRight (30).withSizeKeepingCentre (30, 22));
    }
    else
    {
        for (auto* btn : { &filterAll, &filterAmps, &filterPedals, &filterRigs, &filterIRs })
            btn->setVisible (false);
        prevPageBtn.setVisible (false);
        nextPageBtn.setVisible (false);
        pageLabel.setVisible (false);
    }

    auto gridBounds = bounds.reduced (8);

    // Two-button toggle for categories that support both views.
    const bool toggleVisible = ! showingCloud && categorySupportsBothViews (currentCategoryID);
    btnIconView.setVisible (toggleVisible);
    btnTableView.setVisible (toggleVisible);
    if (toggleVisible)
    {
        auto bar = gridBounds.removeFromTop (28);
        gridBounds.removeFromTop (4);
        btnIconView.setBounds  (bar.removeFromLeft (60).withSizeKeepingCentre (60, 22));
        bar.removeFromLeft (2);
        btnTableView.setBounds (bar.removeFromLeft (60).withSizeKeepingCentre (60, 22));

        // Visual state: highlight whichever view is active.
        bool showIcons = currentCategoryShowsIcons();
        btnIconView .setColour (juce::TextButton::buttonColourId, showIcons ? PedalForgeLookAndFeel::accent : PedalForgeLookAndFeel::bgLight);
        btnTableView.setColour (juce::TextButton::buttonColourId, showIcons ? PedalForgeLookAndFeel::bgLight : PedalForgeLookAndFeel::accent);
    }

    const bool useTable = ! showingCloud
                          && ! (categorySupportsBothViews (currentCategoryID) && currentCategoryShowsIcons())
                          && currentCategoryID != "Images";

    if (! useTable)
    {
        gridViewport.setVisible (true);
        assetTable.setVisible (false);
        gridViewport.setBounds (gridBounds);
        assetGrid.updateSize (gridBounds.getWidth());
    }
    else
    {
        gridViewport.setVisible (false);
        assetTable.setVisible (true);
        assetTable.setBounds (gridBounds);
    }
}


void LibraryComponent::refreshAssets()
{
    if (currentCategoryID == "Boards")
    {
        currentAssets.clear();
        auto boardsDir = getBoardsDir();
        if (boardsDir.isDirectory())
        {
            for (const auto& f : boardsDir.findChildFiles (juce::File::findFiles, false, "*.pfboard"))
            {
                AssetLibrary::AssetItem item;
                item.name      = f.getFileNameWithoutExtension();
                item.category  = "Boards";
                item.file      = f;
                item.extension = ".pfboard";
                item.sizeBytes = f.getSize();
                item.dateAdded = f.getLastModificationTime();
                currentAssets.push_back (item);
            }
        }
    }
    else if (currentCategoryID == "Pedals")
    {
        // Pedal designs live in ~/Library/PedalForge/designs/. Surface them here
        // so the Library tab is the single hub. Cache the parsed PedalDesign for
        // the icon view (PedalPainter needs it; we don't want to re-parse per paint).
        currentAssets.clear();
        pedalDesignCache.clear();

        auto designsDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                              .getChildFile ("PedalForge").getChildFile ("designs");

        if (designsDir.isDirectory())
        {
            for (const auto& f : designsDir.findChildFiles (juce::File::findFiles, false, "*.json"))
            {
                AssetLibrary::AssetItem item;
                try
                {
                    auto d = std::make_shared<PedalDesign> (PedalDesign::loadFromFile (f));
                    if (d->name.isEmpty()) continue;
                    item.name        = d->name;
                    item.tags        = d->tags;
                    if (d->author.isNotEmpty()) item.tags.add ("by " + d->author);
                    pedalDesignCache[f.getFullPathName()] = d;
                }
                catch (...)
                {
                    item.name = f.getFileNameWithoutExtension();
                }
                item.category    = "Pedals";
                item.file        = f;
                item.extension   = ".pfpedal";   // friendly label in the Type column
                item.sizeBytes   = f.getSize();
                item.dateAdded   = f.getLastModificationTime();
                currentAssets.push_back (item);
            }
        }
    }
    else
    {
        currentAssets = library.getAssets (currentCategoryID, currentSubcategory);
    }

    if (currentCategoryID == "Images")
        rebuildSubcategoryCombo();

    applyFilter();
    resized();
}

void LibraryComponent::applyFilter()
{
    auto query = searchBox.getText().trim().toLowerCase();

    if (query.isEmpty())
    {
        filteredAssets = currentAssets;
    }
    else
    {
        filteredAssets.clear();
        for (const auto& item : currentAssets)
        {
            bool match = item.name.toLowerCase().contains (query);
            if (!match)
            {
                for (auto& tag : item.tags)
                {
                    if (tag.toLowerCase().contains(query))
                    {
                        match = true;
                        break;
                    }
                }
            }
            if (match)
                filteredAssets.push_back (item);
        }
    }

    assetGrid.updateSize (gridViewport.getWidth());
    assetGrid.repaint();
    assetTable.refresh();
}

void LibraryComponent::selectCategory (const juce::String& category)
{
    if (auto* root = categoryTree.getRootItem())
    {
        std::function<bool(juce::TreeViewItem*)> search = [&](juce::TreeViewItem* item) -> bool
        {
            if (auto* catItem = dynamic_cast<CategoryTreeItem*>(item))
            {
                if (catItem->categoryID == category)
                {
                    catItem->setSelected(true, true);
                    return true;
                }
            }
            for (int i = 0; i < item->getNumSubItems(); ++i)
            {
                if (search(item->getSubItem(i)))
                    return true;
            }
            return false;
        };
        search(root);
    }
}

//==============================================================================
// AssetGrid
//==============================================================================
void LibraryComponent::AssetGrid::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111111));

    // ── CLOUD MODE ──
    if (parent.showingCloud)
    {
        auto& results = parent.cloudResults;

        if (results.empty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.4f));
            g.setFont (18.0f);
            g.drawText ("Search TONE3000 for NAM captures and IRs", getLocalBounds(), juce::Justification::centred);
            return;
        }

        int iw = currentItemW(), ih = currentItemH();
        int cols = juce::jmax (1, (getWidth() - padding) / (iw + padding));

        for (int i = 0; i < (int) results.size(); ++i)
        {
            int col = i % cols;
            int row = i / cols;
            int x = padding + col * (iw + padding);
            int y = padding + row * (ih + padding);

            auto itemBounds = juce::Rectangle<int> (x, y, iw, ih);
            bool isSel = selectedIndices.count(i) > 0;
            bool isDownloading = parent.downloadingIds.count(results[i].id) > 0;

            // Card background with subtle gradient
            g.setColour (isSel ? juce::Colour (0xff2a2a4a) : juce::Colour (0xff1e1e2e));
            g.fillRoundedRectangle (itemBounds.toFloat(), 6.0f);

            // Border
            g.setColour (isDownloading ? juce::Colour (0xffffff44) : 
                         isSel ? PedalForgeLookAndFeel::accent : juce::Colour (0xff333344));
            g.drawRoundedRectangle (itemBounds.toFloat().reduced (0.5f), 6.0f, isSel ? 2.0f : 1.0f);

            // Gear type icon
            auto iconArea = itemBounds.removeFromLeft (ih);
            g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.2f));
            g.fillRoundedRectangle (iconArea.reduced (8).toFloat(), 4.0f);
            g.setColour (PedalForgeLookAndFeel::accent);
            g.setFont (20.0f);
            juce::String icon = results[i].gearType == "amp"      ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9a\xa1")) :
                                results[i].gearType == "pedal"    ? juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x8e\xb8")) :
                                results[i].gearType == "full-rig" ? juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x8e\x9b")) :
                                results[i].gearType == "ir"       ? juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x94\x8a")) :
                                                                    juce::String (juce::CharPointer_UTF8 ("\xe2\x9a\xa1"));
            g.drawText (icon, iconArea.reduced (8), juce::Justification::centred);

            auto textArea = juce::Rectangle<int> (iconArea.getRight() + 4, itemBounds.getY(), iw - ih - 8, ih);

            // Tone name
            g.setColour (juce::Colours::white.withAlpha (0.95f));
            g.setFont (13.0f);
            g.drawText (results[i].name, textArea.removeFromTop (18).reduced (0, 2), juce::Justification::centredLeft, true);

            // Author + downloads
            g.setColour (PedalForgeLookAndFeel::textMuted);
            g.setFont (10.0f);
            juce::String meta = results[i].author;
            if (results[i].downloads > 0)
                meta += juce::String (juce::CharPointer_UTF8 ("  \xe2\xac\x87 ")) + juce::String (results[i].downloads);
            g.drawText (meta, textArea.removeFromTop (14), juce::Justification::centredLeft, true);

            // Model size badge + download indicator
            auto badgeArea = textArea;
            if (results[i].modelSize.isNotEmpty())
            {
                auto badge = badgeArea.removeFromLeft (60).withHeight (14);
                g.setColour (juce::Colour (0xff333355));
                g.fillRoundedRectangle (badge.toFloat(), 3.0f);
                g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.8f));
                g.setFont (9.0f);
                g.drawText (results[i].modelSize.toUpperCase(), badge, juce::Justification::centred);
                badgeArea.removeFromLeft (4);
            }

            // Download status
            if (isDownloading)
            {
                g.setColour (juce::Colour (0xffffff44));
                g.setFont (9.0f);
                g.drawText ("Downloading...", badgeArea, juce::Justification::centredLeft);
            }
        }
        return;
    }

    // ── LOCAL MODE ──
    auto& assets = parent.filteredAssets;

    if (assets.empty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (18.0f);
        juce::String msg;
        if (parent.currentCategoryID == "Pedals") msg = "Pedal library coming soon";
        else msg = "No " + parent.currentCategoryDisplay + " found.\nClick Import to add files.";
        g.drawText (msg, getLocalBounds(), juce::Justification::centred);
        return;
    }

    int iw = currentItemW(), ih = currentItemH();
    int cols = juce::jmax (1, (getWidth() - padding) / (iw + padding));

    for (int i = 0; i < (int) assets.size(); ++i)
    {
        int col = i % cols;
        int row = i / cols;
        int x = padding + col * (iw + padding);
        int y = padding + row * (ih + padding);

        auto itemBounds = juce::Rectangle<int> (x, y, iw, ih);
        bool isSel = selectedIndices.count(i) > 0;

        // Card background
        g.setColour (isSel ? juce::Colour (0xff2a2a4a) : juce::Colour (0xff1e1e2e));
        g.fillRoundedRectangle (itemBounds.toFloat(), 6.0f);

        // Border
        g.setColour (isSel ? PedalForgeLookAndFeel::accent : juce::Colour (0xff333344));
        g.drawRoundedRectangle (itemBounds.toFloat().reduced (0.5f), 6.0f, isSel ? 2.0f : 1.0f);

        if (isPedalMode())
        {
            // ── Pedal preview card ──
            // Render the actual chassis using PedalPainter so the user sees
            // what the pedal looks like, not just a name.
            auto cardArea = itemBounds.reduced (6).toFloat();
            auto previewArea = cardArea.removeFromTop (cardArea.getHeight() - 24);

            auto it = parent.pedalDesignCache.find (assets[i].file.getFullPathName());
            const PedalDesign* design = (it != parent.pedalDesignCache.end()) ? it->second.get() : nullptr;

            if (design != nullptr)
            {
                PedalPainter::paintDesign (g, previewArea, design,
                                           /*controlValues*/ {},
                                           /*controlTexts*/  {},
                                           /*controlData*/   {},
                                           /*bypassed*/      false,
                                           /*alpha*/         1.0f);
            }
            else
            {
                g.setColour (PedalForgeLookAndFeel::bgLight);
                g.fillRoundedRectangle (previewArea, 4.0f);
                g.setColour (PedalForgeLookAndFeel::textMuted);
                g.setFont (11.0f);
                g.drawText ("No Preview", previewArea.toNearestInt(), juce::Justification::centred);
            }

            // Name below
            g.setColour (isSel ? juce::Colours::white : PedalForgeLookAndFeel::textPrimary);
            g.setFont (juce::FontOptions (12.0f));
            g.drawText (assets[i].name, cardArea.toNearestInt(), juce::Justification::centred, true);
        }
        else if (isBoardMode())
        {
            // ── Board summary card ──
            // Full board mini-render is more work; for now show the file name
            // and pedal-count chip derived from the JSON.
            auto cardArea = itemBounds.reduced (8).toFloat();

            int pedalCount = 0;
            auto json = juce::JSON::parse (assets[i].file.loadFileAsString());
            if (auto* obj = json.getDynamicObject())
                if (auto* arr = obj->getProperty ("pedals").getArray())
                    pedalCount = arr->size();

            // Big board icon + count chip
            auto iconArea = cardArea.removeFromTop (cardArea.getHeight() - 30);
            g.setColour (PedalForgeLookAndFeel::accent.withAlpha (isSel ? 0.35f : 0.18f));
            g.fillRoundedRectangle (iconArea.reduced (12), 6.0f);
            g.setColour (PedalForgeLookAndFeel::accent);
            g.setFont (28.0f);
            g.drawText (juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x8e\x9b")),  // 🎛
                        iconArea.toNearestInt(), juce::Justification::centred);

            auto chip = juce::Rectangle<float> (iconArea.getRight() - 60.0f, iconArea.getBottom() - 22.0f, 50.0f, 18.0f);
            g.setColour (PedalForgeLookAndFeel::bgDark.withAlpha (0.85f));
            g.fillRoundedRectangle (chip, 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.setFont (10.0f);
            g.drawText (juce::String (pedalCount) + " pedal" + (pedalCount == 1 ? "" : "s"),
                        chip.toNearestInt(), juce::Justification::centred);

            g.setColour (isSel ? juce::Colours::white : PedalForgeLookAndFeel::textPrimary);
            g.setFont (juce::FontOptions (12.0f));
            g.drawText (assets[i].name, cardArea.toNearestInt(), juce::Justification::centred, true);
        }
        else if (isImageMode())
        {
            // ── Image thumbnail card ──
            auto thumbArea = itemBounds.reduced (6);
            auto imgArea = thumbArea.removeFromTop (thumbArea.getHeight() - 30);

            auto thumb = parent.getThumbnail (assets[i].file);
            if (thumb.isValid())
            {
                // Draw with aspect fit
                float imgAspect = (float) thumb.getWidth() / (float) thumb.getHeight();
                float areaAspect = (float) imgArea.getWidth() / (float) imgArea.getHeight();
                juce::Rectangle<float> drawArea;
                if (imgAspect > areaAspect)
                    drawArea = { (float) imgArea.getX(), (float) imgArea.getCentreY() - imgArea.getWidth() / imgAspect / 2.0f,
                                 (float) imgArea.getWidth(), imgArea.getWidth() / imgAspect };
                else
                    drawArea = { (float) imgArea.getCentreX() - imgArea.getHeight() * imgAspect / 2.0f, (float) imgArea.getY(),
                                 imgArea.getHeight() * imgAspect, (float) imgArea.getHeight() };

                g.drawImage (thumb, drawArea, juce::RectanglePlacement::centred);
            }
            else
            {
                // Placeholder
                g.setColour (PedalForgeLookAndFeel::bgLight);
                g.fillRoundedRectangle (imgArea.toFloat(), 4.0f);
                g.setColour (PedalForgeLookAndFeel::textMuted);
                g.setFont (12.0f);
                g.drawText ("No Preview", imgArea, juce::Justification::centred);
            }

            // Name below
            g.setColour (isSel ? juce::Colours::white : PedalForgeLookAndFeel::textPrimary);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (assets[i].name, thumbArea.removeFromTop(16), juce::Justification::centred, true);

            // Subcategory badge
            if (assets[i].subcategory.isNotEmpty())
            {
                g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.6f));
                g.setFont (juce::FontOptions (9.0f));
                g.drawText (assets[i].subcategory, thumbArea, juce::Justification::centred, true);
            }
        }
        else
        {
            // ── Standard list card (NAM, IR, etc.) ──
            auto iconArea = itemBounds.removeFromLeft (ih);
            g.setColour (PedalForgeLookAndFeel::accent.withAlpha (isSel ? 0.35f : 0.2f));
            g.fillRoundedRectangle (iconArea.reduced (8).toFloat(), 4.0f);
            g.setColour (PedalForgeLookAndFeel::accent);
            g.setFont (20.0f);
            g.drawText (assets[i].category == "NAM" ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9a\xa1")) :
                        assets[i].category.startsWith("IR") ? juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x94\x8a")) :
                        assets[i].category == "Images" ? juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x96\xbc")) : juce::String(),
                        iconArea.reduced (8), juce::Justification::centred);

            auto textArea = juce::Rectangle<int> (iconArea.getRight() + 4, itemBounds.getY(), iw - ih - 8, ih);
            g.setColour (isSel ? juce::Colours::white : juce::Colours::white.withAlpha (0.9f));
            g.setFont (13.0f);
            g.drawText (assets[i].name, textArea.removeFromTop (18).reduced (0, 2), juce::Justification::centredLeft, true);

            if (!assets[i].tags.isEmpty())
            {
                g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.8f));
                g.setFont (10.0f);
                g.drawText (assets[i].tags.joinIntoString(", "), textArea.removeFromTop (14), juce::Justification::centredLeft, true);
            }

            float sizeKB = (float) assets[i].sizeBytes / 1024.0f;
            juce::String sizeStr = sizeKB > 1024.0f
                ? juce::String (sizeKB / 1024.0f, 1) + " MB"
                : juce::String ((int) sizeKB) + " KB";
            g.setColour (juce::Colours::grey);
            g.setFont (11.0f);
            g.drawText (sizeStr, textArea, juce::Justification::centredLeft, true);
        }
    }
}

void LibraryComponent::AssetGrid::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    int idx = getItemAtPosition (e.getPosition());

    // ── Cloud mode: double-click to download ──
    if (parent.showingCloud)
    {
        if (idx >= 0 && idx < (int) parent.cloudResults.size())
        {
            if (e.getNumberOfClicks() >= 2)
            {
                parent.triggerCloudDownload (parent.cloudResults[(size_t) idx]);
            }
            else
            {
                if (!e.mods.isShiftDown() && !e.mods.isCommandDown())
                    selectedIndices.clear();
                selectedIndices.insert(idx);
            }
            repaint();
        }
        else
        {
            selectedIndices.clear();
            repaint();
        }
        return;
    }

    // ── Local mode ──
    if (idx >= 0 && idx < (int) parent.filteredAssets.size())
    {
        auto& asset = parent.filteredAssets[(size_t) idx];

        if (e.mods.isRightButtonDown())
        {
            if (selectedIndices.count(idx) == 0)
            {
                selectedIndices.clear();
                selectedIndices.insert(idx);
                repaint();
            }

            // Right-click context menu
            juce::PopupMenu menu;
            if (selectedIndices.size() == 1)
            {
                menu.addItem (1, "Load");
                menu.addItem (5, "Rename...");
                menu.addItem (2, "Show in Finder");
                menu.addItem (4, "Edit Tags...");

                // Move To submenu (Images only)
                if (parent.currentCategoryID == "Images")
                {
                    juce::PopupMenu moveMenu;
                    moveMenu.addItem (100, "(Root)");
                    auto subcats = parent.library.getSubcategories ("Images");
                    for (int si = 0; si < subcats.size(); ++si)
                        moveMenu.addItem (101 + si, subcats[si]);
                    menu.addSubMenu ("Move To...", moveMenu);
                }
                menu.addSeparator();
            }
            menu.addItem (3, selectedIndices.size() > 1 ? "Remove Selected from Library" : "Remove from Library");

            juce::Component::SafePointer<LibraryComponent> sp (&parent);
            
            std::vector<juce::File> filesToDelete;
            for (int si : selectedIndices)
            {
                if (si >= 0 && si < parent.filteredAssets.size())
                    filesToDelete.push_back(parent.filteredAssets[si].file);
            }
            auto fileToLoad = asset.file;
            
            menu.showMenuAsync (juce::PopupMenu::Options(), [sp, fileToLoad, filesToDelete, asset] (int result)
            {
                if (sp == nullptr) return;
                if (result == 1 && sp->onAssetSelected)
                    sp->onAssetSelected (fileToLoad);
                else if (result == 2)
                    fileToLoad.revealToUser();
                else if (result == 3)
                {
                    for (auto& f : filesToDelete)
                    {
                        AssetLibrary::AssetItem itemToDelete;
                        itemToDelete.file = f;
                        sp->library.removeAsset(itemToDelete);
                    }
                    sp->assetGrid.selectedIndices.clear();
                    sp->refreshAssets();
                }
                else if (result == 4)
                {
                    juce::AlertWindow* alert = new juce::AlertWindow ("Edit Tags", "Enter tags separated by commas:", juce::AlertWindow::NoIcon);
                    alert->addTextEditor ("tags", asset.tags.joinIntoString (", "));
                    alert->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
                    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                    
                    alert->enterModalState (true, juce::ModalCallbackFunction::create ([sp, asset, alert] (int r) {
                        if (r == 1 && sp != nullptr)
                        {
                            auto text = alert->getTextEditorContents ("tags");
                            auto newTags = juce::StringArray::fromTokens (text, ",", "\"");
                            newTags.trim();
                            newTags.removeEmptyStrings (true);
                            
                            auto updatedAsset = asset;
                            updatedAsset.tags = newTags;
                            sp->library.saveMetadata (updatedAsset);
                            sp->refreshAssets();
                        }
                    }));
                }
                else if (result == 5)
                {
                    juce::AlertWindow* alert = new juce::AlertWindow ("Rename", "Enter new name:", juce::AlertWindow::NoIcon);
                    alert->addTextEditor ("name", asset.name);
                    alert->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
                    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                    alert->enterModalState (true, juce::ModalCallbackFunction::create ([sp, asset, alert] (int r) {
                        if (r == 1 && sp != nullptr)
                        {
                            auto newName = alert->getTextEditorContents ("name").trim();
                            if (newName.isNotEmpty() && newName != asset.name)
                            {
                                sp->library.renameAsset (asset, newName);
                                sp->clearThumbnailCache();
                                sp->refreshAssets();
                            }
                        }
                    }));
                }
                else if (result >= 100)
                {
                    juce::String targetSubcat;
                    if (result == 100) targetSubcat = "";
                    else {
                        auto subcats = sp->library.getSubcategories ("Images");
                        int subIdx = result - 101;
                        if (subIdx >= 0 && subIdx < subcats.size())
                            targetSubcat = subcats[subIdx];
                    }
                    sp->library.moveToSubcategory (asset, targetSubcat);
                    sp->clearThumbnailCache();
                    sp->refreshAssets();
                }
            });
        }
        else if (e.getNumberOfClicks() >= 2)
        {
            // Double-click: load the asset
            if (parent.onAssetSelected)
                parent.onAssetSelected (asset.file);
        }
        else
        {
            // Single-click: select
            if (e.mods.isShiftDown() && !selectedIndices.empty())
            {
                int first = *selectedIndices.begin(); // Simple approach: from first selected to this
                int startIdx = juce::jmin(first, idx);
                int endIdx = juce::jmax(first, idx);
                selectedIndices.clear();
                for (int i = startIdx; i <= endIdx; ++i)
                    selectedIndices.insert(i);
            }
            else if (e.mods.isCommandDown() || e.mods.isCtrlDown())
            {
                if (selectedIndices.count(idx) > 0)
                    selectedIndices.erase(idx);
                else
                    selectedIndices.insert(idx);
            }
            else
            {
                selectedIndices.clear();
                selectedIndices.insert(idx);
            }
            repaint();
        }
    }
    else
    {
        // Clicked empty space — deselect
        selectedIndices.clear();
        repaint();
    }
}

bool LibraryComponent::AssetGrid::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (!selectedIndices.empty())
        {
            for (int si : selectedIndices)
            {
                if (si >= 0 && si < (int)parent.filteredAssets.size())
                {
                    parent.filteredAssets[(size_t)si].file.deleteFile();
                }
            }
            selectedIndices.clear();
            parent.refreshAssets();
            return true;
        }
    }
    else if (key == juce::KeyPress::returnKey)
    {
        if (selectedIndices.size() == 1)
        {
            int si = *selectedIndices.begin();
            if (si >= 0 && si < (int)parent.filteredAssets.size())
            {
                if (parent.onAssetSelected)
                    parent.onAssetSelected (parent.filteredAssets[(size_t)si].file);
                return true;
            }
        }
    }
    return false;
}

int LibraryComponent::AssetGrid::getItemAtPosition (juce::Point<int> pos) const
{
    int iw = currentItemW(), ih = currentItemH();
    int cols = juce::jmax (1, (getWidth() - padding) / (iw + padding));

    int col = (pos.x - padding) / (iw + padding);
    int row = (pos.y - padding) / (ih + padding);

    if (col < 0 || col >= cols) return -1;

    int idx = row * cols + col;

    int x = padding + col * (iw + padding);
    int y = padding + row * (ih + padding);
    auto itemBounds = juce::Rectangle<int> (x, y, iw, ih);

    if (! itemBounds.contains (pos))
        return -1;

    return idx;
}

//==============================================================================
juce::Image LibraryComponent::getThumbnail (const juce::File& file)
{
    auto key = file.getFullPathName();
    auto it = thumbnailCache.find (key);
    if (it != thumbnailCache.end())
        return it->second;

    // Load and downscale
    auto img = juce::ImageFileFormat::loadFrom (file);
    if (img.isValid())
    {
        int maxDim = 200;
        float scale = juce::jmin ((float)maxDim / img.getWidth(), (float)maxDim / img.getHeight());
        if (scale < 1.0f)
            img = img.rescaled ((int)(img.getWidth() * scale), (int)(img.getHeight() * scale));
    }

    thumbnailCache[key] = img;
    return img;
}

void LibraryComponent::rebuildSubcategoryCombo()
{
    subcategoryCombo.clear (juce::dontSendNotification);
    subcategoryCombo.addItem ("All", 1);
    auto subcats = library.getSubcategories (currentCategoryID);
    for (int i = 0; i < subcats.size(); ++i)
        subcategoryCombo.addItem (subcats[i], i + 2);

    if (currentSubcategory.isEmpty())
        subcategoryCombo.setSelectedId (1, juce::dontSendNotification);
    else
    {
        int idx = subcats.indexOf (currentSubcategory);
        if (idx >= 0)
            subcategoryCombo.setSelectedId (idx + 2, juce::dontSendNotification);
    }
}

//==============================================================================
// TONE3000 Cloud Integration
//==============================================================================

void LibraryComponent::switchToCloudMode (bool cloud)
{
    showingCloud = cloud;

    // Update toggle button colours
    cloudToggle.setColour (juce::TextButton::buttonColourId,
        cloud ? PedalForgeLookAndFeel::accent : PedalForgeLookAndFeel::bgLight);
    localToggle.setColour (juce::TextButton::buttonColourId,
        cloud ? PedalForgeLookAndFeel::bgLight : PedalForgeLookAndFeel::accent);

    // Clear search text on every mode switch — a query relevant to one side
    // will almost never be the right one for the other (local file names vs
    // cloud tone titles), and a stale query silently emptied the local view.
    searchBox.setText ({}, juce::dontSendNotification);

    if (cloud)
    {
        // Switch search to cloud mode — debounce to avoid per-keystroke requests
        searchBox.setTextToShowWhenEmpty ("Search TONE3000...", juce::Colours::grey);
        searchBox.onTextChange = [this]
        {
            cloudCurrentPage = 1;
            scheduleCloudSearch();
        };

        // Hide local sidebar
        categoryTree.setVisible (false);
        sidebarVisible = false;
    }
    else
    {
        // Switch back to local mode
        stopTimer();
        searchBox.setTextToShowWhenEmpty ("Search library...", juce::Colours::grey);
        searchBox.onTextChange = [this] { applyFilter(); };

        categoryTree.setVisible (true);
        sidebarVisible = true;

        // Clear cloud results
        cloudResults.clear();
        refreshAssets();
    }

    resized();
    assetGrid.repaint();
}

void LibraryComponent::setGearFilter (const juce::String& filter)
{
    activeGearFilter = filter;

    // Update button highlighting
    filterAll.setColour    (juce::TextButton::buttonColourId, filter.isEmpty()       ? PedalForgeLookAndFeel::accent : PedalForgeLookAndFeel::bgLight);
    filterAmps.setColour   (juce::TextButton::buttonColourId, filter == "amp"        ? PedalForgeLookAndFeel::accent : PedalForgeLookAndFeel::bgLight);
    filterPedals.setColour (juce::TextButton::buttonColourId, filter == "pedal"      ? PedalForgeLookAndFeel::accent : PedalForgeLookAndFeel::bgLight);
    filterRigs.setColour   (juce::TextButton::buttonColourId, filter == "full-rig"   ? PedalForgeLookAndFeel::accent : PedalForgeLookAndFeel::bgLight);
    filterIRs.setColour    (juce::TextButton::buttonColourId, filter == "ir"         ? PedalForgeLookAndFeel::accent : PedalForgeLookAndFeel::bgLight);

    cloudCurrentPage = 1;
    performCloudSearch();
}

void LibraryComponent::performCloudSearch()
{
    // Need an access token before we can search. If the user isn't signed in,
    // kick off the OAuth flow first and re-enter on success.
    if (! cloudAuth.isSignedIn())
    {
        startSignIn ([this] { performCloudSearch(); });
        return;
    }

    ToneSearchParams params;
    params.query = searchBox.getText().trim();
    params.platform = "nam"; // Default to NAM captures
    params.gearFilter = activeGearFilter;
    params.page = cloudCurrentPage;
    params.pageSize = 25;
    params.sort = params.query.isEmpty() ? "trending" : "best-match";

    if (activeGearFilter == "ir")
        params.platform = "ir";

    juce::Component::SafePointer<LibraryComponent> sp (this);

    cloudClient.search (params, [sp] (ToneSearchResult result)
    {
        if (sp == nullptr) return;

        // Handle error responses from API search
        if (result.errorMessage.isNotEmpty())
        {
            juce::Logger::writeToLog ("[Tone3000] Search failed: " + result.errorMessage);

            // If authorization fails, drop tokens and re-trigger sign-in.
            if (result.errorMessage.containsIgnoreCase ("Authorization")
                || result.errorMessage.containsIgnoreCase ("key")
                || result.errorMessage.containsIgnoreCase ("unauthorized"))
            {
                sp->cloudAuth.signOut();

                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                    "TONE3000 Sign-in Required",
                    "Your TONE3000 session has expired. Click Sign in to reauthorize.");
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                    "TONE3000 Search Error", result.errorMessage);
            }
            return;
        }

        sp->cloudResults = std::move (result.tones);
        sp->cloudTotalPages = result.totalPages;
        sp->cloudCurrentPage = result.currentPage > 0 ? result.currentPage : sp->cloudCurrentPage;

        // Update pagination label
        sp->pageLabel.setText (
            "Page " + juce::String (sp->cloudCurrentPage) + " of " + juce::String (juce::jmax (1, sp->cloudTotalPages)),
            juce::dontSendNotification);

        sp->prevPageBtn.setEnabled (sp->cloudCurrentPage > 1);
        sp->nextPageBtn.setEnabled (sp->cloudCurrentPage < sp->cloudTotalPages);

        sp->assetGrid.updateSize (sp->gridViewport.getWidth());
        sp->assetGrid.repaint();
    });
}

void LibraryComponent::triggerCloudDownload (const ToneResult& tone)
{
    if (downloadingIds.count (tone.id) > 0) return; // Already downloading

    downloadingIds.insert (tone.id);
    assetGrid.repaint();

    // Determine target directory based on platform
    juce::String category = tone.platform == "ir" ? "IR_CAB" : "NAM";
    auto targetDir = library.getCategoryDir (category);

    juce::Component::SafePointer<LibraryComponent> sp (this);
    juce::String toneId = tone.id;
    juce::String toneName = tone.name;
    juce::String toneT3kId = tone.id;

    cloudClient.downloadTone (tone, targetDir, [sp, toneId, toneName, toneT3kId, category] (juce::File downloadedFile, bool success)
    {
        if (sp == nullptr) return;

        sp->downloadingIds.erase (toneId);

        if (success && downloadedFile.existsAsFile())
        {
            // Save TONE3000 metadata
            AssetLibrary::AssetItem item;
            item.name = downloadedFile.getFileNameWithoutExtension();
            item.category = category;
            item.file = downloadedFile;
            item.extension = downloadedFile.getFileExtension();
            item.sizeBytes = downloadedFile.getSize();
            item.dateAdded = juce::Time::getCurrentTime();
            item.tone3000Id = toneT3kId;
            item.tags.add ("TONE3000");
            sp->library.saveMetadata (item);

            juce::Logger::writeToLog ("[TONE3000] Downloaded: " + toneName + " -> " + downloadedFile.getFullPathName());

            // If the user happens to be looking at the matching local category,
            // refresh so the new file shows up immediately.
            if (! sp->showingCloud && sp->currentCategoryID == category)
                sp->refreshAssets();
        }
        else
        {
            juce::Logger::writeToLog ("[TONE3000] Download failed for: " + toneName);
        }

        sp->assetGrid.repaint();
    });
}

void LibraryComponent::timerCallback()
{
    stopTimer();
    performCloudSearch();
}

void LibraryComponent::scheduleCloudSearch()
{
    startTimer (500); // 500ms debounce
}

void LibraryComponent::startSignIn (std::function<void()> onSuccess)
{
    signInBtn.setButtonText ("Opening browser...");
    signInBtn.setEnabled (false);

    juce::Component::SafePointer<LibraryComponent> sp (this);
    cloudAuth.signInAsync ([sp, onSuccess] (bool ok, juce::String error)
    {
        if (sp == nullptr) return;

        sp->refreshSignInButton();

        if (ok)
        {
            if (onSuccess) onSuccess();
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                "TONE3000 Sign-in Failed", error.isEmpty() ? juce::String ("Unknown error.") : error);
        }
    });
}

void LibraryComponent::refreshSignInButton()
{
    signInBtn.setEnabled (true);
    signInBtn.setButtonText (cloudAuth.isSignedIn() ? juce::String ("Sign out") : juce::String ("Sign in"));
}

//==============================================================================
// AssetTable implementation

namespace
{
    juce::String formatSize (juce::int64 bytes)
    {
        if (bytes < 1024)            return juce::String (bytes) + " B";
        if (bytes < 1024 * 1024)     return juce::String (bytes / 1024.0, 1) + " KB";
        if (bytes < 1024LL * 1024 * 1024)
                                     return juce::String (bytes / (1024.0 * 1024.0), 1) + " MB";
        return juce::String (bytes / (1024.0 * 1024.0 * 1024.0), 2) + " GB";
    }

    juce::String formatDate (const juce::Time& t)
    {
        if (t.toMilliseconds() <= 0) return {};
        return t.formatted ("%Y-%m-%d %H:%M");
    }
}

LibraryComponent::AssetTable::AssetTable (LibraryComponent& owner) : parent (owner)
{
    addAndMakeVisible (table);
    table.setColour (juce::TableListBox::backgroundColourId, PedalForgeLookAndFeel::bgDark);
    table.setColour (juce::TableHeaderComponent::backgroundColourId, PedalForgeLookAndFeel::bgMid);
    table.setColour (juce::TableHeaderComponent::textColourId,       juce::Colour (0xffcccccc));
    table.setColour (juce::TableHeaderComponent::outlineColourId,    PedalForgeLookAndFeel::gridLine);
    table.setRowHeight (24);
    table.setMultipleSelectionEnabled (true);

    auto& header = table.getHeader();
    // Columns: id, name, default width, min, max, flags. Flags allow drag-resize +
    // drag-reorder + click-to-sort.
    const int flags = juce::TableHeaderComponent::defaultFlags;
    header.addColumn ("Name",     1, 280, 80,  -1, flags);
    header.addColumn ("Type",     2,  60, 40, 100, flags);
    header.addColumn ("Size",     3,  90, 50, 150, flags);
    header.addColumn ("Modified", 4, 150, 80, 250, flags);
    header.addColumn ("Tags",     5, 220, 60,  -1, flags);
    header.setSortColumnId (sortColumnId, sortAscending);
}

void LibraryComponent::AssetTable::refresh()
{
    recomputeOrder();
    table.updateContent();
    table.repaint();
}

int LibraryComponent::AssetTable::getNumRows()
{
    return (int) displayOrder.size();
}

void LibraryComponent::AssetTable::paintRowBackground (juce::Graphics& g,
                                                        int rowNumber, int /*width*/, int /*height*/,
                                                        bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll (PedalForgeLookAndFeel::accent.withAlpha (0.35f));
    else if (rowNumber % 2 == 0)
        g.fillAll (PedalForgeLookAndFeel::bgMid.withAlpha (0.30f));
}

void LibraryComponent::AssetTable::paintCell (juce::Graphics& g,
                                                int rowNumber, int columnId,
                                                int width, int height,
                                                bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int) displayOrder.size()) return;
    int assetIdx = displayOrder[(size_t) rowNumber];
    if (assetIdx < 0 || assetIdx >= (int) parent.filteredAssets.size()) return;
    const auto& a = parent.filteredAssets[(size_t) assetIdx];

    g.setColour (rowIsSelected ? juce::Colours::white : juce::Colour (0xffd0d0d0));
    g.setFont (juce::FontOptions (13.0f));

    juce::String text;
    auto justification = juce::Justification::centredLeft;
    switch (columnId)
    {
        case 1: text = a.name; break;
        case 2: text = a.extension.startsWithChar ('.') ? a.extension.substring (1).toUpperCase()
                                                        : a.extension.toUpperCase(); break;
        case 3: text = formatSize (a.sizeBytes); justification = juce::Justification::centredRight; break;
        case 4: text = formatDate (a.dateAdded); break;
        case 5: text = a.tags.joinIntoString (", "); break;
    }

    g.drawText (text, 6, 0, width - 12, height, justification, true);
}

void LibraryComponent::AssetTable::sortOrderChanged (int newSortColumnId, bool isForwards)
{
    sortColumnId  = newSortColumnId;
    sortAscending = isForwards;
    recomputeOrder();
    table.updateContent();
    table.repaint();
}

void LibraryComponent::AssetTable::cellClicked (int rowNumber, int /*columnId*/, const juce::MouseEvent& e)
{
    if (rowNumber < 0 || rowNumber >= (int) displayOrder.size()) return;

    if (e.mods.isRightButtonDown())
        showRowContextMenu (rowNumber);
}

void LibraryComponent::AssetTable::cellDoubleClicked (int rowNumber, int /*columnId*/, const juce::MouseEvent&)
{
    if (rowNumber < 0 || rowNumber >= (int) displayOrder.size()) return;
    int assetIdx = displayOrder[(size_t) rowNumber];
    if (assetIdx < 0 || assetIdx >= (int) parent.filteredAssets.size()) return;
    if (parent.onAssetSelected)
        parent.onAssetSelected (parent.filteredAssets[(size_t) assetIdx].file);
}

void LibraryComponent::AssetTable::backgroundClicked (const juce::MouseEvent&)
{
    table.deselectAllRows();
}

juce::String LibraryComponent::AssetTable::getCellTooltip (int rowNumber, int /*columnId*/)
{
    if (rowNumber < 0 || rowNumber >= (int) displayOrder.size()) return {};
    int assetIdx = displayOrder[(size_t) rowNumber];
    if (assetIdx < 0 || assetIdx >= (int) parent.filteredAssets.size()) return {};
    return parent.filteredAssets[(size_t) assetIdx].file.getFullPathName();
}

void LibraryComponent::AssetTable::recomputeOrder()
{
    const auto& assets = parent.filteredAssets;
    displayOrder.resize (assets.size());
    for (size_t i = 0; i < assets.size(); ++i)
        displayOrder[i] = (int) i;

    auto cmp = [&] (int la, int lb)
    {
        const auto& a = assets[(size_t) la];
        const auto& b = assets[(size_t) lb];
        int c = 0;
        switch (sortColumnId)
        {
            case 1: c = a.name.compareIgnoreCase (b.name); break;
            case 2: c = a.extension.compareIgnoreCase (b.extension); break;
            case 3: c = (a.sizeBytes < b.sizeBytes) ? -1 : (a.sizeBytes > b.sizeBytes ? 1 : 0); break;
            case 4: c = (a.dateAdded < b.dateAdded) ? -1 : (a.dateAdded > b.dateAdded ? 1 : 0); break;
            case 5: c = a.tags.joinIntoString (",").compareIgnoreCase (b.tags.joinIntoString (",")); break;
            default: c = a.name.compareIgnoreCase (b.name);
        }
        if (c == 0) c = a.name.compareIgnoreCase (b.name);
        return sortAscending ? c < 0 : c > 0;
    };

    std::stable_sort (displayOrder.begin(), displayOrder.end(), cmp);
}

void LibraryComponent::AssetTable::showRowContextMenu (int rowNumber)
{
    int assetIdx = displayOrder[(size_t) rowNumber];
    if (assetIdx < 0 || assetIdx >= (int) parent.filteredAssets.size()) return;
    auto asset = parent.filteredAssets[(size_t) assetIdx];

    // Ensure this row is selected before the menu shows.
    if (! table.isRowSelected (rowNumber))
    {
        table.selectRow (rowNumber);
    }

    // Collect every selected row for batch operations.
    std::vector<juce::File> filesToDelete;
    auto selectedRanges = table.getSelectedRows();
    for (int r = 0; r < selectedRanges.size(); ++r)
    {
        int sr = selectedRanges[r];
        if (sr < 0 || sr >= (int) displayOrder.size()) continue;
        int ai = displayOrder[(size_t) sr];
        if (ai >= 0 && ai < (int) parent.filteredAssets.size())
            filesToDelete.push_back (parent.filteredAssets[(size_t) ai].file);
    }

    juce::PopupMenu menu;
    if (filesToDelete.size() == 1)
    {
        menu.addItem (1, "Load");
        menu.addItem (5, "Rename...");
        menu.addItem (2, "Show in Finder");
        menu.addItem (4, "Edit Tags...");
        menu.addItem (6, "Export...");
        menu.addSeparator();
    }
    menu.addItem (3, filesToDelete.size() > 1 ? "Remove Selected from Library" : "Remove from Library");

    juce::Component::SafePointer<LibraryComponent> sp (&parent);
    auto fileToLoad = asset.file;
    bool isPedalRow = (parent.currentCategoryID == "Pedals");

    menu.showMenuAsync (juce::PopupMenu::Options(),
        [sp, asset, fileToLoad, filesToDelete, isPedalRow] (int result)
    {
        if (sp == nullptr) return;
        if (result == 1 && sp->onAssetSelected)
            sp->onAssetSelected (fileToLoad);
        else if (result == 2)
            fileToLoad.revealToUser();
        else if (result == 6)
        {
            // Export — copy the underlying file to a user-chosen location.
            // For Pedals we rename .json → .pfpedal so the recipient gets the
            // friendly extension.
            juce::String defaultExt = isPedalRow ? ".pfpedal" : fileToLoad.getFileExtension();
            auto suggested = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                                 .getChildFile (asset.name.replace (" ", "_") + defaultExt);

            sp->exportChooser = std::make_unique<juce::FileChooser> (
                "Export " + asset.name, suggested, "*" + defaultExt);

            int flags = juce::FileBrowserComponent::saveMode
                      | juce::FileBrowserComponent::canSelectFiles
                      | juce::FileBrowserComponent::warnAboutOverwriting;

            sp->exportChooser->launchAsync (flags, [fileToLoad, defaultExt] (const juce::FileChooser& fc)
            {
                auto dest = fc.getResult();
                if (dest == juce::File()) return;
                if (! dest.hasFileExtension (defaultExt.substring (1)))
                    dest = dest.withFileExtension (defaultExt);
                if (dest.existsAsFile()) dest.deleteFile();
                if (! fileToLoad.copyFileTo (dest))
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                        "Export Failed", "Could not write to:\n" + dest.getFullPathName());
                }
            });
        }
        else if (result == 3)
        {
            for (auto& f : filesToDelete)
            {
                AssetLibrary::AssetItem it; it.file = f;
                sp->library.removeAsset (it);
            }
            sp->refreshAssets();
        }
        else if (result == 4)
        {
            auto* alert = new juce::AlertWindow ("Edit Tags", "Enter tags separated by commas:", juce::AlertWindow::NoIcon);
            alert->addTextEditor ("tags", asset.tags.joinIntoString (", "));
            alert->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
            alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            alert->enterModalState (true, juce::ModalCallbackFunction::create ([sp, asset, alert] (int r)
            {
                if (r == 1 && sp != nullptr)
                {
                    auto text = alert->getTextEditorContents ("tags");
                    auto newTags = juce::StringArray::fromTokens (text, ",", "\"");
                    newTags.trim();
                    newTags.removeEmptyStrings (true);

                    auto updated = asset;
                    updated.tags = newTags;
                    sp->library.saveMetadata (updated);
                    sp->refreshAssets();
                }
            }));
        }
        else if (result == 5)
        {
            auto* alert = new juce::AlertWindow ("Rename", "Enter new name:", juce::AlertWindow::NoIcon);
            alert->addTextEditor ("name", asset.name);
            alert->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
            alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            alert->enterModalState (true, juce::ModalCallbackFunction::create ([sp, asset, alert] (int r)
            {
                if (r == 1 && sp != nullptr)
                {
                    auto newName = alert->getTextEditorContents ("name").trim();
                    if (newName.isNotEmpty() && newName != asset.name)
                    {
                        sp->library.renameAsset (asset, newName);
                        sp->refreshAssets();
                    }
                }
            }));
        }
    });
}

//==============================================================================
// View-mode preference persistence
//
// Lives in the same settings.json that holds the TONE3000 OAuth tokens, under
// a "categoryViewModes" object whose keys are category IDs and whose values
// are bools (true = icon view, false = table view).

namespace
{
    juce::File librarySettingsFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("PedalForge").getChildFile ("settings.json");
    }
}

void LibraryComponent::loadViewModePrefs()
{
    auto file = librarySettingsFile();
    if (! file.existsAsFile()) return;

    auto json = juce::JSON::parse (file);
    auto modes = json.getProperty ("categoryViewModes", juce::var());
    if (auto* obj = modes.getDynamicObject())
    {
        for (const auto& prop : obj->getProperties())
            categoryShowsIcons[prop.name.toString()] = (bool) prop.value;
    }
}

void LibraryComponent::saveViewModePrefs() const
{
    auto file = librarySettingsFile();
    file.getParentDirectory().createDirectory();

    // Preserve other settings (OAuth tokens, etc.) — merge instead of overwriting.
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    if (file.existsAsFile())
    {
        auto existing = juce::JSON::parse (file);
        if (auto* existingObj = existing.getDynamicObject())
            for (const auto& p : existingObj->getProperties())
                root->setProperty (p.name, p.value);
    }

    auto* modesObj = new juce::DynamicObject();
    for (const auto& [cat, isIcons] : categoryShowsIcons)
        modesObj->setProperty (cat, isIcons);
    root->setProperty ("categoryViewModes", juce::var (modesObj));

    file.replaceWithText (juce::JSON::toString (juce::var (root.get())));
}

