#include "ClaudeCodeProvider.h"

namespace pf::ai
{
namespace
{
    // Pull the first balanced {...} JSON object out of a blob of text the
    // model produced (it may wrap protocol JSON in prose or ``` fences).
    juce::String extractFirstJsonObject (const juce::String& text)
    {
        const auto open = text.indexOfChar ('{');
        if (open < 0) return {};
        int depth = 0;
        bool inStr = false, esc = false;
        for (int i = open; i < text.length(); ++i)
        {
            auto c = text[i];
            if (inStr)
            {
                if (esc)          esc = false;
                else if (c == '\\') esc = true;
                else if (c == '"')  inStr = false;
            }
            else
            {
                if (c == '"')      inStr = true;
                else if (c == '{') ++depth;
                else if (c == '}') { if (--depth == 0) return text.substring (open, i + 1); }
            }
        }
        return {};
    }
}

//==============================================================================
ClaudeCodeProvider::ClaudeCodeProvider() {}

juce::String ClaudeCodeProvider::findClaudeBinary()
{
    // Common install locations first (native installer, Homebrew, npm).
    juce::StringArray candidates {
        "~/.local/bin/claude",
        "/usr/local/bin/claude",
        "/opt/homebrew/bin/claude",
        "/home/linuxbrew/.linuxbrew/bin/claude"
    };
    for (auto c : candidates)
    {
        juce::File f (c.replace ("~", juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName()));
        if (f.existsAsFile()) return f.getFullPathName();
    }

    // Fall back to PATH lookup via `which`.
    juce::ChildProcess which;
    if (which.start (juce::StringArray { "/usr/bin/which", "claude" }))
    {
        auto out = which.readAllProcessOutput().trim();
        if (out.isNotEmpty() && juce::File (out).existsAsFile())
            return out;
    }
    return {};
}

bool ClaudeCodeProvider::isConfigured() const
{
    return findClaudeBinary().isNotEmpty();
}

//==============================================================================
juce::String ClaudeCodeProvider::buildProtocolSystemPrompt (const juce::String& tabSystemPrompt,
                                                            const std::vector<ToolDef>& tools) const
{
    juce::String s;
    s << tabSystemPrompt << "\n\n"
      << "=== TOOL PROTOCOL (follow EXACTLY) ===\n"
      << "You operate by emitting ONE JSON object per reply and nothing else "
         "(no prose, no markdown fences):\n"
      << "  • To call a tool:   {\"tool\":\"<name>\",\"args\":{ ... }}\n"
      << "  • To answer the user (final): {\"say\":\"<message>\"}\n"
      << "  To call SEVERAL independent tools AT ONCE (preferred, much "
         "faster): {\"tools\":[{\"tool\":\"<name>\",\"args\":{...}}, ...]}\n"
      << "When you need to do many independent things (e.g. add several "
         "pedals), batch them in ONE {\"tools\":[...]} reply instead of one "
         "tool per turn. After tool calls you will be shown all their "
         "results, then reply again. Always finish with {\"say\":...}. Keep "
         "\"say\" concise.\n\n"
      << "Available tools:\n";
    for (const auto& t : tools)
    {
        s << "  - " << t.name << ": " << t.description << "\n";
        s << "      args schema: " << juce::JSON::toString (t.inputSchema, true) << "\n";
    }
    s << "\nDo NOT use any other tools or attempt file/system operations. "
         "Only the JSON protocol above.";
    return s;
}

juce::String ClaudeCodeProvider::renderLatestTurn (const std::vector<Message>& conversation) const
{
    // The warm claude session already holds prior history, so we only send
    // the newest turn: either the user's latest message, or the results of
    // the tool calls from the previous round.
    if (conversation.empty())
        return "(no input)";

    const auto& m = conversation.back();
    juce::String s;
    if (m.text.isNotEmpty())
    {
        s << m.text;
    }
    else if (! m.toolResults.empty())
    {
        s << "Tool results:\n";
        for (const auto& tr : m.toolResults)
            s << "  - (" << (tr.isError ? "error" : "ok") << ") " << tr.content << "\n";
    }
    s << "\n\nReply with your next single JSON object "
         "({\"tool\":...} | {\"tools\":[...]} | {\"say\":...}).";
    return s;
}

//==============================================================================
Response ClaudeCodeProvider::send (const juce::String& systemPrompt,
                                   const std::vector<Message>& conversation,
                                   const std::vector<ToolDef>& tools,
                                   std::function<void (const juce::String&)> onTextDelta)
{
    Response resp;

    auto bin = findClaudeBinary();
    if (bin.isEmpty())
    {
        resp.error = "Claude Code isn't installed. Install it from claude.com/code "
                     "and run `claude` once to log in with your subscription.";
        return resp;
    }

    const auto prompt = renderLatestTurn (conversation);

    // /usr/bin/env -u ANTHROPIC_API_KEY -u ANTHROPIC_AUTH_TOKEN claude -p …
    // Stripping the API-key env vars guarantees the user's SUBSCRIPTION is
    // used (those vars would otherwise switch to metered API billing).
    juce::StringArray args {
        "/usr/bin/env", "-u", "ANTHROPIC_API_KEY", "-u", "ANTHROPIC_AUTH_TOKEN",
        bin,
        "-p", prompt,
        "--output-format", "json",
        "--model", modelAlias,
        "--permission-mode", "dontAsk",      // can't run its own tools; text only
        "--strict-mcp-config"                // skip project MCP discovery (faster startup)
    };

    // Session reuse: first turn creates a session (with the system prompt);
    // later turns --resume it so the prompt cache stays warm and we send
    // only the newest turn. Big speed win on multi-round tasks.
    if (! sessionStarted)
    {
        sessionId = juce::Uuid().toString();
        const auto sys = buildProtocolSystemPrompt (systemPrompt, tools);
        args.addArray ({ "--session-id", sessionId, "--system-prompt", sys });
        sessionStarted = true;
    }
    else
    {
        args.addArray ({ "--resume", sessionId });
    }

    juce::ChildProcess proc;
    if (! proc.start (args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
    {
        resp.error = "Could not launch Claude Code (" + bin + ").";
        return resp;
    }

    auto raw = proc.readAllProcessOutput();   // blocks until exit
    proc.waitForProcessToFinish (1000);

    auto parsed = juce::JSON::parse (raw);
    if (! parsed.isObject())
    {
        resp.error = "Claude Code returned no JSON. Output: " + raw.substring (0, 300);
        return resp;
    }

    if ((bool) parsed.getProperty ("is_error", false))
    {
        resp.error = "Claude Code error: " + parsed.getProperty ("result", raw.substring (0, 300)).toString();
        return resp;
    }

    auto result = parsed.getProperty ("result", "").toString();
    if (result.isEmpty())
    {
        resp.error = "Claude Code returned an empty result.";
        return resp;
    }

    // Parse the JSON tool protocol out of the model's reply.
    auto jsonStr = extractFirstJsonObject (result);
    auto protocol = jsonStr.isNotEmpty() ? juce::JSON::parse (jsonStr) : juce::var();

    auto makeToolCall = [] (const juce::var& v) -> ToolCall
    {
        ToolCall tc;
        tc.id    = juce::Uuid().toString();
        tc.name  = v.getProperty ("tool", "").toString();
        tc.input = v.getProperty ("args", juce::var (new juce::DynamicObject()));
        return tc;
    };

    if (protocol.isObject() && protocol.hasProperty ("tools"))
    {
        // Batched tool calls — dispatched together in one round.
        if (auto* arr = protocol.getProperty ("tools", {}).getArray())
            for (const auto& v : *arr)
                if (v.isObject() && v.hasProperty ("tool"))
                    resp.toolCalls.push_back (makeToolCall (v));
        resp.stopReason = "tool_use";
    }
    else if (protocol.isObject() && protocol.hasProperty ("tool"))
    {
        resp.toolCalls.push_back (makeToolCall (protocol));
        resp.stopReason = "tool_use";
    }
    else if (protocol.isObject() && protocol.hasProperty ("say"))
    {
        resp.text = protocol.getProperty ("say", "").toString();
        resp.stopReason = "end_turn";
        if (onTextDelta && resp.text.isNotEmpty()) onTextDelta (resp.text);
    }
    else
    {
        // Model answered in prose without the protocol — show it as-is.
        resp.text = result;
        resp.stopReason = "end_turn";
        if (onTextDelta) onTextDelta (resp.text);
    }

    if (auto usage = parsed.getProperty ("usage", {}); usage.isObject())
    {
        resp.inputTokens  = (int) usage.getProperty ("input_tokens", 0);
        resp.outputTokens = (int) usage.getProperty ("output_tokens", 0);
    }

    resp.ok = true;
    return resp;
}
}
