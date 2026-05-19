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

    if (auto* first = root->getSubItem(0))
        first->setSelected(true, true);
}

//==============================================================================
void LibraryComponent::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgDark);

    if (sidebarVisible)
    {
        // Sidebar separator
        g.setColour (PedalForgeLookAndFeel::gridLine);
        g.drawVerticalLine (200, 0.0f, (float) getHeight());

        // Sidebar header
        g.setColour (PedalForgeLookAndFeel::textPrimary);
        g.setFont (juce::FontOptions (16.0f).withStyle ("Bold"));
        g.drawText ("Library", 0, 10, 200, 30, juce::Justification::centred);
    }

    // Top bar separator
    g.drawHorizontalLine (60, sidebarVisible ? 201.0f : 0.0f, (float) getWidth());
}

void LibraryComponent::resized()
{
    auto bounds = getLocalBounds();

    if (sidebarVisible)
    {
        // Sidebar
        auto sidebar = bounds.removeFromLeft (200);
        sidebar.removeFromTop (50); // header space
        categoryTree.setBounds (sidebar);
    }

    // Main Content
    auto topBar = bounds.removeFromTop (60);
    topBar.reduce (20, 10);

    searchBox.setBounds (topBar.removeFromLeft (300));
    topBar.removeFromLeft (20);
    importBtn.setBounds (topBar.removeFromRight (100));

    auto gridBounds = bounds.reduced (12);
    gridViewport.setBounds (gridBounds);
    assetGrid.updateSize (gridBounds.getWidth());
}


void LibraryComponent::refreshAssets()
{

    if (currentCategoryID == "Pedals")
    {
        // Pedals aren't file-based yet — show empty
        currentAssets.clear();
    }
    else
    {
        currentAssets = library.getAssets (currentCategoryID);
    }

    applyFilter();
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
        if (parent.currentCategoryID == "Pedals")
            msg = "Pedal library coming soon";
        else
            msg = "No " + parent.currentCategoryDisplay + " found.\nClick Import to add files.";

        g.drawText (msg, getLocalBounds(), juce::Justification::centred);
        return;
    }

    int cols = juce::jmax (1, (getWidth() - padding) / (itemW + padding));

    for (int i = 0; i < (int) assets.size(); ++i)
    {
        int col = i % cols;
        int row = i / cols;
        int x = padding + col * (itemW + padding);
        int y = padding + row * (itemH + padding);

        auto itemBounds = juce::Rectangle<int> (x, y, itemW, itemH);
        bool isSelected = selectedIndices.count(i) > 0;

        // Card background
        g.setColour (isSelected ? juce::Colour (0xff2a2a4a) : juce::Colour (0xff1e1e2e));
        g.fillRoundedRectangle (itemBounds.toFloat(), 6.0f);

        // Border — accent for selected, subtle for unselected
        if (isSelected)
        {
            g.setColour (PedalForgeLookAndFeel::accent);
            g.drawRoundedRectangle (itemBounds.toFloat().reduced (0.5f), 6.0f, 2.0f);
        }
        else
        {
            g.setColour (juce::Colour (0xff333344));
            g.drawRoundedRectangle (itemBounds.toFloat().reduced (0.5f), 6.0f, 1.0f);
        }

        // Icon area
        auto iconArea = itemBounds.removeFromLeft (itemH);
        g.setColour (PedalForgeLookAndFeel::accent.withAlpha (isSelected ? 0.35f : 0.2f));
        g.fillRoundedRectangle (iconArea.reduced (8).toFloat(), 4.0f);
        g.setColour (PedalForgeLookAndFeel::accent);
        g.setFont (20.0f);
        g.drawText (assets[i].category == "NAM" ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9a\xa1")) :
                    assets[i].category.startsWith("IR") ? juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x94\x8a")) : 
                    assets[i].category == "Images" ? juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x96\xbc")) : juce::String(),
                    iconArea.reduced (8), juce::Justification::centred);

        // Text
        auto textArea = juce::Rectangle<int> (iconArea.getRight() + 4, itemBounds.getY(), itemW - itemH - 8, itemH);
        g.setColour (isSelected ? juce::Colours::white : juce::Colours::white.withAlpha (0.9f));
        g.setFont (13.0f);
        auto nameArea = textArea.removeFromTop (18).reduced (0, 2);
        g.drawText (assets[i].name, nameArea, juce::Justification::centredLeft, true);

        // Tags
        if (!assets[i].tags.isEmpty())
        {
            g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.8f));
            g.setFont (10.0f);
            auto tagsArea = textArea.removeFromTop (14).reduced (0, 0);
            g.drawText (assets[i].tags.joinIntoString(", "), tagsArea, juce::Justification::centredLeft, true);
        }

        // File size
        float sizeKB = (float) assets[i].sizeBytes / 1024.0f;
        juce::String sizeStr = sizeKB > 1024.0f
            ? juce::String (sizeKB / 1024.0f, 1) + " MB"
            : juce::String ((int) sizeKB) + " KB";
        g.setColour (juce::Colours::grey);
        g.setFont (11.0f);
        g.drawText (sizeStr, textArea, juce::Justification::centredLeft, true);
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
                menu.addItem (2, "Show in Finder");
                menu.addItem (4, "Edit Tags...");
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
    int cols = juce::jmax (1, (getWidth() - padding) / (itemW + padding));

    int col = (pos.x - padding) / (itemW + padding);
    int row = (pos.y - padding) / (itemH + padding);

    if (col < 0 || col >= cols) return -1;

    int idx = row * cols + col;

    // Verify the click is actually within the item bounds
    int x = padding + col * (itemW + padding);
    int y = padding + row * (itemH + padding);
    auto itemBounds = juce::Rectangle<int> (x, y, itemW, itemH);

    if (! itemBounds.contains (pos))
        return -1;

    return idx;
}
