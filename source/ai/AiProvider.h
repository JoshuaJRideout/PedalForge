#pragma once

#include "AiTypes.h"

//==============================================================================
// Provider abstraction for the AI assistant (task #64).
//
// v1 ships ClaudeProvider. The interface is streaming-ready (onTextDelta
// callback) even though the v1 Claude impl delivers the assistant text in
// one delta — token-by-token streaming is the immediate fast-follow and
// won't change this interface or any caller.
//==============================================================================
namespace pf::ai
{
    class AiProvider
    {
    public:
        virtual ~AiProvider() = default;

        /** Stable id, e.g. "anthropic". */
        virtual juce::String getProviderID() const = 0;

        /** Human-readable, e.g. "Claude (Anthropic)". */
        virtual juce::String getDisplayName() const = 0;

        /** True when a usable credential is present (key in SecureStore). */
        virtual bool isConfigured() const = 0;

        /** The model id used for requests, e.g. "claude-3-5-sonnet-latest". */
        virtual juce::String getModelID() const = 0;
        virtual void setModelID (const juce::String& modelID) = 0;

        /** Run one assistant turn. BLOCKS — must be called from a worker
            thread, never the message thread. `onTextDelta` is invoked as
            text arrives (may be called once with the whole turn in v1).
            `systemPrompt` is the per-tab system prompt. */
        virtual Response send (const juce::String& systemPrompt,
                               const std::vector<Message>& conversation,
                               const std::vector<ToolDef>& tools,
                               std::function<void (const juce::String&)> onTextDelta) = 0;

        /** Drop any cached per-conversation state (e.g. a warm CLI session).
            Called when the conversation is cleared so the next turn starts
            fresh. Default no-op for stateless providers. */
        virtual void resetSession() {}
    };
}
