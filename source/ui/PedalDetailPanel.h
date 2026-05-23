#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/PedalInstance.h"
#include "../engine/AudioGraphEngine.h"
#include "../midi/MidiLearn.h"

//==============================================================================
/**
 * Full-size detail view of a selected pedal.
 * Draws the same pedal visual as the grid (via PedalPainter) at a larger
 * scale, with interactive knob sliders overlaid on the painted knob positions.
 * Also provides bypass toggle and remove button.
 */
class PedalDetailPanel : public juce::Component,
                          public juce::Slider::Listener,
                          public juce::Button::Listener,
                          public juce::Timer
{
public:
    /** Listener for panel events. */
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void pedalRemoved (juce::AudioProcessorGraph::NodeID nodeId) = 0;
        virtual void pedalValuesChanged (juce::AudioProcessorGraph::NodeID nodeId) {}
    };

    PedalDetailPanel();
    ~PedalDetailPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void sliderValueChanged (juce::Slider* slider) override;
    void buttonClicked (juce::Button* button) override;
    void timerCallback() override;

    juce::Rectangle<float> getPedalRect() const;

    /** Show the detail panel for a specific pedal. */
    void showPedal (PedalInstance& instance, AudioGraphEngine& engine, MidiLearnManager* midiLearn);

    /** Clear the panel (deselect). */
    void clearSelection();

    /** Check if a pedal is currently shown. */
    bool hasSelection() const { return selectedInstance != nullptr; }

    void addListener (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    /** Called when a library_loader control is clicked in the detail panel. */
    std::function<void (const juce::String& category, std::function<void(const juce::File&)> onFileSelected)> onOpenLibrary;

    /** Called when an overlay_launcher control is clicked on the pedal chassis. */
    std::function<void (PedalInstance* instance, const juce::String& controlID)> onOpenOverlay;

private:
    struct KnobEntry
    {
        std::unique_ptr<juce::Slider> knob;
        juce::String paramId;
        juce::String paramName;
    };

    struct FileLoaderEntry
    {
        std::unique_ptr<juce::TextButton> button;
        int targetNodeID = -1;
        juce::String controlID;
    };

    PedalInstance* selectedInstance = nullptr;
    AudioGraphEngine* engineRef = nullptr;
    MidiLearnManager* midiLearnRef = nullptr;

    std::vector<KnobEntry> knobEntries;
    std::vector<FileLoaderEntry> fileLoaders;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::TextButton bypassButton { "BYPASS" };
    juce::TextButton removeButton { "Remove" };
    juce::TextButton saveDefaultButton { "Save as Default" };
    juce::TextButton closeButton  { "x" };
    
    juce::TextButton infoToggleButton { "Info & MIDI Learn" };
    juce::Label infoLabel;
    bool infoExpanded = false;

    juce::ListenerList<Listener> listeners;

    void rebuildKnobs();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalDetailPanel)
};
