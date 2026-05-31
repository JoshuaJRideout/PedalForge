#include "InventoryPanel.h"
#include "LookAndFeel.h"
#include "StyleKit.h"    // pf::StyleKitRegistry, pf::ControlState, pf::Colorway
#include "NodeColours.h" // pf::nodeColourForType (node thumbnails)

//==============================================================================
// One draggable item row: thumbnail + name/description. Drag places it on the
// workspace via the shared drag descriptor; a plain click fires onClick.
//==============================================================================
class InventoryPanel::Cell : public juce::Component
{
public:
    Cell (const pf::inv::Item& it) : item (it) {}

    std::function<void (const pf::inv::Item&)> onClick;

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.0f);

        if (hovered)
        {
            g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.12f));
            g.fillRoundedRectangle (b, 4.0f);
        }

        // Thumbnail (left square).
        auto thumb = b.removeFromLeft (b.getHeight()).reduced (4.0f);
        if (item.mainCategory == "Parts")
        {
            // Hardware part: render the real control glyph through the kit.
            pf::StyleKitRegistry::draw (g, "default", item.hardwareType, thumb,
                                        pf::ControlState (0.5f), pf::Colorway{}, nullptr);
        }
        else
        {
            // Pedals and DSP nodes get a coloured chip + initials. (Nodes have
            // no real icon yet — colour comes from the shared node-colour map so
            // it matches the FX canvas; pedals use their chassis colour.)
            const juce::Colour chip = (item.mainCategory == "Pedals")
                                        ? item.pedalInfo.colour
                                        : pf::nodeColourForType (item.hardwareType);
            g.setColour (chip.withAlpha (0.92f));
            g.fillRoundedRectangle (thumb, 3.0f);

            // Node chips read as "blocks" with little I/O port nubs on the sides.
            if (item.mainCategory != "Pedals")
            {
                g.setColour (juce::Colours::black.withAlpha (0.30f));
                const float r = 1.6f, cy = thumb.getCentreY();
                g.fillEllipse (thumb.getX() - r, cy - r, r * 2, r * 2);
                g.fillEllipse (thumb.getRight() - r, cy - r, r * 2, r * 2);
            }

            g.setColour (chip.contrasting (0.85f));
            g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
            g.drawText (item.displayName.substring (0, 2).toUpperCase(), thumb,
                        juce::Justification::centred);
        }

        // Name + sub-text.
        b.removeFromLeft (6.0f);
        auto textArea = b;
        g.setColour (PedalForgeLookAndFeel::textPrimary);
        g.setFont (juce::FontOptions (12.5f).withStyle ("Bold"));
        g.drawText (item.displayName, textArea.removeFromTop (16.0f).toNearestInt(),
                    juce::Justification::centredLeft, true);

        g.setColour (PedalForgeLookAndFeel::textMuted);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (item.category, textArea.removeFromTop (13.0f).toNearestInt(),
                    juce::Justification::centredLeft, true);
    }

    void mouseEnter (const juce::MouseEvent&) override { hovered = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }
    void mouseDown  (const juce::MouseEvent&) override { dragStarted = false; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragStarted && e.getDistanceFromDragStart() > 5)
        {
            dragStarted = true;
            if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
            {
                const float rx = e.getMouseDownX() / (float) juce::jmax (1, getWidth());
                const float ry = e.getMouseDownY() / (float) juce::jmax (1, getHeight());
                const juce::String desc = pf::inv::dragDescriptor (item, rx, ry);
                if (desc.isNotEmpty())
                {
                    juce::Image emptyImage (juce::Image::ARGB, 1, 1, true);
                    container->startDragging (desc, this, emptyImage, false);
                }
            }
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! dragStarted && e.mouseWasClicked() && ! e.mods.isRightButtonDown())
            if (onClick) onClick (item);
    }

private:
    pf::inv::Item item;
    bool hovered = false;
    bool dragStarted = false;
};

//==============================================================================
// A non-interactive category header row.
//==============================================================================
namespace
{
    class HeaderRow : public juce::Component
    {
    public:
        explicit HeaderRow (juce::String t) : text (std::move (t)) {}
        void paint (juce::Graphics& g) override
        {
            g.setColour (PedalForgeLookAndFeel::textMuted);
            g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
            g.drawText (text.toUpperCase(), getLocalBounds().reduced (6, 0),
                        juce::Justification::centredLeft);
            g.setColour (PedalForgeLookAndFeel::gridLine);
            g.drawHorizontalLine (getHeight() - 1, 6.0f, (float) getWidth() - 6.0f);
        }
    private:
        juce::String text;
    };
}

//==============================================================================
InventoryPanel::InventoryPanel()
{
    searchBox.setTextToShowWhenEmpty ("Search...", PedalForgeLookAndFeel::textMuted);
    searchBox.setColour (juce::TextEditor::backgroundColourId, PedalForgeLookAndFeel::bgLight);
    searchBox.setColour (juce::TextEditor::textColourId, PedalForgeLookAndFeel::textPrimary);
    searchBox.setColour (juce::TextEditor::outlineColourId, PedalForgeLookAndFeel::gridLine);
    searchBox.onTextChange = [this] { searchQuery = searchBox.getText().trim(); rebuildRows(); };
    addAndMakeVisible (searchBox);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    refresh();
}

void InventoryPanel::setContext (pf::inv::Context ctx)
{
    if (context == ctx) return;
    context = ctx;
    rebuildRows();
}

void InventoryPanel::refresh()
{
    allItems = pf::inv::buildItems();
    rebuildRows();
}

void InventoryPanel::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgDark);
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
}

void InventoryPanel::resized()
{
    auto area = getLocalBounds().reduced (4);
    searchBox.setBounds (area.removeFromTop (26));
    area.removeFromTop (4);
    viewport.setBounds (area);

    // Re-flow rows to the (possibly new) width, minus the scrollbar.
    const int w = viewport.getMaximumVisibleWidth();
    int y = 0;
    for (auto* r : rows)
    {
        const int h = (dynamic_cast<Cell*> (r) != nullptr) ? 40 : 20;
        r->setBounds (0, y, w, h);
        y += h;
    }
    content.setSize (w, y);
}

void InventoryPanel::rebuildRows()
{
    rows.clear();
    content.removeAllChildren();

    const auto mainCats = pf::inv::mainCategoriesForContext (context);
    const juce::String q = searchQuery;

    auto matches = [&] (const pf::inv::Item& it)
    {
        if (! mainCats.contains (it.mainCategory)) return false;
        if (q.isEmpty()) return true;
        if (it.displayName.containsIgnoreCase (q)) return true;
        if (it.category.containsIgnoreCase (q))    return true;
        if (it.description.containsIgnoreCase (q)) return true;
        for (const auto& t : it.tags) if (t.containsIgnoreCase (q)) return true;
        return false;
    };

    // Group matching items by sub-category, preserving first-seen order.
    juce::StringArray catOrder;
    std::map<juce::String, std::vector<const pf::inv::Item*>> byCat;
    for (const auto& it : allItems)
        if (matches (it))
        {
            if (! catOrder.contains (it.category)) catOrder.add (it.category);
            byCat[it.category].push_back (&it);
        }

    for (const auto& cat : catOrder)
    {
        auto* header = new HeaderRow (cat);
        rows.add (header);
        content.addAndMakeVisible (header);

        for (const auto* it : byCat[cat])
        {
            auto* cell = new Cell (*it);
            cell->onClick = [this] (const pf::inv::Item& clicked) { if (onItemClicked) onItemClicked (clicked); };
            rows.add (cell);
            content.addAndMakeVisible (cell);
        }
    }

    resized();
    repaint();
}
