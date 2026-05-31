#include "InventoryOverlay.h"
#include "LookAndFeel.h"
#include "../util/AppPaths.h"
#include <algorithm>
#include "HardwareDrawing.h"
#include "StyleKit.h"
#include "PedalPainter.h"
#include "../dsp/PedalDesign.h"
#include "../dsp/NodeCatalog.h"

//==============================================================================
//  CategoryPanel
//==============================================================================
InventoryOverlay::CategoryPanel::CategoryPanel()
{
    searchBox.setTextToShowWhenEmpty ("Search...", PedalForgeLookAndFeel::textMuted);
    searchBox.setColour (juce::TextEditor::backgroundColourId, PedalForgeLookAndFeel::bgLight);
    searchBox.setColour (juce::TextEditor::textColourId, PedalForgeLookAndFeel::textPrimary);
    searchBox.setColour (juce::TextEditor::outlineColourId, PedalForgeLookAndFeel::gridLine);
    searchBox.setFont (juce::FontOptions (13.0f));
    searchBox.onTextChange = [this]
    {
        if (onSearchChanged)
            onSearchChanged (searchBox.getText());
    };
    addAndMakeVisible (searchBox);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (4);
    addAndMakeVisible (viewport);
}

void InventoryOverlay::CategoryPanel::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgDark);

    // Right border
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
}

void InventoryOverlay::CategoryPanel::resized()
{
    auto area = getLocalBounds().reduced (6, 6);
    searchBox.setBounds (area.removeFromTop (28));
    area.removeFromTop (6);
    viewport.setBounds (area);

    // Layout buttons inside content
    int y = 0;
    int w = area.getWidth() - viewport.getScrollBarThickness() - 2;
    for (auto* btn : buttons)
    {
        int h = btn->subCat.isEmpty() ? 30 : 26;
        btn->setBounds (btn->subCat.isEmpty() ? 0 : 12, y, btn->subCat.isEmpty() ? w : w - 12, h);
        y += h + 2;
    }
    content.setSize (w, y + 4);
}

void InventoryOverlay::CategoryPanel::setCategories (
    const juce::StringArray& mainCategories,
    const std::map<juce::String, juce::StringArray>& subCategories)
{
    activeButton = nullptr;
    buttons.clear();
    content.removeAllChildren();

    for (const auto& mainCat : mainCategories)
    {
        // Main category header button
        auto* mainBtn = buttons.add (new CatButton());
        mainBtn->mainCat = mainCat;
        mainBtn->setButtonText (mainCat);
        mainBtn->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        mainBtn->setColour (juce::TextButton::textColourOffId, PedalForgeLookAndFeel::textPrimary);
        mainBtn->onClick = [this, mainBtn]
        {
            if (activeButton) activeButton->setToggleState (false, juce::dontSendNotification);
            activeButton = mainBtn;
            mainBtn->setToggleState (true, juce::dontSendNotification);
            if (onCategorySelected)
                onCategorySelected (mainBtn->mainCat, "");
        };
        content.addAndMakeVisible (mainBtn);

        // Sub-categories
        auto it = subCategories.find (mainCat);
        if (it != subCategories.end())
        {
            for (const auto& sub : it->second)
            {
                auto* subBtn = buttons.add (new CatButton());
                subBtn->mainCat = mainCat;
                subBtn->subCat = sub;
                subBtn->setButtonText (sub);
                subBtn->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
                subBtn->setColour (juce::TextButton::textColourOffId, PedalForgeLookAndFeel::textSecondary);
                subBtn->onClick = [this, subBtn]
                {
                    if (activeButton) activeButton->setToggleState (false, juce::dontSendNotification);
                    activeButton = subBtn;
                    subBtn->setToggleState (true, juce::dontSendNotification);
                    if (onCategorySelected)
                        onCategorySelected (subBtn->mainCat, subBtn->subCat);
                };
                content.addAndMakeVisible (subBtn);
            }
        }
    }

    resized();
}

void InventoryOverlay::CategoryPanel::selectCategory (const juce::String& main, const juce::String& sub)
{
    for (auto* btn : buttons)
    {
        if (btn->mainCat == main && btn->subCat == sub)
        {
            if (activeButton) activeButton->setToggleState (false, juce::dontSendNotification);
            activeButton = btn;
            btn->setToggleState (true, juce::dontSendNotification);
            break;
        }
    }
}

//==============================================================================
//  ItemGrid::GridCell
//==============================================================================
InventoryOverlay::ItemGrid::GridCell::GridCell (InventoryItem& i) : item (i) {}

void InventoryOverlay::ItemGrid::GridCell::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (3.0f);

    // Card background
    g.setColour (PedalForgeLookAndFeel::bgLight.withAlpha (0.6f));
    g.fillRoundedRectangle (bounds, 8.0f);

    // Hover highlight
    if (isMouseOver())
    {
        g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.15f));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.5f));
        g.drawRoundedRectangle (bounds, 8.0f, 1.5f);
    }

    auto inner = bounds.reduced (6.0f);
    auto textArea = inner.removeFromBottom (22.0f);

    // Draw the item visual
    if (item.mainCategory == "Parts" && item.hardwareType.isNotEmpty())
    {
        pf::StyleKitRegistry::draw (g, "default", item.hardwareType, inner.reduced (4.0f),
                                    pf::ControlState (0.5f), pf::Colorway{}, nullptr);
    }
    else if (item.mainCategory == "Pedals" && item.pedalDesign != nullptr)
    {
        // Mini pedal preview
        float ratio = 0.55f;
        float w = inner.getWidth();
        float h = inner.getHeight();
        float pw, ph;
        if (w / h > ratio) { ph = h; pw = ph * ratio; }
        else                { pw = w; ph = pw / ratio; }
        auto pedalRect = juce::Rectangle<float> (
            inner.getCentreX() - pw * 0.5f,
            inner.getCentreY() - ph * 0.5f, pw, ph);
        std::map<juce::String, float> dv;
        std::map<juce::String, juce::String> dt;
        PedalPainter::paintDesign (g, pedalRect, item.pedalDesign.get(), dv, dt, {}, false, 1.0f);
    }

    // Name label
    g.setColour (PedalForgeLookAndFeel::textPrimary);
    g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
    g.drawText (item.displayName, textArea, juce::Justification::centred);
}

void InventoryOverlay::ItemGrid::GridCell::mouseEnter (const juce::MouseEvent&)
{
    if (onHover) onHover (&item);
    repaint();
}

void InventoryOverlay::ItemGrid::GridCell::mouseDown (const juce::MouseEvent& e)
{
    dragStarted = false;

    // Right-click context menu for custom items
    if ((e.mods.isRightButtonDown() || e.mods.isCtrlDown()) && !item.isFactory)
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Delete");
        menu.showMenuAsync (juce::PopupMenu::Options(), [this] (int result)
        {
            if (result == 1 && item.mainCategory == "Pedals" && item.pedalDesign != nullptr)
            {
                auto designsDir = pf::paths::getDesignsDir();
                auto targetUuid = item.pedalDesign->uuid;
                for (const auto& file : designsDir.findChildFiles (juce::File::findFiles, false, "*.json"))
                {
                    auto d = PedalDesign::loadFromFile (file);
                    // Match by UUID first; fall back to name only if the file
                    // predates the uuid field and the user gave it one (legacy).
                    if ((targetUuid.isNotEmpty() && d.uuid == targetUuid)
                        || (targetUuid.isEmpty() && d.name == item.pedalDesign->name))
                    {
                        file.deleteFile();
                        break;
                    }
                }
            }
        });
    }
}

void InventoryOverlay::ItemGrid::GridCell::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragStarted && e.getDistanceFromDragStart() > 5)
    {
        dragStarted = true;
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            float ratioX = e.getMouseDownX() / (float) getWidth();
            float ratioY = e.getMouseDownY() / (float) getHeight();

            juce::String desc;
            if (item.mainCategory == "Pedals")
                desc = "pedal:" + item.pedalInfo.name + ":" + juce::String (ratioX) + ":" + juce::String (ratioY);
            else if (item.mainCategory == "Parts")
                desc = "hardware:" + item.hardwareType + ":" + juce::String (ratioX) + ":" + juce::String (ratioY);
            else if (item.mainCategory == "Nodes" || item.mainCategory == "Effects")
                desc = "node:" + item.hardwareType + ":" + juce::String (ratioX) + ":" + juce::String (ratioY);

            juce::Image emptyImage (juce::Image::ARGB, 1, 1, true);

            // Hide the overlay BEFORE starting the drag so the workspace
            // underneath can receive the drop
            if (onDragStart) onDragStart();

            container->startDragging (desc, this, emptyImage, false);
        }
    }
}

void InventoryOverlay::ItemGrid::GridCell::mouseUp (const juce::MouseEvent& e)
{
    if (! dragStarted && e.mouseWasClicked() && ! e.mods.isRightButtonDown() && ! e.mods.isCtrlDown())
    {
        if (onClick) onClick (&item);
    }
}

//==============================================================================
//  ItemGrid
//==============================================================================
InventoryOverlay::ItemGrid::ItemGrid()
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (6);
    addAndMakeVisible (viewport);
}

void InventoryOverlay::ItemGrid::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgMid);
}

void InventoryOverlay::ItemGrid::resized()
{
    viewport.setBounds (getLocalBounds());

    int availW = getWidth() - viewport.getScrollBarThickness();
    int cols = juce::jmax (2, availW / 130);
    int cellW = availW / cols;
    int cellH = cellW + 10;  // slightly taller for text
    int y = 8;

    for (int i = 0; i < (int) cells.size(); ++i)
    {
        int col = i % cols;
        int row = i / cols;
        cells[i]->setBounds (col * cellW + 4, y + row * cellH, cellW - 8, cellH - 8);
    }

    int totalRows = cells.size() > 0 ? (((int) cells.size() + cols - 1) / cols) : 1;
    content.setSize (availW, y + totalRows * cellH + 8);
}

void InventoryOverlay::ItemGrid::setItems (const std::vector<InventoryItem*>& itemsToShow)
{
    cells.clear();
    content.removeAllChildren();

    for (auto* item : itemsToShow)
    {
        auto* cell = cells.add (new GridCell (*item));
        cell->onHover = onItemHovered;
        cell->onClick = onItemSelected;
        cell->onDragStart = onDragStarted;
        content.addAndMakeVisible (cell);
    }

    resized();
}

//==============================================================================
//  PreviewPanel
//==============================================================================
void InventoryOverlay::PreviewPanel::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgDark);

    // Left border
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawVerticalLine (0, 0.0f, (float) getHeight());

    auto area = getLocalBounds().reduced (16);

    if (currentItem == nullptr)
    {
        g.setColour (PedalForgeLookAndFeel::textMuted);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("Hover over an item\nto see details", area, juce::Justification::centred);
        return;
    }

    // Title
    g.setColour (PedalForgeLookAndFeel::textPrimary);
    g.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
    g.drawText (currentItem->displayName, area.removeFromTop (28), juce::Justification::centredLeft);

    // Category badge
    area.removeFromTop (4);
    g.setColour (PedalForgeLookAndFeel::accent);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (currentItem->category, area.removeFromTop (18), juce::Justification::centredLeft);

    // Tags
    if (currentItem->tags.size() > 0)
    {
        area.removeFromTop (2);
        g.setColour (PedalForgeLookAndFeel::textSecondary);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (currentItem->tags.joinIntoString ("  -  "), area.removeFromTop (14), juce::Justification::centredLeft);
    }

    // Preview area
    area.removeFromTop (12);
    auto previewArea = area.removeFromTop (juce::jmin (area.getHeight() / 2, 200));

    if (currentItem->mainCategory == "Parts" && currentItem->hardwareType.isNotEmpty())
    {
        auto iconArea = previewArea.reduced (20).toFloat();
        float side = juce::jmin (iconArea.getWidth(), iconArea.getHeight());
        auto centered = juce::Rectangle<float> (side, side).withCentre (iconArea.getCentre());
        pf::StyleKitRegistry::draw (g, "default", currentItem->hardwareType, centered,
                                    pf::ControlState (0.5f), pf::Colorway{}, nullptr);
    }
    else if (currentItem->mainCategory == "Pedals" && currentItem->pedalDesign != nullptr)
    {
        float ratio = 0.55f;
        auto pf = previewArea.toFloat().reduced (10.0f);
        float pw, ph;
        if (pf.getWidth() / pf.getHeight() > ratio) { ph = pf.getHeight(); pw = ph * ratio; }
        else { pw = pf.getWidth(); ph = pw / ratio; }
        auto pedalRect = juce::Rectangle<float> (
            pf.getCentreX() - pw * 0.5f, pf.getCentreY() - ph * 0.5f, pw, ph);
        std::map<juce::String, float> dv;
        std::map<juce::String, juce::String> dt;
        PedalPainter::paintDesign (g, pedalRect, currentItem->pedalDesign.get(), dv, dt, {}, false, 1.0f);
    }

    // Description
    area.removeFromTop (12);
    g.setColour (PedalForgeLookAndFeel::textSecondary);
    g.setFont (juce::FontOptions (12.0f));
    g.drawFittedText (currentItem->description, area, juce::Justification::topLeft, 6);

    // Factory badge
    if (currentItem->isFactory)
    {
        auto badgeArea = getLocalBounds().reduced (16).removeFromBottom (24);
        g.setColour (PedalForgeLookAndFeel::textMuted);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("FACTORY", badgeArea, juce::Justification::centredRight);
    }
}

void InventoryOverlay::PreviewPanel::showItem (InventoryItem* item)
{
    currentItem = item;
    repaint();
}

//==============================================================================
//  InventoryOverlay — main
//==============================================================================
InventoryOverlay::InventoryOverlay()
{
    setInterceptsMouseClicks (false, false);
    setAlwaysOnTop (true);

    // Wire up the category panel
    categoryPanel.onCategorySelected = [this] (const juce::String& main, const juce::String& sub)
    {
        currentMainCategory = main;
        currentSubCategory = sub;
        filterItems();
    };

    categoryPanel.onSearchChanged = [this] (const juce::String& query)
    {
        searchQuery = query;
        filterItems();
    };

    // Wire up the item grid
    itemGrid.onItemHovered = [this] (InventoryItem* item)
    {
        previewPanel.showItem (item);
    };

    itemGrid.onItemSelected = [this] (InventoryItem* item)
    {
        previewPanel.showItem (item);
        if (onPedalClicked)
            onPedalClicked (item->id);
    };

    // When a drag starts, hide the overlay so the workspace can receive the drop
    itemGrid.onDragStarted = [this] { hide(); };

    addChildComponent (categoryPanel);
    addChildComponent (itemGrid);
    addChildComponent (previewPanel);

    buildItemDatabase();
}

InventoryOverlay::~InventoryOverlay() = default;

void InventoryOverlay::setContext (Context ctx)
{
    if (context == ctx) return;
    context = ctx;
    
    juce::StringArray mainCats;
    std::map<juce::String, juce::StringArray> subCats;

    switch (context)
    {
        case Context::Board:
        case Context::Route:
            mainCats.add ("Pedals");
            break;
        case Context::Forge:
            mainCats.add ("Parts");
            break;
        case Context::FX:
            mainCats.add ("Nodes");
            mainCats.add ("Effects");
            break;
    }

    for (auto& item : allItems)
    {
        if (mainCats.contains (item.mainCategory))
        {
            auto& subs = subCats[item.mainCategory];
            if (! subs.contains (item.category))
                subs.add (item.category);
        }
    }

    categoryPanel.setCategories (mainCats, subCats);

    if (mainCats.size() > 0)
    {
        currentMainCategory = mainCats[0];
        currentSubCategory = "";
        categoryPanel.selectCategory (currentMainCategory, "");
    }

    filterItems();
}

void InventoryOverlay::refresh()
{
    allItems.clear();
    buildItemDatabase();
    filterItems();
    repaint();
}

void InventoryOverlay::buildItemDatabase()
{
    allItems.clear();

    // ── Pedals ──────────────────────────────────────────────────────────
    for (auto& info : getFactoryPedals())
    {
        InventoryItem item;
        item.id = info.factoryID();           // stable factory identity
        item.displayName = info.name;
        item.category = info.category;
        item.mainCategory = "Pedals";
        item.description = info.category + " pedal with " + juce::String (info.numKnobs) + " knob(s).";
        item.isFactory = true;
        item.pedalInfo = info;
        if (info.designFactory)
        {
            item.pedalDesign = info.designFactory();
            item.tags = item.pedalDesign->tags;
        }
        allItems.push_back (std::move (item));
    }

    // User-designed pedals
    auto designsDir = pf::paths::getDesignsDir();
    if (designsDir.isDirectory())
    {
        for (const auto& file : designsDir.findChildFiles (juce::File::findFiles, false, "*.json"))
        {
            auto design = std::make_shared<PedalDesign> (PedalDesign::loadFromFile (file));
            if (design->name.isEmpty()) continue;

            InventoryItem item;
            item.id = design->uuid;            // stable per-design identity
            item.displayName = design->name;
            item.category = design->category.isNotEmpty() ? design->category : "Custom";
            item.mainCategory = "Pedals";
            item.tags = design->tags;
            item.description = design->tags.size() > 0
                ? "Custom pedal. Tags: " + design->tags.joinIntoString (", ")
                : "Custom user-designed pedal.";
            item.isFactory = false;
            item.pedalDesign = design;

            // Build a minimal PedalInfo for drag compatibility
            item.pedalInfo.name = design->name;
            item.pedalInfo.category = item.category;
            item.pedalInfo.gridW = 1;
            item.pedalInfo.gridH = 2;
            item.pedalInfo.numKnobs = 0;
            for (const auto& c : design->controls)
                if (c.type == "knob") item.pedalInfo.numKnobs++;
            item.pedalInfo.colour = design->chassisColour;

            allItems.push_back (std::move (item));
        }
    }

    // ── Parts (Hardware) ────────────────────────────────────────────────
    struct PartDef { const char* type; const char* name; const char* category; const char* desc; };
    PartDef parts[] = {
        { "knob",        "Knob",        "Controls",    "Rotary potentiometer control. Maps to a continuous parameter (0-1)." },
        { "switch",      "Switch",      "Controls",    "Toggle switch. Maps to a binary on/off parameter." },
        { "selector",    "Selector",    "Controls",    "Rotary multi-position selector. Pairs with a Selector Node; defaults to 4 positions." },
        { "fader",       "Fader",       "Controls",    "Linear slider control. Maps to a continuous parameter (0-1)." },
        { "xypad",       "XY Pad",      "Controls",    "Two-axis touch surface. Pairs with an XY Pad Node; outputs X and Y (0-1 each)." },
        { "joystick",    "Joystick",    "Controls",    "Self-centering two-axis stick. Pairs with an XY Pad Node; centre = 0.5,0.5." },
        { "footswitch",  "Stomp",       "Controls",    "3PDT footswitch for bypass/engage control." },
        { "led",         "LED",         "Lights",      "Single-color LED indicator." },
        { "rgb_led",     "RGB LED",     "Lights",      "Full-color RGB LED indicator." },
        { "indicator",   "Indicator",   "Lights",      "Status indicator light." },
        { "7seg",        "7-Seg",       "Screens",     "7-segment numeric display." },
        { "display",     "Display",     "Screens",     "Small graphical display panel." },
        { "text_screen", "Text",        "Screens",     "Text-based screen for status messages." },
        { "console",     "Console",     "Screens",     "Debug console / text output." },
        { "file_loader", "File Loader", "Controls",    "Button that opens a file browser to load files." },
        { "plugin_browser", "Plugin Browser", "Controls", "Button that opens a popup menu to select installed VST/AU plugins." },
        { "overlay_launcher", "Overlay Switch", "Controls", "Button that opens a visual overlay page or closes it." },
        { "label",       "Label",       "Decoration",  "Custom text label for the pedal face." },
        { "graphic",     "Graphic",     "Decoration",  "Custom image layer (supports transparency)." },
        { "vu_meter",    "VU Meter",    "Instruments", "Analog-style VU level meter." },
        { "oscilloscope","Scope",       "Instruments", "Mini oscilloscope waveform display." },
        { "pixel_display","Pixel Grid", "Screens",     "Pixel matrix display. Maps to a control signal driving each pixel." },
        { "library_loader","Library",   "Controls",    "Button that opens the Library overlay to select a NAM/IR/Image asset." },
    };

    for (auto& p : parts)
    {
        InventoryItem item;
        item.id = p.type;
        item.displayName = p.name;
        item.category = p.category;
        item.mainCategory = "Parts";
        item.description = p.desc;
        item.isFactory = true;
        item.hardwareType = p.type;
        allItems.push_back (std::move (item));
    }

    // ── Nodes (DSP Blocks) ──────────────────────────────────────────────
    // Pulled from NodeCatalog::getEntries() — the single source of truth shared
    // with the FX graph right-click menu. To add a new node type, edit NodeCatalog.h.
    {
        for (const auto& e : NodeCatalog::getEntries())
        {
            if (! e.inInventory) continue;

            // menuPath like "Effects/Filters" or "Nodes/MIDI/Receive". First
            // segment is the inventory main category; remaining segments are
            // joined with " / " as the sub-category.
            auto segs = juce::StringArray::fromTokens (e.menuPath, "/", {});
            segs.removeEmptyStrings();
            if (segs.isEmpty()) continue;

            juce::String mainCat = segs[0];
            segs.remove (0);
            juce::String subCat  = segs.joinIntoString (" / ");

            InventoryItem item;
            item.id           = e.type;
            item.displayName  = e.displayName;
            item.category     = subCat.isNotEmpty() ? subCat : juce::String ("General");
            item.mainCategory = mainCat;
            item.description  = e.description;
            item.isFactory    = true;
            item.hardwareType = e.type;  // used for drag-and-drop target
            allItems.push_back (std::move (item));
        }
    }

    // ── Build category tree (context-aware) ─────────────────────────
    juce::StringArray mainCats;
    std::map<juce::String, juce::StringArray> subCats;

    // Determine which main categories to show based on context
    switch (context)
    {
        case Context::Board:
        case Context::Route:
            mainCats.add ("Pedals");
            break;
        case Context::Forge:
            mainCats.add ("Parts");
            break;
        case Context::FX:
            mainCats.add ("Nodes");
            mainCats.add ("Effects");
            break;
    }

    // Collect unique sub-categories for visible main categories
    for (auto& item : allItems)
    {
        if (mainCats.contains (item.mainCategory))
        {
            auto& subs = subCats[item.mainCategory];
            if (! subs.contains (item.category))
                subs.add (item.category);
        }
    }

    categoryPanel.setCategories (mainCats, subCats);

    // Select the first main category
    if (mainCats.size() > 0)
    {
        currentMainCategory = mainCats[0];
        categoryPanel.selectCategory (currentMainCategory, "");
    }

    filterItems();
}

static bool compareInventoryItems (const InventoryOverlay::InventoryItem* a, const InventoryOverlay::InventoryItem* b)
{
    return a->displayName.compareIgnoreCase (b->displayName) < 0;
}

void InventoryOverlay::filterItems()
{
    filteredItems.clear();

    for (auto& item : allItems)
    {
        // Main category filter
        if (item.mainCategory != currentMainCategory)
            continue;

        // Sub-category filter (empty = show all)
        if (currentSubCategory.isNotEmpty() && item.category != currentSubCategory)
            continue;

        // Search filter
        if (searchQuery.isNotEmpty())
        {
            bool matches = item.displayName.containsIgnoreCase (searchQuery)
                        || item.category.containsIgnoreCase (searchQuery)
                        || item.description.containsIgnoreCase (searchQuery);
            if (! matches)
            {
                for (const auto& tag : item.tags)
                {
                    if (tag.containsIgnoreCase (searchQuery))
                    { matches = true; break; }
                }
            }
            if (! matches) continue;
        }

        filteredItems.push_back (&item);
    }

    // Sort items alphabetically by display name
    std::sort (filteredItems.begin(), filteredItems.end(), compareInventoryItems);

    itemGrid.setItems (filteredItems);
}

//==============================================================================
//  Paint / Layout
//==============================================================================
void InventoryOverlay::paint (juce::Graphics& g)
{
    if (! visible) return;

    // Dark backdrop
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillAll();

    // Panel background
    auto panelBounds = categoryPanel.getBounds()
                           .getUnion (itemGrid.getBounds())
                           .getUnion (previewPanel.getBounds())
                           .toFloat().expanded (1.0f);

    g.setColour (PedalForgeLookAndFeel::bgDark);
    g.fillRoundedRectangle (panelBounds, 10.0f);

    // Accent border
    g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.3f));
    g.drawRoundedRectangle (panelBounds, 10.0f, 1.5f);
}

void InventoryOverlay::resized()
{
    auto area = getLocalBounds();

    // Panel occupies ~90% of available space, centered
    int panelW = juce::jmin (area.getWidth() - 40, 1100);
    int panelH = juce::jmin (area.getHeight() - 40, 750);

    auto panel = juce::Rectangle<int> (panelW, panelH).withCentre (area.getCentre());

    // 3-column split: 180 | flex | 220
    auto leftCol  = panel.removeFromLeft (180);
    auto rightCol = panel.removeFromRight (220);
    auto centerCol = panel;

    categoryPanel.setBounds (leftCol);
    itemGrid.setBounds (centerCol);
    previewPanel.setBounds (rightCol);
}

void InventoryOverlay::mouseDown (const juce::MouseEvent& e)
{
    // Click on the backdrop (outside the panel) to close
    auto panelBounds = categoryPanel.getBounds()
                           .getUnion (itemGrid.getBounds())
                           .getUnion (previewPanel.getBounds());

    if (! panelBounds.contains (e.getPosition()))
        hide();
}

//==============================================================================
//  Show / Hide / Toggle
//==============================================================================
void InventoryOverlay::toggle()
{
    if (visible) hide(); else show();
}

void InventoryOverlay::show()
{
    visible = true;
    buildItemDatabase();  // refresh in case user designs changed
    categoryPanel.setVisible (true);
    itemGrid.setVisible (true);
    previewPanel.setVisible (true);
    setInterceptsMouseClicks (true, true);
    setVisible (true);
    toFront (true);
    repaint();
}

void InventoryOverlay::hide()
{
    visible = false;
    categoryPanel.setVisible (false);
    itemGrid.setVisible (false);
    previewPanel.setVisible (false);
    setInterceptsMouseClicks (false, false);
    setVisible (false);
    repaint();
}

bool InventoryOverlay::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if (key == juce::KeyPress::tabKey)
    {
        toggle();
        return true;
    }

    if (key == juce::KeyPress::escapeKey && visible)
    {
        hide();
        return true;
    }

    return false;
}
