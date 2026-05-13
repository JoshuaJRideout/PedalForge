#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/PedalInstance.h"
#include "../engine/AudioGraphEngine.h"

//==============================================================================
/**
 * Full-size detail view of a selected pedal.
 * Draws the same pedal visual as the grid (via PedalPainter) at a larger
 * scale, with interactive knob sliders overlaid on the painted knob positions.
 * Also provides bypass toggle and remove button.
 */
class PedalDetailPanel : public juce::Component,
                          public juce::Slider::Listener,
                          public juce::Button::Listener
{
public:
    /** Listener for panel events. */
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void pedalRemoved (juce::AudioProcessorGraph::NodeID nodeId) = 0;
    };

    PedalDetailPanel();
    ~PedalDetailPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void sliderValueChanged (juce::Slider* slider) override;
    void buttonClicked (juce::Button* button) override;

    /** Show the detail panel for a specific pedal. */
    void showPedal (PedalInstance& instance, AudioGraphEngine& engine);

    /** Clear the panel (deselect). */
    void clearSelection();

    /** Check if a pedal is currently shown. */
    bool hasSelection() const { return selectedInstance != nullptr; }

    void addListener (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

private:
    struct KnobEntry
    {
        std::unique_ptr<juce::Slider> knob;
        juce::String paramId;
        juce::String paramName;
    };

    PedalInstance* selectedInstance = nullptr;
    AudioGraphEngine* engineRef = nullptr;

    std::vector<KnobEntry> knobEntries;
    juce::TextButton bypassButton { "BYPASS" };
    juce::TextButton removeButton { "Remove" };
    juce::TextButton closeButton  { "×" };

    juce::ListenerList<Listener> listeners;

    void rebuildKnobs();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalDetailPanel)
};
