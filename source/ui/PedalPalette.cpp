#include "PedalPalette.h"
#include "PedalPainter.h"
#include "LookAndFeel.h"
#include "../dsp/PedalDesign.h"

//==============================================================================
// PaletteItem — mini pedal thumbnail using shared PedalPainter
//==============================================================================
PedalPalette::PaletteItem::PaletteItem (const PedalInfo& pedalInfo, std::shared_ptr<PedalDesign> d)
    : info (pedalInfo), design(d)
{
}

void PedalPalette::PaletteItem::paint (juce::Graphics& g)
{
    auto itemBounds = getLocalBounds().toFloat().reduced (6.0f, 3.0f);

    // Constrain to portrait pedal aspect ratio
    float desiredRatio = 0.55f; // w/h
    float availW = itemBounds.getWidth();
    float availH = itemBounds.getHeight();
    float pedalW, pedalH;

    if (availW / availH > desiredRatio)
    {
        pedalH = availH;
        pedalW = pedalH * desiredRatio;
    }
    else
    {
        pedalW = availW;
        pedalH = pedalW / desiredRatio;
    }

    auto bounds = juce::Rectangle<float> (
        itemBounds.getCentreX() - pedalW * 0.5f,
        itemBounds.getCentreY() - pedalH * 0.5f,
        pedalW, pedalH);

    std::map<juce::String, float> dummyValues;
    std::map<juce::String, juce::String> dummyTexts;
    PedalPainter::paintDesign (g, bounds, design.get(), dummyValues, dummyTexts, false, 1.0f);
    
    // Draw the name of the pedal below or on top so they know what it is if it has no design
    if (design == nullptr)
    {
        g.setColour (PedalForgeLookAndFeel::textPrimary);
        g.setFont (juce::FontOptions (12.0f).withStyle("Bold"));
        g.drawText (info.name, bounds.withTrimmedTop(15), juce::Justification::centredTop);
    }
}

void PedalPalette::PaletteItem::mouseDown (const juce::MouseEvent& /*e*/)
{
    dragStarted = false;
}

void PedalPalette::PaletteItem::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragStarted && e.getDistanceFromDragStart() > 5)
    {
        dragStarted = true;
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            float ratioX = e.getMouseDownX() / (float) getWidth();
            float ratioY = e.getMouseDownY() / (float) getHeight();
            container->startDragging ("pedal:" + info.name + ":" + juce::String (ratioX) + ":" + juce::String (ratioY), this);
        }
    }
}

//==============================================================================
// PaletteContent — layout container for the Viewport
//==============================================================================
void PedalPalette::PaletteContent::layoutItems (int width)
{
    int itemHeight = 100;
    int gap = 3;
    int y = 4;

    for (auto& item : items)
    {
        item->setBounds (4, y, width - 8, itemHeight);
        y += itemHeight + gap;
    }

    setSize (width, y + 4);
}

//==============================================================================
// PedalPalette
//==============================================================================
PedalPalette::PedalPalette()
{
    // Factory pedals
    for (auto& info : getFactoryPedals())
    {
        auto item = std::make_unique<PaletteItem> (info);
        content.addAndMakeVisible (*item);
        content.items.push_back (std::move (item));
    }

    // User-designed pedals from saved designs
    loadUserDesigns();

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false); // vertical only
    viewport.setScrollBarThickness (6);
    addAndMakeVisible (viewport);
}

void PedalPalette::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgMid);

    auto header = getLocalBounds().removeFromTop (36);
    g.setColour (PedalForgeLookAndFeel::bgDark);
    g.fillRect (header);

    g.setColour (PedalForgeLookAndFeel::textSecondary);
    g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
    g.drawText ("PEDALS", header.reduced (12, 0), juce::Justification::centredLeft);

    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawHorizontalLine (header.getBottom(), 0.0f, (float) getWidth());
}

void PedalPalette::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (38);

    viewport.setBounds (area);
    content.layoutItems (area.getWidth() - viewport.getScrollBarThickness());
}

void PedalPalette::loadUserDesigns()
{
    auto designsDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                          .getChildFile ("PedalForge").getChildFile ("designs");
    if (! designsDir.isDirectory()) return;

    for (const auto& file : designsDir.findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        auto design = std::make_shared<PedalDesign> (PedalDesign::loadFromFile (file));
        if (design->name.isEmpty()) continue;

        // Create a factory PedalInfo wrapper for the palette
        PedalInfo info;
        info.name = design->name;
        info.category = design->category.isNotEmpty() ? design->category : "Custom";
        info.gridW = 1;
        info.gridH = 2;
        info.numKnobs = 0;
        for (const auto& c : design->controls)
            if (c.type == "knob") info.numKnobs++;
        info.colour = design->chassisColour;
        info.factory = nullptr; // Custom designs don't have a processor factory (yet)

        auto item = std::make_unique<PaletteItem> (info, design);
        content.addAndMakeVisible (*item);
        content.items.push_back (std::move (item));

        loadedDesigns.push_back (design);
    }
}
