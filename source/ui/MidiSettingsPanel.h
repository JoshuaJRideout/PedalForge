#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/AudioGraphEngine.h"
#include "../midi/MidiLearn.h"

class MidiSettingsPanel : public juce::Component,
                          public juce::Timer
{
public:
    MidiSettingsPanel (AudioGraphEngine& engine);
    ~MidiSettingsPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
    void timerCallback() override;
    
    void refresh();

    /** Set the MidiLearnManagers so we can display all CC-to-param bindings. */
    void setMidiLearnManagers (MidiLearnManager* boardMidi, MidiLearnManager* playMidi);

private:
    AudioGraphEngine& engine;
    MidiLearnManager* boardMidiLearn = nullptr;
    MidiLearnManager* playMidiLearn  = nullptr;
    
    // ── App-level MIDI settings ──
    juce::Label titleLabel { {}, "MIDI Configuration" };
    
    juce::Label turingPrevLabel { {}, "Turing Prev Pedal CC:" };
    juce::TextEditor turingPrevInput;
    
    juce::Label turingNextLabel { {}, "Turing Next Pedal CC:" };
    juce::TextEditor turingNextInput;
    
    juce::Label playModeLabel { {}, "Play Mode Toggle CC:" };
    juce::TextEditor playModeInput;

    struct BoardMidiUI
    {
        juce::Label title;
        juce::Label prevLabel;
        juce::TextEditor prevInput;
        juce::Label nextLabel;
        juce::TextEditor nextInput;
    };
    
    std::vector<std::unique_ptr<BoardMidiUI>> boardUIs;

    // ── MidiLearn binding display ──
    struct BindingRow
    {
        juce::Label paramLabel;
        juce::Label ccLabel;
        juce::TextButton deleteBtn { "X" };
    };

    struct BindingSection
    {
        juce::String sectionName;
        MidiLearnManager* manager = nullptr;
        juce::Label headerLabel;
        std::vector<std::unique_ptr<BindingRow>> rows;
    };

    std::vector<std::unique_ptr<BindingSection>> bindingSections;

    void rebuildBindings();
    int lastBindingCount = 0;

    // Scrollable viewport
    std::unique_ptr<juce::Component> contentComponent;
    juce::Viewport viewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiSettingsPanel)
};
