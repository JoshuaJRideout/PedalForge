#include "NodeGraphEditor.h"
#include "LookAndFeel.h"
#include "../dsp/DSPNodeLibrary.h"
#include "../dsp/NodeCatalog.h"

//==============================================================================
// NodeGraphEditor
//==============================================================================
NodeGraphEditor::NodeGraphEditor() : canvas (*this), propertiesPanel (*this)
{
    setWantsKeyboardFocus (true);

    int inID  = graph.addNode (std::make_unique<AudioInputNode>());
    int outID = graph.addNode (std::make_unique<AudioOutputNode>());
    nodeVisuals[inID]  = { 80, 200, nodeW, 0, false };
    nodeVisuals[outID] = { 500, 200, nodeW, 0, false };

    addAndMakeVisible (canvas);
    addAndMakeVisible (propertiesPanel);

    canvas.onNodeSelected = [this] (int id) { selectNode (id); };
    propertiesPanel.onDeleteNode = [this] (int id) { deleteNode (id); };
    propertiesPanel.onParamChanged = [this] {
        if (selectedNodeID >= 0) nodeVisuals[selectedNodeID].height = computeNodeHeight (selectedNodeID);
        if (onGraphChanged) onGraphChanged();
        canvas.repaint();
    };

    // Notes
    notesOverlay.setNotes (fxNotes);
    addChildComponent (notesOverlay);
    btnNotes.setClickingTogglesState (true);
    btnNotes.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFFF59E0B)); // beautiful active amber
    btnNotes.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    btnNotes.setToggleState (NotesOverlay::globallyVisible, juce::dontSendNotification);
    addAndMakeVisible (btnNotes);
    btnNotes.setTooltip ("Toggle Notes");
    btnNotes.onClick = [this] {
        NotesOverlay::globallyVisible = btnNotes.getToggleState();
        bool show = NotesOverlay::globallyVisible;
        notesOverlay.setVisible (show);
        if (show && fxNotes.empty())
            notesOverlay.addNote (120, 80);
    };
}

NodeGraphEditor::~NodeGraphEditor() = default;

void NodeGraphEditor::loadDesign (const juce::var& effectsGraphJSON)
{
    graph.fromJSON (effectsGraphJSON);
    nodeVisuals.clear();
    selectedNodeID = -1;
    propertiesPanel.clearSelection();
    canvas.repaint();

    // Reconstruct visuals
    float fallbackX = 50, fallbackY = 100;
    for (const auto& pair : graph.getNodes())
    {
        auto* n = pair.second.get();
        float x = n->visualX;
        float y = n->visualY;

        // If coordinates are default/invalid (e.g. legacy), use fallback layout
        if (x < 0.0f || y < 0.0f)
        {
            x = fallbackX;
            y = fallbackY;
            fallbackX += 200;
            if (fallbackX > 800) { fallbackX = 50; fallbackY += 150; }
        }

        nodeVisuals[pair.first] = { x, y, nodeW, computeNodeHeight(pair.first), false };
    }
}

void NodeGraphEditor::loadNotes (const std::vector<StickyNote>& notes)
{
    fxNotes = notes;
    notesOverlay.setNotes (fxNotes);
    notesOverlay.setVisible (!fxNotes.empty());
    btnNotes.setToggleState (NotesOverlay::globallyVisible, juce::dontSendNotification);
}

void NodeGraphEditor::visibilityChanged()
{
    if (isVisible())
    {
        btnNotes.setToggleState (NotesOverlay::globallyVisible, juce::dontSendNotification);
        notesOverlay.setVisible (!fxNotes.empty());
    }
}

void NodeGraphEditor::clearGraph()
{
    nodeVisuals.clear();
    selectNode (-1);

    // Reset graph with fresh I/O
    graph.clear();
    int inID  = graph.addNode (std::make_unique<AudioInputNode>());
    int outID = graph.addNode (std::make_unique<AudioOutputNode>());
    nodeVisuals[inID]  = { 80, 200, nodeW, 0, false };
    nodeVisuals[outID] = { 500, 200, nodeW, 0, false };

    if (onGraphChanged) onGraphChanged();
    canvas.repaint();
}

void NodeGraphEditor::paint (juce::Graphics& g)
{
    auto toolbarArea = getLocalBounds().removeFromTop (36);
    g.setGradientFill (juce::ColourGradient (
        PedalForgeLookAndFeel::bgMid.darker (0.1f), 0, (float)toolbarArea.getY(),
        PedalForgeLookAndFeel::bgMid.darker (0.35f), 0, (float)toolbarArea.getBottom(), false));
    g.fillRect (toolbarArea);
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawHorizontalLine (35, 0.0f, (float)getWidth());
}

void NodeGraphEditor::resized()
{
    auto area = getLocalBounds();
    auto toolbar = area.removeFromTop (36);
    toolbar.reduce (8, 4);
    btnNotes.setBounds (toolbar.removeFromLeft (60).withSizeKeepingCentre (60, 24));

    propertiesPanel.setBounds (area.removeFromRight (propertiesWidth));
    canvas.setBounds (area);
    notesOverlay.setBounds (area);
}

void NodeGraphEditor::selectNode (int nodeID)
{
    if (selectedNodeID >= 0 && nodeVisuals.count (selectedNodeID))
        nodeVisuals[selectedNodeID].selected = false;

    selectedNodeID = nodeID;

    if (nodeID >= 0 && nodeVisuals.count (nodeID))
    {
        nodeVisuals[nodeID].selected = true;
        propertiesPanel.showNode (nodeID, graph.getNode (nodeID));
    }
    else
        propertiesPanel.clearSelection();

    canvas.repaint();
}

void NodeGraphEditor::addNodeAt (const juce::String& type, float cx, float cy)
{
    auto node = createNodeByType (type);
    if (!node) return;
    int id = graph.addNode (std::move (node));
    nodeVisuals[id] = { snapToGrid(cx), snapToGrid(cy), nodeW, computeNodeHeight (id), false };
    selectNode (id);
    if (onGraphChanged) onGraphChanged();
}

void NodeGraphEditor::deleteNode (int nodeID)
{
    if (nodeID < 0) return;
    auto* n = graph.getNode (nodeID);
    if (!n) return;
    if (n->getType() == "audio_input" || n->getType() == "audio_output") return;
    graph.removeNode (nodeID);
    nodeVisuals.erase (nodeID);
    selectNode (-1);
    if (onGraphChanged) onGraphChanged();
}

float NodeGraphEditor::computeNodeHeight (int nodeID) const
{
    auto* node = const_cast<DSPGraph&>(graph).getNode (nodeID);
    if (!node) return headerH + 20;
    int standardInputs = 0;
    for (const auto& p : node->getInputPorts())
        if (!p.isParameterCV) standardInputs++;
    int np = juce::jmax (standardInputs, (int)node->getOutputPorts().size());
    
    int displayedParamsCount = 0;
    for (const auto& param : node->getParams())
    {
        if (node->getType() == "grid_sequencer")
        {
            if (param.id != "bpm" && param.id != "run")
                continue;
        }
        displayedParamsCount++;
    }
    
    return headerH + juce::jmax (np, 1) * portSpacing + displayedParamsCount * paramRowH + 10;
}

juce::Point<float> NodeGraphEditor::getPortPosition (int nodeID, bool isOutput, int portIndex) const
{
    auto it = nodeVisuals.find (nodeID);
    if (it == nodeVisuals.end()) return {};
    const auto& v = it->second;
    if (isOutput)
        return { v.x + v.width, v.y + headerH + portSpacing * 0.5f + portIndex * portSpacing };
        
    auto* node = const_cast<DSPGraph&>(graph).getNode (nodeID);
    if (!node || portIndex < 0 || portIndex >= (int)node->getInputPorts().size()) return {};
    
    const auto& port = node->getInputPorts()[portIndex];
    if (port.isParameterCV)
    {
        juce::String paramID = port.name.upToLastOccurrenceOf ("_cv", false, false);
        int pIndex = 0;
        int currentVisualIndex = 0;
        for (int i = 0; i < (int)node->getParams().size(); ++i) {
            if (node->getType() == "grid_sequencer")
            {
                if (node->getParams()[i].id != "bpm" && node->getParams()[i].id != "run")
                    continue;
            }
            if (node->getParams()[i].id == paramID) {
                pIndex = currentVisualIndex;
                break;
            }
            currentVisualIndex++;
        }
        
        int standardInputs = 0;
        for (const auto& p : node->getInputPorts())
            if (!p.isParameterCV) standardInputs++;
        int np = juce::jmax (standardInputs, (int)node->getOutputPorts().size());
        float py = v.y + headerH + juce::jmax (np, 1) * portSpacing + 2 + pIndex * paramRowH + paramRowH * 0.5f;
        
        return { v.x, py };
    }
    else
    {
        int standardIdx = 0;
        for (int i = 0; i < portIndex; ++i)
            if (!node->getInputPorts()[i].isParameterCV)
                standardIdx++;
                
        return { v.x, v.y + headerH + portSpacing * 0.5f + standardIdx * portSpacing };
    }
}

juce::Colour NodeGraphEditor::getNodeColour (const juce::String& type) const
{
    if (type == "audio_input" || type == "audio_output") return juce::Colour (0xFF4ADE80);  // green
    if (type == "gain" || type == "mix" || type == "split") return juce::Colour (0xFF94A3B8);  // slate
    if (type == "lowpass" || type == "highpass" || type == "allpass" || type == "tonestack") return juce::Colour (0xFF38BDF8);  // sky blue
    if (type == "softclip" || type == "hardclip") return juce::Colour (0xFFF97316);  // orange
    if (type == "lfo") return juce::Colour (0xFFA78BFA);  // purple
    if (type == "delay" || type == "mod_delay") return juce::Colour (0xFF22D3EE);  // cyan
    if (type == "compressor" || type == "noisegate") return juce::Colour (0xFFFBBF24);  // yellow
    if (type == "reverb") return juce::Colour (0xFF818CF8);  // indigo
    if (type == "ir") return juce::Colour (0xFF6366F1);      // darker indigo
    if (type == "ram" || type == "sampler") return juce::Colour (0xFF14B8A6);  // teal
    if (type == "faust_custom") return juce::Colour (0xFFEC4899);  // pink
    
    // Synthesizer nodes
    if (type == "oscillator" || type == "noise" || type == "adsr" || type == "ar_env" ||
        type == "svf" || type == "ladder_filter" || type == "vca" || type == "glide" || type == "voice_alloc")
        return juce::Colour (0xFFD946EF);  // fuchsia
    // Logic — teal
    if (type == "and_gate" || type == "or_gate" || type == "not_gate" || type == "xor_gate"
        || type == "nand_gate" || type == "nor_gate" || type == "xnor_gate"
        || type == "buffer" || type == "pulse" || type == "gate_buffer"
        || type == "sr_latch" || type == "d_latch" || type == "d_ff" || type == "t_ff" || type == "jk_ff"
        || type == "comparator" || type == "latch" || type == "mux" || type == "demux" || type == "priority" || type == "constant")
        return juce::Colour (0xFF2DD4BF);
    // Math — rose/coral
    if (type == "add" || type == "subtract" || type == "multiply" || type == "divide" || type == "modulo" ||
        type == "ranger" || type == "smooth" || type == "clamp" || type == "abs" || type == "negate" ||
        type == "round" || type == "floor" || type == "ceiling" || type == "sqrt" || type == "power" ||
        type == "min" || type == "max" || type == "sign" || type == "reciprocal" ||
        type == "increment" || type == "decrement" || type == "average")
        return juce::Colour (0xFFFB7185);
    // Timing / Sensors — amber
    if (type == "clock" || type == "counter" || type == "sequencer"
        || type == "env_follower" || type == "sample_hold")
        return juce::Colour (0xFFF59E0B);
    // Scripting — hot pink / magenta
    if (type == "expression")
        return juce::Colour (0xFFE879F9);
    // MIDI — electric blue
    if (type == "midi_note" || type == "midi_cc" || type == "midi_pitchbend" || type == "midi_clock"
        || type == "midi_program" || type == "midi_pressure" || type == "midi_poly_pressure"
        || type == "midi_cc14" || type == "midi_song_pos" || type == "midi_transport"
        || type == "midi_note_gen" || type == "midi_cc_gen"
        || type == "midi_program_gen" || type == "midi_pressure_gen" || type == "midi_poly_pressure_gen"
        || type == "midi_pitchbend_gen" || type == "midi_transport_gen")
        return juce::Colour (0xFF3B82F6);
    // Control Surface — warm peach (these export to the pedal face)
    if (type == "ctrl_knob" || type == "ctrl_fader" || type == "ctrl_button"
        || type == "ctrl_toggle" || type == "ctrl_selector" || type == "ctrl_xy")
        return juce::Colour (0xFFE8A855);
    // Displays & Gadgets — soft cyan
    if (type == "disp_led" || type == "disp_rgb_led" || type == "disp_display"
        || type == "disp_vu" || type == "disp_tuner" || type == "disp_7seg"
        || type == "disp_text" || type == "disp_console" || type == "disp_scope"
        || type == "disp_pixel" || type == "disp_indicator" || type == "disp_sound")
        return juce::Colour (0xFF22D3EE);
    // I/O Peripherals — indigo (matches I/O category)
    if (type == "io_expression" || type == "io_footswitch" || type == "io_cv_in" || type == "io_cv_out")
        return juce::Colour (0xFF818CF8);
    return juce::Colour (0xFF6B7280);
}

//==============================================================================
// GraphCanvas
//==============================================================================
NodeGraphEditor::GraphCanvas::GraphCanvas (NodeGraphEditor& o) : editor (o) { setWantsKeyboardFocus (true); }

juce::Point<float> NodeGraphEditor::GraphCanvas::screenToCanvas (float sx, float sy) const
{ return { (sx - panX) / scale, (sy - panY) / scale }; }

NodeGraphEditor::PortHit NodeGraphEditor::GraphCanvas::hitTestPort (juce::Point<float> cp) const
{
    float hitR = portR * 2.5f;
    for (const auto& [id, vis] : editor.nodeVisuals)
    {
        auto* node = editor.graph.getNode (id);
        if (!node) continue;
        for (int i = 0; i < (int)node->getOutputPorts().size(); ++i)
            if (cp.getDistanceFrom (editor.getPortPosition (id, true, i)) < hitR)
                return { id, true, i };
        for (int i = 0; i < (int)node->getInputPorts().size(); ++i)
            if (cp.getDistanceFrom (editor.getPortPosition (id, false, i)) < hitR)
                return { id, false, i };
    }
    return {};
}

int NodeGraphEditor::GraphCanvas::hitTestNode (juce::Point<float> cp) const
{
    for (const auto& [id, v] : editor.nodeVisuals)
        if (juce::Rectangle<float>(v.x, v.y, v.width, v.height).contains (cp)) return id;
    return -1;
}

int NodeGraphEditor::GraphCanvas::hitTestConnection (juce::Point<float> cp) const
{
    const auto& conns = editor.graph.getConnections();
    for (int i = 0; i < (int)conns.size(); ++i)
    {
        auto s = editor.getPortPosition (conns[i].sourceNodeID, true, conns[i].sourcePort);
        auto d = editor.getPortPosition (conns[i].destNodeID, false, conns[i].destPort);
        float dist = std::abs (d.x - s.x) * 0.5f;
        // High-resolution check: 100 sample points along the Bezier curve
        for (float t = 0.0f; t <= 1.0f; t += 0.01f)
        {
            float u = 1.0f - t;
            float bx = u*u*u*s.x + 3*u*u*t*(s.x+dist) + 3*u*t*t*(d.x-dist) + t*t*t*d.x;
            float by = u*u*u*s.y + 3*u*u*t*s.y + 3*u*t*t*d.y + t*t*t*d.y;
            if (cp.getDistanceFrom ({bx, by}) < 10.0f) return i;
        }
    }
    return -1;
}

void NodeGraphEditor::GraphCanvas::drawNode (juce::Graphics& g, int nodeID, DSPNode* node, const NodeVisual& visual) const
{
    auto bounds = juce::Rectangle<float>(visual.x, visual.y, visual.width, visual.height);
    auto colour = editor.getNodeColour (node->getType());

    g.setColour (juce::Colour (0x30000000));
    g.fillRoundedRectangle (bounds.translated (3, 3), 6.0f);
    g.setColour (juce::Colour (0xFF1E1E2E));
    g.fillRoundedRectangle (bounds, 6.0f);

    // Header
    g.setColour (colour.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), headerH + 3, 6.0f);
    g.setColour (juce::Colour (0xFF1E1E2E));
    g.fillRect (bounds.getX(), bounds.getY() + headerH - 3, bounds.getWidth(), 3.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
    g.drawText (node->getName(), bounds.withHeight (headerH).reduced (8, 0), juce::Justification::centredLeft);

    // Outline
    g.setColour (visual.selected ? PedalForgeLookAndFeel::accent : colour.withAlpha (0.3f));
    g.drawRoundedRectangle (bounds, 6.0f, visual.selected ? 2.5f : 1.0f);

    // Ports
    auto portColour = [](NodePort::Type t) -> juce::Colour {
        switch (t) {
            case NodePort::Audio:   return juce::Colour (0xFF60A5FA); // blue
            case NodePort::Control: return juce::Colour (0xFF4ADE80); // green
            case NodePort::Midi:    return juce::Colour (0xFFFBBF24); // yellow
            case NodePort::Gate:    return juce::Colour (0xFFF87171); // red
            default:                return juce::Colour (0xFFAAAAAA);
        }
    };
    for (int i = 0; i < (int)node->getInputPorts().size(); ++i)
    {
        auto pp = editor.getPortPosition (nodeID, false, i);
        auto& port = node->getInputPorts()[i];
        auto pc = portColour (port.type);
        
        // Dim incompatible ports during wire drag
        if (editor.canvas.draggingWire && editor.canvas.wireStart.isOutput)
        {
            auto* srcNode = editor.graph.getNode (editor.canvas.wireStart.nodeID);
            if (srcNode)
            {
                auto& srcPorts = srcNode->getOutputPorts();
                if (editor.canvas.wireStart.portIndex < (int)srcPorts.size())
                {
                    if (!NodePort::areCompatible (srcPorts[editor.canvas.wireStart.portIndex].type, port.type))
                        pc = pc.withAlpha (0.15f);
                }
            }
        }
        
        g.setColour (pc);
        g.fillEllipse (pp.x - portR, pp.y - portR, portR * 2, portR * 2);
        
        if (!port.isParameterCV)
        {
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.setFont (juce::FontOptions (9.0f));
            g.drawText (node->getInputPorts()[i].name, pp.x + portR + 4, pp.y - 6, 60, 12, juce::Justification::centredLeft);
        }
    }
    for (int i = 0; i < (int)node->getOutputPorts().size(); ++i)
    {
        auto pp = editor.getPortPosition (nodeID, true, i);
        auto& port = node->getOutputPorts()[i];
        auto pc = portColour (port.type);
        
        // Dim incompatible ports during wire drag
        if (editor.canvas.draggingWire && !editor.canvas.wireStart.isOutput)
        {
            auto* srcNode = editor.graph.getNode (editor.canvas.wireStart.nodeID);
            if (srcNode)
            {
                auto& srcPorts = srcNode->getInputPorts();
                if (editor.canvas.wireStart.portIndex < (int)srcPorts.size())
                {
                    if (!NodePort::areCompatible (port.type, srcPorts[editor.canvas.wireStart.portIndex].type))
                        pc = pc.withAlpha (0.15f);
                }
            }
        }
        
        g.setColour (pc);
        g.fillEllipse (pp.x - portR, pp.y - portR, portR * 2, portR * 2);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (juce::FontOptions (9.0f));
        g.drawText (node->getOutputPorts()[i].name, pp.x - portR - 64, pp.y - 6, 60, 12, juce::Justification::centredRight);
    }

    // Inline param values
    int standardInputs = 0;
    for (const auto& p : node->getInputPorts())
        if (!p.isParameterCV) standardInputs++;
    int np = juce::jmax (standardInputs, (int)node->getOutputPorts().size());
    float py = visual.y + headerH + juce::jmax (np, 1) * portSpacing + 2;
    g.setFont (juce::FontOptions (10.0f));
    for (const auto& param : node->getParams())
    {
        if (node->getType() == "grid_sequencer")
        {
            if (param.id != "bpm" && param.id != "run")
                continue;
        }

        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawText (param.name, visual.x + 10, py, visual.width * 0.5f - 10, paramRowH, juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawText (juce::String (param.get(), 2), visual.x + visual.width * 0.5f, py, visual.width * 0.5f - 10, paramRowH, juce::Justification::centredRight);
        py += paramRowH;
    }
}

void NodeGraphEditor::GraphCanvas::drawConnection (juce::Graphics& g, const NodeConnection& conn, bool highlighted) const
{
    auto s = editor.getPortPosition (conn.sourceNodeID, true, conn.sourcePort);
    auto d = editor.getPortPosition (conn.destNodeID, false, conn.destPort);
    float dist = std::abs (d.x - s.x) * 0.5f;
    juce::Path path;
    path.startNewSubPath (s);
    path.cubicTo (s.x + dist, s.y, d.x - dist, d.y, d.x, d.y);

    if (highlighted)
    {
        g.setColour (juce::Colour (0xFFFF6B6B));
        g.strokePath (path, juce::PathStrokeType (3.5f));
    }
    else
    {
        // Determine wire colour from source port type
        NodePort::Type portType = NodePort::Audio;
        if (auto* srcNode = editor.graph.getNode (conn.sourceNodeID))
        {
            auto& ports = srcNode->getOutputPorts();
            if (conn.sourcePort < (int) ports.size())
                portType = ports[conn.sourcePort].type;
        }
        
        juce::Colour wireCol;
        switch (portType) {
            case NodePort::Audio:   wireCol = juce::Colour (0xBB60A5FA); break; // blue
            case NodePort::Control: wireCol = juce::Colour (0xBB4ADE80); break; // green
            case NodePort::Midi:    wireCol = juce::Colour (0xBBFBBF24); break; // yellow
            case NodePort::Gate:    wireCol = juce::Colour (0xBBF87171); break; // red
            default:                wireCol = juce::Colour (0xBBAAAA00); break;
        }
        g.setColour (wireCol);
        g.strokePath (path, juce::PathStrokeType (2.5f));
    }
}

void NodeGraphEditor::GraphCanvas::drawWirePreview (juce::Graphics& g) const
{
    if (!draggingWire) return;
    auto sp = editor.getPortPosition (wireStart.nodeID, wireStart.isOutput, wireStart.portIndex);
    auto ep = juce::Point<float>(wireEndX, wireEndY);
    float dist = std::abs (ep.x - sp.x) * 0.5f;
    juce::Path path;
    path.startNewSubPath (sp);
    if (wireStart.isOutput)
        path.cubicTo (sp.x + dist, sp.y, ep.x - dist, ep.y, ep.x, ep.y);
    else
        path.cubicTo (sp.x - dist, sp.y, ep.x + dist, ep.y, ep.x, ep.y);
    
    // Color the preview wire by the source port type
    juce::Colour previewCol (0x99FFFFFF);
    if (auto* srcNode = editor.graph.getNode (wireStart.nodeID))
    {
        auto& ports = wireStart.isOutput ? srcNode->getOutputPorts() : srcNode->getInputPorts();
        if (wireStart.portIndex < (int)ports.size())
        {
            switch (ports[wireStart.portIndex].type) {
                case NodePort::Audio:   previewCol = juce::Colour (0xCC60A5FA); break;
                case NodePort::Control: previewCol = juce::Colour (0xCC4ADE80); break;
                case NodePort::Midi:    previewCol = juce::Colour (0xCCFBBF24); break;
                case NodePort::Gate:    previewCol = juce::Colour (0xCCF87171); break;
                default: break;
            }
        }
    }
    g.setColour (previewCol);
    g.strokePath (path, juce::PathStrokeType (2.0f));
}

void NodeGraphEditor::GraphCanvas::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF0F0F1A));
    g.saveState();
    g.addTransform (juce::AffineTransform::scale (scale).translated (panX, panY));

    // Grid dots (aligned to snap grid)
    float gs = NodeGraphEditor::gridSize;
    auto tl = screenToCanvas (0, 0), br = screenToCanvas ((float)getWidth(), (float)getHeight());
    for (float gx = std::floor (tl.x/gs)*gs; gx < br.x; gx += gs)
    {
        for (float gy = std::floor (tl.y/gs)*gs; gy < br.y; gy += gs)
        {
            // Major grid every 5 cells (100px) = brighter crosshair
            bool major = (std::fmod(std::abs(gx), gs * 5.0f) < 0.5f) && (std::fmod(std::abs(gy), gs * 5.0f) < 0.5f);
            g.setColour (juce::Colour (major ? 0x25FFFFFF : 0x12FFFFFF));
            g.fillEllipse (gx - 1, gy - 1, 2, 2);
        }
    }

    // Connections
    const auto& conns = editor.graph.getConnections();
    for (int i = 0; i < (int)conns.size(); ++i)
        drawConnection (g, conns[i], hoveredConnectionIndex == i);

    drawWirePreview (g);

    // Nodes
    for (auto& [id, vis] : editor.nodeVisuals)
    {
        vis.height = editor.computeNodeHeight (id);
        if (auto* node = editor.graph.getNode (id))
            drawNode (g, id, node, vis);
    }
    g.restoreState();

    // HUD
    g.setColour (PedalForgeLookAndFeel::textMuted);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Right-click to add  |  Drag ports to connect  |  Delete/Backspace to remove",
                getLocalBounds().removeFromBottom (24).reduced (12, 0), juce::Justification::centredLeft);
}

void NodeGraphEditor::GraphCanvas::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    auto cp = screenToCanvas ((float)e.x, (float)e.y);
    draggingNodeID = -1;

    auto ph = hitTestPort (cp);
    if (ph.nodeID >= 0) { draggingWire = true; wireStart = ph; wireEndX = cp.x; wireEndY = cp.y; return; }

    // Check connection click for deletion
    int ci = hitTestConnection (cp);
    if (ci >= 0)
    {
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            menu.addItem (1, "Disconnect / Delete Wire");
            int choice = menu.show();
            if (choice == 1)
            {
                auto& c = editor.graph.getConnections()[ci];
                editor.graph.disconnect (c.sourceNodeID, c.sourcePort, c.destNodeID, c.destPort);
                hoveredConnectionIndex = -1;
                setMouseCursor (juce::MouseCursor::NormalCursor);
                if (editor.onGraphChanged) editor.onGraphChanged();
                repaint();
            }
        }
        else
        {
            auto& c = editor.graph.getConnections()[ci];
            editor.graph.disconnect (c.sourceNodeID, c.sourcePort, c.destNodeID, c.destPort);
            hoveredConnectionIndex = -1;
            setMouseCursor (juce::MouseCursor::NormalCursor);
            if (editor.onGraphChanged) editor.onGraphChanged();
            repaint();
        }
        return;
    }

    if (e.mods.isPopupMenu()) { showAddNodeMenu (cp); return; }

    int hit = hitTestNode (cp);
    if (hit >= 0)
    {
        if (e.mods.isAltDown())
        {
            auto* srcNode = editor.graph.getNode (hit);
            if (srcNode)
            {
                auto node = createNodeByType(srcNode->getType());
                if (node)
                {
                    for (auto& p : srcNode->getParams())
                    {
                        if (auto* param = node->getParam (p.id))
                            param->set (p.get());
                    }
                        
                    int newID = editor.graph.addNode (std::move (node));
                    editor.nodeVisuals[newID] = { NodeGraphEditor::snapToGrid(editor.nodeVisuals[hit].x + 20.0f), 
                                                  NodeGraphEditor::snapToGrid(editor.nodeVisuals[hit].y + 20.0f), 
                                                  editor.nodeW, editor.computeNodeHeight(newID), false };
                    hit = newID;
                    if (editor.onGraphChanged) editor.onGraphChanged();
                }
            }
        }
        
        editor.selectNode (hit);
        draggingNodeID = hit;
        nodeDragOffset = { editor.nodeVisuals[hit].x - cp.x, editor.nodeVisuals[hit].y - cp.y };
    }
    else
    {
        editor.selectNode (-1);
        dragStartPan = { panX, panY };
    }
}

void NodeGraphEditor::GraphCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (hoveredConnectionIndex != -1)
    {
        hoveredConnectionIndex = -1;
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }
    
    auto cp = screenToCanvas ((float)e.x, (float)e.y);
    if (draggingWire) { wireEndX = cp.x; wireEndY = cp.y; repaint(); return; }
    if (draggingNodeID >= 0)
    {
        editor.nodeVisuals[draggingNodeID].x = cp.x + nodeDragOffset.x;
        editor.nodeVisuals[draggingNodeID].y = cp.y + nodeDragOffset.y;
        repaint();
        return;
    }
    panX = dragStartPan.x + (float)e.getOffsetFromDragStart().x;
    panY = dragStartPan.y + (float)e.getOffsetFromDragStart().y;
    repaint();
}

void NodeGraphEditor::GraphCanvas::mouseUp (const juce::MouseEvent& e)
{
    if (draggingWire)
    {
        auto cp = screenToCanvas ((float)e.x, (float)e.y);
        auto ph = hitTestPort (cp);
        if (ph.nodeID >= 0 && ph.nodeID != wireStart.nodeID)
        {
            if (wireStart.isOutput && !ph.isOutput)
            {
                if (editor.graph.connect (wireStart.nodeID, wireStart.portIndex, ph.nodeID, ph.portIndex))
                    if (editor.onGraphChanged) editor.onGraphChanged();
            }
            else if (!wireStart.isOutput && ph.isOutput)
            {
                if (editor.graph.connect (ph.nodeID, ph.portIndex, wireStart.nodeID, wireStart.portIndex))
                    if (editor.onGraphChanged) editor.onGraphChanged();
            }
        }
        draggingWire = false;
        repaint();
    }
    if (draggingNodeID >= 0)
    {
        // Snap to grid on drop
        auto& vis = editor.nodeVisuals[draggingNodeID];
        vis.x = NodeGraphEditor::snapToGrid (vis.x);
        vis.y = NodeGraphEditor::snapToGrid (vis.y);
        if (editor.onGraphChanged) editor.onGraphChanged();
        repaint();
    }
    draggingNodeID = -1;
}

void NodeGraphEditor::GraphCanvas::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    float z = 1.0f + w.deltaY * 2.0f;
    if (z > 0) { scale = juce::jlimit (0.2f, 4.0f, scale * z); repaint(); }
}

void NodeGraphEditor::GraphCanvas::mouseMove (const juce::MouseEvent& e)
{
    auto cp = screenToCanvas ((float)e.x, (float)e.y);
    int newHoveredIdx = hitTestConnection (cp);

    // Disable connection hover highlight if mouse is currently over a node or port
    if (hitTestPort (cp).nodeID >= 0 || hitTestNode (cp) >= 0)
        newHoveredIdx = -1;

    if (newHoveredIdx != hoveredConnectionIndex)
    {
        hoveredConnectionIndex = newHoveredIdx;

        if (hoveredConnectionIndex >= 0)
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        else
            setMouseCursor (juce::MouseCursor::NormalCursor);

        repaint();
    }
}

void NodeGraphEditor::GraphCanvas::mouseExit (const juce::MouseEvent&)
{
    if (hoveredConnectionIndex != -1)
    {
        hoveredConnectionIndex = -1;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }
}

bool NodeGraphEditor::GraphCanvas::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    { 
        editor.deleteNode (editor.selectedNodeID); 
        return true; 
    }
    
    if (key.getModifiers().isCommandDown())
    {
        if (key.getKeyCode() == 'C' || key.getKeyCode() == 'c')
        {
            if (editor.selectedNodeID >= 0)
            {
                auto* n = editor.graph.getNode (editor.selectedNodeID);
                if (n)
                {
                    editor.clipboardNode = std::make_unique<ClipboardNode>();
                    editor.clipboardNode->type = n->getType();
                    for (auto& p : n->getParams())
                        editor.clipboardNode->params.push_back({p.id, p.get()});
                }
            }
            return true;
        }
        if (key.getKeyCode() == 'V' || key.getKeyCode() == 'v')
        {
            if (editor.clipboardNode)
            {
                auto node = createNodeByType (editor.clipboardNode->type);
                if (node)
                {
                    for (auto& p : editor.clipboardNode->params)
                    {
                        if (auto* param = node->getParam (p.first))
                            param->set (p.second);
                    }
                        
                    float cx = -panX / scale + getWidth() / (2.0f * scale);
                    float cy = -panY / scale + getHeight() / (2.0f * scale);
                    if (editor.selectedNodeID >= 0)
                    {
                        cx = editor.nodeVisuals[editor.selectedNodeID].x + 20.0f;
                        cy = editor.nodeVisuals[editor.selectedNodeID].y + 20.0f;
                    }
                        
                    int newID = editor.graph.addNode (std::move (node));
                    editor.nodeVisuals[newID] = { NodeGraphEditor::snapToGrid(cx), NodeGraphEditor::snapToGrid(cy), 
                                                  editor.nodeW, editor.computeNodeHeight(newID), false };
                    editor.selectNode (newID);
                    if (editor.onGraphChanged) editor.onGraphChanged();
                    repaint();
                }
            }
            return true;
        }
    }
    
    return false;
}

bool NodeGraphEditor::GraphCanvas::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& dragSourceDetails)
{
    return dragSourceDetails.description.toString().startsWith ("node:");
}

void NodeGraphEditor::GraphCanvas::itemDropped (const juce::DragAndDropTarget::SourceDetails& dragSourceDetails)
{
    juce::String desc = dragSourceDetails.description.toString();
    if (desc.startsWith ("node:"))
    {
        auto parts = juce::StringArray::fromTokens (desc, ":", "");
        if (parts.size() >= 2)
        {
            juce::String type = parts[1];
            auto cp = screenToCanvas ((float)dragSourceDetails.localPosition.x, (float)dragSourceDetails.localPosition.y);
            editor.addNodeAt (type, cp.x, cp.y);
        }
    }
}

namespace
{
    // ── Catalog-driven menu builder ──────────────────────────────────────────
    // Walks NodeCatalog::getEntries() once, splitting each entry's menuPath
    // ("Effects/Filters", "Nodes/MIDI/Receive") into nested PopupMenu nodes.
    // Each leaf carries an item ID equal to (catalog index + 1) so the click
    // callback can recover the type string.

    struct MenuNode
    {
        juce::String                 name;            // display label (empty for root)
        std::vector<MenuNode>        children;        // submenus
        std::vector<std::pair<int, juce::String>> items;  // {itemId, displayName}

        MenuNode& getOrCreateChild (const juce::String& childName)
        {
            for (auto& c : children)
                if (c.name == childName) return c;
            children.push_back ({});
            children.back().name = childName;
            return children.back();
        }
    };

    void buildMenuTree (MenuNode& root)
    {
        const auto& entries = NodeCatalog::getEntries();
        for (size_t i = 0; i < entries.size(); ++i)
        {
            const auto& e = entries[i];
            if (! e.inAddNodeMenu) continue;

            auto segments = juce::StringArray::fromTokens (e.menuPath, "/", {});
            segments.removeEmptyStrings();
            if (segments.isEmpty()) continue;

            MenuNode* node = &root;
            for (int s = 0; s < segments.size(); ++s)
                node = & node->getOrCreateChild (segments[s]);

            node->items.push_back ({ (int) i + 1, e.displayName });
        }
    }

    void populatePopup (juce::PopupMenu& out, const MenuNode& node)
    {
        for (const auto& [id, name] : node.items)
            out.addItem (id, name);

        // Separator between leaf items and submenus only if both exist
        if (! node.items.empty() && ! node.children.empty())
            out.addSeparator();

        for (const auto& child : node.children)
        {
            juce::PopupMenu sub;
            populatePopup (sub, child);
            out.addSubMenu (child.name, sub);
        }
    }
}

void NodeGraphEditor::GraphCanvas::showAddNodeMenu (juce::Point<float> cp)
{
    MenuNode tree;
    buildMenuTree (tree);

    juce::PopupMenu menu;
    populatePopup (menu, tree);

    float cx = cp.x, cy = cp.y;
    menu.showMenuAsync ({}, [this, cx, cy] (int r)
    {
        if (r <= 0) return;
        const auto& entries = NodeCatalog::getEntries();
        int idx = r - 1;
        if (idx < 0 || idx >= (int) entries.size()) return;
        editor.addNodeAt (entries[(size_t) idx].type, cx, cy);
    });
}


//==============================================================================
// NodePropertiesPanel
//==============================================================================
NodeGraphEditor::NodePropertiesPanel::NodePropertiesPanel (NodeGraphEditor& owner)
    : editor (owner)
{
    deleteButton.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::danger.withAlpha (0.3f));
    deleteButton.setColour (juce::TextButton::textColourOffId, PedalForgeLookAndFeel::danger);
    deleteButton.onClick = [this] { if (onDeleteNode && currentNodeID >= 0) onDeleteNode (currentNodeID); };
    addChildComponent (deleteButton);

    viewport.setViewedComponent (&contentArea, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (6);
    addAndMakeVisible (viewport);

    // Code editor setup
    expressionEditor = std::make_unique<juce::CodeEditorComponent> (codeDocument, &luaTokeniser);
    addChildComponent (*expressionEditor);

    expressionError.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::danger);
    expressionError.setFont (juce::FontOptions (11.0f));
    addChildComponent (expressionError);

    codeDocument.addListener (this);
}

void NodeGraphEditor::NodePropertiesPanel::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgDark);
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawVerticalLine (0, 0, (float)getHeight());

    g.setColour (PedalForgeLookAndFeel::textSecondary);
    g.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
    g.drawText ("NODE PROPERTIES", getLocalBounds().withTrimmedTop(10), juce::Justification::centredTop);

    if (!currentNode)
    {
        g.setColour (PedalForgeLookAndFeel::textMuted);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("Select a node\non the canvas", getLocalBounds().reduced (20), juce::Justification::centred);
        return;
    }

    int m = 16, y = 38;

    // Node name
    g.setColour (PedalForgeLookAndFeel::textPrimary);
    g.setFont (juce::FontOptions (16.0f).withStyle ("Bold"));
    g.drawText (currentNode->getName(), m, y, getWidth() - m*2, 22, juce::Justification::centredLeft);
    y += 24;

    // Node type
    g.setColour (PedalForgeLookAndFeel::textMuted);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Type: " + currentNode->getType(), m, y, getWidth() - m*2, 16, juce::Justification::centredLeft);
    y += 18;

    // Ports
    g.drawText ("Inputs: " + juce::String ((int)currentNode->getInputPorts().size())
                + "   Outputs: " + juce::String ((int)currentNode->getOutputPorts().size()),
                m, y, getWidth() - m*2, 16, juce::Justification::centredLeft);
    y += 22;

    // Separator
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawHorizontalLine (y, (float)m, (float)(getWidth() - m));
    y += 8;

    // Expression code section
    if (isExpressionNode)
    {
        g.setColour (juce::Colour (0xFFE879F9));
        g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
        g.drawText ("EXPRESSION CODE", m, y, getWidth() - m*2, 16, juce::Justification::centredLeft);
        y += 16;

        g.setColour (PedalForgeLookAndFeel::textMuted);
        g.setFont (juce::FontOptions (9.0f));
        g.drawText ("Vars: in, in2, out, sr, t, dt, p1-p4, x1-x8", m, y, getWidth() - m*2, 12, juce::Justification::centredLeft);
        y += 12;
        g.drawText ("Funcs: sin cos tanh abs clamp min max pow lerp", m, y, getWidth() - m*2, 12, juce::Justification::centredLeft);
    }
    else
    {
        // "Parameters" section label
        if (!currentNode->getParams().empty())
        {
            g.setColour (PedalForgeLookAndFeel::textSecondary);
            g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
            g.drawText ("PARAMETERS", m, y, getWidth() - m*2, 16, juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (PedalForgeLookAndFeel::textMuted);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText ("No parameters", m, y, getWidth() - m*2, 16, juce::Justification::centredLeft);
        }
    }
}

void NodeGraphEditor::NodePropertiesPanel::resized()
{
    auto area = getLocalBounds().reduced (1, 0);
    deleteButton.setBounds (area.getX() + 16, area.getBottom() - 50, area.getWidth() - 32, 32);

    if (isExpressionNode)
    {
        // Expression code editor takes the top portion
        auto exprArea = area.withTrimmedTop (150).withTrimmedBottom (60);
        int editorH = juce::jmin (exprArea.getHeight() / 2, 180);
        if (expressionEditor != nullptr)
            expressionEditor->setBounds (exprArea.removeFromTop (editorH).reduced (8, 0));
        auto btnRow = exprArea.removeFromTop (30).reduced (8, 3);
        expressionError.setBounds (btnRow);
        viewport.setBounds (exprArea.withTrimmedBottom (0));
    }
    else
    {
        viewport.setBounds (area.withTrimmedTop (150).withTrimmedBottom (60));
    }
}

void NodeGraphEditor::NodePropertiesPanel::showNode (int nodeID, DSPNode* node)
{
    currentNodeID = nodeID;
    currentNode = node;
    isExpressionNode = (node && (node->getType() == "expression" || node->getType() == "disp_shader"));
    rebuildSliders();
    setupExpressionEditor();
    bool canDelete = node != nullptr;
    deleteButton.setVisible (canDelete);
    repaint();
    resized();
    
    if (node != nullptr)
        startTimerHz (20);
    else
        stopTimer();
}

void NodeGraphEditor::NodePropertiesPanel::clearSelection()
{
    stopTimer();
    currentNodeID = -1;
    currentNode = nullptr;
    isExpressionNode = false;
    selectedTrackIndex = 0;
    paramSliders.clear();
    fileLoaders.clear();
    fileLabels.clear();
    customComponents.clear();
    deleteButton.setVisible (false);
    if (expressionEditor != nullptr)
        expressionEditor->setVisible (false);
    expressionError.setVisible (false);
    repaint();
}

void NodeGraphEditor::NodePropertiesPanel::timerCallback()
{
    if (currentNode != nullptr)
    {
        for (auto& ps : paramSliders)
        {
            if (ps.param != nullptr && ps.slider != nullptr)
            {
                float val = ps.param->get();
                if (std::abs (ps.slider->getValue() - val) > 0.001f)
                {
                    ps.slider->setValue (val, juce::dontSendNotification);
                }
            }
        }

        // Try to fetch the corresponding node from the active engine graph
        DSPNode* engineNode = nullptr;
        if (editor.getEngineDSPGraph != nullptr)
        {
            if (auto* engineGraph = editor.getEngineDSPGraph())
            {
                engineNode = engineGraph->getNode (currentNodeID);
            }
        }

        DSPNode* sourceNode = (engineNode != nullptr) ? engineNode : currentNode;

        for (auto& dpl : debugPortLabels)
        {
            if (dpl.valueLabel != nullptr)
            {
                float val = 0.0f;
                if (dpl.isInput)
                {
                    if (dpl.portIndex >= 0 && dpl.portIndex < (int) sourceNode->lastInputValues.size())
                        val = sourceNode->lastInputValues[(size_t) dpl.portIndex];
                }
                else
                {
                    if (dpl.portIndex >= 0 && dpl.portIndex < (int) sourceNode->lastOutputValues.size())
                        val = sourceNode->lastOutputValues[(size_t) dpl.portIndex];
                }
                
                dpl.valueLabel->setText (juce::String (val, 2), juce::dontSendNotification);
            }
        }
    }
}

void NodeGraphEditor::NodePropertiesPanel::setupExpressionEditor()
{
    if (isExpressionNode)
    {
        codeDocument.removeListener (this);
        if (auto* exprNode = dynamic_cast<ExpressionNode*>(currentNode))
        {
            codeDocument.replaceAllContent (exprNode->getExpression());
            expressionError.setText ("", juce::dontSendNotification);
        }
        else if (auto* shaderNode = dynamic_cast<ShaderDisplayNode*>(currentNode))
        {
            codeDocument.replaceAllContent (shaderNode->getExpression());
            expressionError.setText ("", juce::dontSendNotification);
        }
        codeDocument.addListener (this);
        
        if (expressionEditor != nullptr)
            expressionEditor->setVisible (true);
        expressionError.setVisible (true);
    }
    else
    {
        if (expressionEditor != nullptr)
            expressionEditor->setVisible (false);
        expressionError.setVisible (false);
    }
}

void NodeGraphEditor::NodePropertiesPanel::handleExpressionTextChanged()
{
    if (currentNode == nullptr) return;
    
    juce::String text = codeDocument.getAllContent();
    
    if (auto* exprNode = dynamic_cast<ExpressionNode*>(currentNode))
    {
        if (exprNode->getExpression() != text)
        {
            bool ok = exprNode->setExpression (text);
            if (ok)
            {
                expressionError.setColour (juce::Label::textColourId, juce::Colour (0xFF4ADE80));
                expressionError.setText ("Compiled OK!", juce::dontSendNotification);
                if (onParamChanged) onParamChanged();
            }
            else
            {
                expressionError.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::danger);
                expressionError.setText (exprNode->getCompileError(), juce::dontSendNotification);
            }
        }
    }
    else if (auto* shaderNode = dynamic_cast<ShaderDisplayNode*>(currentNode))
    {
        if (shaderNode->getExpression() != text)
        {
            bool ok = shaderNode->setExpression (text);
            if (ok)
            {
                expressionError.setColour (juce::Label::textColourId, juce::Colour (0xFF4ADE80));
                expressionError.setText ("Compiled OK!", juce::dontSendNotification);
                if (onParamChanged) onParamChanged();
            }
            else
            {
                expressionError.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::danger);
                expressionError.setText (shaderNode->getCompileError(), juce::dontSendNotification);
            }
        }
    }
}

void NodeGraphEditor::NodePropertiesPanel::rebuildSliders()
{
    paramSliders.clear();
    fileLoaders.clear();
    fileLabels.clear();
    customComponents.clear();
    contentArea.removeAllChildren();
    if (!currentNode) { contentArea.setSize (0, 0); return; }

    int y = 5;
    int w = juce::jmax (viewport.getWidth() - 12, 100);

    if (currentNode->getType() == "ir" || currentNode->getType() == "sampler" || currentNode->getType() == "nam")
    {
        auto* btn = new juce::TextButton ("Load File...");
        btn->setBounds (12, y, w, 24);
        
        auto* lbl = new juce::Label();
        juce::String fpath = currentNode->getFilePath();
        lbl->setText (fpath.isNotEmpty() ? juce::File(fpath).getFileName() : "No file loaded", juce::dontSendNotification);
        lbl->setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textSecondary);
        lbl->setBounds (12, y + 30, w, 20);

        btn->onClick = [this, lbl, node = currentNode]() {
            juce::String ext = (node->getType() == "nam") ? "*.nam" : "*.wav;*.mp3;*.aif;*.flac";
            fileChooser = std::make_unique<juce::FileChooser> ("Select File", juce::File{}, ext);
            auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
            fileChooser->launchAsync (chooserFlags, [this, lbl, node](const juce::FileChooser& fc) {
                if (fc.getResult().existsAsFile() && currentNode == node) {
                    currentNode->setFilePath (fc.getResult().getFullPathName());
                    lbl->setText (fc.getResult().getFileName(), juce::dontSendNotification);
                    if (onParamChanged) onParamChanged();
                }
            });
        };
        
        contentArea.addAndMakeVisible (btn);
        contentArea.addAndMakeVisible (lbl);
        fileLoaders.add (btn);
        fileLabels.add (lbl);
        
        y += 54;
    }

    if (currentNode->getType() == "grid_sequencer")
    {
        auto* trackLbl = new juce::Label();
        trackLbl->setText ("Select Track:", juce::dontSendNotification);
        trackLbl->setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
        trackLbl->setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textSecondary);
        trackLbl->setBounds (12, y, w, 16);
        contentArea.addAndMakeVisible (trackLbl);
        customComponents.add (trackLbl);
        y += 18;

        auto* cb = new juce::ComboBox();
        for (int i = 0; i < 8; ++i)
            cb->addItem ("Track " + juce::String (i + 1), i + 1);
        cb->setSelectedId (selectedTrackIndex + 1, juce::dontSendNotification);
        cb->setBounds (12, y, w, 24);
        cb->onChange = [this, cb] {
            selectedTrackIndex = cb->getSelectedId() - 1;
            rebuildSliders();
        };
        contentArea.addAndMakeVisible (cb);
        customComponents.add (cb);
        y += 34;
    }

    for (auto& param : currentNode->getParams())
    {
        // Special filtering for Grid Sequencer parameters to prevent properties panel bloat
        if (currentNode->getType() == "grid_sequencer")
        {
            // Skip individual step parameters entirely (user edits these via visual step grid)
            bool isStepParam = param.id.contains ("_s") && !param.id.contains ("_swing");
            if (isStepParam) continue;

            // Only show the currently selected track's parameters, plus global parameters (bpm, run)
            if (param.id.startsWith ("tr"))
            {
                juce::String trackPrefix = "tr" + juce::String (selectedTrackIndex) + "_";
                if (!param.id.startsWith (trackPrefix))
                    continue;
            }
        }

        ParamSlider ps;
        ps.param = &param;

        ps.label = std::make_unique<juce::Label>();
        ps.label->setText (param.name, juce::dontSendNotification);
        ps.label->setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textSecondary);
        ps.label->setFont (juce::FontOptions (11.0f));
        ps.label->setBounds (12, y, w, 16);
        contentArea.addAndMakeVisible (*ps.label);
        y += 16;

        ps.slider = std::make_unique<juce::Slider>();
        ps.slider->setSliderStyle (juce::Slider::LinearHorizontal);
        ps.slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 55, 20);
        ps.slider->setRange (param.minVal, param.maxVal, 0.01);
        ps.slider->setValue (param.get(), juce::dontSendNotification);
        ps.slider->addListener (this);
        ps.slider->setBounds (12, y, w, 24);
        contentArea.addAndMakeVisible (*ps.slider);
        y += 30;

        paramSliders.push_back (std::move (ps));
    }

    // ── Live Debugger Section ──────────────────────────────────────────
    debugPortLabels.clear();

    if (!currentNode->getInputPorts().empty() || !currentNode->getOutputPorts().empty())
    {
        y += 10;
        auto* headerLabel = new juce::Label();
        headerLabel->setText ("LIVE DEBUGGER", juce::dontSendNotification);
        headerLabel->setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
        headerLabel->setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textPrimary.withAlpha (0.6f));
        headerLabel->setBounds (12, y, w, 16);
        contentArea.addAndMakeVisible (headerLabel);
        fileLabels.add (headerLabel); // Add to fileLabels for automatic cleanup
        y += 20;

        // Render Inputs
        if (!currentNode->getInputPorts().empty())
        {
            auto* subLabel = new juce::Label();
            subLabel->setText ("Inputs:", juce::dontSendNotification);
            subLabel->setFont (juce::FontOptions (10.0f).withStyle ("Italic"));
            subLabel->setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textSecondary);
            subLabel->setBounds (12, y, w, 14);
            contentArea.addAndMakeVisible (subLabel);
            fileLabels.add (subLabel);
            y += 16;

            for (int i = 0; i < (int) currentNode->getInputPorts().size(); ++i)
            {
                auto& port = currentNode->getInputPorts()[(size_t) i];
                DebugPortLabel dpl;
                dpl.isInput = true;
                dpl.portIndex = i;

                dpl.nameLabel = std::make_unique<juce::Label>();
                dpl.nameLabel->setText ("  " + port.name, juce::dontSendNotification);
                dpl.nameLabel->setFont (juce::FontOptions (11.0f));
                dpl.nameLabel->setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textSecondary);
                dpl.nameLabel->setBounds (12, y, w - 80, 16);
                contentArea.addAndMakeVisible (*dpl.nameLabel);

                dpl.valueLabel = std::make_unique<juce::Label>();
                dpl.valueLabel->setText ("0.00", juce::dontSendNotification);
                dpl.valueLabel->setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
                dpl.valueLabel->setColour (juce::Label::textColourId, juce::Colour (0xFF4ADE80)); // beautiful bright green
                dpl.valueLabel->setBounds (w - 70, y, 70, 16);
                dpl.valueLabel->setJustificationType (juce::Justification::centredRight);
                contentArea.addAndMakeVisible (*dpl.valueLabel);

                y += 18;
                debugPortLabels.push_back (std::move (dpl));
            }
        }

        // Render Outputs
        if (!currentNode->getOutputPorts().empty())
        {
            y += 5;
            auto* subLabel = new juce::Label();
            subLabel->setText ("Outputs:", juce::dontSendNotification);
            subLabel->setFont (juce::FontOptions (10.0f).withStyle ("Italic"));
            subLabel->setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textSecondary);
            subLabel->setBounds (12, y, w, 14);
            contentArea.addAndMakeVisible (subLabel);
            fileLabels.add (subLabel);
            y += 16;

            for (int i = 0; i < (int) currentNode->getOutputPorts().size(); ++i)
            {
                auto& port = currentNode->getOutputPorts()[(size_t) i];
                DebugPortLabel dpl;
                dpl.isInput = false;
                dpl.portIndex = i;

                dpl.nameLabel = std::make_unique<juce::Label>();
                dpl.nameLabel->setText ("  " + port.name, juce::dontSendNotification);
                dpl.nameLabel->setFont (juce::FontOptions (11.0f));
                dpl.nameLabel->setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textSecondary);
                dpl.nameLabel->setBounds (12, y, w - 80, 16);
                contentArea.addAndMakeVisible (*dpl.nameLabel);

                dpl.valueLabel = std::make_unique<juce::Label>();
                dpl.valueLabel->setText ("0.00", juce::dontSendNotification);
                dpl.valueLabel->setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
                dpl.valueLabel->setColour (juce::Label::textColourId, juce::Colour (0xFF38BDF8)); // beautiful bright blue
                dpl.valueLabel->setBounds (w - 70, y, 70, 16);
                dpl.valueLabel->setJustificationType (juce::Justification::centredRight);
                contentArea.addAndMakeVisible (*dpl.valueLabel);

                y += 18;
                debugPortLabels.push_back (std::move (dpl));
            }
        }
    }

    contentArea.setSize (viewport.getWidth(), y + 10);
}

void NodeGraphEditor::NodePropertiesPanel::sliderValueChanged (juce::Slider* slider)
{
    for (auto& ps : paramSliders)
    {
        if (ps.slider.get() == slider && ps.param)
        {
            ps.param->set ((float)slider->getValue());
            if (onParamChanged) onParamChanged();
            break;
        }
    }
}
