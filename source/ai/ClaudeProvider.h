#pragma once

#include "AiProvider.h"

//==============================================================================
// Anthropic Claude provider (task #64, v1).
//
// Talks to https://api.anthropic.com/v1/messages with the user's own API
// key (stored in SecureStore under "anthropic.apiKey"). Implements the
// tool-use turn: builds the request from the conversation + tool defs,
// POSTs, and parses content blocks (text + tool_use) into a Response.
//
// v1 is request/response (non-streaming): onTextDelta is called once with
// the full assistant text. Token streaming is the immediate fast-follow
// and won't change the AiProvider interface.
//==============================================================================
namespace pf::ai
{
    class ClaudeProvider : public AiProvider
    {
    public:
        ClaudeProvider();

        juce::String getProviderID()  const override { return "anthropic"; }
        juce::String getDisplayName() const override { return "Claude (Anthropic)"; }
        bool isConfigured() const override;

        juce::String getModelID() const override { return modelID; }
        void setModelID (const juce::String& m) override { modelID = m; }

        /** Store the user's API key (SecureStore). */
        static void setApiKey (const juce::String& key);
        static bool hasApiKey();
        static void clearApiKey();

        Response send (const juce::String& systemPrompt,
                       const std::vector<Message>& conversation,
                       const std::vector<ToolDef>& tools,
                       std::function<void (const juce::String&)> onTextDelta) override;

    private:
        juce::var buildRequestBody (const juce::String& systemPrompt,
                                    const std::vector<Message>& conversation,
                                    const std::vector<ToolDef>& tools) const;

        juce::String modelID { "claude-3-5-sonnet-latest" };
        int maxTokens { 4096 };
    };
}
