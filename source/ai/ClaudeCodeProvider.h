#pragma once

#include "AiProvider.h"

//==============================================================================
// Claude Code provider (task #64) — drives the user's LOCALLY INSTALLED
// `claude` CLI on their Claude subscription (Pro/Max), NOT the metered
// Anthropic API. No API key required; uses the user's existing login.
//
// How it works:
//  - Shells out to `claude -p <prompt> --output-format json` via
//    /usr/bin/env -u ANTHROPIC_API_KEY (so a stray API key in the
//    environment can't silently switch billing to the paid API).
//  - The whole conversation is rendered into the prompt each call
//    (stateless, --no-session-persistence) — robust, no session-id
//    bookkeeping.
//  - Tool use rides a strict JSON protocol: the model is told to reply
//    with exactly {"tool":"name","args":{…}} to call a tool, or
//    {"say":"…"} to answer. The provider parses that back into the
//    shared ToolCall / Response types so the existing AiAgent loop +
//    ToolHost + undo wrapping all work unchanged.
//
// This trades a little token efficiency (re-sends context per turn) for
// reliability and zero extra infrastructure (no in-process MCP server).
// A native MCP-tools integration is a future refinement.
//==============================================================================
namespace pf::ai
{
    class ClaudeCodeProvider : public AiProvider
    {
    public:
        ClaudeCodeProvider();

        juce::String getProviderID()  const override { return "claude-code"; }
        juce::String getDisplayName() const override { return "Claude Code (your subscription)"; }
        bool isConfigured() const override;   // true if the claude binary is found

        juce::String getModelID() const override { return modelAlias; }
        void setModelID (const juce::String& m) override { modelAlias = m; }

        Response send (const juce::String& systemPrompt,
                       const std::vector<Message>& conversation,
                       const std::vector<ToolDef>& tools,
                       std::function<void (const juce::String&)> onTextDelta) override;

        void resetSession() override { sessionId.clear(); sessionStarted = false; }

        /** Absolute path to the `claude` binary, or "" if not found. */
        static juce::String findClaudeBinary();

    private:
        juce::String buildProtocolSystemPrompt (const juce::String& tabSystemPrompt,
                                                const std::vector<ToolDef>& tools) const;
        /** Render only the newest turn to send this call — the warm claude
            session already holds the prior history (see resume logic). */
        juce::String renderLatestTurn (const std::vector<Message>& conversation) const;

        juce::String modelAlias { "sonnet" };

        // Warm-session state: we reuse one claude session per conversation so
        // the prompt cache stays hot and we only send the new turn each round.
        juce::String sessionId;
        bool sessionStarted = false;
    };
}
