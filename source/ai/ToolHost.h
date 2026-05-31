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

        // ── FX-graph sticky notes ── teaching/explanatory annotations that live
        //    on a pedal's FX (node) graph. Used to make pedals self-documenting.
        /** JSON array of the pedal's FX notes: [{index,text,x,y,w,h}, ...]. */
        virtual juce::String readFxNotes (const juce::String& pedalUuid) = 0;
        /** Append a note at (x,y). Returns a short confirmation incl. its index. */
        virtual juce::String addFxNote (const juce::String& pedalUuid,
                                        const juce::String& text, int x, int y) = 0;
        /** Replace the text of the note at the given index. */
        virtual juce::String editFxNote (const juce::String& pedalUuid,
                                         int index, const juce::String& text) = 0;
        /** Remove the note at the given index. */
        virtual juce::String deleteFxNote (const juce::String& pedalUuid, int index) = 0;

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

        /** Create a NEW blank custom pedal on the board (not a factory one),
            optionally named, and return JSON { uuid, name }. The agent then
            shapes it with pedal/fx scripts. "" + errorOut on failure. */
        virtual juce::String createBlankPedal (const juce::String& name,
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

        /** Inspect a pedal's LIVE DSP graph and report ground truth the agent
            can't otherwise see: node list, every connection, whether audio
            actually flows audio_input -> … -> audio_output, and orphaned
            (unconnected) nodes. The agent calls this after building to catch
            failed connections and fix them. */
        virtual juce::String verifyPedal (const juce::String& pedalUuid) = 0;

        /** The agent's "ears": run test signals through an offline clone of the
            pedal's DSP graph and report how it SOUNDS — gain, clipping +
            harmonic distortion (THD), dynamics/compression, spectral tilt
            (bright/dark), noise floor, DC, latency/tail, NaN sanity, plus a
            one-line diagnosis. Complements verifyPedal (topology vs. audio). */
        virtual juce::String probePedal (const juce::String& pedalUuid) = 0;

        /** The agent's "eyes": render a view to a PNG and return it base64-
            encoded (or "" on failure). `target` selects what to capture:
            "app"/"" = the whole editor (what the user sees), "board" = the
            pedalboard, "pedal:<uuid>" = a specific pedal's face. The provider
            forwards the image to the model as an image block. */
        virtual juce::String captureView (const juce::String& target) = 0;

        //======================================================================
        // PLAY TAB — the live performance rig. This is a SEPARATE pedal chain
        // from the Board (its own engine + tone presets), so it needs its own
        // tools; the board tools (create_pedal / add_pedal_to_board / board
        // script) do NOT touch it.
        virtual juce::String listPlayPresets() = 0;                          // built-in + saved
        virtual juce::String loadPlayPreset (const juce::String& name) = 0;  // replace chain w/ preset
        virtual juce::String readPlayChain() = 0;                            // current chain, in order
        virtual juce::String playAddPedal (const juce::String& pedalName) = 0; // append a pedal
        virtual juce::String playClear() = 0;                                // empty the chain

        //======================================================================
        // ROUTE TAB — manual audio routing between board pedals. Pedals added
        // to the board are auto-wired left-to-right; these let the agent build
        // custom topologies (parallel chains, splits) and inspect the graph.
        virtual juce::String readRouting() = 0;                                              // list connections
        virtual juce::String connectPedals (const juce::String& fromUuid, const juce::String& toUuid) = 0;
        virtual juce::String disconnectPedals (const juce::String& fromUuid, const juce::String& toUuid) = 0;

        //======================================================================
        // MIDI TAB — map hardware controller CCs to board-pedal parameters.
        // Mapping ids are "<nodeUID>:<paramID>" (get them from listPedalParams).
        virtual juce::String listMidiMappings() = 0;
        virtual juce::String listPedalParams (const juce::String& pedalUuid) = 0;
        virtual juce::String mapMidiCc (const juce::String& param, int cc, int channel) = 0;
        virtual juce::String removeMidiMapping (const juce::String& param) = 0;
        virtual juce::String clearMidiMappings() = 0;

        //======================================================================
        // Navigation + Library. switchTab lets the agent move to ANY tab (then
        // screenshot to see it); listAssets surfaces NAM/IR/image/pedal/board
        // files so the agent knows what's available to use.
        virtual juce::String switchTab (const juce::String& tabName) = 0;
        virtual juce::String listAssets (const juce::String& category) = 0;

        //======================================================================
        // WIKI — read docs as TEXT (cheap), and bring a page up for the user.
        virtual juce::String listWikiPages() = 0;
        virtual juce::String readWikiPage (const juce::String& pageId) = 0;   // markdown text for the agent
        virtual juce::String openWikiPage (const juce::String& pageId) = 0;   // navigate the user's view

        // STYLE ENGINE — enumerate registered StyleKits; set a pedal's kit + colorway.
        // For setPedalStyle pass juce::var() (void) for any field to leave it unchanged.
        virtual juce::String listStyleKits() = 0;
        virtual bool setPedalStyle (const juce::String& uuid,
                                    const juce::var& styleKit,
                                    const juce::var& colorway,
                                    const juce::var& colorwayMode,
                                    juce::String& errorOut) = 0;
    };
}
