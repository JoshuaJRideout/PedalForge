#include "ClaudeProvider.h"
#include "SecureStore.h"

namespace pf::ai
{
namespace
{
    constexpr const char* kApiKeyName = "anthropic.apiKey";
    constexpr const char* kEndpoint   = "https://api.anthropic.com/v1/messages";
    constexpr const char* kApiVersion = "2023-06-01";

    juce::DynamicObject* obj() { return new juce::DynamicObject(); }

    juce::var textBlock (const juce::String& t)
    {
        auto* o = obj();
        o->setProperty ("type", "text");
        o->setProperty ("text", t);
        return juce::var (o);
    }

    juce::var toolUseBlock (const ToolCall& tc)
    {
        auto* o = obj();
        o->setProperty ("type", "tool_use");
        o->setProperty ("id", tc.id);
        o->setProperty ("name", tc.name);
        o->setProperty ("input", tc.input.isVoid() ? juce::var (obj()) : tc.input);
        return juce::var (o);
    }

    juce::var toolResultBlock (const ToolResult& tr)
    {
        auto* o = obj();
        o->setProperty ("type", "tool_result");
        o->setProperty ("tool_use_id", tr.toolUseId);
        o->setProperty ("content", tr.content);
        if (tr.isError) o->setProperty ("is_error", true);
        return juce::var (o);
    }
}

//==============================================================================
ClaudeProvider::ClaudeProvider() {}

bool ClaudeProvider::isConfigured() const { return hasApiKey(); }

void ClaudeProvider::setApiKey (const juce::String& key) { pf::secure::store (kApiKeyName, key.trim()); }
bool ClaudeProvider::hasApiKey()                          { return pf::secure::has (kApiKeyName); }
void ClaudeProvider::clearApiKey()                        { pf::secure::remove (kApiKeyName); }

//==============================================================================
juce::var ClaudeProvider::buildRequestBody (const juce::String& systemPrompt,
                                            const std::vector<Message>& conversation,
                                            const std::vector<ToolDef>& tools) const
{
    auto* root = obj();
    root->setProperty ("model", modelID);
    root->setProperty ("max_tokens", maxTokens);
    if (systemPrompt.isNotEmpty())
        root->setProperty ("system", systemPrompt);

    // Messages
    juce::Array<juce::var> msgs;
    for (const auto& m : conversation)
    {
        auto* mo = obj();
        mo->setProperty ("role", m.role == Message::Role::User ? "user" : "assistant");

        juce::Array<juce::var> content;
        if (m.text.isNotEmpty())
            content.add (textBlock (m.text));
        for (const auto& tc : m.toolCalls)
            content.add (toolUseBlock (tc));
        for (const auto& tr : m.toolResults)
            content.add (toolResultBlock (tr));

        mo->setProperty ("content", content);
        msgs.add (juce::var (mo));
    }
    root->setProperty ("messages", msgs);

    // Tools
    if (! tools.empty())
    {
        juce::Array<juce::var> toolArr;
        for (const auto& t : tools)
        {
            auto* to = obj();
            to->setProperty ("name", t.name);
            to->setProperty ("description", t.description);
            to->setProperty ("input_schema", t.inputSchema);
            toolArr.add (juce::var (to));
        }
        root->setProperty ("tools", toolArr);
    }

    return juce::var (root);
}

//==============================================================================
Response ClaudeProvider::send (const juce::String& systemPrompt,
                               const std::vector<Message>& conversation,
                               const std::vector<ToolDef>& tools,
                               std::function<void (const juce::String&)> onTextDelta)
{
    Response resp;

    auto apiKey = pf::secure::retrieve (kApiKeyName);
    if (apiKey.isEmpty())
    {
        resp.error = "No Anthropic API key set. Open AI settings and paste your key.";
        return resp;
    }

    auto bodyJson = juce::JSON::toString (buildRequestBody (systemPrompt, conversation, tools));

    juce::URL url (kEndpoint);
    url = url.withPOSTData (bodyJson);

    juce::StringPairArray responseHeaders;
    int statusCode = 0;

    auto headers = juce::String ("x-api-key: ") + apiKey + "\r\n"
                 + "anthropic-version: " + kApiVersion + "\r\n"
                 + "content-type: application/json";

    auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                       .withExtraHeaders (headers)
                       .withConnectionTimeoutMs (60000)
                       .withResponseHeaders (&responseHeaders)
                       .withStatusCode (&statusCode);

    auto stream = url.createInputStream (options);
    if (stream == nullptr)
    {
        resp.error = "Could not reach api.anthropic.com (network error).";
        return resp;
    }

    auto raw = stream->readEntireStreamAsString();

    auto parsed = juce::JSON::parse (raw);
    if (! parsed.isObject())
    {
        resp.error = "Unexpected response from Anthropic (not JSON). HTTP " + juce::String (statusCode);
        return resp;
    }

    // API-level error?
    if (statusCode < 200 || statusCode >= 300 || parsed.getProperty ("type", "").toString() == "error")
    {
        auto errObj = parsed.getProperty ("error", {});
        auto msg = errObj.getProperty ("message", "").toString();
        resp.error = "Anthropic error (HTTP " + juce::String (statusCode) + "): "
                   + (msg.isNotEmpty() ? msg : raw.substring (0, 300));
        return resp;
    }

    // Parse content blocks: text + tool_use.
    if (auto* contentArr = parsed.getProperty ("content", {}).getArray())
    {
        for (const auto& block : *contentArr)
        {
            auto type = block.getProperty ("type", "").toString();
            if (type == "text")
            {
                resp.text += block.getProperty ("text", "").toString();
            }
            else if (type == "tool_use")
            {
                ToolCall tc;
                tc.id    = block.getProperty ("id", "").toString();
                tc.name  = block.getProperty ("name", "").toString();
                tc.input = block.getProperty ("input", juce::var());
                resp.toolCalls.push_back (std::move (tc));
            }
        }
    }

    resp.stopReason = parsed.getProperty ("stop_reason", "").toString();
    if (auto usage = parsed.getProperty ("usage", {}); usage.isObject())
    {
        resp.inputTokens  = (int) usage.getProperty ("input_tokens", 0);
        resp.outputTokens = (int) usage.getProperty ("output_tokens", 0);
    }

    resp.ok = true;

    // v1: deliver the whole text in one delta (streaming is the fast-follow).
    if (onTextDelta && resp.text.isNotEmpty())
        onTextDelta (resp.text);

    return resp;
}
}
