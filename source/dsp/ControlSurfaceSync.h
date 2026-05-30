#pragma once

#include "DSPGraph.h"
#include "PedalDesign.h"
#include <set>

//==============================================================================
// A faceplate control widget and its control-surface DSP node are two views of
// ONE object. This table is the canonical face-type <-> node-type mapping used
// by both reconcile directions. Interactive controls only — displays and pure
// decoration (label/image) are not bonded here.
namespace ControlBinding
{
    inline juce::String nodeTypeForFaceType (const juce::String& faceType)
    {
        if (faceType == "knob")       return "ctrl_knob";
        if (faceType == "fader")      return "ctrl_fader";
        if (faceType == "switch")     return "ctrl_toggle";
        if (faceType == "footswitch") return "ctrl_button";
        if (faceType == "selector")   return "ctrl_selector";
        return {};
    }

    /** True for face widget types that should spawn/own a control-surface node. */
    inline bool isBindableFaceType (const juce::String& faceType)
    {
        return nodeTypeForFaceType (faceType).isNotEmpty();
    }

    constexpr const char* kAutoPrefix = "auto_node_";
}

//==============================================================================
/**
 * Reconcile the active pedal's PedalDesign with the control-surface nodes in
 * its DSP graph. Called whenever the FX graph changes.
 *
 * Rules:
 *   - Every node where isControlSurface() is true gets a matching pedal-face
 *     Control (auto-positioned) and a Mapping binding the control to the node's
 *     "value" parameter.
 *   - Auto-managed controls have controlID prefixed with "auto_node_" so we can
 *     tell them apart from user-placed controls.
 *   - When a control-surface node is removed (or stops being a surface), the
 *     auto-managed Control + Mapping are removed.
 *   - User-placed Controls and Mappings are never touched.
 *
 * This sync is one-way (graph → face). Moving a knob on the face still updates
 * the node's value via the existing Mapping plumbing.
 */
inline void syncControlSurfaceNodes (PedalDesign& design, const DSPGraph& graph)
{
    constexpr const char* kPrefix = "auto_node_";

    auto isAutoManaged = [&] (const juce::String& controlID)
    {
        return controlID.startsWith (kPrefix);
    };

    // 1. Gather currently-valid control-surface nodes.
    // primaryParam is whatever the user moves on the face (KnobNode → "value",
    // ButtonNode → "pressed", ToggleNode → "state", etc.). We use the node's
    // first declared param, since control-surface nodes by convention put the
    // user-facing value first.
    struct Snap { int id; juce::String type; juce::String label; juce::String primaryParam; int positions; };
    std::vector<Snap> ctrlNodes;
    std::set<int> validNodeIDs;     // control-surface + auto-placed display nodes
    std::set<int> allNodeIDs;       // every live node — for user-mapping orphan detection
    for (const auto& [nid, node] : graph.getNodes())
    {
        if (node) allNodeIDs.insert (nid);

        // Easy Display is a full-screen widget rather than a knob, so it doesn't
        // go through the control-surface path — but, like a knob, it should
        // auto-appear on the faceplate when added to the graph. (Small gadget
        // displays — LED/VU/scope — stay user-placed; only the "screen" display
        // types auto-place here.) primaryParam "display" makes the synthetic
        // mapping nodeParam "<id>_display", which the poller resolves to the node.
        if (node && node->isDisplayNode() && node->getDisplayType() == "easy_display")
        {
            ctrlNodes.push_back ({ nid, "easy_display", node->getName(), "display", 0 });
            validNodeIDs.insert (nid);
            continue;
        }

        if (node && node->isControlSurface())
        {
            auto t = node->getControlType();
            if (t.isEmpty()) continue;
            const auto& params = node->getParams();
            if (params.empty()) continue;
            // For selectors, pull the "positions" param so the face control
            // knows how many ticks to draw.
            int positions = 4;
            if (t == "selector")
            {
                if (auto* posParam = node->getParam ("positions"))
                    positions = juce::jlimit (2, 16, (int) posParam->get());
            }
            ctrlNodes.push_back ({ nid, t, node->getName(), params[0].id, positions });
            validNodeIDs.insert (nid);
        }
    }

    // 2. Remove auto-managed Mappings + Controls whose node is gone (or stopped
    //    being a control surface).
    std::vector<juce::String> orphanedControlIDs;
    design.mappings.erase (
        std::remove_if (design.mappings.begin(), design.mappings.end(),
            [&] (const PedalDesign::Mapping& m)
            {
                if (! isAutoManaged (m.controlID)) return false;
                int us = m.nodeParam.indexOfChar ('_');
                if (us <= 0) return false;
                int nid = m.nodeParam.substring (0, us).getIntValue();
                if (validNodeIDs.count (nid)) return false;
                orphanedControlIDs.push_back (m.controlID);
                return true;
            }),
        design.mappings.end());

    if (! orphanedControlIDs.empty())
    {
        design.controls.erase (
            std::remove_if (design.controls.begin(), design.controls.end(),
                [&] (const PedalDesign::Control& c)
                {
                    for (const auto& id : orphanedControlIDs)
                        if (c.controlID == id) return true;
                    return false;
                }),
            design.controls.end());
    }

    // 2b. Also clean USER-placed mappings whose referenced DSP node is gone.
    //    Previously these survived silently — the audit's "orphaned mappings
    //    cause silent failures" case. We drop the mapping (so it stops
    //    being applied) but keep the user-placed control so the user can
    //    re-wire it.
    design.mappings.erase (
        std::remove_if (design.mappings.begin(), design.mappings.end(),
            [&] (const PedalDesign::Mapping& m)
            {
                if (isAutoManaged (m.controlID)) return false;  // handled above
                int us = m.nodeParam.indexOfChar ('_');
                if (us <= 0) return false;
                int nid = m.nodeParam.substring (0, us).getIntValue();
                return allNodeIDs.count (nid) == 0;
            }),
        design.mappings.end());

    // 3. For each control-surface node, ensure there's a Control + Mapping.
    //    We compute a simple grid layout for fresh ones; existing auto-controls
    //    keep their (possibly user-tweaked) position.
    int autoX = 30, autoY = 80;
    constexpr int padX = 20, padY = 24;

    auto advanceLayout = [&] (float w, float h)
    {
        autoX += (int) w + padX;
        if ((float) autoX + w > design.chassisW - 20.0f)
        {
            autoX = 30;
            autoY += (int) h + padY;
        }
    };

    for (const auto& cn : ctrlNodes)
    {
        juce::String wantedParam = juce::String (cn.id) + "_" + cn.primaryParam;
        juce::String autoID      = juce::String (kPrefix) + juce::String (cn.id);

        // Is there already a mapping that targets this node's value? If yes,
        // assume the pairing is intact (whether auto- or user-created). Refresh
        // the auto-managed twin's node-derived props (e.g. a selector's tick
        // count) so "change positions in FX -> face updates" works, then skip.
        bool alreadyMapped = false;
        for (const auto& m : design.mappings)
            if (m.nodeParam == wantedParam) { alreadyMapped = true; break; }
        if (alreadyMapped)
        {
            for (auto& c : design.controls)
                if (c.controlID == autoID)
                {
                    if (cn.type == "selector") c.positions = cn.positions;
                    break;
                }
            continue;
        }

        PedalDesign::Control c;
        c.type      = cn.type;
        c.controlID = autoID;
        c.label     = cn.label.isNotEmpty() ? cn.label : cn.type;

        // Sensible defaults per type
        if      (c.type == "knob" || c.type == "switch" || c.type == "led" || c.type == "footswitch")
        { c.width = 40; c.height = 40; }
        else if (c.type == "selector")
        { c.width = 56; c.height = 56; c.positions = cn.positions; }
        else if (c.type == "fader")
        { c.width = 30; c.height = 120; }
        else if (c.type == "easy_display")
        { c.width = 130; c.height = 90; }
        else
        { c.width = 40; c.height = 40; }

        c.x = (float) autoX;
        c.y = (float) autoY;
        design.controls.push_back (c);

        PedalDesign::Mapping m;
        m.controlID = autoID;
        m.nodeParam = wantedParam;
        design.mappings.push_back (m);

        advanceLayout (c.width, c.height);
    }
}

//==============================================================================
/**
 * The reverse reconcile (face -> graph): the faceplate is authoritative. Run
 * this when leaving the Pedal tab, where the user can ADD or DELETE control
 * widgets but cannot touch the FX graph directly.
 *
 *   - A freshly placed, interactive control widget that is NOT yet bonded
 *     (empty parameterID, not an auto_node_ twin, no existing mapping) spawns
 *     its matching control-surface node, is re-keyed to "auto_node_<newID>",
 *     and gets a mapping to the node's primary param. The user then WIRES that
 *     node's output to an effect's parameter CV input in the FX tab.
 *   - A control-surface node whose twin widget has been deleted is removed from
 *     the graph (they are one object). Only control-surface node types are ever
 *     auto-removed, so user-wired effect nodes are never touched.
 *
 * Controls already bound to a real node param (the legacy direct-mapping style,
 * e.g. factory pedals) are left untouched: they have a non-empty mapping, so
 * they are never re-spawned.
 *
 * Invariant relied upon: every control-surface node already has its auto_node_
 * twin in design.controls before this runs (maintained by syncControlSurfaceNodes
 * on every FX edit). Thus a twinless control-surface node here means "deleted on
 * the face", not "brand new in FX".
 */
inline void syncFaceControlsToGraph (PedalDesign& design, DSPGraph& graph)
{
    using namespace ControlBinding;

    auto isAutoManaged = [] (const juce::String& id) { return id.startsWith (kAutoPrefix); };
    auto hasMapping = [&] (const juce::String& cid)
    {
        for (const auto& m : design.mappings) if (m.controlID == cid) return true;
        return false;
    };

    // A. Spawn a control-surface node for each new, unbound, bindable widget.
    int spawnRow = 0;
    for (auto& c : design.controls)
    {
        if (isAutoManaged (c.controlID))       continue;   // already a twin
        if (! isBindableFaceType (c.type))     continue;   // not an interactive control
        if (hasMapping (c.controlID))          continue;   // already bound (factory direct map or hand-wired)

        auto node = createNodeByType (nodeTypeForFaceType (c.type));
        if (! node) continue;

        // Seed node config from the widget where it has a counterpart.
        if (c.type == "selector")
            if (auto* pp = node->getParam ("positions"))
                pp->set ((float) juce::jlimit (2, 16, c.positions));

        // Lay the new node out in a tidy column to the right of I/O.
        node->visualX = 320.0f;
        node->visualY = 60.0f + (float) (spawnRow++) * 90.0f;

        int newID = graph.addNode (std::move (node));

        juce::String primary = "value";
        if (auto* n = graph.getNode (newID))
            if (! n->getParams().empty()) primary = n->getParams()[0].id;

        // Re-key the widget as the node's twin and bond them.
        c.controlID = juce::String (kAutoPrefix) + juce::String (newID);
        PedalDesign::Mapping m;
        m.controlID = c.controlID;
        m.nodeParam = juce::String (newID) + "_" + primary;
        design.mappings.push_back (m);
    }

    // B. Remove control-surface nodes whose twin widget no longer exists.
    std::set<int> liveTwinNodeIDs;
    for (const auto& c : design.controls)
        if (isAutoManaged (c.controlID))
            liveTwinNodeIDs.insert (
                c.controlID.fromFirstOccurrenceOf (kAutoPrefix, false, false).getIntValue());

    std::vector<int> toRemove;
    for (const auto& [nid, node] : graph.getNodes())
        if (node && node->isControlSurface() && liveTwinNodeIDs.count (nid) == 0)
            toRemove.push_back (nid);

    for (int id : toRemove)
    {
        graph.removeNode (id);
        // Drop any now-dangling auto mapping for the removed node.
        juce::String pfx = juce::String (kAutoPrefix) + juce::String (id);
        design.mappings.erase (
            std::remove_if (design.mappings.begin(), design.mappings.end(),
                [&] (const PedalDesign::Mapping& m) { return m.controlID == pfx; }),
            design.mappings.end());
    }
}
