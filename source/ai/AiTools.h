#pragma once

#include "AiTypes.h"
#include "ToolHost.h"

//==============================================================================
// Tool definitions + dispatch for the AI assistant (task #64).
//
// `buildToolDefs()` produces the schema list sent to the model.
// `dispatch()` executes one ToolCall against the ToolHost and returns a
// ToolResult. dispatch() MUST be called on the message thread (it touches
// JUCE/engine state via the host); the agent marshals it there.
//==============================================================================
namespace pf::ai::tools
{
    /** The toolset advertised to the model. v1 minimum surface. */
    std::vector<ToolDef> buildToolDefs();

    /** Execute one tool call. Appends a line to <dataRoot>/logs/ai_audit.log.
        Never throws — failures come back as ToolResult{isError=true}. */
    ToolResult dispatch (ToolHost& host, const ToolCall& call);
}
