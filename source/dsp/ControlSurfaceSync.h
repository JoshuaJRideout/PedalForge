#pragma once

#include "DSPGraph.h"
#include "PedalDesign.h"
#include <set>

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
    struct Snap { int id; juce::String type; juce::String label; juce::String primaryParam; };
    std::vector<Snap> ctrlNodes;
    std::set<int> validNodeIDs;
    for (const auto& [nid, node] : graph.getNodes())
    {
        if (node && node->isControlSurface())
        {
            auto t = node->getControlType();
            if (t.isEmpty()) continue;
            const auto& params = node->getParams();
            if (params.empty()) continue;
            ctrlNodes.push_back ({ nid, t, node->getName(), params[0].id });
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
        // assume the pairing is intact (whether auto- or user-created) and skip.
        bool alreadyMapped = false;
        for (const auto& m : design.mappings)
            if (m.nodeParam == wantedParam) { alreadyMapped = true; break; }
        if (alreadyMapped) continue;

        PedalDesign::Control c;
        c.type      = cn.type;
        c.controlID = autoID;
        c.label     = cn.label.isNotEmpty() ? cn.label : cn.type;

        // Sensible defaults per type
        if      (c.type == "knob" || c.type == "switch" || c.type == "led" || c.type == "footswitch")
        { c.width = 40; c.height = 40; }
        else if (c.type == "fader")
        { c.width = 30; c.height = 120; }
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
