#include "InventoryOverlay.h"
#include "LookAndFeel.h"
#include "HardwareDrawing.h"
#include "PedalPainter.h"
#include "../dsp/PedalDesign.h"

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
        HardwareDrawing::drawForType (g, item.hardwareType, inner.reduced (4.0f));
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
        PedalPainter::paintDesign (g, pedalRect, item.pedalDesign.get(), dv, dt, false, 1.0f);
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
    if (onClick) onClick (&item);

    // Right-click context menu for custom items
    if ((e.mods.isRightButtonDown() || e.mods.isCtrlDown()) && !item.isFactory)
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Delete");
        menu.showMenuAsync (juce::PopupMenu::Options(), [this] (int result)
        {
            if (result == 1 && item.mainCategory == "Pedals" && item.pedalDesign != nullptr)
            {
                auto designsDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                      .getChildFile ("PedalForge").getChildFile ("designs");
                for (const auto& file : designsDir.findChildFiles (juce::File::findFiles, false, "*.json"))
                {
                    auto d = PedalDesign::loadFromFile (file);
                    if (d.name == item.pedalDesign->name)
                    {
                        file.deleteFile();
                        // The overlay will need to rebuild its database
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

    // Preview area
    area.removeFromTop (12);
    auto previewArea = area.removeFromTop (juce::jmin (area.getHeight() / 2, 200));

    if (currentItem->mainCategory == "Parts" && currentItem->hardwareType.isNotEmpty())
    {
        auto iconArea = previewArea.reduced (20).toFloat();
        float side = juce::jmin (iconArea.getWidth(), iconArea.getHeight());
        auto centered = juce::Rectangle<float> (side, side).withCentre (iconArea.getCentre());
        HardwareDrawing::drawForType (g, currentItem->hardwareType, centered);
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
        PedalPainter::paintDesign (g, pedalRect, currentItem->pedalDesign.get(), dv, dt, false, 1.0f);
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

void InventoryOverlay::buildItemDatabase()
{
    allItems.clear();

    // ── Pedals ──────────────────────────────────────────────────────────
    for (auto& info : getFactoryPedals())
    {
        InventoryItem item;
        item.id = info.name;
        item.displayName = info.name;
        item.category = info.category;
        item.mainCategory = "Pedals";
        item.description = info.category + " pedal with " + juce::String (info.numKnobs) + " knob(s).";
        item.isFactory = true;
        item.pedalInfo = info;
        if (info.designFactory)
            item.pedalDesign = info.designFactory();
        allItems.push_back (std::move (item));
    }

    // User-designed pedals
    auto designsDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                          .getChildFile ("PedalForge").getChildFile ("designs");
    if (designsDir.isDirectory())
    {
        for (const auto& file : designsDir.findChildFiles (juce::File::findFiles, false, "*.json"))
        {
            auto design = std::make_shared<PedalDesign> (PedalDesign::loadFromFile (file));
            if (design->name.isEmpty()) continue;

            InventoryItem item;
            item.id = design->name;
            item.displayName = design->name;
            item.category = design->category.isNotEmpty() ? design->category : "Custom";
            item.mainCategory = "Pedals";
            item.description = "Custom user-designed pedal.";
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
        { "fader",       "Fader",       "Controls",    "Linear slider control. Maps to a continuous parameter (0-1)." },
        { "footswitch",  "Stomp",       "Controls",    "3PDT footswitch for bypass/engage control." },
        { "led",         "LED",         "Lights",      "Single-color LED indicator." },
        { "rgb_led",     "RGB LED",     "Lights",      "Full-color RGB LED indicator." },
        { "indicator",   "Indicator",   "Lights",      "Status indicator light." },
        { "7seg",        "7-Seg",       "Screens",     "7-segment numeric display." },
        { "display",     "Display",     "Screens",     "Small graphical display panel." },
        { "text_screen", "Text",        "Screens",     "Text-based screen for status messages." },
        { "console",     "Console",     "Screens",     "Debug console / text output." },
        { "label",       "Label",       "Decoration",  "Custom text label for the pedal face." },
        { "vu_meter",    "VU Meter",    "Instruments", "Analog-style VU level meter." },
        { "oscilloscope","Scope",       "Instruments", "Mini oscilloscope waveform display." },
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
    struct NodeDef { const char* type; const char* name; const char* category; const char* mainCategory; const char* desc; };
    NodeDef nodes[] = {
        {"audio_input", "Audio Input", "I/O", "Nodes", "Receives audio from the pedalboard input."},
        {"audio_output", "Audio Output", "I/O", "Nodes", "Sends audio to the pedalboard output."},
        {"midi_input", "MIDI Input", "I/O", "Nodes", "Receives MIDI events."},
        {"midi_output", "MIDI Output", "I/O", "Nodes", "Sends MIDI events out."},
        {"expression", "Expression Pedal", "I/O", "Nodes", "Reads expression pedal input."},
        
        {"gain", "Gain", "Utility", "Nodes", "Simple volume adjustment."},
        {"mix", "Mix", "Utility", "Nodes", "Crossfades between two signals."},
        {"split", "Split", "Utility", "Nodes", "Splits one signal into two."},

        {"lowpass", "Low Pass", "Filters", "Effects", "Cuts high frequencies."},
        {"highpass", "High Pass", "Filters", "Effects", "Cuts low frequencies."},
        {"allpass", "All Pass", "Filters", "Effects", "Changes phase without affecting amplitude."},
        {"tonestack", "Tone Stack", "Filters", "Effects", "Classic guitar amp 3-band EQ."},
        {"peq", "Parametric EQ", "Filters", "Effects", "Precise multi-band equalization."},

        {"softclip", "Soft Clip", "Drive", "Effects", "Smooth overdrive."},
        {"hardclip", "Hard Clip", "Drive", "Effects", "Harsh distortion."},
        {"fuzz", "Fuzz", "Drive", "Effects", "Classic transistor fuzz."},

        {"lfo", "LFO", "Modulation", "Effects", "Low frequency oscillator for CV."},
        {"phaser", "Phaser", "Modulation", "Effects", "Phase shifting effect."},
        {"flanger", "Flanger", "Modulation", "Effects", "Classic flanging effect."},

        {"delay", "Delay", "Delay", "Effects", "Standard digital delay."},
        {"mod_delay", "Mod Delay", "Delay", "Effects", "Delay with modulated time."},

        {"compressor", "Compressor", "Dynamics", "Effects", "Reduces dynamic range."},
        {"noisegate", "Noise Gate", "Dynamics", "Effects", "Mutes signal below threshold."},

        {"reverb", "Reverb", "Reverb", "Effects", "Algorithmic room/hall simulation."},
        {"ir", "IR Convolution", "Reverb", "Effects", "Loads impulse responses."},
        {"cabinet", "Cabinet Sim", "Guitar Utility", "Effects", "Speaker cabinet simulation."},

        {"ram", "RAM / Delay Line", "Memory", "Nodes", "Raw memory buffer."},
        {"sampler", "File Sampler", "Memory", "Nodes", "Plays back audio files."},

        {"oscillator", "Oscillator", "Synth", "Nodes", "Standard waveform generator."},
        {"noise", "Noise Gen", "Synth", "Nodes", "White/pink noise source."},
        {"adsr", "ADSR Envelope", "Synth", "Nodes", "4-stage envelope generator."},
        {"ar_env", "AR Envelope", "Synth", "Nodes", "2-stage attack/release envelope."},
        {"svf", "State Variable Filter", "Synth", "Nodes", "Multi-mode resonant filter."},
        {"ladder_filter", "Ladder Filter", "Synth", "Nodes", "Moog-style resonant lowpass."},
        {"vca", "VCA", "Synth", "Nodes", "Voltage controlled amplifier."},

        {"and_gate", "AND Gate", "Logic", "Nodes", "Outputs 1 if all inputs are 1."},
        {"or_gate", "OR Gate", "Logic", "Nodes", "Outputs 1 if any input is 1."},
        {"not_gate", "NOT Gate", "Logic", "Nodes", "Inverts a binary signal."},
        {"comparator", "Comparator", "Logic", "Nodes", "Compares two signals."},
        {"latch", "Latch", "Logic", "Nodes", "Holds a value on trigger."},
        {"mux", "Mux", "Logic", "Nodes", "Selects between multiple inputs."},
        {"constant", "Constant", "Logic", "Nodes", "Outputs a fixed value."},

        {"add", "Add", "Math", "Nodes", "A + B"},
        {"subtract", "Subtract", "Math", "Nodes", "A - B"},
        {"multiply", "Multiply", "Math", "Nodes", "A * B"},
        {"divide", "Divide", "Math", "Nodes", "A / B"},
        {"modulo", "Modulo", "Math", "Nodes", "A % B"},
        {"ranger", "Ranger", "Math", "Nodes", "Remaps a value from one range to another."},
        {"smooth", "Smooth", "Math", "Nodes", "Slews a signal to prevent clicks."},
        {"clamp", "Clamp", "Math", "Nodes", "Constrains a signal between min/max."},
        {"abs", "Abs", "Math", "Nodes", "Absolute value."},

        {"clock", "Clock", "Timing", "Nodes", "Generates steady trigger pulses."},
        {"counter", "Counter", "Timing", "Nodes", "Counts trigger pulses."},
        {"sequencer", "Sequencer", "Timing", "Nodes", "8-step CV sequencer."},
        {"env_follower", "Env Follower", "Timing", "Nodes", "Tracks the amplitude of an audio signal."},
        {"sample_hold", "Sample & Hold", "Timing", "Nodes", "Samples a value on trigger."},

        {"midi_note", "MIDI Note Rx", "MIDI", "Nodes", "Receives MIDI notes as pitch/gate."},
        {"midi_cc", "MIDI CC Rx", "MIDI", "Nodes", "Receives MIDI CC values."},
        {"midi_note_gen", "MIDI Note Tx", "MIDI", "Nodes", "Generates MIDI notes from CV."},

        {"ctrl_knob", "UI Knob", "UI Controls", "Nodes", "Exposes a knob on the pedal face."},
        {"ctrl_switch", "UI Switch", "UI Controls", "Nodes", "Exposes a switch on the pedal face."},
        {"ctrl_button", "UI Button", "UI Controls", "Nodes", "Exposes a momentary button."},
        
        {"led", "UI LED", "Displays", "Nodes", "A simple light on the pedal face."},
        {"vu_meter", "UI VU Meter", "Displays", "Nodes", "Shows audio level on the pedal face."}
    };

    for (auto& n : nodes)
    {
        InventoryItem item;
        item.id = n.type;
        item.displayName = n.name;
        item.category = n.category;
        item.mainCategory = n.mainCategory;
        item.description = n.desc;
        item.isFactory = true;
        // Hardware type can be re-used to store the node type for drag-and-drop
        item.hardwareType = n.type;
        allItems.push_back (std::move (item));
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
            if (! matches) continue;
        }

        filteredItems.push_back (&item);
    }

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
