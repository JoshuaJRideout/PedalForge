#include "MidiSettingsPanel.h"
#include "LookAndFeel.h"

//==============================================================================
MidiSettingsPanel::MidiSettingsPanel (AudioGraphEngine& eng) : engine(eng)
{
    // ── Scrollable viewport ──
    contentComponent = std::make_unique<juce::Component>();
    viewport.setViewedComponent (contentComponent.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setColour (juce::ScrollBar::thumbColourId, PedalForgeLookAndFeel::accent.withAlpha(0.4f));
    addAndMakeVisible (viewport);

    // ── Title ──
    titleLabel.setFont (juce::FontOptions (20.0f).withStyle("Bold"));
    titleLabel.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textPrimary);
    contentComponent->addAndMakeVisible (titleLabel);
    
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
    contentComponent->addAndMakeVisible (turingPrevLabel);
    contentComponent->addAndMakeVisible (turingPrevInput);
    setupInput (turingPrevInput, engine.appMidiConfig.turingPrevCC);
    turingPrevInput.onTextChange = [this] {
        engine.appMidiConfig.turingPrevCC = turingPrevInput.getText().isEmpty() ? -1 : turingPrevInput.getText().getIntValue();
    };
    
    // Global Turing Next
    contentComponent->addAndMakeVisible (turingNextLabel);
    contentComponent->addAndMakeVisible (turingNextInput);
    setupInput (turingNextInput, engine.appMidiConfig.turingNextCC);
    turingNextInput.onTextChange = [this] {
        engine.appMidiConfig.turingNextCC = turingNextInput.getText().isEmpty() ? -1 : turingNextInput.getText().getIntValue();
    };
    
    // Global Play Mode
    contentComponent->addAndMakeVisible (playModeLabel);
    contentComponent->addAndMakeVisible (playModeInput);
    setupInput (playModeInput, engine.appMidiConfig.playModeToggleCC);
    playModeInput.onTextChange = [this] {
        engine.appMidiConfig.playModeToggleCC = playModeInput.getText().isEmpty() ? -1 : playModeInput.getText().getIntValue();
    };

    refresh();
}

MidiSettingsPanel::~MidiSettingsPanel()
{
    stopTimer();
}

void MidiSettingsPanel::setMidiLearnManagers (MidiLearnManager* boardMidi, MidiLearnManager* playMidi)
{
    boardMidiLearn = boardMidi;
    playMidiLearn  = playMidi;
}

void MidiSettingsPanel::visibilityChanged()
{
    if (isVisible())
    {
        refresh();
        startTimerHz (4); // poll for new bindings while visible
    }
    else
    {
        stopTimer();
    }
}

void MidiSettingsPanel::timerCallback()
{
    // Check if binding count changed and rebuild if so
    int total = 0;
    if (boardMidiLearn) total += (int) boardMidiLearn->getMappings().size();
    if (playMidiLearn)  total += (int) playMidiLearn->getMappings().size();
    if (total != lastBindingCount)
        rebuildBindings();
}

//==============================================================================
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

    // Rebuild board UIs
    for (auto& ui : boardUIs)
    {
        contentComponent->removeChildComponent (&ui->title);
        contentComponent->removeChildComponent (&ui->prevLabel);
        contentComponent->removeChildComponent (&ui->prevInput);
        contentComponent->removeChildComponent (&ui->nextLabel);
        contentComponent->removeChildComponent (&ui->nextInput);
    }
    boardUIs.clear();

    for (auto& board : engine.getBoards())
    {
        auto ui = std::make_unique<BoardMidiUI>();
        
        ui->title.setText ("Board: " + board.name, juce::dontSendNotification);
        ui->title.setFont (juce::FontOptions (15.0f).withStyle("Bold"));
        ui->title.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::accent);
        contentComponent->addAndMakeVisible (ui->title);
        
        ui->prevLabel.setText ("Prev Page CC:", juce::dontSendNotification);
        contentComponent->addAndMakeVisible (ui->prevLabel);
        
        contentComponent->addAndMakeVisible (ui->prevInput);
        setupInput (ui->prevInput, board.prevPageCC);
        ui->prevInput.onTextChange = [&board, rawUi = ui.get()] {
            board.prevPageCC = rawUi->prevInput.getText().isEmpty() ? -1 : rawUi->prevInput.getText().getIntValue();
        };
        
        ui->nextLabel.setText ("Next Page CC:", juce::dontSendNotification);
        contentComponent->addAndMakeVisible (ui->nextLabel);
        
        contentComponent->addAndMakeVisible (ui->nextInput);
        setupInput (ui->nextInput, board.nextPageCC);
        ui->nextInput.onTextChange = [&board, rawUi = ui.get()] {
            board.nextPageCC = rawUi->nextInput.getText().isEmpty() ? -1 : rawUi->nextInput.getText().getIntValue();
        };
        
        boardUIs.push_back (std::move (ui));
    }
    
    rebuildBindings();
    resized();
}

//==============================================================================
void MidiSettingsPanel::rebuildBindings()
{
    // Clear old sections
    for (auto& section : bindingSections)
    {
        contentComponent->removeChildComponent (&section->headerLabel);
        for (auto& row : section->rows)
        {
            contentComponent->removeChildComponent (&row->paramLabel);
            contentComponent->removeChildComponent (&row->ccLabel);
            contentComponent->removeChildComponent (&row->deleteBtn);
        }
    }
    bindingSections.clear();

    auto addSection = [this](const juce::String& name, MidiLearnManager* mgr)
    {
        if (!mgr) return;
        auto& mappings = mgr->getMappings();
        if (mappings.empty()) return;

        auto section = std::make_unique<BindingSection>();
        section->sectionName = name;
        section->manager = mgr;

        section->headerLabel.setText (name + " (" + juce::String((int)mappings.size()) + ")", juce::dontSendNotification);
        section->headerLabel.setFont (juce::FontOptions (14.0f).withStyle("Bold"));
        section->headerLabel.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::accent);
        contentComponent->addAndMakeVisible (section->headerLabel);

        for (auto& [paramId, mapping] : mappings)
        {
            auto row = std::make_unique<BindingRow>();
            
            // Param label: try to make human-readable
            juce::String displayParam = paramId;
            // Strip numeric prefix if present (e.g. "3_gain" → "gain")
            if (displayParam.containsChar('_'))
            {
                int underscorePos = displayParam.indexOf("_");
                juce::String prefix = displayParam.substring(0, underscorePos);
                if (prefix.containsOnly("0123456789"))
                    displayParam = "Node " + prefix + " : " + displayParam.substring(underscorePos + 1);
            }
            
            row->paramLabel.setText (displayParam, juce::dontSendNotification);
            row->paramLabel.setFont (juce::FontOptions (12.0f));
            row->paramLabel.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textPrimary);
            contentComponent->addAndMakeVisible (row->paramLabel);

            juce::String ccText = "CC " + juce::String(mapping.ccNumber);
            if (mapping.channel > 0) ccText += "  Ch " + juce::String(mapping.channel);
            else ccText += "  (Omni)";
            
            row->ccLabel.setText (ccText, juce::dontSendNotification);
            row->ccLabel.setFont (juce::FontOptions (12.0f));
            row->ccLabel.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textSecondary);
            contentComponent->addAndMakeVisible (row->ccLabel);

            row->deleteBtn.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::danger.withAlpha(0.2f));
            row->deleteBtn.setColour (juce::TextButton::textColourOffId, PedalForgeLookAndFeel::danger);
            juce::String paramCopy = paramId;
            row->deleteBtn.onClick = [this, paramCopy, mgr] {
                mgr->removeMapping (paramCopy);
                rebuildBindings();
                resized();
                repaint();
            };
            contentComponent->addAndMakeVisible (row->deleteBtn);

            section->rows.push_back (std::move (row));
        }

        bindingSections.push_back (std::move (section));
    };

    addSection ("Board MIDI Learn", boardMidiLearn);
    addSection ("Play MIDI Learn", playMidiLearn);

    lastBindingCount = 0;
    if (boardMidiLearn) lastBindingCount += (int) boardMidiLearn->getMappings().size();
    if (playMidiLearn)  lastBindingCount += (int) playMidiLearn->getMappings().size();

    resized();
    repaint();
}

//==============================================================================
void MidiSettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgDark);

    // Toolbar gradient
    auto toolbarArea = getLocalBounds().removeFromTop (36);
    g.setGradientFill (juce::ColourGradient (
        PedalForgeLookAndFeel::bgMid.darker (0.1f), 0, (float)toolbarArea.getY(),
        PedalForgeLookAndFeel::bgMid.darker (0.35f), 0, (float)toolbarArea.getBottom(), false));
    g.fillRect (toolbarArea);
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawHorizontalLine (35, 0.0f, (float) getWidth());

    // Toolbar label
    g.setColour (PedalForgeLookAndFeel::textMuted);
    g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
    g.drawText ("  MIDI CONFIGURATION", toolbarArea.reduced (8, 0), juce::Justification::centredLeft);
}

void MidiSettingsPanel::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (36); // toolbar
    viewport.setBounds (bounds);

    int m = 24;
    int contentW = juce::jmax (400, getWidth() - 48);
    int y = 12;
    
    titleLabel.setBounds (m, y, contentW, 30);
    y += 40;

    // ── Section: App-Level Settings ──
    auto layoutRow = [&](juce::Label& lbl, juce::TextEditor& input) {
        lbl.setBounds (m, y, 180, 24);
        input.setBounds (m + 184, y, 56, 24);
        y += 32;
    };
    
    layoutRow (turingPrevLabel, turingPrevInput);
    layoutRow (turingNextLabel, turingNextInput);
    layoutRow (playModeLabel, playModeInput);
    
    y += 12;
    
    // ── Section: Per-Board Settings ──
    for (auto& ui : boardUIs)
    {
        ui->title.setBounds (m, y, contentW, 28);
        y += 32;
        layoutRow (ui->prevLabel, ui->prevInput);
        layoutRow (ui->nextLabel, ui->nextInput);
        y += 8;
    }

    // ── Section: MIDI Learn Bindings ──
    for (auto& section : bindingSections)
    {
        y += 8;
        section->headerLabel.setBounds (m, y, contentW, 24);
        y += 28;

        for (auto& row : section->rows)
        {
            row->paramLabel.setBounds (m + 8, y, contentW / 2 - 40, 22);
            row->ccLabel.setBounds (m + contentW / 2 - 24, y, 120, 22);
            row->deleteBtn.setBounds (m + contentW - 40, y, 24, 22);
            y += 28;
        }
        y += 4;
    }

    y += 20;
    contentComponent->setSize (getWidth(), juce::jmax (y, getHeight()));
}
