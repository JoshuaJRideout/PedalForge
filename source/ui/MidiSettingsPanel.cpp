#include "MidiSettingsPanel.h"
#include "LookAndFeel.h"
#include "ToastOverlay.h"

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

    auto setupLearnBtn = [this](juce::TextButton& btn) {
        contentComponent->addAndMakeVisible (btn);
        btn.setClickingTogglesState (true);
        btn.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgDark.brighter (0.15f));
        btn.setColour (juce::TextButton::buttonOnColourId, PedalForgeLookAndFeel::accent.withAlpha (0.3f));
        btn.setColour (juce::TextButton::textColourOffId, PedalForgeLookAndFeel::textSecondary);
        btn.setColour (juce::TextButton::textColourOnId, PedalForgeLookAndFeel::accent);
    };

    auto wireLearnBtn = [this](juce::TextButton& btn, LearnTarget target) {
        btn.onClick = [this, &btn, target] {
            if (btn.getToggleState())
            {
                cancelActiveLearning();
                activeLearnTarget = target;
                activeLearnBoardIndex = -1;
                activeLearnBtn = &btn;
                learnStartTime = juce::Time::getCurrentTime();
                btn.setButtonText ("Learning...");
            }
            else
            {
                cancelActiveLearning();
            }
        };
    };
    
    // Global Turing Prev
    contentComponent->addAndMakeVisible (turingPrevLabel);
    contentComponent->addAndMakeVisible (turingPrevInput);
    setupInput (turingPrevInput, engine.appMidiConfig.turingPrevCC);
    turingPrevInput.onTextChange = [this] {
        engine.appMidiConfig.turingPrevCC = turingPrevInput.getText().isEmpty() ? -1 : turingPrevInput.getText().getIntValue();
    };
    setupLearnBtn (turingPrevLearnBtn);
    wireLearnBtn (turingPrevLearnBtn, LearnTarget::TuringPrev);
    
    // Global Turing Next
    contentComponent->addAndMakeVisible (turingNextLabel);
    contentComponent->addAndMakeVisible (turingNextInput);
    setupInput (turingNextInput, engine.appMidiConfig.turingNextCC);
    turingNextInput.onTextChange = [this] {
        engine.appMidiConfig.turingNextCC = turingNextInput.getText().isEmpty() ? -1 : turingNextInput.getText().getIntValue();
    };
    setupLearnBtn (turingNextLearnBtn);
    wireLearnBtn (turingNextLearnBtn, LearnTarget::TuringNext);
    
    // Global Play Mode
    contentComponent->addAndMakeVisible (playModeLabel);
    contentComponent->addAndMakeVisible (playModeInput);
    setupInput (playModeInput, engine.appMidiConfig.playModeToggleCC);
    playModeInput.onTextChange = [this] {
        engine.appMidiConfig.playModeToggleCC = playModeInput.getText().isEmpty() ? -1 : playModeInput.getText().getIntValue();
    };
    setupLearnBtn (playModeLearnBtn);
    wireLearnBtn (playModeLearnBtn, LearnTarget::PlayMode);

    // Navigation — Page buttons
    contentComponent->addAndMakeVisible (pageLeftLabel);
    contentComponent->addAndMakeVisible (pageLeftInput);
    setupInput (pageLeftInput, engine.appMidiConfig.pageLeftCC);
    pageLeftInput.onTextChange = [this] {
        engine.appMidiConfig.pageLeftCC = pageLeftInput.getText().isEmpty() ? -1 : pageLeftInput.getText().getIntValue();
    };
    setupLearnBtn (pageLeftLearnBtn);
    wireLearnBtn (pageLeftLearnBtn, LearnTarget::PageLeft);

    contentComponent->addAndMakeVisible (pageRightLabel);
    contentComponent->addAndMakeVisible (pageRightInput);
    setupInput (pageRightInput, engine.appMidiConfig.pageRightCC);
    pageRightInput.onTextChange = [this] {
        engine.appMidiConfig.pageRightCC = pageRightInput.getText().isEmpty() ? -1 : pageRightInput.getText().getIntValue();
    };
    setupLearnBtn (pageRightLearnBtn);
    wireLearnBtn (pageRightLearnBtn, LearnTarget::PageRight);

    // Navigation — Track buttons
    contentComponent->addAndMakeVisible (trackLeftLabel);
    contentComponent->addAndMakeVisible (trackLeftInput);
    setupInput (trackLeftInput, engine.appMidiConfig.trackLeftCC);
    trackLeftInput.onTextChange = [this] {
        engine.appMidiConfig.trackLeftCC = trackLeftInput.getText().isEmpty() ? -1 : trackLeftInput.getText().getIntValue();
    };
    setupLearnBtn (trackLeftLearnBtn);
    wireLearnBtn (trackLeftLearnBtn, LearnTarget::TrackLeft);

    contentComponent->addAndMakeVisible (trackRightLabel);
    contentComponent->addAndMakeVisible (trackRightInput);
    setupInput (trackRightInput, engine.appMidiConfig.trackRightCC);
    trackRightInput.onTextChange = [this] {
        engine.appMidiConfig.trackRightCC = trackRightInput.getText().isEmpty() ? -1 : trackRightInput.getText().getIntValue();
    };
    setupLearnBtn (trackRightLearnBtn);
    wireLearnBtn (trackRightLearnBtn, LearnTarget::TrackRight);

    // Novation Mode
    contentComponent->addAndMakeVisible (novationModeLabel);
    contentComponent->addAndMakeVisible (novationModeCombo);
    novationModeCombo.addItem ("Auto-Map (Focus Pedal)", 1);
    novationModeCombo.addItem ("Passthrough (Pedal-Driven)", 2);
    novationModeCombo.addItem ("Preset Recall", 3);
    
    if (engine.appMidiConfig.novationMode == AppMidiConfig::NovationMode::AutoMap)
        novationModeCombo.setSelectedId (1, juce::dontSendNotification);
    else if (engine.appMidiConfig.novationMode == AppMidiConfig::NovationMode::Passthrough)
        novationModeCombo.setSelectedId (2, juce::dontSendNotification);
    else if (engine.appMidiConfig.novationMode == AppMidiConfig::NovationMode::PresetRecall)
        novationModeCombo.setSelectedId (3, juce::dontSendNotification);
        
    novationModeCombo.onChange = [this] {
        int id = novationModeCombo.getSelectedId();
        if (id == 1) engine.appMidiConfig.novationMode = AppMidiConfig::NovationMode::AutoMap;
        else if (id == 2) engine.appMidiConfig.novationMode = AppMidiConfig::NovationMode::Passthrough;
        else if (id == 3) engine.appMidiConfig.novationMode = AppMidiConfig::NovationMode::PresetRecall;
    };

    // ── MIDI Monitor UI Setup ──
    contentComponent->addAndMakeVisible (midiMonitorTitle);
    midiMonitorTitle.setFont (juce::FontOptions (16.0f).withStyle ("Bold"));
    midiMonitorTitle.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textPrimary);

    // Filters
    for (auto* filter : { &filterNotesBtn, &filterCCsBtn, &filterOtherBtn })
    {
        contentComponent->addAndMakeVisible (filter);
        filter->setToggleState (true, juce::dontSendNotification);
        filter->onClick = [this] { updateMidiMonitorUI(); };
    }

    // Clear / Pause
    contentComponent->addAndMakeVisible (clearBtn);
    clearBtn.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::danger.withAlpha (0.15f));
    clearBtn.setColour (juce::TextButton::textColourOffId, PedalForgeLookAndFeel::danger);
    clearBtn.onClick = [this] {
        engine.clearMidiMonitor();
        midiLogText.clear();
    };

    contentComponent->addAndMakeVisible (pauseBtn);
    pauseBtn.setClickingTogglesState (true);
    pauseBtn.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::accent.withAlpha (0.15f));
    pauseBtn.setColour (juce::TextButton::buttonOnColourId, PedalForgeLookAndFeel::accent.withAlpha (0.4f));
    pauseBtn.setColour (juce::TextButton::textColourOffId, PedalForgeLookAndFeel::accent);
    pauseBtn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    pauseBtn.onClick = [this] {
        isPaused = pauseBtn.getToggleState();
        pauseBtn.setButtonText (isPaused ? "Resume" : "Pause");
    };

    // Monospace Terminal Text Console
    contentComponent->addAndMakeVisible (midiLogText);
    midiLogText.setMultiLine (true);
    midiLogText.setReadOnly (true);
    midiLogText.setScrollbarsShown (true);
    midiLogText.setCaretVisible (false);
    midiLogText.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    midiLogText.setColour (juce::TextEditor::backgroundColourId, PedalForgeLookAndFeel::bgDark.darker (0.4f));
    midiLogText.setColour (juce::TextEditor::textColourId, juce::Colour (0xff8cfca4)); // cyber neon green
    midiLogText.setColour (juce::TextEditor::outlineColourId, PedalForgeLookAndFeel::gridLine);
    midiLogText.setColour (juce::TextEditor::focusedOutlineColourId, PedalForgeLookAndFeel::accent.withAlpha (0.5f));

    refresh();
}

MidiSettingsPanel::~MidiSettingsPanel()
{
    cancelActiveLearning();
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
        startTimerHz (20); // Dynamic 20Hz snappy polling for MIDI monitor and bindings
    }
    else
    {
        cancelActiveLearning();
        stopTimer();
    }
}

void MidiSettingsPanel::timerCallback()
{
    // Check if learning a CC
    if (activeLearnTarget != LearnTarget::None)
    {
        auto events = engine.getMidiMonitorEvents();
        for (int i = events.size() - 1; i >= 0; --i)
        {
            const auto& ev = events.getReference (i);
            if (ev.time >= learnStartTime && ev.message.isController())
            {
                int ccNumber = ev.message.getControllerNumber();
                assignLearnedCC (ccNumber);
                break;
            }
        }
    }

    // Check if binding count changed and rebuild if so
    int total = 0;
    if (boardMidiLearn) total += (int) boardMidiLearn->getMappings().size();
    if (playMidiLearn)  total += (int) playMidiLearn->getMappings().size();
    if (total != lastBindingCount)
        rebuildBindings();

    // Snappy MIDI monitor updates
    if (engine.hasNewMidiMonitorEvents())
    {
        engine.resetMidiMonitorTrigger();
        updateMidiMonitorUI();
    }
}

//==============================================================================
void MidiSettingsPanel::refresh()
{
    // Sync global configuration controls
    auto updateInputWithoutCallback = [](juce::TextEditor& input, int value, std::function<void()> callback) {
        input.onTextChange = nullptr;
        input.setText (value >= 0 ? juce::String(value) : "");
        input.onTextChange = callback;
    };

    updateInputWithoutCallback (turingPrevInput, engine.appMidiConfig.turingPrevCC, [this] {
        engine.appMidiConfig.turingPrevCC = turingPrevInput.getText().isEmpty() ? -1 : turingPrevInput.getText().getIntValue();
    });
    updateInputWithoutCallback (turingNextInput, engine.appMidiConfig.turingNextCC, [this] {
        engine.appMidiConfig.turingNextCC = turingNextInput.getText().isEmpty() ? -1 : turingNextInput.getText().getIntValue();
    });
    updateInputWithoutCallback (playModeInput, engine.appMidiConfig.playModeToggleCC, [this] {
        engine.appMidiConfig.playModeToggleCC = playModeInput.getText().isEmpty() ? -1 : playModeInput.getText().getIntValue();
    });
    updateInputWithoutCallback (pageLeftInput, engine.appMidiConfig.pageLeftCC, [this] {
        engine.appMidiConfig.pageLeftCC = pageLeftInput.getText().isEmpty() ? -1 : pageLeftInput.getText().getIntValue();
    });
    updateInputWithoutCallback (pageRightInput, engine.appMidiConfig.pageRightCC, [this] {
        engine.appMidiConfig.pageRightCC = pageRightInput.getText().isEmpty() ? -1 : pageRightInput.getText().getIntValue();
    });
    updateInputWithoutCallback (trackLeftInput, engine.appMidiConfig.trackLeftCC, [this] {
        engine.appMidiConfig.trackLeftCC = trackLeftInput.getText().isEmpty() ? -1 : trackLeftInput.getText().getIntValue();
    });
    updateInputWithoutCallback (trackRightInput, engine.appMidiConfig.trackRightCC, [this] {
        engine.appMidiConfig.trackRightCC = trackRightInput.getText().isEmpty() ? -1 : trackRightInput.getText().getIntValue();
    });

    novationModeCombo.onChange = nullptr;
    if (engine.appMidiConfig.novationMode == AppMidiConfig::NovationMode::AutoMap)
        novationModeCombo.setSelectedId (1, juce::dontSendNotification);
    else if (engine.appMidiConfig.novationMode == AppMidiConfig::NovationMode::Passthrough)
        novationModeCombo.setSelectedId (2, juce::dontSendNotification);
    else if (engine.appMidiConfig.novationMode == AppMidiConfig::NovationMode::PresetRecall)
        novationModeCombo.setSelectedId (3, juce::dontSendNotification);
    
    novationModeCombo.onChange = [this] {
        int id = novationModeCombo.getSelectedId();
        if (id == 1) engine.appMidiConfig.novationMode = AppMidiConfig::NovationMode::AutoMap;
        else if (id == 2) engine.appMidiConfig.novationMode = AppMidiConfig::NovationMode::Passthrough;
        else if (id == 3) engine.appMidiConfig.novationMode = AppMidiConfig::NovationMode::PresetRecall;
    };

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

    // Cancel active learning when refreshing
    cancelActiveLearning();

    // Rebuild board UIs
    for (auto& ui : boardUIs)
    {
        contentComponent->removeChildComponent (&ui->title);
        contentComponent->removeChildComponent (&ui->prevLabel);
        contentComponent->removeChildComponent (&ui->prevInput);
        contentComponent->removeChildComponent (&ui->prevLearnBtn);
        contentComponent->removeChildComponent (&ui->nextLabel);
        contentComponent->removeChildComponent (&ui->nextInput);
        contentComponent->removeChildComponent (&ui->nextLearnBtn);
    }
    boardUIs.clear();

    int idx = 0;
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

        // Prev Page Learn
        contentComponent->addAndMakeVisible (ui->prevLearnBtn);
        ui->prevLearnBtn.setClickingTogglesState (true);
        ui->prevLearnBtn.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgDark.brighter (0.15f));
        ui->prevLearnBtn.setColour (juce::TextButton::buttonOnColourId, PedalForgeLookAndFeel::accent.withAlpha (0.3f));
        ui->prevLearnBtn.setColour (juce::TextButton::textColourOffId, PedalForgeLookAndFeel::textSecondary);
        ui->prevLearnBtn.setColour (juce::TextButton::textColourOnId, PedalForgeLookAndFeel::accent);
        ui->prevLearnBtn.onClick = [this, rawUi = ui.get(), idx] {
            if (rawUi->prevLearnBtn.getToggleState())
            {
                cancelActiveLearning();
                activeLearnTarget = LearnTarget::BoardPrev;
                activeLearnBoardIndex = idx;
                activeLearnBtn = &rawUi->prevLearnBtn;
                learnStartTime = juce::Time::getCurrentTime();
                rawUi->prevLearnBtn.setButtonText ("Learning...");
            }
            else
            {
                cancelActiveLearning();
            }
        };
        
        ui->nextLabel.setText ("Next Page CC:", juce::dontSendNotification);
        contentComponent->addAndMakeVisible (ui->nextLabel);
        
        contentComponent->addAndMakeVisible (ui->nextInput);
        setupInput (ui->nextInput, board.nextPageCC);
        ui->nextInput.onTextChange = [&board, rawUi = ui.get()] {
            board.nextPageCC = rawUi->nextInput.getText().isEmpty() ? -1 : rawUi->nextInput.getText().getIntValue();
        };

        // Next Page Learn
        contentComponent->addAndMakeVisible (ui->nextLearnBtn);
        ui->nextLearnBtn.setClickingTogglesState (true);
        ui->nextLearnBtn.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgDark.brighter (0.15f));
        ui->nextLearnBtn.setColour (juce::TextButton::buttonOnColourId, PedalForgeLookAndFeel::accent.withAlpha (0.3f));
        ui->nextLearnBtn.setColour (juce::TextButton::textColourOffId, PedalForgeLookAndFeel::textSecondary);
        ui->nextLearnBtn.setColour (juce::TextButton::textColourOnId, PedalForgeLookAndFeel::accent);
        ui->nextLearnBtn.onClick = [this, rawUi = ui.get(), idx] {
            if (rawUi->nextLearnBtn.getToggleState())
            {
                cancelActiveLearning();
                activeLearnTarget = LearnTarget::BoardNext;
                activeLearnBoardIndex = idx;
                activeLearnBtn = &rawUi->nextLearnBtn;
                learnStartTime = juce::Time::getCurrentTime();
                rawUi->nextLearnBtn.setButtonText ("Learning...");
            }
            else
            {
                cancelActiveLearning();
            }
        };
        
        boardUIs.push_back (std::move (ui));
        idx++;
    }
    
    rebuildBindings();
    resized();
}

//==============================================================================
void MidiSettingsPanel::commitBindingEdit (BindingRow* row)
{
    if (row == nullptr || row->manager == nullptr) return;
    const int cc = juce::jlimit (0, 127, row->ccInput.getText().getIntValue());
    const int ch = juce::jlimit (0, 16,  row->channelInput.getText().getIntValue());
    row->manager->setMapping (row->paramId, cc, ch);
    pf::toastInfo ("Updated binding - " + row->paramId
                   + " -> CC " + juce::String (cc)
                   + (ch == 0 ? " (Omni)" : " ch " + juce::String (ch)));
    rebuildBindings();
    resized();
    repaint();
}

void MidiSettingsPanel::rebuildBindings()
{
    // Clear old sections
    for (auto& section : bindingSections)
    {
        contentComponent->removeChildComponent (&section->headerLabel);
        for (auto& row : section->rows)
        {
            contentComponent->removeChildComponent (&row->paramLabel);
            contentComponent->removeChildComponent (&row->ccInput);
            contentComponent->removeChildComponent (&row->channelInput);
            contentComponent->removeChildComponent (&row->relearnBtn);
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

            row->paramId = paramId;
            row->manager = mgr;

            // Editable CC# input
            row->ccInput.setText (juce::String (mapping.ccNumber), juce::dontSendNotification);
            row->ccInput.setInputRestrictions (3, "0123456789");
            row->ccInput.setJustification (juce::Justification::centred);
            row->ccInput.setFont (juce::FontOptions (12.0f));
            row->ccInput.setColour (juce::TextEditor::textColourId, PedalForgeLookAndFeel::textPrimary);
            // On commit (Enter or focus loss), apply via setMapping.
            {
                auto* r = row.get();
                row->ccInput.onReturnKey = [this, r] { commitBindingEdit (r); };
                row->ccInput.onFocusLost = [this, r] { commitBindingEdit (r); };
            }
            contentComponent->addAndMakeVisible (row->ccInput);

            // Editable channel input (0 = Omni)
            row->channelInput.setText (juce::String (mapping.channel), juce::dontSendNotification);
            row->channelInput.setInputRestrictions (2, "0123456789");
            row->channelInput.setJustification (juce::Justification::centred);
            row->channelInput.setFont (juce::FontOptions (12.0f));
            row->channelInput.setColour (juce::TextEditor::textColourId, PedalForgeLookAndFeel::textPrimary);
            {
                auto* r = row.get();
                row->channelInput.onReturnKey = [this, r] { commitBindingEdit (r); };
                row->channelInput.onFocusLost = [this, r] { commitBindingEdit (r); };
            }
            contentComponent->addAndMakeVisible (row->channelInput);

            // Relearn ⟳
            row->relearnBtn.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::accent.withAlpha (0.18f));
            row->relearnBtn.setColour (juce::TextButton::textColourOffId, PedalForgeLookAndFeel::accent);
            row->relearnBtn.setTooltip ("Re-learn this binding (press a control on your MIDI device)");
            {
                auto paramCopy = paramId;
                row->relearnBtn.onClick = [this, paramCopy, mgr] {
                    mgr->startLearning (paramCopy);
                    pf::toastInfo ("Move a CC on your MIDI controller to bind it to: " + paramCopy);
                };
            }
            contentComponent->addAndMakeVisible (row->relearnBtn);

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
    auto layoutRow = [&](juce::Label& lbl, juce::TextEditor& input, juce::TextButton& learnBtn) {
        lbl.setBounds (m, y, 180, 24);
        input.setBounds (m + 184, y, 56, 24);
        learnBtn.setBounds (m + 248, y, 64, 24);
        y += 32;
    };
    
    layoutRow (turingPrevLabel, turingPrevInput, turingPrevLearnBtn);
    layoutRow (turingNextLabel, turingNextInput, turingNextLearnBtn);
    layoutRow (playModeLabel, playModeInput, playModeLearnBtn);

    y += 8;
    layoutRow (pageLeftLabel,  pageLeftInput,  pageLeftLearnBtn);
    layoutRow (pageRightLabel, pageRightInput, pageRightLearnBtn);
    layoutRow (trackLeftLabel,  trackLeftInput,  trackLeftLearnBtn);
    layoutRow (trackRightLabel, trackRightInput, trackRightLearnBtn);
    
    novationModeLabel.setBounds (m, y, 220, 24);
    novationModeCombo.setBounds (m + 224, y, 200, 24);
    y += 32;
    
    y += 12;
    
    // ── Section: Per-Board Settings ──
    for (auto& ui : boardUIs)
    {
        ui->title.setBounds (m, y, contentW, 28);
        y += 32;
        layoutRow (ui->prevLabel, ui->prevInput, ui->prevLearnBtn);
        layoutRow (ui->nextLabel, ui->nextInput, ui->nextLearnBtn);
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
            // Param name takes the left half; CC# + Ch + ⟳ + X stack on the right.
            const int rowRight = m + contentW;
            const int btnW = 28;
            const int gap  = 4;
            row->deleteBtn.setBounds  (rowRight - btnW,                                 y, btnW, 22);
            row->relearnBtn.setBounds (rowRight - btnW * 2 - gap,                       y, btnW, 22);
            row->channelInput.setBounds (rowRight - btnW * 2 - gap - 36 - gap,          y, 36,   22);
            row->ccInput.setBounds      (rowRight - btnW * 2 - gap - 36 - gap - 40 - gap, y, 40,  22);
            row->paramLabel.setBounds (m + 8, y, row->ccInput.getX() - (m + 16), 22);
            y += 28;
        }
        y += 4;
    }

    // ── Section: MIDI Monitor ──
    y += 24;
    midiMonitorTitle.setBounds (m, y, contentW, 28);
    y += 32;

    filterNotesBtn.setBounds (m, y, 70, 24);
    filterCCsBtn.setBounds (m + 80, y, 60, 24);
    filterOtherBtn.setBounds (m + 150, y, 70, 24);

    pauseBtn.setBounds (m + contentW - 144, y, 64, 24);
    clearBtn.setBounds (m + contentW - 72, y, 64, 24);
    y += 32;

    midiLogText.setBounds (m, y, contentW, 220);
    y += 232;

    y += 20;
    contentComponent->setSize (getWidth(), juce::jmax (y, getHeight()));
}

void MidiSettingsPanel::updateMidiMonitorUI()
{
    if (isPaused)
        return;

    auto events = engine.getMidiMonitorEvents();
    
    bool showNotes = filterNotesBtn.getToggleState();
    bool showCCs = filterCCsBtn.getToggleState();
    bool showOther = filterOtherBtn.getToggleState();

    juce::String text;
    for (const auto& ev : events)
    {
        auto& msg = ev.message;
        
        bool isNote = msg.isNoteOn() || msg.isNoteOff();
        bool isCC = msg.isController();
        bool isOther = !isNote && !isCC;

        if (isNote && !showNotes) continue;
        if (isCC && !showCCs) continue;
        if (isOther && !showOther) continue;

        // Format wall-clock timestamp [HH:MM:SS.mmm]
        juce::String timestamp = ev.time.formatted ("%H:%M:%S") + "." + juce::String (ev.time.getMilliseconds()).paddedLeft ('0', 3);

        juce::String msgDesc;
        if (msg.isNoteOn())
            msgDesc = "Note On: " + juce::MidiMessage::getMidiNoteName (msg.getNoteNumber(), true, true, 3) 
                      + " (" + juce::String (msg.getNoteNumber()) + "), Vel " + juce::String (msg.getVelocity());
        else if (msg.isNoteOff())
            msgDesc = "Note Off: " + juce::MidiMessage::getMidiNoteName (msg.getNoteNumber(), true, true, 3) 
                      + " (" + juce::String (msg.getNoteNumber()) + ")";
        else if (msg.isController())
            msgDesc = "CC: #" + juce::String (msg.getControllerNumber()) + ", Value " + juce::String (msg.getControllerValue());
        else if (msg.isProgramChange())
            msgDesc = "Program Change: " + juce::String (msg.getProgramChangeNumber());
        else if (msg.isPitchWheel())
            msgDesc = "Pitch Wheel: " + juce::String (msg.getPitchWheelValue());
        else if (msg.isAftertouch())
            msgDesc = "Aftertouch: " + juce::MidiMessage::getMidiNoteName (msg.getNoteNumber(), true, true, 3) 
                      + ", Value " + juce::String (msg.getAfterTouchValue());
        else if (msg.isChannelPressure())
            msgDesc = "Channel Pressure: " + juce::String (msg.getChannelPressureValue());
        else if (msg.isSysEx())
            msgDesc = "SysEx: " + juce::String (msg.getSysExDataSize()) + " bytes";
        else
            msgDesc = "Other MIDI Event";

        juce::String line = "[" + timestamp + "] [" + ev.source + "] Ch " + juce::String (msg.getChannel()) + " | " + msgDesc;
        text += line + "\n";
    }

    midiLogText.setText (text, false);
    
    // Auto-scroll to bottom (moveCaretToEnd automatically handles scrolling to the bottom in JUCE)
    midiLogText.moveCaretToEnd();
}

void MidiSettingsPanel::cancelActiveLearning()
{
    if (activeLearnBtn != nullptr)
    {
        activeLearnBtn->setToggleState (false, juce::dontSendNotification);
        activeLearnBtn->setButtonText ("Learn");
        activeLearnBtn = nullptr;
    }
    activeLearnTarget = LearnTarget::None;
    activeLearnBoardIndex = -1;
}

void MidiSettingsPanel::assignLearnedCC (int ccNumber)
{
    if (activeLearnTarget == LearnTarget::None)
        return;

    switch (activeLearnTarget)
    {
        case LearnTarget::TuringPrev:
            engine.appMidiConfig.turingPrevCC = ccNumber;
            turingPrevInput.setText (juce::String (ccNumber));
            break;
        case LearnTarget::TuringNext:
            engine.appMidiConfig.turingNextCC = ccNumber;
            turingNextInput.setText (juce::String (ccNumber));
            break;
        case LearnTarget::PlayMode:
            engine.appMidiConfig.playModeToggleCC = ccNumber;
            playModeInput.setText (juce::String (ccNumber));
            break;
        case LearnTarget::PageLeft:
            engine.appMidiConfig.pageLeftCC = ccNumber;
            pageLeftInput.setText (juce::String (ccNumber));
            break;
        case LearnTarget::PageRight:
            engine.appMidiConfig.pageRightCC = ccNumber;
            pageRightInput.setText (juce::String (ccNumber));
            break;
        case LearnTarget::TrackLeft:
            engine.appMidiConfig.trackLeftCC = ccNumber;
            trackLeftInput.setText (juce::String (ccNumber));
            break;
        case LearnTarget::TrackRight:
            engine.appMidiConfig.trackRightCC = ccNumber;
            trackRightInput.setText (juce::String (ccNumber));
            break;
        case LearnTarget::BoardPrev:
            if (activeLearnBoardIndex >= 0 && activeLearnBoardIndex < (int) boardUIs.size())
            {
                auto& boards = engine.getBoards();
                int idx = 0;
                for (auto& board : boards)
                {
                    if (idx == activeLearnBoardIndex)
                    {
                        board.prevPageCC = ccNumber;
                        boardUIs[idx]->prevInput.setText (juce::String (ccNumber));
                        break;
                    }
                    idx++;
                }
            }
            break;
        case LearnTarget::BoardNext:
            if (activeLearnBoardIndex >= 0 && activeLearnBoardIndex < (int) boardUIs.size())
            {
                auto& boards = engine.getBoards();
                int idx = 0;
                for (auto& board : boards)
                {
                    if (idx == activeLearnBoardIndex)
                    {
                        board.nextPageCC = ccNumber;
                        boardUIs[idx]->nextInput.setText (juce::String (ccNumber));
                        break;
                    }
                    idx++;
                }
            }
            break;
        default:
            break;
    }

    cancelActiveLearning();
}
