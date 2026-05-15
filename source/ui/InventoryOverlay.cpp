#include "InventoryOverlay.h"
#include "LookAndFeel.h"
#include "HardwareDrawing.h"

//==============================================================================
// HardwareItemGrid::HwItem
//==============================================================================
InventoryOverlay::HardwareItemGrid::HwItem::HwItem (const juce::String& t, const juce::String& n)
    : type (t), name (n) {}

void InventoryOverlay::HardwareItemGrid::HwItem::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (4.0f);
    auto iconArea = area.removeFromTop (area.getHeight() - 18.0f);

    // Background card
    g.setColour (PedalForgeLookAndFeel::bgLight.withAlpha (0.5f));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);

    HardwareDrawing::drawForType (g, type, iconArea.reduced (6.0f));

    g.setColour (PedalForgeLookAndFeel::textSecondary);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText (name, area, juce::Justification::centredBottom);
}

void InventoryOverlay::HardwareItemGrid::HwItem::mouseDown (const juce::MouseEvent&)
{
    dragStarted = false;
}

void InventoryOverlay::HardwareItemGrid::HwItem::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragStarted && e.getDistanceFromDragStart() > 5)
    {
        dragStarted = true;
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            float ratioX = e.getMouseDownX() / (float) getWidth();
            float ratioY = e.getMouseDownY() / (float) getHeight();
            juce::String desc = "hardware:" + type + ":" + juce::String (ratioX) + ":" + juce::String (ratioY);
            juce::Image emptyImage (juce::Image::ARGB, 1, 1, true);
            container->startDragging (desc, this, emptyImage, false);
        }
    }
}

//==============================================================================
// HardwareItemGrid
//==============================================================================
InventoryOverlay::HardwareItemGrid::HardwareItemGrid()
{
    // Controls
    items.push_back (std::make_unique<HwItem> ("knob",       "Knob"));
    items.push_back (std::make_unique<HwItem> ("switch",     "Switch"));
    items.push_back (std::make_unique<HwItem> ("fader",      "Fader"));
    items.push_back (std::make_unique<HwItem> ("footswitch", "Stomp"));
    // Lights
    items.push_back (std::make_unique<HwItem> ("led",        "LED"));
    items.push_back (std::make_unique<HwItem> ("rgb_led",    "RGB LED"));
    items.push_back (std::make_unique<HwItem> ("indicator",  "Indicator"));
    // Screens
    items.push_back (std::make_unique<HwItem> ("7seg",       "7-Seg"));
    items.push_back (std::make_unique<HwItem> ("display",    "Display"));
    items.push_back (std::make_unique<HwItem> ("text_screen","Text"));
    items.push_back (std::make_unique<HwItem> ("console",    "Console"));
    // Decoration
    items.push_back (std::make_unique<HwItem> ("label",      "Label"));
    // Instruments
    items.push_back (std::make_unique<HwItem> ("vu_meter",   "VU Meter"));
    items.push_back (std::make_unique<HwItem> ("oscilloscope","Scope"));

    for (auto& item : items)
        content.addAndMakeVisible (*item);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (6);
    addAndMakeVisible (viewport);
}

void InventoryOverlay::HardwareItemGrid::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgMid);
}

void InventoryOverlay::HardwareItemGrid::resized()
{
    viewport.setBounds (getLocalBounds());

    int cols = juce::jmax (3, getWidth() / 90);
    int cellSize = (getWidth() - viewport.getScrollBarThickness()) / cols;
    int y = 8;

    for (int i = 0; i < (int) items.size(); ++i)
    {
        int col = i % cols;
        int row = i / cols;
        items[(size_t) i]->setBounds (col * cellSize + 4, y + row * cellSize, cellSize - 8, cellSize - 8);
    }

    int totalRows = ((int) items.size() + cols - 1) / cols;
    content.setSize (getWidth() - viewport.getScrollBarThickness(), y + totalRows * cellSize + 8);
}

//==============================================================================
// InventoryOverlay
//==============================================================================
InventoryOverlay::InventoryOverlay()
{
    setInterceptsMouseClicks (false, false);  // transparent when hidden
    setAlwaysOnTop (true);

    tabs.setOutline (0);
    tabs.setTabBarDepth (36);

    auto bg = PedalForgeLookAndFeel::bgDark;

    tabs.addTab ("Pedals",  bg, &pedalPalette, false);
    tabs.addTab ("Parts",   bg, &hardwareGrid,  false);
    tabs.addTab ("Nodes",   bg, &nodesTab,      false);
    tabs.addTab ("IRs",     bg, &irsTab,        false);
    tabs.addTab ("Images",  bg, &imagesTab,     false);

    addChildComponent (tabs);
}

InventoryOverlay::~InventoryOverlay() = default;

void InventoryOverlay::paint (juce::Graphics& g)
{
    if (animAlpha <= 0.0f) return;

    // Dark backdrop
    g.setColour (juce::Colours::black.withAlpha (animAlpha * 0.65f));
    g.fillAll();

    // Subtle border glow around the panel
    auto panelBounds = tabs.getBounds().toFloat().expanded (2.0f);
    g.setColour (PedalForgeLookAndFeel::accent.withAlpha (animAlpha * 0.3f));
    g.drawRoundedRectangle (panelBounds, 12.0f, 2.0f);
}

void InventoryOverlay::resized()
{
    // Center the panel, occupying ~80% of the available area
    auto area = getLocalBounds();
    int panelW = juce::jmin (area.getWidth()  - 80, 900);
    int panelH = juce::jmin (area.getHeight() - 80, 700);

    auto panelArea = juce::Rectangle<int> (panelW, panelH)
                         .withCentre (area.getCentre());

    tabs.setBounds (panelArea);
}

void InventoryOverlay::toggle()
{
    if (visible)
        hide();
    else
        show();
}

void InventoryOverlay::show()
{
    visible = true;
    animAlpha = 1.0f;
    tabs.setVisible (true);
    setInterceptsMouseClicks (true, true);
    setVisible (true);
    toFront (true);
    repaint();
}

void InventoryOverlay::hide()
{
    visible = false;
    animAlpha = 0.0f;
    tabs.setVisible (false);
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

    // Escape closes
    if (key == juce::KeyPress::escapeKey && visible)
    {
        hide();
        return true;
    }

    return false;
}
