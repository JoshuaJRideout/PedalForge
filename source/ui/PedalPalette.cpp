#include "PedalPalette.h"
#include "PedalPainter.h"
#include "LookAndFeel.h"
#include "../dsp/PedalDesign.h"
#include "../util/AppPaths.h"

//==============================================================================
// PaletteItem — mini pedal thumbnail using shared PedalPainter
//==============================================================================
PedalPalette::PaletteItem::PaletteItem (const PedalInfo& i, std::shared_ptr<PedalDesign> d, std::function<void()> cb)
    : info (i), design (d), onChange (cb)
{
}

void PedalPalette::PaletteItem::paint (juce::Graphics& g)
{
    auto itemBounds = getLocalBounds().toFloat().reduced (6.0f, 3.0f);
    
    // Reserve space for text
    auto textBounds = itemBounds.removeFromBottom (20.0f);

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
    PedalPainter::paintDesign (g, bounds, design.get(), dummyValues, dummyTexts, {}, false, 1.0f);
    
    // Draw the name of the pedal below
    g.setColour (PedalForgeLookAndFeel::textPrimary);
    g.setFont (juce::FontOptions (14.0f).withStyle("Bold"));
    g.drawText (info.name, textBounds, juce::Justification::centred);
}

void PedalPalette::PaletteItem::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown() || e.mods.isCtrlDown())
    {
        // Allow deleting custom designs
        if (design != nullptr && !design->isFactory && onChange)
        {
            juce::PopupMenu menu;
            menu.addItem (1, "Delete Custom Pedal");
            menu.showMenuAsync (juce::PopupMenu::Options(), [this] (int result)
            {
                if (result == 1)
                {
                    // Find the JSON file that matches this design
                    auto designsDir = pf::paths::getDesignsDir();
                    
                    for (const auto& file : designsDir.findChildFiles (juce::File::findFiles, false, "*.json"))
                    {
                        auto d = PedalDesign::loadFromFile (file);
                        if (d.name == design->name)
                        {
                            file.deleteFile();
                            if (onChange) onChange();
                            break;
                        }
                    }
                }
            });
        }
        return;
    }
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
    // Make the pedals tall enough so they take up most of the sidebar width
    int itemHeight = 220; 
    int gap = 12;
    int y = 8;

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
    refreshPedals();

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false); // vertical only
    viewport.setScrollBarThickness (6);
    addAndMakeVisible (viewport);
}

void PedalPalette::refreshPedals()
{
    content.items.clear();
    content.removeAllChildren();
    loadedDesigns.clear();

    auto onChange = [this] { refreshPedals(); };

    // Factory pedals
    for (auto& info : getFactoryPedals())
    {
        std::shared_ptr<PedalDesign> defaultDesign = nullptr;
        if (info.designFactory)
            defaultDesign = info.designFactory();
            
        auto item = std::make_unique<PaletteItem> (info, defaultDesign, onChange);
        content.addAndMakeVisible (*item);
        content.items.push_back (std::move (item));
    }

    // User-designed pedals from saved designs
    loadUserDesigns();
    
    resized();
}

void PedalPalette::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgMid);
}

void PedalPalette::resized()
{
    auto area = getLocalBounds();
    viewport.setBounds (area);
    content.layoutItems (area.getWidth() - viewport.getScrollBarThickness());
}

void PedalPalette::loadUserDesigns()
{
    auto designsDir = pf::paths::getDesignsDir();
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

        auto item = std::make_unique<PaletteItem> (info, design, [this] { refreshPedals(); });
        content.addAndMakeVisible (*item);
        content.items.push_back (std::move (item));

        loadedDesigns.push_back (design);
    }
}
