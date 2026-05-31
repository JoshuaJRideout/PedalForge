#include "ClaudeCodeProvider.h"
#include "../util/AppPaths.h"

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

    // Build a single stream-json user message line: a text block plus one image
    // block per base64 PNG. juce::JSON handles all escaping.
    juce::String buildStreamJsonInput (const juce::String& text,
                                       const std::vector<juce::String>& imagesB64)
    {
        juce::Array<juce::var> content;
        {
            auto* t = new juce::DynamicObject();
            t->setProperty ("type", "text");
            t->setProperty ("text", text);
            content.add (juce::var (t));
        }
        for (const auto& b64 : imagesB64)
        {
            auto* src = new juce::DynamicObject();
            src->setProperty ("type", "base64");
            src->setProperty ("media_type", "image/png");
            src->setProperty ("data", b64);
            auto* img = new juce::DynamicObject();
            img->setProperty ("type", "image");
            img->setProperty ("source", juce::var (src));
            content.add (juce::var (img));
        }
        auto* msg = new juce::DynamicObject();
        msg->setProperty ("role", "user");
        msg->setProperty ("content", content);
        auto* root = new juce::DynamicObject();
        root->setProperty ("type", "user");
        root->setProperty ("message", juce::var (msg));
        return juce::JSON::toString (juce::var (root), true);   // one line
    }

    // stream-json output is JSONL; the final {"type":"result",...} object has
    // the same shape as the single-object json envelope. Find it (search from
    // the end — it's emitted last).
    juce::var extractResultObjectFromStream (const juce::String& raw)
    {
        auto lines = juce::StringArray::fromLines (raw);
        for (int i = lines.size(); --i >= 0;)
        {
            auto line = lines[i].trim();
            if (line.isEmpty() || ! line.startsWith ("{")) continue;
            auto v = juce::JSON::parse (line);
            if (v.isObject() && v.getProperty ("type", "").toString() == "result")
                return v;
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
      << "  - To call a tool:   {\"tool\":\"<name>\",\"args\":{ ... }}\n"
      << "  - To answer the user (final): {\"say\":\"<message>\"}\n"
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

    // Did the latest tool-results turn produce any images (screenshot tool)?
    // If so we must use stream-json input to deliver them as image blocks (#67).
    std::vector<juce::String> images;
    if (! conversation.empty())
        for (const auto& tr : conversation.back().toolResults)
            if (tr.imageBase64.isNotEmpty())
                images.push_back (tr.imageBase64);
    const bool vision = ! images.empty();

    // /usr/bin/env -u ANTHROPIC_API_KEY -u ANTHROPIC_AUTH_TOKEN claude -p …
    // Stripping the API-key env vars guarantees the user's SUBSCRIPTION is
    // used (those vars would otherwise switch to metered API billing).
    // Permission / privacy hardening (task #66): macOS attributes the child's
    // filesystem access to the parent PedalForge bundle, so if claude reads the
    // cwd / ~/Documents / the Photos library, the USER gets scary "PedalForge
    // would like to access …" TCC prompts. We shut that down at the source:
    //   --tools ""             disable ALL built-in tools — our protocol is pure
    //                          text, so it never needs Bash/Read/Glob; with no
    //                          tools it cannot touch the filesystem. Images ride
    //                          inline via stream-json, so this stays enforced
    //                          even for "eyes" (#67).
    //   --setting-sources user load only ~/.claude; skip project/local settings
    //                          discovery that walks the cwd tree.
    //   (cwd confinement below) run inside an app-owned scratch dir.
    juce::StringArray inner {
        "/usr/bin/env", "-u", "ANTHROPIC_API_KEY", "-u", "ANTHROPIC_AUTH_TOKEN",
        bin,
        "--model", modelAlias,
        "--permission-mode", "dontAsk",
        "--tools", "",                       // no built-in tools at all (#66)
        "--setting-sources", "user",         // skip cwd-tree settings discovery
        "--strict-mcp-config"                // skip project MCP discovery (faster startup)
    };

    if (vision)
        // Prompt + image blocks arrive on stdin as a stream-json user message.
        inner.addArray ({ "-p", "--input-format", "stream-json",
                          "--output-format", "stream-json", "--verbose" });
    else
        inner.addArray ({ "-p", prompt, "--output-format", "json" });

    // Session reuse: first turn creates a session (with the system prompt);
    // later turns --resume it so the prompt cache stays warm and we send
    // only the newest turn. Big speed win on multi-round tasks.
    if (! sessionStarted)
    {
        // Claude Code requires a dashed UUID (8-4-4-4-12); juce::Uuid::toString()
        // omits the dashes, which --session-id rejects.
        sessionId = juce::Uuid().toDashedString();
        const auto sys = buildProtocolSystemPrompt (systemPrompt, tools);
        inner.addArray ({ "--session-id", sessionId, "--system-prompt", sys });
        sessionStarted = true;
    }
    else
    {
        inner.addArray ({ "--resume", sessionId });
    }

    // Confine the child to an app-owned scratch directory. The /bin/sh wrapper
    // does `cd <scratch>` then exec's the real command. Every argument is passed
    // as a SEPARATE positional parameter ($1, $2, …), so the prompt/system-prompt
    // text is never re-parsed by the shell — no quoting/escaping hazards even
    // with newlines or quotes in the content. For vision, the stream-json
    // payload is written to a scratch file and redirected onto stdin.
    const auto scratch = pf::paths::getAiScratchDir().getFullPathName();
    juce::StringArray args;
    if (vision)
    {
        auto stdinFile = pf::paths::getAiScratchDir().getChildFile ("ai_vision_input.json");
        stdinFile.replaceWithText (buildStreamJsonInput (prompt, images));
        args = { "/bin/sh", "-c", "cd \"$1\" && in=\"$2\" && shift 2 && exec \"$@\" < \"$in\"",
                 "pf-ai", scratch, stdinFile.getFullPathName() };
    }
    else
    {
        args = { "/bin/sh", "-c", "cd \"$1\" && shift && exec \"$@\"", "pf-ai", scratch };
    }
    args.addArray (inner);

    juce::ChildProcess proc;
    if (! proc.start (args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
    {
        resp.error = "Could not launch Claude Code (" + bin + ").";
        return resp;
    }

    auto raw = proc.readAllProcessOutput();   // blocks until exit
    proc.waitForProcessToFinish (1000);

    // Both paths converge on a single result envelope object.
    auto parsed = vision ? extractResultObjectFromStream (raw) : juce::JSON::parse (raw);
    if (! parsed.isObject())
    {
        resp.error = "Claude Code returned no "
                     + juce::String (vision ? "result object (stream-json)" : "JSON")
                     + ". Output: " + raw.substring (0, 300);
        return resp;
    }

    if ((bool) parsed.getProperty ("is_error", false))
    {
        const auto resultText = parsed.getProperty ("result", raw.substring (0, 300)).toString();
        const int  apiStatus  = (int) parsed.getProperty ("api_error_status", 0);

        // Subscription login expired / missing. The headless `claude -p` we
        // spawn cannot refresh an expired OAuth token (it has no interactive
        // browser flow and can't always write the refreshed token back to the
        // keychain), so it keeps presenting the stale token and the server
        // returns 401. Detect that specifically so the UI can offer re-login
        // instead of dumping raw JSON at the user.
        const auto lower = resultText.toLowerCase();
        const bool authFail = apiStatus == 401
                           || lower.contains ("authentication_error")
                           || lower.contains ("invalid authentication credentials")
                           || lower.contains ("not logged in")
                           || lower.contains ("please run /login");

        if (authFail)
        {
            resp.authExpired = true;
            resp.error = "Your Claude subscription login has expired. Sign in again to "
                         "keep using the assistant - it uses your local Claude Code login, "
                         "no API key needed.";
            // A dead-auth session can't be resumed; force the next turn to start
            // a fresh session once the user has re-authenticated.
            resetSession();
            return resp;
        }

        resp.error = "Claude Code error: " + resultText;
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
