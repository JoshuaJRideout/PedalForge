#include "RoutingGraphEditor.h"
#include "LookAndFeel.h"

//==============================================================================
// RoutingGraphEditor
//==============================================================================
RoutingGraphEditor::RoutingGraphEditor (AudioGraphEngine& eng)
    : engine (eng), canvas (*this)
{
    addAndMakeVisible (canvas);
    addAndMakeVisible (propertiesPanel);

    canvas.onNodeSelected = [this] (int idx) { selectNode (idx); };

    syncFromEngine();
}

RoutingGraphEditor::~RoutingGraphEditor() = default;

void RoutingGraphEditor::paint (juce::Graphics& g)
{
    // Tab Toolbar Background
    auto toolbarArea = getLocalBounds().removeFromTop (36);
    g.setColour (PedalForgeLookAndFeel::bgMid.darker(0.2f));
    g.fillRect (toolbarArea);
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawHorizontalLine (35, 0.0f, (float)getWidth());
}

void RoutingGraphEditor::resized()
{
    auto area = getLocalBounds();
    auto toolbar = area.removeFromTop (36);
    propertiesPanel.setBounds (area.removeFromRight (propertiesWidth));
    canvas.setBounds (area);
}

//==============================================================================
// Sync visual graph from the AudioGraphEngine
//==============================================================================
void RoutingGraphEditor::syncFromEngine()
{
    nodes.clear();
    connections.clear();
    selectedNodeIdx = -1;
    propertiesPanel.clearSelection();

    // -- I/O endpoint nodes --------------------------------------------------
    {
        RoutingNode audioIn;
        audioIn.engineNodeId = engine.getAudioInputNodeID();
        audioIn.name = "Audio In";
        audioIn.x = 80;  audioIn.y = 200;
        audioIn.isIONode = true;
        audioIn.outputs.push_back ({ "Left",  PortType::AudioStereo, true,  0 });
        audioIn.outputs.push_back ({ "Right", PortType::AudioStereo, true,  1 });
        nodes.push_back (std::move (audioIn));
    }
    {
        RoutingNode audioOut;
        audioOut.engineNodeId = engine.getAudioOutputNodeID();
        audioOut.name = "Audio Out";
        audioOut.x = 800; audioOut.y = 200;
        audioOut.isIONode = true;
        audioOut.inputs.push_back ({ "Left",  PortType::AudioStereo, false, 0 });
        audioOut.inputs.push_back ({ "Right", PortType::AudioStereo, false, 1 });
        nodes.push_back (std::move (audioOut));
    }

    // -- Pedal nodes ---------------------------------------------------------
    float nx = 300, ny = 100;
    for (const auto& inst : engine.getPedalInstances())
    {
        RoutingNode node;
        node.engineNodeId = inst.nodeID;
        node.name = inst.name;
        node.x = nx;  node.y = ny;
        node.isIONode = false;

        // Standard stereo audio ports
        node.inputs.push_back  ({ "Audio L",  PortType::AudioStereo, false, 0 });
        node.inputs.push_back  ({ "Audio R",  PortType::AudioStereo, false, 1 });
        node.outputs.push_back ({ "Audio L",  PortType::AudioStereo, true,  0 });
        node.outputs.push_back ({ "Audio R",  PortType::AudioStereo, true,  1 });

        // TODO: Add MIDI/Expression ports based on pedal capabilities

        nodes.push_back (std::move (node));

        ny += 120;
        if (ny > 500) { ny = 100; nx += 220; }
    }

    // Compute node heights
    for (auto& node : nodes)
        node.height = computeNodeHeight (node);

    // -- Connections from the AudioProcessorGraph ----------------------------
    auto& graph = engine.getGraph();
    for (const auto& conn : graph.getConnections())
    {
        int srcIdx = findNodeByEngineId (conn.source.nodeID);
        int dstIdx = findNodeByEngineId (conn.destination.nodeID);
        if (srcIdx < 0 || dstIdx < 0) continue;

        // Find port indices matching the channel
        int srcPort = -1, dstPort = -1;
        auto& srcNode = nodes[(size_t) srcIdx];
        auto& dstNode = nodes[(size_t) dstIdx];

        for (int i = 0; i < (int) srcNode.outputs.size(); ++i)
            if (srcNode.outputs[(size_t) i].engineChannel == conn.source.channelIndex)
            { srcPort = i; break; }

        for (int i = 0; i < (int) dstNode.inputs.size(); ++i)
            if (dstNode.inputs[(size_t) i].engineChannel == conn.destination.channelIndex)
            { dstPort = i; break; }

        if (srcPort >= 0 && dstPort >= 0)
            connections.push_back ({ srcIdx, srcPort, dstIdx, dstPort });
    }

    canvas.repaint();
}

//==============================================================================
// Helpers
//==============================================================================
int RoutingGraphEditor::findNodeByEngineId (AudioGraphEngine::NodeID id) const
{
    for (int i = 0; i < (int) nodes.size(); ++i)
        if (nodes[(size_t) i].engineNodeId == id) return i;
    return -1;
}

void RoutingGraphEditor::selectNode (int idx)
{
    if (selectedNodeIdx >= 0 && selectedNodeIdx < (int) nodes.size())
        nodes[(size_t) selectedNodeIdx].selected = false;

    selectedNodeIdx = idx;

    if (idx >= 0 && idx < (int) nodes.size())
    {
        nodes[(size_t) idx].selected = true;
        propertiesPanel.showNode (&nodes[(size_t) idx]);
    }
    else
        propertiesPanel.clearSelection();

    canvas.repaint();
}

float RoutingGraphEditor::computeNodeHeight (const RoutingNode& node) const
{
    int maxPorts = juce::jmax ((int) node.inputs.size(), (int) node.outputs.size());
    return headerH + juce::jmax (maxPorts, 1) * portSpacing + 10;
}

juce::Point<float> RoutingGraphEditor::getPortPosition (int nodeIdx, bool isOutput, int portIdx) const
{
    if (nodeIdx < 0 || nodeIdx >= (int) nodes.size()) return {};
    const auto& n = nodes[(size_t) nodeIdx];
    float px = isOutput ? (n.x + n.width) : n.x;
    float py = n.y + headerH + portSpacing * 0.5f + portIdx * portSpacing;
    return { px, py };
}

juce::Colour RoutingGraphEditor::getPortColour (PortType type) const
{
    switch (type)
    {
        case PortType::AudioStereo: return juce::Colour (0xFF5B9BD5);  // Blue
        case PortType::MIDI:        return juce::Colour (0xFFFFD700);  // Gold
        case PortType::Expression:  return juce::Colour (0xFF4CAF50);  // Green
    }
    return juce::Colours::white;
}

//==============================================================================
// RoutingCanvas
//==============================================================================
RoutingGraphEditor::RoutingCanvas::RoutingCanvas (RoutingGraphEditor& o)
    : editor (o) { setWantsKeyboardFocus (true); }

juce::Point<float> RoutingGraphEditor::RoutingCanvas::screenToCanvas (float sx, float sy) const
{
    return { (sx - panX) / scale, (sy - panY) / scale };
}

RoutingGraphEditor::PortHit RoutingGraphEditor::RoutingCanvas::hitTestPort (juce::Point<float> cp) const
{
    for (int i = 0; i < (int) editor.nodes.size(); ++i)
    {
        const auto& n = editor.nodes[(size_t) i];
        for (int p = 0; p < (int) n.outputs.size(); ++p)
        {
            auto pp = editor.getPortPosition (i, true, p);
            if (cp.getDistanceFrom (pp) < portR * 2.5f)
                return { i, true, p };
        }
        for (int p = 0; p < (int) n.inputs.size(); ++p)
        {
            auto pp = editor.getPortPosition (i, false, p);
            if (cp.getDistanceFrom (pp) < portR * 2.5f)
                return { i, false, p };
        }
    }
    return {};
}

int RoutingGraphEditor::RoutingCanvas::hitTestNode (juce::Point<float> cp) const
{
    for (int i = (int) editor.nodes.size() - 1; i >= 0; --i)
    {
        const auto& n = editor.nodes[(size_t) i];
        if (cp.x >= n.x && cp.x <= n.x + n.width &&
            cp.y >= n.y && cp.y <= n.y + n.height)
            return i;
    }
    return -1;
}

int RoutingGraphEditor::RoutingCanvas::hitTestConnection (juce::Point<float> cp) const
{
    for (int i = 0; i < (int) editor.connections.size(); ++i)
    {
        const auto& conn = editor.connections[(size_t) i];
        auto start = editor.getPortPosition (conn.sourceNodeIdx, true, conn.sourcePortIdx);
        auto end   = editor.getPortPosition (conn.destNodeIdx, false, conn.destPortIdx);

        juce::Path path;
        path.startNewSubPath (start);
        float bezierCP = std::abs (end.x - start.x) * 0.5f;
        path.cubicTo (start.x + bezierCP, start.y,
                      end.x - bezierCP, end.y,
                      end.x, end.y);

        if (path.intersectsLine (juce::Line<float> (cp.x - 5, cp.y - 5, cp.x + 5, cp.y + 5), 4.0f))
            return i;
    }
    return -1;
}

void RoutingGraphEditor::RoutingCanvas::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgDark);

    // Grid dots
    g.setColour (PedalForgeLookAndFeel::gridLine.withAlpha (0.15f));
    float step = gridSize * scale;
    float startX = std::fmod (panX, step);
    float startY = std::fmod (panY, step);
    for (float x = startX; x < getWidth(); x += step)
        for (float y = startY; y < getHeight(); y += step)
            g.fillEllipse (x - 1, y - 1, 2, 2);

    // Apply transform
    g.addTransform (juce::AffineTransform::translation (panX, panY)
                        .scaled (scale, scale, panX, panY));

    // Draw connections
    for (const auto& conn : editor.connections)
        drawConnection (g, conn);

    // Draw wire preview
    if (draggingWire)
        drawWirePreview (g);

    // Draw nodes
    for (int i = 0; i < (int) editor.nodes.size(); ++i)
        drawNode (g, i, editor.nodes[(size_t) i]);
}

void RoutingGraphEditor::RoutingCanvas::drawNode (juce::Graphics& g, int idx, const RoutingNode& node) const
{
    auto bounds = juce::Rectangle<float> (node.x, node.y, node.width, node.height);

    // Body
    auto bodyColour = node.isIONode ? juce::Colour (0xFF2D5A3D) : PedalForgeLookAndFeel::bgLight;
    g.setColour (bodyColour);
    g.fillRoundedRectangle (bounds, 8.0f);

    // Selection border
    if (node.selected)
    {
        g.setColour (PedalForgeLookAndFeel::accent);
        g.drawRoundedRectangle (bounds, 8.0f, 2.0f);
    }
    else
    {
        g.setColour (PedalForgeLookAndFeel::gridLine.withAlpha (0.4f));
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
    }

    // Header
    auto headerRect = bounds.removeFromTop (headerH);
    g.setColour (node.isIONode ? juce::Colour (0xFF1A3D25) : PedalForgeLookAndFeel::bgMid);
    g.fillRoundedRectangle (headerRect.getX(), headerRect.getY(),
                             headerRect.getWidth(), headerRect.getHeight(), 8.0f);
    // Fix bottom corners
    g.fillRect (headerRect.getX(), headerRect.getBottom() - 8,
                headerRect.getWidth(), 8.0f);

    g.setColour (PedalForgeLookAndFeel::textPrimary);
    g.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
    g.drawText (node.name, headerRect.reduced (8, 0), juce::Justification::centredLeft);

    // Input ports (left side)
    for (int i = 0; i < (int) node.inputs.size(); ++i)
    {
        auto pp = editor.getPortPosition (idx, false, i);
        g.setColour (editor.getPortColour (node.inputs[(size_t) i].type));
        g.fillEllipse (pp.x - portR, pp.y - portR, portR * 2, portR * 2);

        g.setColour (PedalForgeLookAndFeel::textSecondary);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (node.inputs[(size_t) i].name,
                    pp.x + portR + 4, pp.y - 7, 70, 14,
                    juce::Justification::centredLeft);
    }

    // Output ports (right side)
    for (int i = 0; i < (int) node.outputs.size(); ++i)
    {
        auto pp = editor.getPortPosition (idx, true, i);
        g.setColour (editor.getPortColour (node.outputs[(size_t) i].type));
        g.fillEllipse (pp.x - portR, pp.y - portR, portR * 2, portR * 2);

        g.setColour (PedalForgeLookAndFeel::textSecondary);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (node.outputs[(size_t) i].name,
                    pp.x - portR - 74, pp.y - 7, 70, 14,
                    juce::Justification::centredRight);
    }
}

void RoutingGraphEditor::RoutingCanvas::drawConnection (juce::Graphics& g, const RoutingConnection& conn) const
{
    auto start = editor.getPortPosition (conn.sourceNodeIdx, true, conn.sourcePortIdx);
    auto end   = editor.getPortPosition (conn.destNodeIdx, false, conn.destPortIdx);

    // Determine colour from source port type
    PortType type = PortType::AudioStereo;
    if (conn.sourceNodeIdx >= 0 && conn.sourceNodeIdx < (int) editor.nodes.size())
    {
        auto& srcNode = editor.nodes[(size_t) conn.sourceNodeIdx];
        if (conn.sourcePortIdx >= 0 && conn.sourcePortIdx < (int) srcNode.outputs.size())
            type = srcNode.outputs[(size_t) conn.sourcePortIdx].type;
    }

    juce::Path path;
    path.startNewSubPath (start);
    float cp = std::abs (end.x - start.x) * 0.5f;
    path.cubicTo (start.x + cp, start.y,
                  end.x - cp, end.y,
                  end.x, end.y);

    g.setColour (editor.getPortColour (type).withAlpha (0.8f));
    g.strokePath (path, juce::PathStrokeType (2.5f));
}

void RoutingGraphEditor::RoutingCanvas::drawWirePreview (juce::Graphics& g) const
{
    auto start = editor.getPortPosition (wireStart.nodeIdx, wireStart.isOutput, wireStart.portIdx);
    auto end = screenToCanvas (wireEndX, wireEndY);

    juce::Path path;
    path.startNewSubPath (start);
    float cp = std::abs (end.x - start.x) * 0.5f;
    path.cubicTo (start.x + cp, start.y,
                  end.x - cp, end.y,
                  end.x, end.y);

    g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.6f));
    g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

//==============================================================================
// Mouse interaction
//==============================================================================
void RoutingGraphEditor::RoutingCanvas::mouseDown (const juce::MouseEvent& e)
{
    auto cp = screenToCanvas ((float) e.x, (float) e.y);

    if (e.mods.isRightButtonDown())
    {
        int connIdx = hitTestConnection (cp);
        if (connIdx >= 0)
        {
            auto& conn = editor.connections[(size_t) connIdx];
            auto& srcNode = editor.nodes[(size_t) conn.sourceNodeIdx];
            auto& dstNode = editor.nodes[(size_t) conn.destNodeIdx];
            auto& srcPort = srcNode.outputs[(size_t) conn.sourcePortIdx];
            auto& dstPort = dstNode.inputs[(size_t) conn.destPortIdx];

            editor.engine.disconnect (srcNode.engineNodeId, srcPort.engineChannel,
                                      dstNode.engineNodeId, dstPort.engineChannel);
            editor.connections.erase (editor.connections.begin() + connIdx);
            repaint();
        }
        return;
    }

    // Port hit?
    auto port = hitTestPort (cp);
    if (port.nodeIdx >= 0 && port.isOutput)
    {
        draggingWire = true;
        wireStart = port;
        wireEndX = (float) e.x;
        wireEndY = (float) e.y;
        return;
    }

    // Node hit?
    int nodeIdx = hitTestNode (cp);
    if (nodeIdx >= 0)
    {
        auto& n = editor.nodes[(size_t) nodeIdx];
        // Don't allow dragging I/O nodes (keep them fixed at edges)
        if (! n.isIONode)
        {
            draggingNodeIdx = nodeIdx;
            nodeDragOffset = { cp.x - n.x, cp.y - n.y };
        }
        if (onNodeSelected) onNodeSelected (nodeIdx);
        return;
    }

    // Background click — start panning or deselect
    if (e.mods.isMiddleButtonDown() || e.mods.isAltDown())
    {
        dragStartPan = { panX - e.x, panY - e.y };
    }
    else
    {
        if (onNodeSelected) onNodeSelected (-1);
    }
}

void RoutingGraphEditor::RoutingCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingWire)
    {
        wireEndX = (float) e.x;
        wireEndY = (float) e.y;
        repaint();
        return;
    }

    if (draggingNodeIdx >= 0)
    {
        auto cp = screenToCanvas ((float) e.x, (float) e.y);
        auto& n = editor.nodes[(size_t) draggingNodeIdx];
        n.x = snapToGrid (cp.x - nodeDragOffset.x);
        n.y = snapToGrid (cp.y - nodeDragOffset.y);
        repaint();
        return;
    }

    // Panning
    if (e.mods.isMiddleButtonDown() || e.mods.isAltDown())
    {
        panX = dragStartPan.x + e.x;
        panY = dragStartPan.y + e.y;
        repaint();
    }
}

void RoutingGraphEditor::RoutingCanvas::mouseUp (const juce::MouseEvent& e)
{
    if (draggingWire)
    {
        auto cp = screenToCanvas ((float) e.x, (float) e.y);
        auto port = hitTestPort (cp);

        // Valid connection: output → input, same port type, different nodes
        if (port.nodeIdx >= 0 && !port.isOutput && port.nodeIdx != wireStart.nodeIdx)
        {
            auto& srcNode = editor.nodes[(size_t) wireStart.nodeIdx];
            auto& dstNode = editor.nodes[(size_t) port.nodeIdx];
            auto& srcPort = srcNode.outputs[(size_t) wireStart.portIdx];
            auto& dstPort = dstNode.inputs[(size_t) port.portIdx];

            // Types must match
            if (srcPort.type == dstPort.type)
            {
                // Create connection in the engine
                bool connected = editor.engine.connect (
                    srcNode.engineNodeId, srcPort.engineChannel,
                    dstNode.engineNodeId, dstPort.engineChannel);

                if (connected)
                {
                    editor.connections.push_back ({
                        wireStart.nodeIdx, wireStart.portIdx,
                        port.nodeIdx, port.portIdx
                    });
                }
            }
        }

        draggingWire = false;
        repaint();
    }

    draggingNodeIdx = -1;
}

void RoutingGraphEditor::RoutingCanvas::mouseWheelMove (const juce::MouseEvent& e,
                                                         const juce::MouseWheelDetails& w)
{
    float zoomFactor = 1.0f + w.deltaY * 0.1f;
    float newScale = juce::jlimit (0.3f, 3.0f, scale * zoomFactor);

    // Zoom towards cursor
    float cx = (float) e.x;
    float cy = (float) e.y;
    panX = cx - (cx - panX) * (newScale / scale);
    panY = cy - (cy - panY) * (newScale / scale);
    scale = newScale;

    repaint();
}

//==============================================================================
// Properties panel
//==============================================================================
void RoutingGraphEditor::PropertiesPanel::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgMid);

    // Left border
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawVerticalLine (0, 0.0f, (float) getHeight());

    auto area = getLocalBounds().reduced (14);

    if (currentNode == nullptr)
    {
        g.setColour (PedalForgeLookAndFeel::textMuted);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("Select a node\nto see details", area, juce::Justification::centred);
        return;
    }

    // Title
    g.setColour (PedalForgeLookAndFeel::textPrimary);
    g.setFont (juce::FontOptions (16.0f).withStyle ("Bold"));
    g.drawText (currentNode->name, area.removeFromTop (24), juce::Justification::centredLeft);
    area.removeFromTop (8);

    // Type badge
    g.setColour (currentNode->isIONode ? juce::Colour (0xFF4CAF50) : PedalForgeLookAndFeel::accent);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (currentNode->isIONode ? "I/O Endpoint" : "Pedal",
                area.removeFromTop (16), juce::Justification::centredLeft);
    area.removeFromTop (12);

    // Inputs
    if (! currentNode->inputs.empty())
    {
        g.setColour (PedalForgeLookAndFeel::textPrimary);
        g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
        g.drawText ("Inputs", area.removeFromTop (18), juce::Justification::centredLeft);

        g.setFont (juce::FontOptions (11.0f));
        for (const auto& port : currentNode->inputs)
        {
            auto row = area.removeFromTop (16);
            g.setColour (getPortColour (port.type));
            g.fillEllipse (row.getX(), row.getCentreY() - 3.0f, 6, 6);
            g.setColour (PedalForgeLookAndFeel::textSecondary);
            g.drawText (port.name, row.withTrimmedLeft (12), juce::Justification::centredLeft);
        }
        area.removeFromTop (8);
    }

    // Outputs
    if (! currentNode->outputs.empty())
    {
        g.setColour (PedalForgeLookAndFeel::textPrimary);
        g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
        g.drawText ("Outputs", area.removeFromTop (18), juce::Justification::centredLeft);

        g.setFont (juce::FontOptions (11.0f));
        for (const auto& port : currentNode->outputs)
        {
            auto row = area.removeFromTop (16);
            g.setColour (getPortColour (port.type));
            g.fillEllipse (row.getX(), row.getCentreY() - 3.0f, 6, 6);
            g.setColour (PedalForgeLookAndFeel::textSecondary);
            g.drawText (port.name, row.withTrimmedLeft (12), juce::Justification::centredLeft);
        }
    }
}

juce::Colour RoutingGraphEditor::PropertiesPanel::getPortColour (PortType type)
{
    switch (type)
    {
        case PortType::AudioStereo: return juce::Colour (0xFF5B9BD5);
        case PortType::MIDI:        return juce::Colour (0xFFFFD700);
        case PortType::Expression:  return juce::Colour (0xFF4CAF50);
    }
    return juce::Colours::white;
}

void RoutingGraphEditor::PropertiesPanel::showNode (const RoutingNode* node)
{
    currentNode = node;
    repaint();
}

void RoutingGraphEditor::PropertiesPanel::clearSelection()
{
    currentNode = nullptr;
    repaint();
}
