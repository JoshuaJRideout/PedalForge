#include "LibraryComponent.h"
#include "LookAndFeel.h"

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
        auto filter = AssetLibrary::getFileFilterForCategory (currentCategoryID);
        fileChooser = std::make_unique<juce::FileChooser> ("Import to Library", juce::File{}, filter);
        auto flags = juce::FileBrowserComponent::openMode
                   | juce::FileBrowserComponent::canSelectFiles
                   | juce::FileBrowserComponent::canSelectMultipleItems;

        juce::Component::SafePointer<LibraryComponent> sp (this);
        fileChooser->launchAsync (flags, [sp] (const juce::FileChooser& fc)
        {
            if (sp == nullptr) return;
            auto results = fc.getResults();
            for (auto& f : results)
            {
                if (f.existsAsFile())
                    sp->library.importFile (f, sp->currentCategoryID);
            }
            sp->refreshAssets();
        });
    };

    gridViewport.setViewedComponent (&assetGrid, false);
    gridViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (gridViewport);

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
    auto savedKey = loadApiKey();
    if (savedKey.isNotEmpty())
        cloudClient.setApiKey (savedKey);

    addChildComponent (cloudToggle);
    addChildComponent (localToggle);
    addChildComponent (apiKeyBtn);
    cloudToggle.setVisible (true);
    localToggle.setVisible (true);

    cloudToggle.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgLight);
    localToggle.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::accent);
    apiKeyBtn.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgLight);

    cloudToggle.onClick = [this] { switchToCloudMode (true); };
    localToggle.onClick = [this] { switchToCloudMode (false); };
    apiKeyBtn.onClick = [this] { promptForApiKey ([this] { performCloudSearch(); }); };

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
        apiKeyBtn.setVisible (true);
        apiKeyBtn.setBounds (toggleArea.removeFromLeft (80).withSizeKeepingCentre (80, 24));
        toggleArea.removeFromLeft (4);
    }
    else
    {
        apiKeyBtn.setVisible (false);
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
    gridViewport.setBounds (gridBounds);
    assetGrid.updateSize (gridBounds.getWidth());
}


void LibraryComponent::refreshAssets()
{
    if (currentCategoryID == "Pedals")
    {
        currentAssets.clear();
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

        if (isImageMode())
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

        // Trigger initial search if search box has text
        if (searchBox.getText().trim().isNotEmpty())
            performCloudSearch();
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
    // Verify API Key exists before executing search
    if (loadApiKey().isEmpty())
    {
        promptForApiKey ([this] { performCloudSearch(); });
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

            // If authorization or key error is returned, prompt to re-enter
            if (result.errorMessage.containsIgnoreCase ("Authorization") 
                || result.errorMessage.containsIgnoreCase ("key") 
                || result.errorMessage.containsIgnoreCase ("unauthorized"))
            {
                sp->saveApiKey ("");
                sp->cloudClient.setApiKey ("");

                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                    "TONE3000 Authentication Failed", 
                    "Your TONE3000 API Key appears to be invalid or expired. Please re-enter it.");

                sp->promptForApiKey ([sp] { sp->performCloudSearch(); });
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

juce::String LibraryComponent::loadApiKey()
{
    auto file = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                    .getChildFile ("PedalForge")
                    .getChildFile ("settings.json");
    if (file.existsAsFile())
    {
        auto json = juce::JSON::parse (file);
        if (json.isObject())
            return json.getProperty ("tone3000ApiKey", "").toString();
    }
    return {};
}

void LibraryComponent::saveApiKey (const juce::String& key)
{
    auto file = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                    .getChildFile ("PedalForge")
                    .getChildFile ("settings.json");
    file.getParentDirectory().createDirectory();

    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty ("tone3000ApiKey", key);

    file.replaceWithText (juce::JSON::toString (juce::var (obj.get())));
}

void LibraryComponent::promptForApiKey (std::function<void()> onSuccessCallback)
{
    juce::AlertWindow* alert = new juce::AlertWindow ("TONE3000 API Key Required",
        "To search and download from TONE3000, please enter your Secret API Key (starts with 't3k_cs_') OR your Legacy API Key (from the bottom of the Settings -> API Keys page).\n\n"
        "Your key is saved strictly on your local machine and will never be committed to Git or shared on GitHub.",
        juce::AlertWindow::NoIcon);

    alert->addTextEditor ("key", loadApiKey());
    alert->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<LibraryComponent> sp (this);
    alert->enterModalState (true, juce::ModalCallbackFunction::create ([sp, alert, onSuccessCallback] (int r)
    {
        if (r == 1 && sp != nullptr)
        {
            auto enteredKey = alert->getTextEditorContents ("key").trim();
            if (enteredKey.isEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                    "API Key Required", "The API key cannot be empty.");
                return;
            }

            sp->saveApiKey (enteredKey);
            sp->cloudClient.setApiKey (enteredKey);

            if (onSuccessCallback)
                onSuccessCallback();
        }
    }));
}

