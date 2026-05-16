#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/AudioGraphEngine.h"

class MidiSettingsPanel : public juce::Component
{
public:
    MidiSettingsPanel (AudioGraphEngine& engine);
    ~MidiSettingsPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
    
    void refresh();

private:
    AudioGraphEngine& engine;
    
    juce::Label titleLabel { {}, "Global MIDI Settings" };
    
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiSettingsPanel)
};
