#include "AiAgent.h"
#include "AiTools.h"
#include <juce_events/juce_events.h>

namespace pf::ai
{
namespace
{
    // Run a function on the message thread and block the caller until it
    // completes. Safe from the agent worker thread (the message thread never
    // waits on the worker, so no deadlock).
    template <typename Fn>
    void runOnMessageThreadBlocking (Fn&& fn)
    {
        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        {
            fn();
            return;
        }
        juce::WaitableEvent done;
        auto* fnPtr = new std::function<void()> ([&fn, &done]
        {
            fn();
            done.signal();
        });
        juce::MessageManager::callAsync ([fnPtr]
        {
            (*fnPtr)();
            delete fnPtr;
        });
        done.wait();
    }

    template <typename Fn>
    void post (Fn&& fn)
    {
        juce::MessageManager::callAsync (std::forward<Fn> (fn));
    }
}

//==============================================================================
AiAgent::AiAgent (AiProvider& p, ToolHost& h)
    : juce::Thread ("AiAgent"), provider (p), host (h)
{
    toolDefs = tools::buildToolDefs();
}

AiAgent::~AiAgent()
{
    stopThread (3000);
}

//==============================================================================
void AiAgent::sendUserMessage (const juce::String& text)
{
    if (busy.load() || isThreadRunning() || text.trim().isEmpty())
        return;

    // The previous turn's thread may have set busy=false as its last act but
    // not yet fully unwound — join it before reusing the Thread object.
    stopThread (2000);

    pendingUserText = text;
    busy.store (true);
    startThread();
}

void AiAgent::cancelTurn()
{
    signalThreadShouldExit();
}

void AiAgent::clearConversation()
{
    if (busy.load()) return;
    conversation.clear();
    provider.resetSession();
}

//==============================================================================
void AiAgent::run()
{
    // Capture callbacks safely — they only fire via post()/blocking marshal.
    auto fireText   = [this] (const juce::String& t) { auto cb = onAssistantText; if (cb) post ([cb, t] { cb (t); }); };
    auto fireTool   = [this] (const juce::String& t) { auto cb = onToolActivity; if (cb) post ([cb, t] { cb (t); }); };
    auto fireError  = [this] (const juce::String& e) { auto cb = onError;        if (cb) post ([cb, e] { cb (e); }); };
    auto fireAuth   = [this] (const juce::String& e) { auto cb = onAuthExpired;  if (cb) post ([cb, e] { cb (e); }); };
    auto fireFinish = [this]                         { auto cb = onTurnFinished; if (cb) post ([cb]     { cb(); }); };
    auto fireStart  = [this]                         { auto cb = onTurnStarted;  if (cb) post ([cb]     { cb(); }); };

    fireStart();

    // Seed the user's message.
    conversation.push_back (Message::user (pendingUserText));
    pendingUserText.clear();

    constexpr int kMaxToolRounds = 30;   // headroom for multi-step builds; guards runaway loops
    for (int round = 0; round < kMaxToolRounds; ++round)
    {
        if (threadShouldExit()) break;

        // Stream-aware text callback for this turn.
        auto resp = provider.send (systemPrompt, conversation, toolDefs,
                                   [&fireText] (const juce::String& delta) { fireText (delta); });

        if (! resp.ok)
        {
            if (resp.authExpired)
                fireAuth (resp.error.isNotEmpty() ? resp.error : juce::String ("Subscription login expired."));
            else
                fireError (resp.error.isNotEmpty() ? resp.error : juce::String ("Unknown provider error"));
            break;
        }

        // Record the assistant turn.
        Message assistant;
        assistant.role = Message::Role::Assistant;
        assistant.text = resp.text;
        assistant.toolCalls = resp.toolCalls;
        conversation.push_back (assistant);

        if (resp.toolCalls.empty())
            break;   // final answer — done

        // Execute each tool on the message thread, collect results.
        Message toolResultsMsg;
        toolResultsMsg.role = Message::Role::User;
        for (const auto& call : resp.toolCalls)
        {
            if (threadShouldExit()) break;
            fireTool ("Running " + call.name + "...");

            ToolResult result;
            runOnMessageThreadBlocking ([&]
            {
                result = tools::dispatch (host, call);
            });
            toolResultsMsg.toolResults.push_back (result);
        }
        conversation.push_back (toolResultsMsg);

        if (round == kMaxToolRounds - 1)
            fireError ("Reached the tool-call limit for one turn. Ask me to continue if needed.");
    }

    busy.store (false);
    fireFinish();
}
}
