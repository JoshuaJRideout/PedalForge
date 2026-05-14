#include "NodeGraphEditor.h"
#include "LookAndFeel.h"
#include "../dsp/DSPNodeLibrary.h"

//==============================================================================
// NodeGraphEditor
//==============================================================================
NodeGraphEditor::NodeGraphEditor() : canvas (*this)
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
    propertiesPanel.onParamChanged = [this] { canvas.repaint(); };
}

NodeGraphEditor::~NodeGraphEditor() = default;

void NodeGraphEditor::loadDesign (const juce::var& effectsGraphJSON)
{
    nodeVisuals.clear();
    selectNode (-1);
    graph.fromJSON (effectsGraphJSON);

    // Auto-layout nodes in a horizontal flow
    float x = 80, y = 200;
    for (const auto& [id, node] : graph.getNodes())
    {
        nodeVisuals[id] = { x, y, nodeW, computeNodeHeight (id), false };
        x += nodeW + 60;
        if (x > 900) { x = 80; y += 150; }
    }

    canvas.repaint();
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

    canvas.repaint();
}

void NodeGraphEditor::paint (juce::Graphics&) {}

void NodeGraphEditor::resized()
{
    auto area = getLocalBounds();
    propertiesPanel.setBounds (area.removeFromRight (propertiesWidth));
    canvas.setBounds (area);
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
}

void NodeGraphEditor::deleteNode (int nodeID)
{
    if (nodeID < 0) return;
    auto* n = graph.getNode (nodeID);
    if (!n) return;
    graph.removeNode (nodeID);
    nodeVisuals.erase (nodeID);
    selectNode (-1);
}

float NodeGraphEditor::computeNodeHeight (int nodeID) const
{
    auto* node = const_cast<DSPGraph&>(graph).getNode (nodeID);
    if (!node) return headerH + 20;
    int np = juce::jmax ((int)node->getInputPorts().size(), (int)node->getOutputPorts().size());
    return headerH + juce::jmax (np, 1) * portSpacing + (int)node->getParams().size() * paramRowH + 10;
}

juce::Point<float> NodeGraphEditor::getPortPosition (int nodeID, bool isOutput, int portIndex) const
{
    auto it = nodeVisuals.find (nodeID);
    if (it == nodeVisuals.end()) return {};
    const auto& v = it->second;
    return { isOutput ? (v.x + v.width) : v.x, v.y + headerH + portSpacing * 0.5f + portIndex * portSpacing };
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
        // Simple distance check to the midpoint + thickness
        auto mid = (s + d) * 0.5f;
        float len = s.getDistanceFrom (d);
        // Check a few sample points along the bezier
        for (float t = 0.1f; t <= 0.9f; t += 0.1f)
        {
            float u = 1.0f - t;
            float dist = std::abs (d.x - s.x) * 0.5f;
            float bx = u*u*u*s.x + 3*u*u*t*(s.x+dist) + 3*u*t*t*(d.x-dist) + t*t*t*d.x;
            float by = u*u*u*s.y + 3*u*u*t*s.y + 3*u*t*t*d.y + t*t*t*d.y;
            if (cp.getDistanceFrom ({bx, by}) < 8.0f) return i;
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
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (juce::FontOptions (9.0f));
        g.drawText (node->getInputPorts()[i].name, pp.x + portR + 4, pp.y - 6, 60, 12, juce::Justification::centredLeft);
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
    int np = juce::jmax ((int)node->getInputPorts().size(), (int)node->getOutputPorts().size());
    float py = visual.y + headerH + juce::jmax (np, 1) * portSpacing + 2;
    g.setFont (juce::FontOptions (10.0f));
    for (const auto& param : node->getParams())
    {
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
        drawConnection (g, conns[i], false);

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

    if (e.mods.isPopupMenu()) { showAddNodeMenu (cp); return; }

    auto ph = hitTestPort (cp);
    if (ph.nodeID >= 0) { draggingWire = true; wireStart = ph; wireEndX = cp.x; wireEndY = cp.y; return; }

    // Check connection click for deletion
    int ci = hitTestConnection (cp);
    if (ci >= 0)
    {
        auto& c = editor.graph.getConnections()[ci];
        editor.graph.disconnect (c.sourceNodeID, c.sourcePort, c.destNodeID, c.destPort);
        repaint();
        return;
    }

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
                editor.graph.connect (wireStart.nodeID, wireStart.portIndex, ph.nodeID, ph.portIndex);
            else if (!wireStart.isOutput && ph.isOutput)
                editor.graph.connect (ph.nodeID, ph.portIndex, wireStart.nodeID, wireStart.portIndex);
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
        repaint();
    }
    draggingNodeID = -1;
}

void NodeGraphEditor::GraphCanvas::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    float z = 1.0f + w.deltaY * 2.0f;
    if (z > 0) { scale = juce::jlimit (0.2f, 4.0f, scale * z); repaint(); }
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
                    repaint();
                }
            }
            return true;
        }
    }
    
    return false;
}

void NodeGraphEditor::GraphCanvas::showAddNodeMenu (juce::Point<float> cp)
{
    juce::PopupMenu menu;
    juce::PopupMenu io; 
    io.addItem(4, "Audio Input"); io.addItem(5, "Audio Output"); 
    io.addSeparator();
    io.addItem(6, "MIDI Input"); io.addItem(7, "MIDI Output");
    io.addSeparator();
    io.addItem(350, "Expression Pedal"); io.addItem(351, "Footswitch");
    io.addSeparator();
    io.addItem(352, "CV Input"); io.addItem(353, "CV Output");
    menu.addSubMenu("I/O", io);
    
    juce::PopupMenu u; u.addItem(1,"Gain"); u.addItem(2,"Mix"); u.addItem(3,"Split"); menu.addSubMenu("Utility",u);
    juce::PopupMenu f; f.addItem(10,"Low Pass"); f.addItem(11,"High Pass"); f.addItem(12,"All Pass"); f.addItem(13,"Tone Stack"); f.addItem(14,"Parametric EQ"); menu.addSubMenu("Filters",f);
    juce::PopupMenu dr; dr.addItem(20,"Soft Clip"); dr.addItem(21,"Hard Clip"); dr.addItem(22,"Fuzz"); menu.addSubMenu("Drive",dr);
    juce::PopupMenu m; m.addItem(30,"LFO"); m.addItem(31,"Phaser"); m.addItem(32,"Flanger"); menu.addSubMenu("Modulation",m);
    juce::PopupMenu dl; dl.addItem(40,"Delay"); dl.addItem(41,"Mod Delay"); menu.addSubMenu("Delay",dl);
    juce::PopupMenu dy; dy.addItem(50,"Compressor"); dy.addItem(51,"Noise Gate"); menu.addSubMenu("Dynamics",dy);
    juce::PopupMenu rv; rv.addItem(60,"Reverb"); rv.addItem(61,"IR Convolution"); menu.addSubMenu("Reverb",rv);
    juce::PopupMenu ut; ut.addItem(68,"Cabinet Sim"); menu.addSubMenu("Guitar Utility",ut);
    
    juce::PopupMenu mf; 
    mf.addItem(65,"RAM / Delay Line"); mf.addItem(66,"File Sampler");
    menu.addSubMenu("Memory / Files",mf);

    juce::PopupMenu sy; 
    sy.addItem(70,"Oscillator (VCO)"); sy.addItem(71,"Noise Gen");
    sy.addSeparator();
    sy.addItem(72,"ADSR Envelope"); sy.addItem(73,"AR Envelope");
    sy.addSeparator();
    sy.addItem(74,"State Variable Filter"); sy.addItem(75,"Ladder Filter");
    sy.addSeparator();
    sy.addItem(76,"VCA"); sy.addItem(77,"Glide (Portamento)"); sy.addItem(78,"Voice Allocator");
    menu.addSubMenu("Synthesizer",sy);

    // ─── Wiremod-inspired ───
    menu.addSeparator();
    juce::PopupMenu lg;
    lg.addItem(100,"AND Gate"); lg.addItem(101,"OR Gate"); lg.addItem(102,"NOT Gate");
    lg.addItem(200,"NAND Gate"); lg.addItem(201,"NOR Gate"); lg.addItem(103,"XOR Gate"); lg.addItem(202,"XNOR Gate");
    lg.addSeparator();
    lg.addItem(203,"Buffer"); lg.addItem(204,"Pulse"); lg.addItem(205,"Gate (Buffer)");
    lg.addSeparator();
    lg.addItem(206,"SR Latch"); lg.addItem(207,"D Latch"); lg.addItem(208,"D Flip-Flop"); 
    lg.addItem(209,"T Flip-Flop"); lg.addItem(210,"JK Flip-Flop");
    lg.addSeparator();
    lg.addItem(104,"Comparator"); lg.addItem(105,"Latch / Toggle"); lg.addItem(107,"Constant");
    lg.addSeparator();
    lg.addItem(106,"Mux / A|B Switch"); lg.addItem(211,"Demux"); lg.addItem(212,"Priority");
    menu.addSubMenu("Logic",lg);

    juce::PopupMenu mt;
    mt.addItem(110,"Add"); mt.addItem(170,"Subtract"); mt.addItem(111,"Multiply"); mt.addItem(112,"Divide"); mt.addItem(113,"Modulo");
    mt.addSeparator();
    mt.addItem(171,"Round"); mt.addItem(172,"Floor"); mt.addItem(173,"Ceiling");
    mt.addItem(174,"Square Root"); mt.addItem(175,"Power");
    mt.addItem(176,"Min"); mt.addItem(177,"Max");
    mt.addItem(178,"Sign"); mt.addItem(179,"Reciprocal");
    mt.addItem(180,"Increment"); mt.addItem(181,"Decrement"); mt.addItem(182,"Average");
    mt.addSeparator();
    mt.addItem(114,"Ranger / Remap"); mt.addItem(115,"Smooth / Slew"); mt.addItem(116,"Clamp");
    mt.addSeparator();
    mt.addItem(117,"Abs (Rectify)"); mt.addItem(118,"Negate (Invert)");
    menu.addSubMenu("Math",mt);

    juce::PopupMenu ti;
    ti.addItem(120,"Clock / Timer"); ti.addItem(121,"Counter"); ti.addItem(122,"Sequencer (8-step)");
    ti.addSeparator();
    ti.addItem(123,"Envelope Follower"); ti.addItem(124,"Sample & Hold");
    menu.addSubMenu("Timing / Sensors",ti);

    juce::PopupMenu sc;
    sc.addItem(130,"Expression (E2)");
    menu.addSubMenu("Scripting",sc);

    juce::PopupMenu miRx;
    miRx.addItem(140,"Note"); miRx.addItem(141,"CC"); miRx.addItem(250,"CC 14-bit");
    miRx.addItem(142,"Pitch Bend"); miRx.addItem(143,"Clock");
    miRx.addSeparator();
    miRx.addItem(251,"Program Change"); miRx.addItem(252,"Channel Pressure"); miRx.addItem(253,"Poly Pressure");
    miRx.addSeparator();
    miRx.addItem(254,"Song Position"); miRx.addItem(255,"Transport");

    juce::PopupMenu miTx;
    miTx.addItem(256,"Note Gen"); miTx.addItem(257,"CC Gen");
    miTx.addItem(261,"Pitch Bend Gen");
    miTx.addSeparator();
    miTx.addItem(258,"Program Change Gen"); miTx.addItem(259,"Pressure Gen"); miTx.addItem(260,"Poly Pressure Gen");
    miTx.addSeparator();
    miTx.addItem(262,"Transport Gen");

    juce::PopupMenu mi;
    mi.addSubMenu("Receive (MIDI to CV)", miRx);
    mi.addSubMenu("Generate (CV to MIDI)", miTx);
    menu.addSubMenu("MIDI",mi);

    // ─── Control Surface ───
    menu.addSeparator();
    juce::PopupMenu cs;
    cs.addItem(300,"Knob"); cs.addItem(301,"Fader"); cs.addItem(302,"Button (Momentary)");
    cs.addItem(303,"Toggle (Latching)"); cs.addItem(304,"Selector (Multi-Position)");
    cs.addSeparator();
    cs.addItem(305,"XY Pad");
    menu.addSubMenu("Controls (Pedal UI)",cs);

    // ─── Displays & Lights ───
    juce::PopupMenu dp;
    juce::PopupMenu dpLights;
    dpLights.addItem(320,"LED"); dpLights.addItem(321,"RGB LED"); dpLights.addItem(330,"Indicator Light");
    dp.addSubMenu("Lights", dpLights);

    juce::PopupMenu dpScreens;
    dpScreens.addItem(322,"Numeric Display"); dpScreens.addItem(325,"7-Segment Display");
    dpScreens.addSeparator();
    dpScreens.addItem(326,"Text Screen"); dpScreens.addItem(327,"Console Screen");
    dpScreens.addSeparator();
    dpScreens.addItem(329,"Pixel Display (32x16)");
    dp.addSubMenu("Screens", dpScreens);

    juce::PopupMenu dpInst;
    dpInst.addItem(323,"VU Meter"); dpInst.addItem(328,"Oscilloscope"); dpInst.addItem(324,"Tuner");
    dp.addSubMenu("Instruments", dpInst);

    dp.addSeparator();
    dp.addItem(331,"Sound Emitter");
    menu.addSubMenu("Displays & Gadgets",dp);

    float cx = cp.x, cy = cp.y;
    menu.showMenuAsync ({}, [this, cx, cy](int r) {
        juce::String t;
        switch(r) {
            case 1: t="gain"; break; case 2: t="mix"; break; case 3: t="split"; break;
            case 4: t="audio_input"; break; case 5: t="audio_output"; break;
            case 6: t="midi_input"; break; case 7: t="midi_output"; break;
            case 10: t="lowpass"; break; case 11: t="highpass"; break; case 12: t="allpass"; break; case 13: t="tonestack"; break; case 14: t="peq"; break;
            case 20: t="softclip"; break; case 21: t="hardclip"; break; case 22: t="fuzz"; break;
            case 30: t="lfo"; break; case 31: t="phaser"; break; case 32: t="flanger"; break;
            case 40: t="delay"; break; case 41: t="mod_delay"; break;
            case 50: t="compressor"; break; case 51: t="noisegate"; break; 
            case 60: t="reverb"; break; case 61: t="ir"; break;
            case 68: t="cabinet"; break;
            case 65: t="ram"; break; case 66: t="sampler"; break;
            // Synth
            case 70: t="oscillator"; break; case 71: t="noise"; break;
            case 72: t="adsr"; break; case 73: t="ar_env"; break;
            case 74: t="svf"; break; case 75: t="ladder_filter"; break;
            case 76: t="vca"; break; case 77: t="glide"; break; case 78: t="voice_alloc"; break;
            // Logic
            case 100: t="and_gate"; break; case 101: t="or_gate"; break; case 102: t="not_gate"; break; case 103: t="xor_gate"; break;
            case 200: t="nand_gate"; break; case 201: t="nor_gate"; break; case 202: t="xnor_gate"; break;
            case 203: t="buffer"; break; case 204: t="pulse"; break; case 205: t="gate_buffer"; break;
            case 206: t="sr_latch"; break; case 207: t="d_latch"; break; case 208: t="d_ff"; break;
            case 209: t="t_ff"; break; case 210: t="jk_ff"; break;
            case 211: t="demux"; break; case 212: t="priority"; break;
            case 104: t="comparator"; break; case 105: t="latch"; break; case 106: t="mux"; break; case 107: t="constant"; break;
            // Math
            case 110: t="add"; break; case 170: t="subtract"; break; case 111: t="multiply"; break; case 112: t="divide"; break; case 113: t="modulo"; break;
            case 171: t="round"; break; case 172: t="floor"; break; case 173: t="ceiling"; break;
            case 174: t="sqrt"; break; case 175: t="power"; break;
            case 176: t="min"; break; case 177: t="max"; break;
            case 178: t="sign"; break; case 179: t="reciprocal"; break;
            case 180: t="increment"; break; case 181: t="decrement"; break; case 182: t="average"; break;
            case 114: t="ranger"; break; case 115: t="smooth"; break; case 116: t="clamp"; break;
            case 117: t="abs"; break; case 118: t="negate"; break;
            // Timing
            case 120: t="clock"; break; case 121: t="counter"; break; case 122: t="sequencer"; break;
            case 123: t="env_follower"; break; case 124: t="sample_hold"; break;
            // Scripting
            case 130: t="expression"; break;
            // MIDI
            case 140: t="midi_note"; break; case 141: t="midi_cc"; break; case 250: t="midi_cc14"; break;
            case 142: t="midi_pitchbend"; break; case 143: t="midi_clock"; break;
            case 251: t="midi_program"; break; case 252: t="midi_pressure"; break; case 253: t="midi_poly_pressure"; break;
            case 254: t="midi_song_pos"; break; case 255: t="midi_transport"; break;
            case 256: t="midi_note_gen"; break; case 257: t="midi_cc_gen"; break;
            case 258: t="midi_program_gen"; break; case 259: t="midi_pressure_gen"; break;
            case 260: t="midi_poly_pressure_gen"; break; case 261: t="midi_pitchbend_gen"; break;
            case 262: t="midi_transport_gen"; break;
            // Controls
            case 300: t="ctrl_knob"; break; case 301: t="ctrl_fader"; break; case 302: t="ctrl_button"; break;
            case 303: t="ctrl_toggle"; break; case 304: t="ctrl_selector"; break; case 305: t="ctrl_xy"; break;
            // Displays & Gadgets
            case 320: t="disp_led"; break; case 321: t="disp_rgb_led"; break;
            case 322: t="disp_display"; break; case 323: t="disp_vu"; break; case 324: t="disp_tuner"; break;
            case 325: t="disp_7seg"; break; case 326: t="disp_text"; break; case 327: t="disp_console"; break;
            case 328: t="disp_scope"; break; case 329: t="disp_pixel"; break;
            case 330: t="disp_indicator"; break; case 331: t="disp_sound"; break;
            // I/O Peripherals
            case 350: t="io_expression"; break; case 351: t="io_footswitch"; break;
            case 352: t="io_cv_in"; break; case 353: t="io_cv_out"; break;
            default: return;
        }
        editor.addNodeAt (t, cx, cy);
    });
}

//==============================================================================
// NodePropertiesPanel
//==============================================================================
NodeGraphEditor::NodePropertiesPanel::NodePropertiesPanel()
{
    deleteButton.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::danger.withAlpha (0.3f));
    deleteButton.setColour (juce::TextButton::textColourOffId, PedalForgeLookAndFeel::danger);
    deleteButton.onClick = [this] { if (onDeleteNode && currentNodeID >= 0) onDeleteNode (currentNodeID); };
    addChildComponent (deleteButton);

    viewport.setViewedComponent (&contentArea, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (6);
    addAndMakeVisible (viewport);

    // Expression editor setup
    expressionEditor.setMultiLine (true, true);
    expressionEditor.setReturnKeyStartsNewLine (true);
    expressionEditor.setFont (juce::FontOptions (13.0f));
    expressionEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF12121F));
    expressionEditor.setColour (juce::TextEditor::textColourId, juce::Colour (0xFFE0E0FF));
    expressionEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xFF2DD4BF));
    expressionEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xFFE879F9));
    expressionEditor.setColour (juce::CaretComponent::caretColourId, juce::Colour (0xFFE879F9));
    expressionEditor.setScrollbarsShown (true);
    addChildComponent (expressionEditor);

    expressionError.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::danger);
    expressionError.setFont (juce::FontOptions (11.0f));
    addChildComponent (expressionError);

    compileButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2DD4BF).withAlpha (0.3f));
    compileButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFF2DD4BF));
    compileButton.onClick = [this]
    {
        if (auto* exprNode = dynamic_cast<ExpressionNode*>(currentNode))
        {
            bool ok = exprNode->setExpression (expressionEditor.getText());
            if (ok)
            {
                expressionError.setText ("", juce::dontSendNotification);
                expressionError.setColour (juce::Label::textColourId, juce::Colour (0xFF4ADE80));
                expressionError.setText ("Compiled OK!", juce::dontSendNotification);
            }
            else
            {
                expressionError.setColour (juce::Label::textColourId, PedalForgeLookAndFeel::danger);
                expressionError.setText (exprNode->getCompileError(), juce::dontSendNotification);
            }
        }
    };
    addChildComponent (compileButton);
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
        expressionEditor.setBounds (exprArea.removeFromTop (editorH).reduced (8, 0));
        auto btnRow = exprArea.removeFromTop (30).reduced (8, 3);
        compileButton.setBounds (btnRow.removeFromLeft (80));
        expressionError.setBounds (btnRow.withTrimmedLeft (8));
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
    isExpressionNode = (node && node->getType() == "expression");
    rebuildSliders();
    setupExpressionEditor();
    bool canDelete = node != nullptr;
    deleteButton.setVisible (canDelete);
    repaint();
    resized();
}

void NodeGraphEditor::NodePropertiesPanel::clearSelection()
{
    currentNodeID = -1;
    currentNode = nullptr;
    isExpressionNode = false;
    paramSliders.clear();
    deleteButton.setVisible (false);
    expressionEditor.setVisible (false);
    expressionError.setVisible (false);
    compileButton.setVisible (false);
    repaint();
}

void NodeGraphEditor::NodePropertiesPanel::setupExpressionEditor()
{
    if (isExpressionNode)
    {
        if (auto* exprNode = dynamic_cast<ExpressionNode*>(currentNode))
        {
            expressionEditor.setText (exprNode->getExpression(), false);
            expressionError.setText ("", juce::dontSendNotification);
        }
        expressionEditor.setVisible (true);
        compileButton.setVisible (true);
        expressionError.setVisible (true);
    }
    else
    {
        expressionEditor.setVisible (false);
        compileButton.setVisible (false);
        expressionError.setVisible (false);
    }
}

void NodeGraphEditor::NodePropertiesPanel::rebuildSliders()
{
    paramSliders.clear();
    fileLoaders.clear();
    fileLabels.clear();
    contentArea.removeAllChildren();
    if (!currentNode) { contentArea.setSize (0, 0); return; }

    int y = 5;
    int w = juce::jmax (viewport.getWidth() - 12, 100);

    if (currentNode->getType() == "ir" || currentNode->getType() == "sampler")
    {
        auto* btn = new juce::TextButton ("Load File...");
        btn->setBounds (12, y, w, 24);
        
        auto* lbl = new juce::Label();
        juce::String fpath = currentNode->getFilePath();
        lbl->setText (fpath.isNotEmpty() ? juce::File(fpath).getFileName() : "No file loaded", juce::dontSendNotification);
        lbl->setColour (juce::Label::textColourId, PedalForgeLookAndFeel::textSecondary);
        lbl->setBounds (12, y + 30, w, 20);

        btn->onClick = [this, lbl, node = currentNode]() {
            fileChooser = std::make_unique<juce::FileChooser> ("Select Audio File", juce::File{}, "*.wav;*.mp3;*.aif;*.flac");
            auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
            fileChooser->launchAsync (chooserFlags, [this, lbl, node](const juce::FileChooser& fc) {
                if (fc.getResult().existsAsFile() && currentNode == node) {
                    currentNode->setFilePath (fc.getResult().getFullPathName());
                    lbl->setText (fc.getResult().getFileName(), juce::dontSendNotification);
                }
            });
        };
        
        contentArea.addAndMakeVisible (btn);
        contentArea.addAndMakeVisible (lbl);
        fileLoaders.add (btn);
        fileLabels.add (lbl);
        
        y += 54;
    }

    for (auto& param : currentNode->getParams())
    {
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
