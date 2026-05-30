#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <functional>

//==============================================================================
// Shared value types for the in-app AI assistant (task #64).
//
// Kept provider-agnostic so ClaudeProvider (v1) and future OpenAI /
// OpenRouter / Ollama providers all speak the same shapes. The agent loop
// in AiAgent never touches provider-specific JSON.
//==============================================================================
namespace pf::ai
{
    //==========================================================================
    /** A tool the model is allowed to call. `inputSchema` is a JSON-Schema
        object describing the tool's parameters (Anthropic "input_schema"). */
    struct ToolDef
    {
        juce::String name;
        juce::String description;
        juce::var    inputSchema;   // JSON Schema object
    };

    //==========================================================================
    /** A tool invocation requested by the model. */
    struct ToolCall
    {
        juce::String id;     // provider-assigned tool_use id (echoed in the result)
        juce::String name;
        juce::var    input;  // parsed arguments object
    };

    /** The app's answer to a ToolCall, fed back into the conversation. */
    struct ToolResult
    {
        juce::String toolUseId;
        juce::String content;
        bool         isError = false;
        // Optional base64-encoded PNG the tool produced (e.g. a screenshot).
        // When set, the provider delivers it to the model as an image block so
        // the agent can actually SEE the rendered UI (task #67, "eyes").
        juce::String imageBase64;
    };

    //==========================================================================
    /** One conversation turn. Anthropic models content as typed blocks; we
        flatten to the three things a turn can carry:
          - User text                  (role = User, text set)
          - User tool results          (role = User, toolResults set)
          - Assistant text + tool_use  (role = Assistant, text and/or toolCalls) */
    struct Message
    {
        enum class Role { User, Assistant };
        Role role = Role::User;
        juce::String text;
        std::vector<ToolCall>   toolCalls;    // assistant tool_use blocks
        std::vector<ToolResult> toolResults;  // user tool_result blocks

        static Message user (const juce::String& t)
        {
            Message m; m.role = Role::User; m.text = t; return m;
        }
    };

    //==========================================================================
    /** A single assistant turn returned by a provider. */
    struct Response
    {
        bool ok = false;
        juce::String error;          // populated when ok == false
        juce::String text;           // assistant text content
        std::vector<ToolCall> toolCalls;
        juce::String stopReason;     // "end_turn", "tool_use", "max_tokens", …
        int inputTokens = 0;
        int outputTokens = 0;
    };
}
