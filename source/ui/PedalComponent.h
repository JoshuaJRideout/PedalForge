#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/PedalInstance.h"
#include "../engine/AudioGraphEngine.h"

class PedalboardGrid;

//==============================================================================
/**
 * Compact mini-pedal tile for the pedalboard grid.
 * Shows coloured body, name, bypass LED, and I/O indicators.
 * Click to select (opens detail panel). Drag to reposition.
 */
class PedalComponent : public juce::Component
{
public:
    PedalComponent (PedalInstance& instance, AudioGraphEngine& engine);
    ~PedalComponent() override = default;

    void paint (juce::Graphics& g) override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;

    //==========================================================================
    PedalInstance& getInstance() { return instance; }
    const PedalInstance& getInstance() const { return instance; }

    /** Set the parent grid (for snapping / selection). */
    void setGrid (PedalboardGrid* grid) { parentGrid = grid; }

    /** Set selected state (highlight). */
    void setSelected (bool s) { selected = s; repaint(); }
    bool isSelected() const { return selected; }

private:
    PedalInstance& instance;
    AudioGraphEngine& engine;
    PedalboardGrid* parentGrid = nullptr;

    bool dragging = false;
    bool selected = false;

    // Drag state — snap-to-grid with validity feedback
    juce::Point<int> dragOffset;           // Mouse offset from component origin at drag start
    juce::Point<int> dragSnappedGrid;      // Current snapped grid cell during drag
    bool dragValid = false;                // Whether the current snapped position is a valid drop

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalComponent)
};
