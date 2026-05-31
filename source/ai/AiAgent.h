#pragma once

#include "AiProvider.h"
#include "ToolHost.h"
#include <atomic>

//==============================================================================
// The conversation controller for the AI assistant (task #64).
//
// Owns the message history and runs the provider tool-use loop on a worker
// thread. Tool calls are marshalled to the message thread (they touch JUCE
// / engine state via the ToolHost) and the worker blocks for each result.
//
// All callbacks fire on the MESSAGE THREAD, so the UI panel can update
// directly without re-marshalling.
//==============================================================================
namespace pf::ai
{
    class AiAgent : private juce::Thread
    {
    public:
        AiAgent (AiProvider& provider, ToolHost& host);
        ~AiAgent() override;

        //======================================================================
        // Callbacks — all invoked on the message thread.
        std::function<void()>                       onTurnStarted;
        std::function<void (const juce::String&)>   onAssistantText;   // assistant text (may stream)
        std::function<void (const juce::String&)>   onToolActivity;    // human-readable "calling X…"
        std::function<void (const juce::String&)>   onError;
        std::function<void (const juce::String&)>   onAuthExpired;     // subscription login expired/missing — UI should offer re-login
        std::function<void()>                       onTurnFinished;

        //======================================================================
        void setSystemPrompt (const juce::String& s) { systemPrompt = s; }

        /** Begin a turn with the user's message. No-op if already busy. */
        void sendUserMessage (const juce::String& text);

        bool isBusy() const { return busy.load(); }

        /** Request cancellation of the in-flight turn (best-effort). */
        void cancelTurn();

        /** Wipe the conversation history (keeps system prompt). */
        void clearConversation();

    private:
        void run() override;

        AiProvider& provider;
        ToolHost&   host;

        juce::String systemPrompt;
        std::vector<Message> conversation;   // touched only off-thread between turns
        juce::String pendingUserText;
        std::atomic<bool> busy { false };

        std::vector<ToolDef> toolDefs;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AiAgent)
    };
}
