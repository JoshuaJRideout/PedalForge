#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/PedalInstance.h"
#include "../engine/AudioGraphEngine.h"
#include "../midi/MidiLearn.h"

class BoardComponent;

//==============================================================================
/**
 * Compact mini-pedal tile for the pedalboard grid.
 * Shows coloured body, name, bypass LED, and I/O indicators.
 * Click to select (opens detail panel). Drag to reposition.
 */
class PedalComponent : public juce::Component
{
public:
    PedalComponent (PedalInstance& instance, AudioGraphEngine& engine, MidiLearnManager& midiLearn);
    ~PedalComponent() override = default;

    void paint (juce::Graphics& g) override;
    
    bool isDraggingKnob() const { return draggedKnobID.isNotEmpty(); }

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;

    //==========================================================================
    PedalInstance& getInstance() { return instance; }
    const PedalInstance& getInstance() const { return instance; }

    /** Set the parent grid (for snapping / selection). */
    void setGrid (BoardComponent* grid) { parentBoard = grid; }

    /** Set selected state (highlight). */
    void setSelected (bool s) { selected = s; repaint(); }
    bool isSelected() const { return selected; }

    /** Called when user clicks a library_loader control.
        Parameters: category ID (e.g. "NAM"), target DSP node ID within the pedal. */
    std::function<void (const juce::String& category, int targetNodeID)> onOpenLibrary;

private:
    PedalInstance& instance;
    AudioGraphEngine& engine;
    MidiLearnManager& midiLearn;
    BoardComponent* parentBoard = nullptr;

    bool dragging = false;
    bool selected = false;

    // Drag state — snap-to-grid with validity feedback
    juce::Point<int> dragOffset;           // Mouse offset from component origin at drag start
    juce::Point<int> dragSnappedGrid;      // Current snapped grid cell during drag
    bool dragValid = false;                // Whether the current snapped position is a valid drop
    
    juce::String draggedKnobID;
    float draggedKnobStartValue = 0.0f;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalComponent)
};
