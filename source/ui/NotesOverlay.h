#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"
#include <vector>

//==============================================================================
/**
 * A draggable, resizable sticky-note box for annotating designs.
 */
struct StickyNote
{
    juce::String text;
    juce::Rectangle<int> bounds { 100, 100, 200, 120 };
    juce::Colour colour { 0xFFFFEB3B }; // warm yellow
};

//==============================================================================
/**
 * Overlay component that renders and manages sticky notes.
 * Add as a child on top of any canvas/editor — it passes through
 * mouse events that don't hit a note.
 */
class NotesOverlay : public juce::Component
{
public:
    NotesOverlay()
    {
        setInterceptsMouseClicks (false, true);
        setAlwaysOnTop (true);
    }

    void setNotes (std::vector<StickyNote>& notesRef) { notes = &notesRef; }

    void paint (juce::Graphics& g) override
    {
        if (!notes || !visible) return;

        for (int i = 0; i < (int) notes->size(); ++i)
        {
            auto& note = (*notes)[i];
            auto r = note.bounds.toFloat();

            // Shadow
            g.setColour (juce::Colour (0x30000000));
            g.fillRoundedRectangle (r.translated (2, 2), 6.0f);

            // Card
            g.setColour (note.colour.withAlpha (0.92f));
            g.fillRoundedRectangle (r, 6.0f);

            // Selection border
            if (i == selectedIndex)
            {
                g.setColour (PedalForgeLookAndFeel::accent);
                g.drawRoundedRectangle (r, 6.0f, 2.0f);
            }

            // Header bar
            auto header = r.removeFromTop (20);
            g.setColour (note.colour.darker (0.15f));
            g.fillRoundedRectangle (header.getX(), header.getY(), header.getWidth(), 20, 6.0f);
            // Fix bottom corners of header
            g.fillRect (header.getX(), header.getY() + 14, header.getWidth(), 6.0f);

            // Title icon
            g.setColour (juce::Colour (0x99000000));
            g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
            g.drawText ("NOTE", header.reduced (6, 0), juce::Justification::centredLeft);

            // Close X
            g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
            g.drawText ("X", header.reduced (4, 0), juce::Justification::centredRight);

            // Text content
            g.setColour (juce::Colour (0xDD000000));
            g.setFont (juce::FontOptions (12.0f));
            auto textArea = note.bounds.toFloat().withTrimmedTop (22).reduced (8, 4);
            g.drawFittedText (note.text, textArea.toNearestInt(), juce::Justification::topLeft, 100);

            // Resize handle (bottom-right corner)
            g.setColour (note.colour.darker (0.3f));
            float hx = note.bounds.getRight() - 10.0f;
            float hy = note.bounds.getBottom() - 10.0f;
            g.drawLine (hx, hy + 8, hx + 8, hy, 1.5f);
            g.drawLine (hx + 3, hy + 8, hx + 8, hy + 3, 1.5f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (!notes || !visible) return;

        auto pos = e.getPosition();

        // Check notes in reverse order (top-most first)
        for (int i = (int) notes->size() - 1; i >= 0; --i)
        {
            auto& note = (*notes)[i];
            if (note.bounds.contains (pos))
            {
                selectedIndex = i;

                // Close button
                auto header = note.bounds.removeFromTop (20);
                auto closeArea = juce::Rectangle<int> (note.bounds.getRight() - 20, note.bounds.getY(), 20, 20);
                // Reconstruct bounds (removeFromTop mutated it)
                note.bounds = (*notes)[i].bounds; // it's still valid since we have a ref
                closeArea = juce::Rectangle<int> (note.bounds.getRight() - 20, note.bounds.getY(), 20, 20);

                if (closeArea.contains (pos))
                {
                    notes->erase (notes->begin() + i);
                    selectedIndex = -1;
                    repaint();
                    return;
                }

                // Resize handle check
                auto resizeArea = juce::Rectangle<int> (note.bounds.getRight() - 14, note.bounds.getBottom() - 14, 14, 14);
                if (resizeArea.contains (pos))
                {
                    isResizing = true;
                    dragStart = pos;
                    originalBounds = note.bounds;
                    setInterceptsMouseClicks (true, true);
                    repaint();
                    return;
                }

                // Header drag
                auto headerArea = juce::Rectangle<int> (note.bounds.getX(), note.bounds.getY(), note.bounds.getWidth(), 20);
                if (headerArea.contains (pos))
                {
                    isDragging = true;
                    dragStart = pos;
                    originalBounds = note.bounds;
                    setInterceptsMouseClicks (true, true);
                    repaint();
                    return;
                }

                // Click in text area — open editor
                showEditor (i);
                repaint();
                return;
            }
        }

        selectedIndex = -1;
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (!notes || selectedIndex < 0 || selectedIndex >= (int) notes->size()) return;
        auto& note = (*notes)[selectedIndex];
        auto delta = e.getPosition() - dragStart;

        if (isDragging)
        {
            note.bounds = originalBounds.translated (delta.x, delta.y);
            repaint();
        }
        else if (isResizing)
        {
            int newW = juce::jmax (120, originalBounds.getWidth() + delta.x);
            int newH = juce::jmax (60, originalBounds.getHeight() + delta.y);
            note.bounds.setSize (newW, newH);
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        isDragging = false;
        isResizing = false;
        setInterceptsMouseClicks (false, true);
    }

    bool hitTest (int x, int y) override
    {
        if (!notes || !visible) return false;
        if (isDragging || isResizing) return true;
        auto pos = juce::Point<int> (x, y);
        for (auto& note : *notes)
            if (note.bounds.contains (pos))
                return true;
        return false;
    }

    void setVisible (bool shouldBeVisible)
    {
        visible = shouldBeVisible;
        juce::Component::setVisible (shouldBeVisible);
        repaint();
    }

    bool isNotesVisible() const { return visible; }

    void addNote (int x, int y)
    {
        if (!notes) return;
        StickyNote n;
        n.bounds = { x, y, 200, 120 };
        notes->push_back (n);
        repaint();
    }

    // Serialization helpers
    static juce::var toJSON (const std::vector<StickyNote>& notes)
    {
        juce::Array<juce::var> arr;
        for (auto& n : notes)
        {
            juce::DynamicObject::Ptr obj = new juce::DynamicObject();
            obj->setProperty ("text", n.text);
            obj->setProperty ("x", n.bounds.getX());
            obj->setProperty ("y", n.bounds.getY());
            obj->setProperty ("w", n.bounds.getWidth());
            obj->setProperty ("h", n.bounds.getHeight());
            obj->setProperty ("colour", (juce::int64) n.colour.getARGB());
            arr.add (juce::var (obj.get()));
        }
        return arr;
    }

    static std::vector<StickyNote> fromJSON (const juce::var& json)
    {
        std::vector<StickyNote> result;
        if (auto* arr = json.getArray())
        {
            for (auto& item : *arr)
            {
                if (auto* obj = item.getDynamicObject())
                {
                    StickyNote n;
                    n.text = obj->getProperty ("text").toString();
                    n.bounds = { (int) obj->getProperty ("x"), (int) obj->getProperty ("y"),
                                 (int) obj->getProperty ("w"), (int) obj->getProperty ("h") };
                    if (obj->hasProperty ("colour"))
                        n.colour = juce::Colour ((juce::uint32)(juce::int64) obj->getProperty ("colour"));
                    result.push_back (n);
                }
            }
        }
        return result;
    }

private:
    std::vector<StickyNote>* notes = nullptr;
    bool visible = false;
    int selectedIndex = -1;
    bool isDragging = false;
    bool isResizing = false;
    juce::Point<int> dragStart;
    juce::Rectangle<int> originalBounds;

    void showEditor (int index)
    {
        if (!notes || index < 0 || index >= (int) notes->size()) return;
        auto& note = (*notes)[index];

        auto* alert = new juce::AlertWindow ("Edit Note", "", juce::AlertWindow::NoIcon);
        alert->addTextEditor ("text", note.text);
        alert->getTextEditor ("text")->setMultiLine (true);
        alert->getTextEditor ("text")->setReturnKeyStartsNewLine (true);
        alert->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<NotesOverlay> sp (this);
        int noteIdx = index;
        alert->enterModalState (true, juce::ModalCallbackFunction::create ([sp, noteIdx, alert] (int r) {
            if (sp != nullptr && sp->notes && noteIdx < (int) sp->notes->size())
            {
                (*sp->notes)[noteIdx].text = alert->getTextEditorContents ("text");
                sp->repaint();
            }
        }));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NotesOverlay)
};
