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
    juce::TextButton turingPrevLearnBtn { "Learn" };
    
    juce::Label turingNextLabel { {}, "Turing Next Pedal CC:" };
    juce::TextEditor turingNextInput;
    juce::TextButton turingNextLearnBtn { "Learn" };
    
    juce::Label playModeLabel { {}, "Play Mode Toggle CC:" };
    juce::TextEditor playModeInput;
    juce::TextButton playModeLearnBtn { "Learn" };

    // Navigation — Page & Track
    juce::Label pageLeftLabel   { {}, "Page Left CC:" };
    juce::TextEditor pageLeftInput;
    juce::TextButton pageLeftLearnBtn { "Learn" };
    juce::Label pageRightLabel  { {}, "Page Right CC:" };
    juce::TextEditor pageRightInput;
    juce::TextButton pageRightLearnBtn { "Learn" };
    juce::Label trackLeftLabel  { {}, "Track Left CC:" };
    juce::TextEditor trackLeftInput;
    juce::TextButton trackLeftLearnBtn { "Learn" };
    juce::Label trackRightLabel { {}, "Track Right CC:" };
    juce::TextEditor trackRightInput;
    juce::TextButton trackRightLearnBtn { "Learn" };

    juce::Label novationModeLabel { {}, "Novation DAW Mode Integration:" };
    juce::ComboBox novationModeCombo;

    struct BoardMidiUI
    {
        juce::Label title;
        juce::Label prevLabel;
        juce::TextEditor prevInput;
        juce::TextButton prevLearnBtn { "Learn" };
        juce::Label nextLabel;
        juce::TextEditor nextInput;
        juce::TextButton nextLearnBtn { "Learn" };
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

    // ── MIDI Monitor UI ──
    juce::Label midiMonitorTitle { {}, "Live MIDI Monitor" };
    juce::ToggleButton filterNotesBtn { "Notes" };
    juce::ToggleButton filterCCsBtn { "CCs" };
    juce::ToggleButton filterOtherBtn { "Other" };
    juce::TextButton pauseBtn { "Pause" };
    juce::TextButton clearBtn { "Clear" };
    juce::TextEditor midiLogText;
    
    bool isPaused = false;
    void updateMidiMonitorUI();

    // Scrollable viewport
    std::unique_ptr<juce::Component> contentComponent;
    juce::Viewport viewport;

    // ── MIDI Learn for Configuration Parameters ──
public:
    enum class LearnTarget
    {
        None,
        TuringPrev,
        TuringNext,
        PlayMode,
        PageLeft,
        PageRight,
        TrackLeft,
        TrackRight,
        BoardPrev,
        BoardNext
    };

private:
    LearnTarget activeLearnTarget = LearnTarget::None;
    int activeLearnBoardIndex = -1;
    juce::TextButton* activeLearnBtn = nullptr;
    juce::Time learnStartTime;

    void cancelActiveLearning();
    void assignLearnedCC (int ccNumber);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiSettingsPanel)
};
