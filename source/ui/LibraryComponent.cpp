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
    toolbar.removeFromLeft (8);

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
