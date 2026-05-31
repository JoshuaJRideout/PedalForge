#include "AiAssistantPanel.h"
#include "LookAndFeel.h"
#include "ToastOverlay.h"

//==============================================================================
namespace
{
    juce::String buildSystemPrompt (const juce::String& activeTab)
    {
        return
            "You are the in-app assistant for PedalForge, a virtual guitar "
            "pedalboard app (VST3/AU/Standalone). You help musicians build and "
            "modify pedals, FX graphs, and boards by calling tools.\n\n"
            "The user is currently on the \"" + activeTab + "\" tab.\n\n"
            "You can do anything in PedalForge the user can: build pedalboards, "
            "create and edit pedals (chassis, controls, DSP), wire audio and "
            "MIDI, and run scripts.\n\n"
            "Guidelines:\n"
            "- Call read_active_tab first to orient yourself before acting.\n"
            "- Use list_pedals to resolve a pedal name to its uuid; "
            "list_factory_pedals for pedals you can add.\n"
            "- PREFER SCRIPTS for any multi-step construction. The scripting "
            "tools are the most powerful path and build a whole board / pedal / "
            "FX graph in ONE call:\n"
            "    - run_board_script  - build/arrange the whole board\n"
            "    - run_pedal_script  - a pedal's chassis + face controls\n"
            "    - run_fx_script     - a pedal's DSP node graph (audible)\n"
            "    - run_dsp_script    - a per-sample DSP expression\n"
            "  ALWAYS call get_script_api ONCE first to learn the exact script "
            "commands, then write correct scripts. Read existing state as code "
            "with read_board_as_script / read_pedal_as_script / read_fx_as_script.\n"
            "- For small tweaks, the JSON tools (read/write_pedal_design, "
            "read/write_fx_graph) are fine too.\n"
            "- Script tools return console output; if it contains 'ERROR line N', "
            "fix that line and re-run.\n"
            "- Every change is undoable (Cmd-Z), so act without excessive "
            "confirmation - but DO summarise what you changed.\n"
            "- After finishing, call show_toast with a one-line summary.\n"
            "- Be concise. You're a side panel, not a chatbot.\n\n"
            "CRITICAL workflow rules (avoid these common mistakes):\n"
            "- create_pedal and add_pedal_to_board RETURN {uuid,name}. ALWAYS "
            "use that exact returned uuid in follow-up calls. NEVER make up a "
            "uuid like 'ped-001'.\n"
            "- A call that needs a uuid from create_pedal must be a SEPARATE, "
            "LATER step - do NOT batch create_pedal together with a "
            "run_script that uses its uuid (the uuid isn't known until "
            "create_pedal returns).\n"
            "- Pedals you add are AUTO-WIRED left-to-right into the chain. You "
            "do NOT need a board script to connect them.\n"
            "- run_script mode=board CLEARS the board and only re-adds FACTORY "
            "pedals - it deletes custom pedals. Don't use it after create_pedal.\n"
            "- FX graphs build the COMPLETE graph each run: you MUST create an "
            "audio_input and audio_output node and wire a path between them, "
            "referencing nodes by the var you assigned (never a bare type name).\n"
            "- After building an FX graph, ALWAYS call verify_pedal - a script "
            "can say 'ok' while connections silently failed. If verify reports a "
            "BROKEN audio path or a 'Connection failed' WARNING appeared, FIX it "
            "and re-run before telling the user you're done.\n"
            "- Then call probe_pedal to HEAR it: confirm the audio isn't silent, "
            "the effect actually does what it should (a fuzz distorts, a boost "
            "boosts), and there's no NaN/garbage. Report the gain/THD/tone you "
            "measured, not just that it's wired.\n"
            "- You also have EYES: call screenshot to capture the current view "
            "and actually SEE it (layout, colours, knob positions, meters). Use "
            "it to check your visual work or to understand what the user sees.\n"
            "- PLAY TAB vs BOARD: the Play tab is the live PERFORMANCE rig - a "
            "SEPARATE pedal chain from the Board, with its own tone presets. If "
            "the user asks to set up / change the PLAY tab (or a tone to play "
            "with), use the play tools (list_play_presets, load_play_preset, "
            "read_play_chain, play_add_pedal, play_clear) - do NOT build a Board "
            "with create_pedal/add_pedal_to_board. The Board tab is for "
            "arranging & designing pedals; the Play tab is for playing them.\n"
            "- ROUTING: Board pedals auto-wire left-to-right. For custom signal "
            "paths (parallel splits, FX loops) use read_routing to see the graph, "
            "and connect_pedals / disconnect_pedals (endpoints are board pedal "
            "uuids, or 'input'/'output' for the board's audio I/O).\n"
            "- MIDI: to map a hardware knob/CC to a pedal parameter, call "
            "list_pedal_params(uuid) to get the parameter id, then map_midi_cc. "
            "list/remove/clear_midi_mappings manage them.\n"
            "- NAVIGATION: you can switch_tab to any tab and screenshot to SEE "
            "it; list_assets shows the user's NAM/IR/image/pedal/board files.\n"
            "- WIKI/HELP: to answer questions about how PedalForge works, READ the "
            "docs as text with read_wiki_page (list_wiki_pages first) - it's far "
            "cheaper than screenshotting. Use open_wiki_page to bring a page up on "
            "screen for the user when they'd benefit from reading it themselves. "
            "Only screenshot the Wiki if you need to see its rendered layout.";
    }
}

//==============================================================================
AiAssistantPanel::AiAssistantPanel (pf::ai::ToolHost& h) : host (h)
{
    agent = std::make_unique<pf::ai::AiAgent> (provider, host);
    rebuildSystemPrompt();

    transcript.setMultiLine (true);
    transcript.setReadOnly (true);
    transcript.setScrollbarsShown (true);
    transcript.setCaretVisible (false);
    transcript.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF0E1116));
    transcript.setColour (juce::TextEditor::textColourId, juce::Colour (0xFFD1D5DB));
    transcript.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    transcript.setFont (juce::FontOptions (13.0f));
    addChildComponent (transcript);

    input.setMultiLine (false);
    input.setReturnKeyStartsNewLine (false);
    // NOTE: juce::String's (const char*) constructor decodes as ASCII/Latin-1,
    // so a bare UTF-8 literal like "…" becomes mojibake ("â€¦"). Any non-ASCII
    // string literal MUST be wrapped in CharPointer_UTF8 (see also the button
    // glyphs above). "\xe2\x80\xa6" == U+2026 HORIZONTAL ELLIPSIS.
    input.setTextToShowWhenEmpty (juce::CharPointer_UTF8 ("Ask Claude to build or change a pedal\xe2\x80\xa6"),
                                  juce::Colour (0xFF6B7280));
    input.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF1A1F26));
    input.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    input.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xFF2A3340));
    input.setFont (juce::FontOptions (14.0f));
    input.onReturnKey = [this] { sendCurrent(); };
    input.onFocusLost = [] {};
    addAndMakeVisible (input);
    // Expand when the user clicks into the input.
    input.onTextChange = [this]
    {
        if (! expanded && input.getText().isNotEmpty())
            setExpanded (true);
    };

    sendBtn.setTooltip ("Send (Enter)");
    sendBtn.onClick = [this] { sendCurrent(); };
    addAndMakeVisible (sendBtn);

    expandBtn.setTooltip ("Expand / collapse the assistant");
    expandBtn.onClick = [this] { toggleExpanded(); };
    addAndMakeVisible (expandBtn);

    keyBtn.setTooltip ("Claude Code status");
    keyBtn.onClick = [this] { showStatus(); };
    addAndMakeVisible (keyBtn);

    // Shown only after an auth-expired error; opens an interactive re-login.
    signInBtn.setTooltip ("Sign in to your Claude subscription to keep using the assistant");
    signInBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2563EB));
    signInBtn.onClick = [this]
    {
        launchClaudeLogin();
        signInBtn.setVisible (false);
        provider.resetSession();   // next turn starts a fresh session post-login
        resized();
    };
    addChildComponent (signInBtn);

    // Agent callbacks (fire on the message thread).
    agent->onTurnStarted   = [this] { updateBusyState (true); appendActivity ("...thinking"); };
    agent->onAssistantText = [this] (const juce::String& t)
    {
        appendTranscript ("Claude", t);
        if (remoteActive) remoteResponse << "CLAUDE: " << t << "\n";
    };
    agent->onToolActivity  = [this] (const juce::String& t)
    {
        appendActivity (t);
        if (remoteActive) remoteResponse << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7 ")) << t << "\n";
    };
    agent->onError         = [this] (const juce::String& e)
    {
        appendActivity (juce::String (juce::CharPointer_UTF8 ("\xe2\x9a\xa0 ")) + e);
        pf::toastError ("AI: " + e);
        if (remoteActive) remoteResponse << juce::String (juce::CharPointer_UTF8 ("  \xe2\x9a\xa0 ERROR: ")) << e << "\n";
    };
    agent->onAuthExpired   = [this] (const juce::String& msg)
    {
        setExpanded (true);
        appendActivity (juce::String (juce::CharPointer_UTF8 ("\xe2\x9a\xa0 ")) + msg);
        pf::toastError ("AI: subscription login expired - sign in again.");
        signInBtn.setVisible (true);
        resized();
        if (remoteActive) remoteResponse << "  AUTH EXPIRED: " << msg << "\n";
    };
    agent->onTurnFinished  = [this]
    {
        updateBusyState (false);
        if (remoteActive)
        {
            remoteResponse << "[turn complete]\n";
            remoteRespFile.replaceWithText (remoteResponse);
            remoteActive = false;
        }
    };

    setExpanded (false);

    // Poll for remote prompts (testing/automation bridge).
    startTimerHz (3);
}

//==============================================================================
void AiAssistantPanel::timerCallback()
{
    if (remoteActive || agent->isBusy()) return;            // one at a time
    if (! remoteCmdFile.existsAsFile())   return;

    auto cmd = remoteCmdFile.loadFileAsString().trim();
    remoteCmdFile.deleteFile();                              // consume it
    if (cmd.isEmpty()) return;
    if (! provider.isConfigured()) return;

    remoteActive = true;
    remoteResponse.clear();
    remoteResponse << "PROMPT: " << cmd << "\n";
    remoteRespFile.replaceWithText ("(working...)\n");        // signal "in progress"

    setExpanded (true);
    appendTranscript ("You (remote)", cmd);
    agent->sendUserMessage (cmd);
}

AiAssistantPanel::~AiAssistantPanel() = default;

//==============================================================================
void AiAssistantPanel::setActiveTabName (const juce::String& tab)
{
    if (tab == activeTabName) return;
    activeTabName = tab;
    rebuildSystemPrompt();
}

void AiAssistantPanel::rebuildSystemPrompt()
{
    if (agent) agent->setSystemPrompt (buildSystemPrompt (activeTabName));
}

//==============================================================================
void AiAssistantPanel::setExpanded (bool shouldExpand)
{
    if (expanded == shouldExpand) return;
    expanded = shouldExpand;
    transcript.setVisible (expanded);
    expandBtn.setButtonText (expanded ? juce::CharPointer_UTF8 ("\xe2\x8c\x83")   // ⌃ collapse
                                      : juce::CharPointer_UTF8 ("\xe2\x8c\x84")); // ⌄ expand
    if (onExpandedChanged) onExpandedChanged();
    resized();
}

void AiAssistantPanel::focusInput()
{
    setExpanded (true);
    input.grabKeyboardFocus();
}

//==============================================================================
void AiAssistantPanel::sendCurrent()
{
    auto text = input.getText().trim();
    if (text.isEmpty()) return;

    if (! provider.isConfigured())
    {
        setExpanded (true);
        appendActivity (juce::String (juce::CharPointer_UTF8 (
                        "\xe2\x9a\xa0 Claude Code not found. Install it from claude.com/code, then run "
                        "`claude` once in a terminal to log in with your subscription.")));
        pf::toastError ("Claude Code isn't installed / on PATH.");
        return;
    }

    if (agent->isBusy())
    {
        pf::toastWarn ("Assistant is still working - hang on.");
        return;
    }

    setExpanded (true);
    appendTranscript ("You", text);
    input.clear();
    if (signInBtn.isVisible()) { signInBtn.setVisible (false); resized(); }  // clear stale re-login prompt on a fresh attempt
    agent->sendUserMessage (text);
}

//==============================================================================
void AiAssistantPanel::showStatus()
{
    auto bin = pf::ai::ClaudeCodeProvider::findClaudeBinary();
    if (bin.isNotEmpty())
        pf::toastInfo ("Claude Code ready - using your subscription via " + bin);
    else
        pf::toastWarn ("Claude Code not found. Install from claude.com/code and run "
                       "`claude` once to log in.");
}

//==============================================================================
void AiAssistantPanel::launchClaudeLogin()
{
    auto bin = pf::ai::ClaudeCodeProvider::findClaudeBinary();
    if (bin.isEmpty())
    {
        pf::toastWarn ("Claude Code not found. Install from claude.com/code, then run "
                       "`claude` once to log in.");
        return;
    }

   #if JUCE_MAC
    // Open Terminal and run the interactive login. The browser-based OAuth flow
    // and keychain write must happen in a real interactive process — the
    // headless `claude -p` the agent uses can't do it. Single-quote the path so
    // a space-containing path is safe; the shell command carries no double
    // quotes, so it embeds cleanly in the AppleScript string literal.
    const auto shellCmd = bin.quoted ('\'') + " /login";
    juce::String appleScript;
    appleScript << "tell application \"Terminal\"\n"
                << "activate\n"
                << "do script \"" << shellCmd << "\"\n"
                << "end tell";
    juce::ChildProcess osa;
    osa.start (juce::StringArray { "/usr/bin/osascript", "-e", appleScript });
   #else
    // Best effort elsewhere: spawn the login directly (no controlling TTY, but
    // the CLI prints a URL the user can open).
    juce::ChildProcess proc;
    proc.start (juce::StringArray { bin, "/login" });
   #endif

    pf::toastInfo ("Complete the Claude sign-in in Terminal, then press Send to retry.");
}

//==============================================================================
void AiAssistantPanel::appendTranscript (const juce::String& who, const juce::String& text)
{
    transcript.moveCaretToEnd();
    auto prefix = transcript.getText().isEmpty() ? juce::String() : juce::String ("\n\n");
    transcript.insertTextAtCaret (prefix + who + ":  " + text);
    transcript.moveCaretToEnd();
}

void AiAssistantPanel::appendActivity (const juce::String& line)
{
    transcript.moveCaretToEnd();
    auto prefix = transcript.getText().isEmpty() ? juce::String() : juce::String ("\n");
    transcript.insertTextAtCaret (prefix + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7 ")) + line);
    transcript.moveCaretToEnd();
}

void AiAssistantPanel::updateBusyState (bool busy)
{
    input.setEnabled (! busy);
    sendBtn.setEnabled (! busy);
}

//==============================================================================
void AiAssistantPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF12161B));
    g.setColour (juce::Colour (0xFF2A3340));
    g.drawLine (0.0f, 0.0f, (float) getWidth(), 0.0f, 1.0f);
}

void AiAssistantPanel::resized()
{
    auto b = getLocalBounds();
    auto inputRow = b.removeFromBottom (collapsedHeight).reduced (6, 4);

    keyBtn.setBounds    (inputRow.removeFromLeft (28));
    inputRow.removeFromLeft (4);
    expandBtn.setBounds (inputRow.removeFromRight (28));
    inputRow.removeFromRight (4);
    sendBtn.setBounds   (inputRow.removeFromRight (32));
    inputRow.removeFromRight (4);
    input.setBounds     (inputRow);

    // Re-login affordance sits just above the input row when an auth error
    // exposed it; otherwise it takes no space.
    if (signInBtn.isVisible())
    {
        auto signRow = b.removeFromBottom (30).reduced (6, 2);
        signInBtn.setBounds (signRow.removeFromLeft (160));
    }

    if (expanded)
        transcript.setBounds (b.reduced (6, 6).withTrimmedBottom (2));
}
