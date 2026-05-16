#include "LibraryComponent.h"
#include "LookAndFeel.h"

//==============================================================================
LibraryComponent::LibraryComponent()
{
    addAndMakeVisible (categoryList);
    categoryList.setModel (this);
    categoryList.setRowHeight (40);
    categoryList.setColour (juce::ListBox::backgroundColourId, PedalForgeLookAndFeel::bgDark);

    addAndMakeVisible (searchBox);
    searchBox.setTextToShowWhenEmpty ("Search library...", juce::Colours::grey);
    searchBox.setColour (juce::TextEditor::backgroundColourId, PedalForgeLookAndFeel::bgMid);
    searchBox.setColour (juce::TextEditor::outlineColourId, PedalForgeLookAndFeel::gridLine);
    searchBox.onTextChange = [this] { applyFilter(); };

    addChildComponent(irSubcategoryMenu);
    irSubcategoryMenu.addItem("Cabinets", 1);
    irSubcategoryMenu.addItem("Reverbs", 2);
    irSubcategoryMenu.addItem("Mics", 3);
    irSubcategoryMenu.addItem("Instruments", 4);
    irSubcategoryMenu.setSelectedId(1, juce::dontSendNotification);
    irSubcategoryMenu.setColour (juce::ComboBox::backgroundColourId, PedalForgeLookAndFeel::bgMid);
    irSubcategoryMenu.setColour (juce::ComboBox::outlineColourId, PedalForgeLookAndFeel::gridLine);
    irSubcategoryMenu.onChange = [this] { refreshAssets(); };

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

    categoryList.selectRow (0);
}

//==============================================================================
void LibraryComponent::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgDark);

    // Sidebar separator
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawVerticalLine (200, 0.0f, (float) getHeight());

    // Top bar separator
    g.drawHorizontalLine (60, 201.0f, (float) getWidth());

    // Sidebar header
    g.setColour (PedalForgeLookAndFeel::textPrimary);
    g.setFont (juce::FontOptions (16.0f).withStyle ("Bold"));
    g.drawText ("Library", 0, 10, 200, 30, juce::Justification::centred);
}

void LibraryComponent::resized()
{
    auto bounds = getLocalBounds();

    // Sidebar
    auto sidebar = bounds.removeFromLeft (200);
    sidebar.removeFromTop (50); // header space
    categoryList.setBounds (sidebar);

    // Main Content
    auto topBar = bounds.removeFromTop (60);
    topBar.reduce (20, 10);

    searchBox.setBounds (topBar.removeFromLeft (300));
    topBar.removeFromLeft (20);
    irSubcategoryMenu.setBounds(topBar.removeFromLeft (150));
    importBtn.setBounds (topBar.removeFromRight (100));

    auto gridBounds = bounds.reduced (12);
    gridViewport.setBounds (gridBounds);
    assetGrid.updateSize (gridBounds.getWidth());
}

//==============================================================================
int LibraryComponent::getNumRows()
{
    return categories.size();
}

void LibraryComponent::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow (rowNumber, categories.size()))
        return;

    if (rowIsSelected)
    {
        g.fillAll (PedalForgeLookAndFeel::bgMid);
        g.setColour (PedalForgeLookAndFeel::accent);
        g.fillRect (0, 0, 4, height);
    }

    g.setColour (rowIsSelected ? juce::Colours::white : juce::Colour (0xffaaaaaa));
    g.setFont (16.0f);
    g.drawText (categories[rowNumber], 20, 0, width - 20, height, juce::Justification::centredLeft, true);
}

void LibraryComponent::selectedRowsChanged (int lastRowSelected)
{
    if (juce::isPositiveAndBelow (lastRowSelected, categories.size()))
    {
        currentCategoryDisplay = categories[lastRowSelected];
        irSubcategoryMenu.setVisible(currentCategoryDisplay == "Impulse Responses");
        currentCategoryID = getCategoryID (currentCategoryDisplay);
        refreshAssets();
    }
}

//==============================================================================
void LibraryComponent::refreshAssets()
{
    currentCategoryID = getCategoryID (currentCategoryDisplay);

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
            if (item.name.toLowerCase().contains (query))
                filteredAssets.push_back (item);
        }
    }

    assetGrid.updateSize (gridViewport.getWidth());
    assetGrid.repaint();
}

void LibraryComponent::selectCategory (const juce::String& category)
{
    for (int i = 0; i < categories.size(); ++i)
    {
        if (getCategoryID (categories[i]) == category)
        {
            categoryList.selectRow (i);
            return;
        }
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
        bool isSelected = (i == selectedIndex);

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
                    assets[i].category.startsWith("IR") ? juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x94\x8a")) : juce::String(),
                    iconArea.reduced (8), juce::Justification::centred);

        // Text
        auto textArea = juce::Rectangle<int> (iconArea.getRight() + 4, itemBounds.getY(), itemW - itemH - 8, itemH);
        g.setColour (isSelected ? juce::Colours::white : juce::Colours::white.withAlpha (0.9f));
        g.setFont (13.0f);
        g.drawText (assets[i].name, textArea.removeFromTop (28).reduced (0, 4),
                    juce::Justification::centredLeft, true);

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
            selectedIndex = idx;
            repaint();

            // Right-click context menu
            juce::PopupMenu menu;
            menu.addItem (1, "Load");
            menu.addItem (2, "Show in Finder");
            menu.addSeparator();
            menu.addItem (3, "Remove from Library");

            juce::Component::SafePointer<LibraryComponent> sp (&parent);
            auto fileCopy = asset.file;
            menu.showMenuAsync (juce::PopupMenu::Options(), [sp, fileCopy] (int result)
            {
                if (sp == nullptr) return;
                if (result == 1 && sp->onAssetSelected)
                    sp->onAssetSelected (fileCopy);
                else if (result == 2)
                    fileCopy.revealToUser();
                else if (result == 3)
                {
                    fileCopy.deleteFile();
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
            selectedIndex = idx;
            repaint();
        }
    }
    else
    {
        // Clicked empty space — deselect
        selectedIndex = -1;
        repaint();
    }
}

bool LibraryComponent::AssetGrid::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (selectedIndex >= 0 && selectedIndex < (int) parent.filteredAssets.size())
        {
            auto& asset = parent.filteredAssets[(size_t) selectedIndex];
            asset.file.deleteFile();
            selectedIndex = -1;
            parent.refreshAssets();
            return true;
        }
    }
    else if (key == juce::KeyPress::returnKey)
    {
        if (selectedIndex >= 0 && selectedIndex < (int) parent.filteredAssets.size())
        {
            if (parent.onAssetSelected)
                parent.onAssetSelected (parent.filteredAssets[(size_t) selectedIndex].file);
            return true;
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
