#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
// The bridge between the AI agent and the live app (task #64).
//
// PluginEditor implements this. Keeping it an abstract interface means the
// whole ai/ module compiles without depending on the editor, engines, or
// JUCE GUI — and the editor reuses its existing undo/refresh/rebuild logic
// inside these methods rather than the AI layer reaching into engine
// internals.
//
// Every mutating method is responsible for calling engine.saveUndoState()
// BEFORE applying changes, so any agent action is one Cmd-Z away.
//==============================================================================
namespace pf::ai
{
    class ToolHost
    {
    public:
        virtual ~ToolHost() = default;

        /** A compact human-readable description of what the user is looking
            at right now: active tab, focused pedal (name + uuid), selected
            board. Fed to the model as ambient context. */
        virtual juce::String readActiveTab() = 0;

        /** Lightweight index of all pedals the user has on boards:
            JSON array of { uuid, name, board }. Not full payloads. */
        virtual juce::String listPedals() = 0;

        /** Full PedalDesign JSON for the given design uuid, or "" if none. */
        virtual juce::String readPedalDesign (const juce::String& uuid) = 0;

        /** Replace a pedal's full design from JSON. Implementation must
            saveUndoState first, then apply + refresh UI. Returns false and
            sets `errorOut` on parse/apply failure. */
        virtual bool writePedalDesign (const juce::String& uuid,
                                       const juce::String& json,
                                       juce::String& errorOut) = 0;

        /** The FX graph (DSP node graph) JSON for a pedal, or "" if none. */
        virtual juce::String readFxGraph (const juce::String& pedalUuid) = 0;

        /** Replace a pedal's FX graph from JSON and rebuild its processor so
            the change is audible. saveUndoState first. */
        virtual bool writeFxGraph (const juce::String& pedalUuid,
                                   const juce::String& json,
                                   juce::String& errorOut) = 0;

        /** Surface a short message to the user via the toast system. */
        virtual void showToast (const juce::String& message) = 0;

        /** JSON array of factory pedals the agent can add:
            { id, name, category }. `id` is what add_pedal_to_board takes. */
        virtual juce::String listFactoryPedals() = 0;

        /** Add a factory pedal (by id or name) to the current board,
            auto-placed and auto-routed into the chain. Returns JSON
            { uuid, name } for the newly created pedal so the agent can
            then edit it, or "" with errorOut set on failure. */
        virtual juce::String addPedalToBoard (const juce::String& pedalId,
                                              juce::String& errorOut) = 0;

        //======================================================================
        // Scripting-engine access (#65) — the "do anything" surface. Each runs
        // a PedalForge script and returns the console output (success summary
        // or "ERROR line N: …") so the agent can verify and self-correct.

        /** The full scripting API reference (board/pedal/fx commands + VM
            functions). Call before writing scripts. */
        virtual juce::String getScriptApiReference() = 0;

        /** Run a BOARD script — builds the whole pedalboard (adds pedals,
            wires audio). Clears the board first. */
        virtual juce::String runBoardScript (const juce::String& source) = 0;

        /** Run a PEDAL script against the pedal with the given uuid — defines
            its chassis + face controls + control→param mappings. */
        virtual juce::String runPedalScript (const juce::String& pedalUuid,
                                             const juce::String& source) = 0;

        /** Run an FX (DSP node graph) script against the pedal with the given
            uuid — builds the audio-processing graph. Audible immediately. */
        virtual juce::String runFxScript (const juce::String& pedalUuid,
                                          const juce::String& source) = 0;

        /** Run a DSP expression script against the pedal's Expression node. */
        virtual juce::String runDspScript (const juce::String& pedalUuid,
                                           const juce::String& source) = 0;

        /** Round-trip: emit the CURRENT live state as an editable script, so
            the agent can read existing boards/pedals/graphs as code. */
        virtual juce::String readBoardAsScript() = 0;
        virtual juce::String readPedalAsScript (const juce::String& pedalUuid) = 0;
        virtual juce::String readFxAsScript (const juce::String& pedalUuid) = 0;
    };
}
