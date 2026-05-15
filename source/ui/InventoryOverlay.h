#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PedalPalette.h"

//==============================================================================
/**
 * A full-screen, pause-menu-style inventory overlay (inspired by Wiremod's Q-menu).
 *
 * When activated, a semi-transparent dark backdrop fades in, and a centered
 * panel slides up containing a tabbed interface for browsing and dragging
 * pedals, parts, nodes, IRs, images, etc. onto the active workspace.
 *
 * Toggle with a keyboard shortcut (Tab) or toolbar button.
 */
class InventoryOverlay : public juce::Component,
                         public juce::KeyListener
{
public:
    InventoryOverlay();
    ~InventoryOverlay() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Show/hide the overlay with animation. */
    void toggle();
    void show();
    void hide();

    bool isOpen() const { return visible; }

    /** KeyListener — intercept Tab key at the editor level. */
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    /** Access to the pedal palette for drag/drop compatibility. */
    PedalPalette& getPedalPalette() { return pedalPalette; }

    //==========================================================================
    // Hardware items for the "Parts" tab (knobs, switches, etc.)
    //==========================================================================
    class HardwareItemGrid : public juce::Component
    {
    public:
        HardwareItemGrid();
        void paint (juce::Graphics& g) override;
        void resized() override;

    private:
        struct HwItem : public juce::Component
        {
            HwItem (const juce::String& type, const juce::String& name);
            void paint (juce::Graphics& g) override;
            void mouseDown (const juce::MouseEvent&) override;
            void mouseDrag (const juce::MouseEvent& e) override;

            juce::String type, name;
            bool dragStarted = false;
        };

        juce::Viewport viewport;
        juce::Component content;
        std::vector<std::unique_ptr<HwItem>> items;
    };

private:
    bool visible = false;
    float animAlpha = 0.0f;

    // The big central panel
    juce::TabbedComponent tabs { juce::TabbedButtonBar::Orientation::TabsAtTop };

    // Tab contents
    PedalPalette pedalPalette;
    HardwareItemGrid hardwareGrid;
    juce::Component nodesTab;
    juce::Component irsTab;
    juce::Component imagesTab;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InventoryOverlay)
};
