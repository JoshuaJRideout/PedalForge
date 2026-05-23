#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"
#include <vector>

#include "../engine/StickyNoteData.h"
//==============================================================================
/**
 * Overlay component that renders and manages sticky notes.
 * Add as a child on top of any canvas/editor — it passes through
 * mouse events that don't hit a note.
 */
class NotesOverlay : public juce::Component
{
public:
    class CustomTextEditor : public juce::TextEditor
    {
    public:
        std::function<void()> onFocusLostCallback;
        
        void focusLost (FocusChangeType cause) override
        {
            juce::TextEditor::focusLost (cause);
            if (onFocusLostCallback)
                onFocusLostCallback();
        }
    };

    NotesOverlay()
    {
        setInterceptsMouseClicks (false, true);
        setAlwaysOnTop (true);

        addAndMakeVisible (activeEditor);
        activeEditor.setVisible (false);
        activeEditor.setMultiLine (true);
        activeEditor.setReturnKeyStartsNewLine (true);
        activeEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        activeEditor.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        activeEditor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        activeEditor.setColour (juce::CaretComponent::caretColourId, juce::Colours::white);
        activeEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        activeEditor.setIndents (2, 2);
        activeEditor.setFont (juce::FontOptions (12.0f));

        activeEditor.onTextChange = [this]() {
            if (notes && selectedIndex >= 0 && selectedIndex < (int) notes->size())
            {
                (*notes)[selectedIndex].text = activeEditor.getText();
                repaint();
            }
        };

        activeEditor.onEscapeKey = [this]() {
            activeEditor.setVisible (false);
            repaint();
        };

        activeEditor.onFocusLostCallback = [this]() {
            activeEditor.setVisible (false);
            repaint();
        };
    }

    void setNotes (std::vector<StickyNote>& notesRef) { notes = &notesRef; }

    void paint (juce::Graphics& g) override
    {
        if (!notes || !visible) return;

        for (int i = 0; i < (int) notes->size(); ++i)
        {
            auto& note = (*notes)[i];
            auto r = note.bounds.toFloat();

            // Background Card: simple clean dark box
            g.setColour (PedalForgeLookAndFeel::bgDark.withAlpha (0.92f));
            g.fillRoundedRectangle (r, 4.0f);

            // Clean white outline (subtle when not active, bright when selected/active)
            bool isSel = (i == selectedIndex);
            auto outlineColour = isSel ? juce::Colours::white.withAlpha (0.8f)
                                       : juce::Colours::white.withAlpha (0.3f);
            float thickness = isSel ? 1.5f : 1.0f;
            g.setColour (outlineColour);
            g.drawRoundedRectangle (r, 4.0f, thickness);

            // Header Area (no background block, just a thin separator line)
            auto header = r.removeFromTop (22);
            g.setColour (juce::Colours::white.withAlpha (0.1f));
            g.drawHorizontalLine ((int) header.getBottom(), note.bounds.getX(), note.bounds.getRight());

            // Header text: small and elegant
            g.setColour (PedalForgeLookAndFeel::textSecondary.withAlpha (0.6f));
            g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
            g.drawText ("NOTE", header.reduced (8, 0), juce::Justification::centredLeft);

            // Close button (drawn as clean vector lines)
            auto closeArea = header.removeFromRight (24.0f).reduced (6.0f);
            g.setColour (juce::Colours::white.withAlpha (isSel ? 0.7f : 0.4f));
            float strokeThickness = 1.5f;
            g.drawLine (closeArea.getX(), closeArea.getY(), closeArea.getRight(), closeArea.getBottom(), strokeThickness);
            g.drawLine (closeArea.getRight(), closeArea.getY(), closeArea.getX(), closeArea.getBottom(), strokeThickness);

            // Text Content (only paint if not actively being edited)
            if (! (isSel && activeEditor.isVisible()))
            {
                g.setColour (PedalForgeLookAndFeel::textPrimary.withAlpha (0.9f));
                g.setFont (juce::FontOptions (12.0f));
                auto textArea = note.bounds.toFloat().withTrimmedTop (22).reduced (8, 4);
                g.drawFittedText (note.text, textArea.toNearestInt(), juce::Justification::topLeft, 100);
            }

            // Resize handle (subtle diagonal lines)
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            float hx = note.bounds.getRight() - 10.0f;
            float hy = note.bounds.getBottom() - 10.0f;
            g.drawLine (hx, hy + 8, hx + 8, hy, 1.0f);
            g.drawLine (hx + 3, hy + 8, hx + 8, hy + 3, 1.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (!notes || !visible) return;

        auto pos = e.getPosition();
        bool clickedOnAnyNote = false;

        // Check notes in reverse order (top-most first)
        for (int i = (int) notes->size() - 1; i >= 0; --i)
        {
            auto& note = (*notes)[i];
            if (note.bounds.contains (pos))
            {
                clickedOnAnyNote = true;
                selectedIndex = i;

                // Close button area
                auto closeArea = juce::Rectangle<int> (note.bounds.getRight() - 24, note.bounds.getY(), 24, 22);
                if (closeArea.contains (pos))
                {
                    activeEditor.setVisible (false);
                    notes->erase (notes->begin() + i);
                    selectedIndex = -1;
                    repaint();
                    return;
                }

                // Resize handle area
                auto resizeArea = juce::Rectangle<int> (note.bounds.getRight() - 14, note.bounds.getBottom() - 14, 14, 14);
                if (resizeArea.contains (pos))
                {
                    activeEditor.setVisible (false);
                    isResizing = true;
                    dragStart = pos;
                    originalBounds = note.bounds;
                    setInterceptsMouseClicks (true, true);
                    repaint();
                    return;
                }

                // Header drag area
                auto headerArea = juce::Rectangle<int> (note.bounds.getX(), note.bounds.getY(), note.bounds.getWidth(), 22);
                if (headerArea.contains (pos))
                {
                    activeEditor.setVisible (false);
                    isDragging = true;
                    dragStart = pos;
                    originalBounds = note.bounds;
                    setInterceptsMouseClicks (true, true);
                    repaint();
                    return;
                }

                // Click in text area — open editor directly inside the note
                showEditor (i);
                repaint();
                return;
            }
        }

        if (!clickedOnAnyNote)
        {
            activeEditor.setVisible (false);
            selectedIndex = -1;
            repaint();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (!notes || selectedIndex < 0 || selectedIndex >= (int) notes->size()) return;
        auto& note = (*notes)[selectedIndex];
        auto delta = e.getPosition() - dragStart;

        if (isDragging)
        {
            note.bounds = originalBounds.translated (delta.x, delta.y);
            if (activeEditor.isVisible())
            {
                auto textArea = note.bounds.withTrimmedTop (22).reduced (8, 4);
                activeEditor.setBounds (textArea);
            }
            repaint();
        }
        else if (isResizing)
        {
            int newW = juce::jmax (120, originalBounds.getWidth() + delta.x);
            int newH = juce::jmax (60, originalBounds.getHeight() + delta.y);
            note.bounds.setSize (newW, newH);
            if (activeEditor.isVisible())
            {
                auto textArea = note.bounds.withTrimmedTop (22).reduced (8, 4);
                activeEditor.setBounds (textArea);
            }
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

    void setVisible (bool shouldBeVisible) override
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


private:
    std::vector<StickyNote>* notes = nullptr;
    bool visible = false;
    int selectedIndex = -1;
    bool isDragging = false;
    bool isResizing = false;
    juce::Point<int> dragStart;
    juce::Rectangle<int> originalBounds;
    CustomTextEditor activeEditor;

    void showEditor (int index)
    {
        if (!notes || index < 0 || index >= (int) notes->size()) return;
        auto& note = (*notes)[index];

        activeEditor.setText (note.text, juce::dontSendNotification);
        
        auto textArea = note.bounds.withTrimmedTop (22).reduced (8, 4);
        activeEditor.setBounds (textArea);
        activeEditor.setVisible (true);
        activeEditor.grabKeyboardFocus();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NotesOverlay)
};
