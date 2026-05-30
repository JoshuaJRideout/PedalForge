#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../ai/ClaudeCodeProvider.h"
#include "../ai/AiAgent.h"
#include "../ai/ToolHost.h"

//==============================================================================
// The AI assistant chat surface (task #64).
//
// Lives just above the AudioStatusBar. Two states:
//   - Collapsed: a single input row ("Ask Claude…") + expand chevron.
//   - Expanded : transcript above the input row (~45% of the window).
//
// Owns the ClaudeProvider and AiAgent. The agent talks to the rest of the
// app through the ToolHost the editor passes in.
//==============================================================================
class AiAssistantPanel : public juce::Component,
                         private juce::Timer
{
public:
    explicit AiAssistantPanel (pf::ai::ToolHost& host);
    ~AiAssistantPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    bool isExpanded() const { return expanded; }
    void setExpanded (bool shouldExpand);
    void toggleExpanded() { setExpanded (! expanded); }

    /** Move keyboard focus to the input and expand. Used by Cmd-K. */
    void focusInput();

    /** Editor sets this so it can re-lay-out when the panel grows/shrinks. */
    std::function<void()> onExpandedChanged;

    /** The system prompt changes with the active tab; editor pushes it. */
    void setActiveTabName (const juce::String& tab);

    static constexpr int collapsedHeight = 34;

private:
    void sendCurrent();
    void showStatus();
    void appendTranscript (const juce::String& who, const juce::String& text);
    void appendActivity (const juce::String& line);
    void rebuildSystemPrompt();
    void updateBusyState (bool busy);

    pf::ai::ToolHost& host;
    pf::ai::ClaudeCodeProvider provider;
    std::unique_ptr<pf::ai::AiAgent> agent;

    juce::TextEditor transcript;     // read-only, multiline
    juce::TextEditor input;          // single line
    juce::TextButton sendBtn   { juce::CharPointer_UTF8 ("\xe2\x86\x91") };  // ↑
    juce::TextButton expandBtn { juce::CharPointer_UTF8 ("\xe2\x8c\x84") };  // ⌄
    juce::TextButton keyBtn    { juce::CharPointer_UTF8 ("\xe2\x93\x98") };  // ⓘ status/help
    juce::Label      hintLabel;

    bool expanded = false;
    juce::String activeTabName { "Play" };

    //==========================================================================
    // Remote-prompt bridge (testing/automation): the app watches a command
    // file; when it appears, the prompt runs through the REAL in-app agent
    // against the LIVE engine, and the turn's transcript is written to a
    // response file. Lets an external driver (e.g. a developer agent) use the
    // app as a user would and read back what happened.
    void timerCallback() override;
    bool remoteActive = false;
    juce::String remoteResponse;
    juce::File remoteCmdFile  { "/tmp/pedalforge_ai_cmd.txt" };
    juce::File remoteRespFile { "/tmp/pedalforge_ai_response.txt" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AiAssistantPanel)
};
