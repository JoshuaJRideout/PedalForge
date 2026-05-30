#include "AiTools.h"
#include "../util/AppPaths.h"

namespace pf::ai::tools
{
namespace
{
    // Helper to build a JSON-Schema object property entry.
    juce::var schemaObject (std::initializer_list<std::pair<juce::String, juce::var>> props,
                            juce::StringArray required)
    {
        auto* propsObj = new juce::DynamicObject();
        for (const auto& p : props)
            propsObj->setProperty (p.first, p.second);

        auto* schema = new juce::DynamicObject();
        schema->setProperty ("type", "object");
        schema->setProperty ("properties", juce::var (propsObj));
        if (! required.isEmpty())
        {
            juce::Array<juce::var> req;
            for (auto& r : required) req.add (r);
            schema->setProperty ("required", req);
        }
        return juce::var (schema);
    }

    juce::var stringProp (const juce::String& desc)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("type", "string");
        o->setProperty ("description", desc);
        return juce::var (o);
    }

    void auditLog (const juce::String& line)
    {
        auto f = pf::paths::getLogsDir().getChildFile ("ai_audit.log");
        f.appendText ("[" + juce::Time::getCurrentTime().toString (true, true, true) + "] " + line + "\n");
    }

    juce::String argStr (const ToolCall& call, const juce::String& key)
    {
        return call.input.getProperty (key, "").toString();
    }
}

//==============================================================================
std::vector<ToolDef> buildToolDefs()
{
    std::vector<ToolDef> defs;

    defs.push_back ({ "read_active_tab",
        "Describe what the user is currently looking at: the active tab, the "
        "focused pedal (name + uuid) if any, and the selected board. Call this "
        "first to orient yourself before acting.",
        schemaObject ({}, {}) });

    defs.push_back ({ "list_pedals",
        "List every pedal the user has placed on a board, as a JSON array of "
        "{uuid, name, board}. Use this to find a pedal's uuid by name.",
        schemaObject ({}, {}) });

    defs.push_back ({ "read_pedal_design",
        "Return the full PedalDesign JSON (chassis layout, controls, mappings, "
        "scripts) for the pedal with the given design uuid.",
        schemaObject ({ { "uuid", stringProp ("The pedal design's uuid") } }, { "uuid" }) });

    defs.push_back ({ "write_pedal_design",
        "Replace a pedal's full design with new JSON. The JSON must be a "
        "complete PedalDesign object (same shape returned by read_pedal_design). "
        "This is undoable by the user (Cmd-Z).",
        schemaObject ({ { "uuid", stringProp ("The pedal design's uuid") },
                        { "json", stringProp ("The complete new PedalDesign JSON") } },
                      { "uuid", "json" }) });

    defs.push_back ({ "read_fx_graph",
        "Return the FX/DSP node graph JSON for the pedal with the given uuid. "
        "This is the audio-processing graph: nodes and their connections.",
        schemaObject ({ { "pedal_uuid", stringProp ("The pedal design's uuid") } }, { "pedal_uuid" }) });

    defs.push_back ({ "write_fx_graph",
        "Replace a pedal's FX/DSP node graph with new JSON and rebuild it so "
        "the change is immediately audible. Undoable (Cmd-Z).",
        schemaObject ({ { "pedal_uuid", stringProp ("The pedal design's uuid") },
                        { "json", stringProp ("The complete new FX graph JSON") } },
                      { "pedal_uuid", "json" }) });

    defs.push_back ({ "show_toast",
        "Show a short confirmation message to the user in the corner of the app. "
        "Use this to confirm what you did after making changes.",
        schemaObject ({ { "message", stringProp ("The message to show") } }, { "message" }) });

    defs.push_back ({ "list_factory_pedals",
        "List the built-in factory pedals you can add to a board, as a JSON "
        "array of {id, name, category}. Call this before add_pedal_to_board so "
        "you use valid pedal ids.",
        schemaObject ({}, {}) });

    defs.push_back ({ "add_pedal_to_board",
        "Add a factory pedal to the user's current board. It's auto-placed and "
        "auto-wired into the signal chain. Returns {uuid, name} of the new pedal "
        "so you can edit it afterward. Undoable (Cmd-Z).",
        schemaObject ({ { "pedal_id", stringProp ("Factory pedal id or name from list_factory_pedals") } },
                      { "pedal_id" }) });

    //==========================================================================
    // Scripting tools (#65) — the most powerful path. Prefer these for any
    // multi-step construction: build a whole board, pedal, or FX graph in ONE
    // call by writing a script. Call get_script_api FIRST to learn the syntax.
    defs.push_back ({ "get_script_api",
        "Return the PedalForge scripting reference: every command for board / "
        "pedal / fx / dsp scripts plus the ExpressionVM functions. ALWAYS call "
        "this once before writing any script so you use valid commands.",
        schemaObject ({}, {}) });

    defs.push_back ({ "run_board_script",
        "Build the entire pedalboard by running a BOARD script (addPedal, "
        "connect, setPos, focus). The board is cleared first, then rebuilt from "
        "your script. Returns the console output (errors include line numbers). "
        "Best way to create or rearrange a whole board in one step. Undoable.",
        schemaObject ({ { "source", stringProp ("The board script source") } }, { "source" }) });

    defs.push_back ({ "run_pedal_script",
        "Define a pedal's chassis and face controls by running a PEDAL script "
        "(setMeta, setChassis, addKnob, addSwitch, mapControl, …) against the "
        "pedal with the given uuid. Returns console output. Undoable.",
        schemaObject ({ { "pedal_uuid", stringProp ("Target pedal uuid") },
                        { "source", stringProp ("The pedal script source") } },
                      { "pedal_uuid", "source" }) });

    defs.push_back ({ "run_fx_script",
        "Build a pedal's DSP node graph (the audio processing) by running an FX "
        "script (addNode, connect, setParam) against the pedal with the given "
        "uuid. Audible immediately. Returns console output. Undoable.",
        schemaObject ({ { "pedal_uuid", stringProp ("Target pedal uuid") },
                        { "source", stringProp ("The FX graph script source") } },
                      { "pedal_uuid", "source" }) });

    defs.push_back ({ "run_dsp_script",
        "Set a per-sample DSP expression on a pedal's Expression node by running "
        "a DSP script against the pedal with the given uuid. Returns console output.",
        schemaObject ({ { "pedal_uuid", stringProp ("Target pedal uuid") },
                        { "source", stringProp ("The DSP expression source") } },
                      { "pedal_uuid", "source" }) });

    defs.push_back ({ "read_board_as_script",
        "Emit the current live board as an editable BOARD script — read existing "
        "state as code before modifying it.",
        schemaObject ({}, {}) });

    defs.push_back ({ "read_pedal_as_script",
        "Emit a pedal's current chassis/controls as an editable PEDAL script.",
        schemaObject ({ { "pedal_uuid", stringProp ("Target pedal uuid") } }, { "pedal_uuid" }) });

    defs.push_back ({ "read_fx_as_script",
        "Emit a pedal's current DSP graph as an editable FX script.",
        schemaObject ({ { "pedal_uuid", stringProp ("Target pedal uuid") } }, { "pedal_uuid" }) });

    return defs;
}

//==============================================================================
ToolResult dispatch (ToolHost& host, const ToolCall& call)
{
    ToolResult r;
    r.toolUseId = call.id;

    auto fail = [&] (const juce::String& msg) -> ToolResult
    {
        r.isError = true;
        r.content = msg;
        auditLog ("TOOL " + call.name + " ERROR: " + msg);
        return r;
    };

    auditLog ("TOOL " + call.name + " " + juce::JSON::toString (call.input, true));

    if (call.name == "read_active_tab")
    {
        r.content = host.readActiveTab();
        return r;
    }
    if (call.name == "list_pedals")
    {
        r.content = host.listPedals();
        return r;
    }
    if (call.name == "read_pedal_design")
    {
        auto uuid = argStr (call, "uuid");
        if (uuid.isEmpty()) return fail ("Missing 'uuid'");
        auto json = host.readPedalDesign (uuid);
        if (json.isEmpty()) return fail ("No pedal found with uuid " + uuid);
        r.content = json;
        return r;
    }
    if (call.name == "write_pedal_design")
    {
        auto uuid = argStr (call, "uuid");
        auto json = argStr (call, "json");
        if (uuid.isEmpty() || json.isEmpty()) return fail ("Need both 'uuid' and 'json'");
        juce::String err;
        if (! host.writePedalDesign (uuid, json, err))
            return fail ("write_pedal_design failed: " + err);
        r.content = "ok — pedal design updated (user can undo with Cmd-Z)";
        return r;
    }
    if (call.name == "read_fx_graph")
    {
        auto uuid = argStr (call, "pedal_uuid");
        if (uuid.isEmpty()) return fail ("Missing 'pedal_uuid'");
        auto json = host.readFxGraph (uuid);
        if (json.isEmpty()) return fail ("No FX graph for pedal " + uuid);
        r.content = json;
        return r;
    }
    if (call.name == "write_fx_graph")
    {
        auto uuid = argStr (call, "pedal_uuid");
        auto json = argStr (call, "json");
        if (uuid.isEmpty() || json.isEmpty()) return fail ("Need both 'pedal_uuid' and 'json'");
        juce::String err;
        if (! host.writeFxGraph (uuid, json, err))
            return fail ("write_fx_graph failed: " + err);
        r.content = "ok — FX graph rebuilt (user can undo with Cmd-Z)";
        return r;
    }
    if (call.name == "show_toast")
    {
        auto msg = argStr (call, "message");
        host.showToast (msg);
        r.content = "ok";
        return r;
    }
    if (call.name == "list_factory_pedals")
    {
        r.content = host.listFactoryPedals();
        return r;
    }
    if (call.name == "add_pedal_to_board")
    {
        auto pid = argStr (call, "pedal_id");
        if (pid.isEmpty()) return fail ("Missing 'pedal_id'");
        juce::String err;
        auto added = host.addPedalToBoard (pid, err);
        if (added.isEmpty()) return fail ("add_pedal_to_board failed: " + err);
        r.content = added;   // {uuid, name}
        return r;
    }

    //── Scripting tools (#65) ────────────────────────────────────────────
    if (call.name == "get_script_api")
    {
        r.content = host.getScriptApiReference();
        return r;
    }
    if (call.name == "run_board_script")
    {
        auto src = argStr (call, "source");
        if (src.isEmpty()) return fail ("Missing 'source'");
        r.content = host.runBoardScript (src);
        return r;
    }
    if (call.name == "run_pedal_script" || call.name == "run_fx_script" || call.name == "run_dsp_script")
    {
        auto uuid = argStr (call, "pedal_uuid");
        auto src  = argStr (call, "source");
        if (uuid.isEmpty() || src.isEmpty()) return fail ("Need both 'pedal_uuid' and 'source'");
        if (call.name == "run_pedal_script") r.content = host.runPedalScript (uuid, src);
        else if (call.name == "run_fx_script") r.content = host.runFxScript (uuid, src);
        else r.content = host.runDspScript (uuid, src);
        return r;
    }
    if (call.name == "read_board_as_script")
    {
        r.content = host.readBoardAsScript();
        return r;
    }
    if (call.name == "read_pedal_as_script" || call.name == "read_fx_as_script")
    {
        auto uuid = argStr (call, "pedal_uuid");
        if (uuid.isEmpty()) return fail ("Missing 'pedal_uuid'");
        r.content = (call.name == "read_pedal_as_script")
                        ? host.readPedalAsScript (uuid)
                        : host.readFxAsScript (uuid);
        return r;
    }

    return fail ("Unknown tool: " + call.name);
}
}
