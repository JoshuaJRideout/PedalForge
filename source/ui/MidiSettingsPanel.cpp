#include "MidiSettingsPanel.h"
#include "LookAndFeel.h"

MidiSettingsPanel::MidiSettingsPanel (AudioGraphEngine& eng) : engine(eng)
{
    titleLabel.setFont (juce::FontOptions (18.0f).withStyle("Bold"));
    titleLabel.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textPrimary);
    addAndMakeVisible (titleLabel);
    
    auto setupInput = [](juce::TextEditor& input, int initialValue) {
        input.setMultiLine (false);
        input.setReturnKeyStartsNewLine (false);
        input.setReadOnly (false);
        input.setScrollbarsShown (false);
        input.setCaretVisible (true);
        input.setPopupMenuEnabled (false);
        input.setText (initialValue >= 0 ? juce::String(initialValue) : "");
        input.setJustification (juce::Justification::centred);
        input.setInputRestrictions (3, "0123456789");
    };
    
    // Global Turing Prev
    addAndMakeVisible (turingPrevLabel);
    addAndMakeVisible (turingPrevInput);
    setupInput (turingPrevInput, engine.appMidiConfig.turingPrevCC);
    turingPrevInput.onTextChange = [this] {
        engine.appMidiConfig.turingPrevCC = turingPrevInput.getText().isEmpty() ? -1 : turingPrevInput.getText().getIntValue();
    };
    
    // Global Turing Next
    addAndMakeVisible (turingNextLabel);
    addAndMakeVisible (turingNextInput);
    setupInput (turingNextInput, engine.appMidiConfig.turingNextCC);
    turingNextInput.onTextChange = [this] {
        engine.appMidiConfig.turingNextCC = turingNextInput.getText().isEmpty() ? -1 : turingNextInput.getText().getIntValue();
    };
    
    // Global Play Mode
    addAndMakeVisible (playModeLabel);
    addAndMakeVisible (playModeInput);
    setupInput (playModeInput, engine.appMidiConfig.playModeToggleCC);
    playModeInput.onTextChange = [this] {
        engine.appMidiConfig.playModeToggleCC = playModeInput.getText().isEmpty() ? -1 : playModeInput.getText().getIntValue();
    };

    refresh();
}

MidiSettingsPanel::~MidiSettingsPanel()
{
}

void MidiSettingsPanel::visibilityChanged()
{
    if (isVisible())
        refresh();
}

void MidiSettingsPanel::refresh()
{
    auto setupInput = [](juce::TextEditor& input, int initialValue) {
        input.setMultiLine (false);
        input.setReturnKeyStartsNewLine (false);
        input.setReadOnly (false);
        input.setScrollbarsShown (false);
        input.setCaretVisible (true);
        input.setPopupMenuEnabled (false);
        input.setText (initialValue >= 0 ? juce::String(initialValue) : "");
        input.setJustification (juce::Justification::centred);
        input.setInputRestrictions (3, "0123456789");
    };

    boardUIs.clear();

    for (auto& board : engine.getBoards())
    {
        auto ui = std::make_unique<BoardMidiUI>();
        
        ui->title.setText ("Board: " + board.name, juce::dontSendNotification);
        ui->title.setFont (juce::FontOptions (16.0f).withStyle("Bold"));
        ui->title.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textSecondary);
        addAndMakeVisible (ui->title);
        
        ui->prevLabel.setText ("Prev Page CC:", juce::dontSendNotification);
        addAndMakeVisible (ui->prevLabel);
        
        addAndMakeVisible (ui->prevInput);
        setupInput (ui->prevInput, board.prevPageCC);
        ui->prevInput.onTextChange = [&board, rawUi = ui.get()] {
            board.prevPageCC = rawUi->prevInput.getText().isEmpty() ? -1 : rawUi->prevInput.getText().getIntValue();
        };
        
        ui->nextLabel.setText ("Next Page CC:", juce::dontSendNotification);
        addAndMakeVisible (ui->nextLabel);
        
        addAndMakeVisible (ui->nextInput);
        setupInput (ui->nextInput, board.nextPageCC);
        ui->nextInput.onTextChange = [&board, rawUi = ui.get()] {
            board.nextPageCC = rawUi->nextInput.getText().isEmpty() ? -1 : rawUi->nextInput.getText().getIntValue();
        };
        
        boardUIs.push_back (std::move (ui));
    }
    
    resized();
}

void MidiSettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgDark);
}

void MidiSettingsPanel::resized()
{
    auto area = getLocalBounds().reduced (20);
    
    titleLabel.setBounds (area.removeFromTop (30));
    area.removeFromTop (10);
    
    auto layoutRow = [](juce::Rectangle<int>& a, juce::Label& lbl, juce::TextEditor& input) {
        auto row = a.removeFromTop (25);
        lbl.setBounds (row.removeFromLeft (160));
        input.setBounds (row.removeFromLeft (50));
        a.removeFromTop (5);
    };
    
    layoutRow (area, turingPrevLabel, turingPrevInput);
    layoutRow (area, turingNextLabel, turingNextInput);
    layoutRow (area, playModeLabel, playModeInput);
    
    area.removeFromTop (10);
    
    for (auto& ui : boardUIs)
    {
        ui->title.setBounds (area.removeFromTop (25));
        layoutRow (area, ui->prevLabel, ui->prevInput);
        layoutRow (area, ui->nextLabel, ui->nextInput);
        area.removeFromTop (10);
    }
}
