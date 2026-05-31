#include "RoutingGraphEditor.h"
#include "LookAndFeel.h"
#include "../pedals/PedalRegistry.h"
#include "../dsp/GraphPedalProcessor.h"
#include "../dsp/PedalDesign.h"

//==============================================================================
// RoutingGraphEditor
//==============================================================================
RoutingGraphEditor::RoutingGraphEditor (AudioGraphEngine& eng)
    : engine (eng), canvas (*this)
{
    addAndMakeVisible (canvas);

    // Docked "Add" inventory (pedals) on the left — drag a pedal onto the canvas.
    inventoryPanel.setContext (pf::inv::Context::Route);
    addAndMakeVisible (inventoryPanel);

    // Right-side tabbed inspector: Properties (selected node) + Layers (node list).
    routeNodeList.onNodeClicked = [this] (int idx) { selectNode (idx); };
    rightTabs = std::make_unique<juce::TabbedComponent> (juce::TabbedButtonBar::TabsAtTop);
    rightTabs->setTabBarDepth (28);
    rightTabs->addTab ("Properties", PedalForgeLookAndFeel::bgDark, &propertiesPanel, false);
    rightTabs->addTab ("Layers",     PedalForgeLookAndFeel::bgDark, &routeNodeList,   false);
    rightTabs->setOutline (0);
    rightTabs->setCurrentTabIndex (1);  // default to the node list until one is selected
    addAndMakeVisible (*rightTabs);

    canvas.onNodeSelected = [this] (int idx) { selectNode (idx); };

    notesOverlay.setNotes (engine.routeNotes);
    addChildComponent (notesOverlay);
    btnNotes.setClickingTogglesState (true);
    btnNotes.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFFF59E0B)); // amber
    btnNotes.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    btnNotes.setToggleState (NotesOverlay::globallyVisible, juce::dontSendNotification);
    addAndMakeVisible (btnNotes);
    btnNotes.setTooltip ("Toggle Notes");
    btnNotes.onClick = [this] {
        NotesOverlay::globallyVisible = btnNotes.getToggleState();
        bool show = NotesOverlay::globallyVisible;
        notesOverlay.setVisible (show);
        if (show && engine.routeNotes.empty())
            notesOverlay.addNote (120, 80);
    };

    engine.refreshHardwareMidiDevices();
    syncFromEngine();
    startTimer (500);
}

RoutingGraphEditor::~RoutingGraphEditor() = default;

void RoutingGraphEditor::paint (juce::Graphics& g)
{
    auto toolbarArea = getLocalBounds().removeFromTop (36);
    g.setGradientFill (juce::ColourGradient (
        PedalForgeLookAndFeel::bgMid.darker (0.1f), 0, (float)toolbarArea.getY(),
        PedalForgeLookAndFeel::bgMid.darker (0.35f), 0, (float)toolbarArea.getBottom(), false));
    g.fillRect (toolbarArea);
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawHorizontalLine (35, 0.0f, (float)getWidth());
}

void RoutingGraphEditor::resized()
{
    auto area = getLocalBounds();
    auto toolbar = area.removeFromTop (36);
    toolbar.reduce (8, 4);
    btnNotes.setBounds (toolbar.removeFromLeft (60).withSizeKeepingCentre (60, 24));

    if (rightTabs) rightTabs->setBounds (area.removeFromRight (propertiesWidth));
    inventoryPanel.setBounds (area.removeFromLeft (210));
    canvas.setBounds (area);
    notesOverlay.setBounds (area);
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
    notesOverlay.repaint();

    // -- I/O endpoint nodes --------------------------------------------------
    {
        RoutingNode audioIn;
        audioIn.engineNodeId = engine.getAudioInputNodeID();
        audioIn.name = "Audio In";
        audioIn.x = engine.audioInRouteX;
        audioIn.y = engine.audioInRouteY;
        audioIn.isIONode = true;
        
        auto* inNode = engine.getGraph().getNodeForId(audioIn.engineNodeId);
        int numInChannels = inNode ? inNode->getProcessor()->getTotalNumOutputChannels() : 2;
        for (int i = 0; i < numInChannels; ++i)
        {
            juce::String label = "Ch " + juce::String (i + 1);
            audioIn.outputs.push_back ({ label, PortType::AudioStereo, true, i });
        }
        nodes.push_back (std::move (audioIn));
    }
    {
        RoutingNode audioOut;
        audioOut.engineNodeId = engine.getAudioOutputNodeID();
        audioOut.name = "Audio Out";
        audioOut.x = engine.audioOutRouteX;
        audioOut.y = engine.audioOutRouteY;
        audioOut.isIONode = true;
        
        auto* outNode = engine.getGraph().getNodeForId(audioOut.engineNodeId);
        int numOutChannels = outNode ? outNode->getProcessor()->getTotalNumInputChannels() : 2;
        for (int i = 0; i < numOutChannels; ++i)
        {
            juce::String label = "Ch " + juce::String (i + 1);
            audioOut.inputs.push_back ({ label, PortType::AudioStereo, false, i });
        }
        nodes.push_back (std::move (audioOut));
    }

    {
        RoutingNode midiIn;
        midiIn.engineNodeId = engine.getMidiInputNodeID();
        midiIn.name = "MIDI In";
        midiIn.x = engine.midiInRouteX;
        midiIn.y = engine.midiInRouteY;
        midiIn.isIONode = true;
        midiIn.outputs.push_back ({ "MIDI Out", PortType::MIDI, true, juce::AudioProcessorGraph::midiChannelIndex });
        nodes.push_back (std::move (midiIn));
    }
    {
        RoutingNode midiOut;
        midiOut.engineNodeId = engine.getMidiOutputNodeID();
        midiOut.name = "MIDI Out";
        midiOut.x = engine.midiOutRouteX;
        midiOut.y = engine.midiOutRouteY;
        midiOut.isIONode = true;
        midiOut.inputs.push_back ({ "MIDI In", PortType::MIDI, false, juce::AudioProcessorGraph::midiChannelIndex });
        nodes.push_back (std::move (midiOut));
    }

    // -- Pedal nodes ---------------------------------------------------------
    float nx = 300, ny = 100;
    for (const auto& inst : engine.getPedalInstances())
    {
        RoutingNode node;
        node.engineNodeId = inst.nodeID;
        node.name = inst.name;
        node.isIONode = false;

        // Use persisted position if available, otherwise auto-layout
        if (inst.routeX >= 0.0f && inst.routeY >= 0.0f)
        {
            node.x = inst.routeX;
            node.y = inst.routeY;
        }
        else
        {
            node.x = nx;
            node.y = ny;
        }

        auto* pedalNode = engine.getGraph().getNodeForId(inst.nodeID);
        int pInChans = pedalNode ? pedalNode->getProcessor()->getTotalNumInputChannels() : 2;
        int pOutChans = pedalNode ? pedalNode->getProcessor()->getTotalNumOutputChannels() : 2;
        
        for (int i = 0; i < pInChans; ++i)
        {
            juce::String label = (i % 2 == 0) ? "Audio L" : "Audio R";
            if (i >= 2) label += " " + juce::String(i/2 + 1);
            node.inputs.push_back  ({ label, PortType::AudioStereo, false, i });
        }
        for (int i = 0; i < pOutChans; ++i)
        {
            juce::String label = (i % 2 == 0) ? "Audio L" : "Audio R";
            if (i >= 2) label += " " + juce::String(i/2 + 1);
            node.outputs.push_back ({ label, PortType::AudioStereo, true, i });
        }

        // MIDI and Expression ports from the pedal's declared routing ports
        if (inst.design)
        {
            for (const auto& rp : inst.design->routingPorts)
            {
                RoutingPort port;
                port.name = rp.label;
                port.routingPortId = rp.id;

                switch (rp.kind)
                {
                    case PedalDesign::RoutingPort::Kind::MidiIn:
                        port.type = PortType::MIDI;
                        port.isOutput = false;
                        port.engineChannel = juce::AudioProcessorGraph::midiChannelIndex;
                        node.inputs.push_back (port);
                        break;
                    case PedalDesign::RoutingPort::Kind::MidiOut:
                        port.type = PortType::MIDI;
                        port.isOutput = true;
                        port.engineChannel = juce::AudioProcessorGraph::midiChannelIndex;
                        node.outputs.push_back (port);
                        break;
                    case PedalDesign::RoutingPort::Kind::ExpressionIn:
                        port.type = PortType::Expression;
                        port.isOutput = false;
                        node.inputs.push_back (port);
                        break;
                    case PedalDesign::RoutingPort::Kind::ExpressionOut:
                        port.type = PortType::Expression;
                        port.isOutput = true;
                        node.outputs.push_back (port);
                        break;
                }
            }
        }

        nodes.push_back (std::move (node));

        ny += 120;
        if (ny > 500) { ny = 100; nx += 220; }
    }

    // -- Hardware MIDI device nodes ------------------------------------------
    for (auto& dev : engine.getHardwareMidiDevices())
    {
        RoutingNode node;
        node.name      = dev.deviceName + (dev.isInput ? " (MIDI In)" : " (MIDI Out)");
        node.x         = dev.routeX;
        node.y         = dev.routeY;
        node.isIONode  = true;
        node.isHwMidi  = true;
        node.hwMidiName  = dev.deviceName;
        node.hwMidiIsInput = dev.isInput;
        node.engineNodeId = dev.engineNodeId;

        if (dev.isInput)
            node.outputs.push_back ({ "MIDI Out", PortType::MIDI, true, juce::AudioProcessorGraph::midiChannelIndex, "hw_midi_out" });
        else
            node.inputs.push_back  ({ "MIDI In",  PortType::MIDI, false, juce::AudioProcessorGraph::midiChannelIndex, "hw_midi_in"  });

        nodes.push_back (std::move (node));
    }

    // -- Board Config nodes ------------------------------------------
    for (auto& board : engine.getBoards())
    {
        RoutingNode node;
        node.name      = board.name + " (Pedalboard)";
        node.x         = board.routeX;
        node.y         = board.routeY;
        node.isIONode  = false;
        node.isHwMidi  = false;
        node.engineNodeId.uid = board.engineNodeId;
        
        // Boards only have a MIDI input to receive page turns / mute commands
        node.inputs.push_back ({ "MIDI In", PortType::MIDI, false, juce::AudioProcessorGraph::midiChannelIndex, "board_midi_in" });
        
        nodes.push_back (std::move (node));
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

    // -- Board-level (MIDI / Expression) connections -------------------------
    for (const auto& brc : engine.getBoardConnections())
    {
        int srcIdx = findNodeByEngineId (brc.srcNodeId);
        int dstIdx = findNodeByEngineId (brc.dstNodeId);
        if (srcIdx < 0 || dstIdx < 0) continue;

        auto& srcNode = nodes[(size_t) srcIdx];
        auto& dstNode = nodes[(size_t) dstIdx];

        int srcPort = -1, dstPort = -1;
        for (int i = 0; i < (int) srcNode.outputs.size(); ++i)
            if (srcNode.outputs[(size_t) i].routingPortId == brc.srcPortId)
            { srcPort = i; break; }

        for (int i = 0; i < (int) dstNode.inputs.size(); ++i)
            if (dstNode.inputs[(size_t) i].routingPortId == brc.dstPortId)
            { dstPort = i; break; }

        if (srcPort >= 0 && dstPort >= 0)
            connections.push_back ({ srcIdx, srcPort, dstIdx, dstPort });
    }

    routeNodeList.refresh (nodes);
    canvas.repaint();
}

void RoutingGraphEditor::timerCallback()
{
    if (! isShowing()) return;

    auto* inNode = engine.getGraph().getNodeForId(engine.getAudioInputNodeID());
    auto* outNode = engine.getGraph().getNodeForId(engine.getAudioOutputNodeID());
    
    int currentInChannels = inNode ? inNode->getProcessor()->getTotalNumOutputChannels() : 2;
    int currentOutChannels = outNode ? outNode->getProcessor()->getTotalNumInputChannels() : 2;
    
    bool audioChannelsChanged = (currentInChannels != lastKnownInputChannels || currentOutChannels != lastKnownOutputChannels);
    bool midiDevicesChanged = engine.refreshHardwareMidiDevices();

    if (audioChannelsChanged || midiDevicesChanged)
    {
        lastKnownInputChannels = currentInChannels;
        lastKnownOutputChannels = currentOutChannels;
        syncFromEngine();
    }

    // --- Signal Flow Animation ---
    bool needsRepaint = false;

    // Decay connection glow
    for (auto& conn : connections)
    {
        if (conn.glowLevel > 0.0f)
        {
            conn.glowLevel = std::max(0.0f, conn.glowLevel - 0.15f);
            needsRepaint = true;
        }
    }

    // Decay port glow
    for (auto& node : nodes)
    {
        for (auto& p : node.inputs)  if (p.glowLevel > 0.0f) { p.glowLevel = std::max(0.0f, p.glowLevel - 0.15f); needsRepaint = true; }
        for (auto& p : node.outputs) if (p.glowLevel > 0.0f) { p.glowLevel = std::max(0.0f, p.glowLevel - 0.15f); needsRepaint = true; }
    }

    // Read atomics for outputs and apply new glow
    for (int nIdx = 0; nIdx < (int)nodes.size(); ++nIdx)
    {
        auto& node = nodes[nIdx];
        
        float audioLevels[2] {0.0f, 0.0f};
        bool midiActive = false;

        if (node.isIONode && node.name == "Audio In")
        {
            audioLevels[0] = engine.mainInRMS[0].load();
            audioLevels[1] = engine.mainInRMS[1].load();
        }
        else if (node.isIONode && node.isHwMidi && node.hwMidiIsInput)
        {
            for (auto& hw : engine.getHardwareMidiDevices())
                if (hw.deviceName == node.hwMidiName && hw.isInput && hw.activity->exchange(false))
                    midiActive = true;
        }
        else if (!node.isIONode && !node.isHwMidi)
        {
            if (auto* inst = engine.getPedalInstance(node.engineNodeId))
            {
                if (inst->meters)
                {
                    audioLevels[0] = inst->meters->outRMS[0].load();
                    audioLevels[1] = inst->meters->outRMS[1].load();
                    if (inst->meters->midiOut.exchange(false))
                        midiActive = true;
                }
            }
        }

        // Apply audio levels to audio output ports
        for (auto& port : node.outputs)
        {
            float targetLevel = 0.0f;
            if (port.type == PortType::AudioStereo)
                targetLevel = std::min(1.0f, audioLevels[port.engineChannel < 2 ? port.engineChannel : 0] * 3.0f);
            else if (port.type == PortType::MIDI && midiActive)
                targetLevel = 1.0f;

            if (targetLevel > port.glowLevel)
            {
                port.glowLevel = targetLevel;
                needsRepaint = true;
            }
        }
    }

    // Transfer port glow to connections
    for (auto& conn : connections)
    {
        if (conn.sourceNodeIdx >= 0 && conn.sourceNodeIdx < (int)nodes.size())
        {
            auto& srcNode = nodes[conn.sourceNodeIdx];
            if (conn.sourcePortIdx >= 0 && conn.sourcePortIdx < (int)srcNode.outputs.size())
            {
                float srcGlow = srcNode.outputs[conn.sourcePortIdx].glowLevel;
                if (srcGlow > conn.glowLevel)
                {
                    conn.glowLevel = srcGlow;
                    needsRepaint = true;
                }
            }
        }
    }

    if (needsRepaint)
        canvas.repaint();
}

void RoutingGraphEditor::visibilityChanged()
{
    if (isVisible())
    {
        btnNotes.setToggleState (NotesOverlay::globallyVisible, juce::dontSendNotification);
        notesOverlay.setVisible (!engine.routeNotes.empty());
        routeNodeList.refresh (nodes);
    }
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
        if (rightTabs) rightTabs->setCurrentTabIndex (0); // jump to Properties

        if (onPedalSelected)
        {
            if (!nodes[(size_t) idx].isIONode)
                onPedalSelected (engine.getPedalInstance (nodes[(size_t) idx].engineNodeId));
            else
                onPedalSelected (nullptr);
        }
    }
    else
    {
        propertiesPanel.clearSelection();
        if (onPedalSelected)
            onPedalSelected (nullptr);
    }

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
        auto s = editor.getPortPosition (conn.sourceNodeIdx, true, conn.sourcePortIdx);
        auto d = editor.getPortPosition (conn.destNodeIdx, false, conn.destPortIdx);
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
    for (int i = 0; i < (int) editor.connections.size(); ++i)
        drawConnection (g, editor.connections[(size_t) i], hoveredConnectionIndex == i);

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
        auto& port = node.inputs[(size_t) i];
        juce::Colour portColour = editor.getPortColour (port.type);
        
        if (port.glowLevel > 0.01f)
        {
            g.setColour (portColour.withAlpha (port.glowLevel * 0.5f));
            g.fillEllipse (pp.x - portR - port.glowLevel * 3.0f, pp.y - portR - port.glowLevel * 3.0f, 
                           (portR + port.glowLevel * 3.0f) * 2, (portR + port.glowLevel * 3.0f) * 2);
            g.setColour (portColour.brighter (port.glowLevel * 0.8f));
        }
        else
            g.setColour (portColour);
            
        g.fillEllipse (pp.x - portR, pp.y - portR, portR * 2, portR * 2);

        g.setColour (PedalForgeLookAndFeel::textSecondary);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (port.name,
                    pp.x + portR + 4, pp.y - 7, 70, 14,
                    juce::Justification::centredLeft);
    }

    // Output ports (right side)
    for (int i = 0; i < (int) node.outputs.size(); ++i)
    {
        auto pp = editor.getPortPosition (idx, true, i);
        auto& port = node.outputs[(size_t) i];
        juce::Colour portColour = editor.getPortColour (port.type);
        
        if (port.glowLevel > 0.01f)
        {
            g.setColour (portColour.withAlpha (port.glowLevel * 0.5f));
            g.fillEllipse (pp.x - portR - port.glowLevel * 3.0f, pp.y - portR - port.glowLevel * 3.0f, 
                           (portR + port.glowLevel * 3.0f) * 2, (portR + port.glowLevel * 3.0f) * 2);
            g.setColour (portColour.brighter (port.glowLevel * 0.8f));
        }
        else
            g.setColour (portColour);
            
        g.fillEllipse (pp.x - portR, pp.y - portR, portR * 2, portR * 2);

        g.setColour (PedalForgeLookAndFeel::textSecondary);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (port.name,
                    pp.x - portR - 74, pp.y - 7, 70, 14,
                    juce::Justification::centredRight);
    }
}

void RoutingGraphEditor::RoutingCanvas::drawConnection (juce::Graphics& g, const RoutingConnection& conn, bool highlighted) const
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

    juce::Colour baseColour = highlighted ? juce::Colour (0xFFFF6B6B) : editor.getPortColour (type);
    
    if (highlighted)
    {
        // Draw glow halo
        g.setColour (baseColour.withAlpha (0.4f));
        g.strokePath (path, juce::PathStrokeType (5.5f));
        
        // Draw coral red highlighted wire
        g.setColour (baseColour.withAlpha (0.9f));
        g.strokePath (path, juce::PathStrokeType (3.5f));
    }
    else if (conn.glowLevel > 0.01f)
    {
        // Draw glow halo
        g.setColour (baseColour.withAlpha (conn.glowLevel * 0.5f));
        g.strokePath (path, juce::PathStrokeType (2.5f + conn.glowLevel * 6.0f));
        
        // Draw bright core
        g.setColour (baseColour.brighter (conn.glowLevel * 0.8f).withAlpha (0.8f + conn.glowLevel * 0.2f));
        g.strokePath (path, juce::PathStrokeType (2.5f + conn.glowLevel * 1.5f));
    }
    else
    {
        g.setColour (baseColour.withAlpha (0.8f));
        g.strokePath (path, juce::PathStrokeType (2.5f));
    }
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

    // 1. Port click first! If they clicked a port, they want to drag/connect a wire
    auto port = hitTestPort (cp);
    if (port.nodeIdx >= 0 && port.isOutput)
    {
        draggingWire = true;
        wireStart = port;
        wireEndX = (float) e.x;
        wireEndY = (float) e.y;
        return;
    }

    // 2. Connection click second! Only check for connection deletion if they didn't click a port
    int connIdx = hitTestConnection (cp);
    if (connIdx >= 0)
    {
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            menu.addItem (1, "Disconnect / Delete Wire");
            int choice = menu.show();
            if (choice == 1)
            {
                auto& conn = editor.connections[(size_t) connIdx];
                auto& srcNode = editor.nodes[(size_t) conn.sourceNodeIdx];
                auto& dstNode = editor.nodes[(size_t) conn.destNodeIdx];
                auto& srcPort = srcNode.outputs[(size_t) conn.sourcePortIdx];
                auto& dstPort = dstNode.inputs[(size_t) conn.destPortIdx];

                if (srcPort.type == PortType::AudioStereo || srcPort.type == PortType::MIDI)
                {
                    editor.engine.disconnect (srcNode.engineNodeId, srcPort.engineChannel,
                                              dstNode.engineNodeId, dstPort.engineChannel);
                }
                else
                {
                    editor.engine.removeBoardConnection (srcNode.engineNodeId, srcPort.routingPortId,
                                                         dstNode.engineNodeId, dstPort.routingPortId);
                }
                
                editor.connections.erase (editor.connections.begin() + connIdx);
                hoveredConnectionIndex = -1;
                setMouseCursor (juce::MouseCursor::NormalCursor);
                repaint();
                editor.engine.saveUndoState();
            }
        }
        else
        {
            auto& conn = editor.connections[(size_t) connIdx];
            auto& srcNode = editor.nodes[(size_t) conn.sourceNodeIdx];
            auto& dstNode = editor.nodes[(size_t) conn.destNodeIdx];
            auto& srcPort = srcNode.outputs[(size_t) conn.sourcePortIdx];
            auto& dstPort = dstNode.inputs[(size_t) conn.destPortIdx];

            if (srcPort.type == PortType::AudioStereo || srcPort.type == PortType::MIDI)
            {
                editor.engine.disconnect (srcNode.engineNodeId, srcPort.engineChannel,
                                          dstNode.engineNodeId, dstPort.engineChannel);
            }
            else
            {
                editor.engine.removeBoardConnection (srcNode.engineNodeId, srcPort.routingPortId,
                                                     dstNode.engineNodeId, dstPort.routingPortId);
            }
            
            editor.connections.erase (editor.connections.begin() + connIdx);
            hoveredConnectionIndex = -1;
            setMouseCursor (juce::MouseCursor::NormalCursor);
            repaint();
            editor.engine.saveUndoState();
        }
        return;
    }

    if (e.mods.isRightButtonDown())
    {
        // Right-click on a pedal node → context menu
        int nodeIdx = hitTestNode (cp);
        if (nodeIdx >= 0 && ! editor.nodes[(size_t) nodeIdx].isIONode)
        {
            auto nodeId = editor.nodes[(size_t) nodeIdx].engineNodeId;
            juce::PopupMenu menu;
            menu.addItem (1, "Delete");
            menu.showMenuAsync (juce::PopupMenu::Options(),
                                [this, nodeId] (int result)
                                {
                                    if (result == 1)
                                    {
                                        editor.engine.removePedal (nodeId);
                                        editor.syncFromEngine();
                                        editor.engine.saveUndoState();
                                    }
                                });
        }
        return;
    }

    // Node hit?
    int nodeIdx = hitTestNode (cp);
    if (nodeIdx >= 0)
    {
        auto& n = editor.nodes[(size_t) nodeIdx];
        draggingNodeIdx = nodeIdx;
        nodeDragOffset = { cp.x - n.x, cp.y - n.y };
        if (onNodeSelected) onNodeSelected (nodeIdx);
        return;
    }

    // Background click — start panning and/or deselect
    isPanning = true;
    dragStartPan = { panX - e.x, panY - e.y };
    if (onNodeSelected) onNodeSelected (-1);
}

void RoutingGraphEditor::RoutingCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (hoveredConnectionIndex != -1)
    {
        hoveredConnectionIndex = -1;
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

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
    if (isPanning)
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
                if (srcPort.type == PortType::AudioStereo || srcPort.type == PortType::MIDI)
                {
                    // Audio and MIDI connections — routed natively by AudioProcessorGraph
                    bool connected = editor.engine.connect (
                        srcNode.engineNodeId, srcPort.engineChannel,
                        dstNode.engineNodeId, dstPort.engineChannel);

                    if (connected)
                    {
                        editor.connections.push_back ({
                            wireStart.nodeIdx, wireStart.portIdx,
                            port.nodeIdx, port.portIdx
                        });
                        editor.engine.saveUndoState();
                    }
                }
                else
                {
                    // MIDI or Expression — board-level connection (not in AudioProcessorGraph)
                    AudioGraphEngine::BoardRoutingConnection brc;
                    brc.srcNodeId  = srcNode.engineNodeId;
                    brc.srcPortId  = srcPort.routingPortId;
                    brc.dstNodeId  = dstNode.engineNodeId;
                    brc.dstPortId  = dstPort.routingPortId;
                    editor.engine.addBoardConnection (brc);

                    editor.connections.push_back ({
                        wireStart.nodeIdx, wireStart.portIdx,
                        port.nodeIdx, port.portIdx
                    });
                    editor.engine.saveUndoState();
                }
            }
        }

        draggingWire = false;
        isPanning = false;
        repaint();
    }

    if (draggingNodeIdx >= 0)
    {
        // Persist the new position back to the engine / PedalInstance
        auto& n = editor.nodes[(size_t) draggingNodeIdx];
        if (n.engineNodeId == editor.engine.getAudioInputNodeID())
        {
            editor.engine.audioInRouteX = n.x;
            editor.engine.audioInRouteY = n.y;
        }
        else if (n.engineNodeId == editor.engine.getAudioOutputNodeID())
        {
            editor.engine.audioOutRouteX = n.x;
            editor.engine.audioOutRouteY = n.y;
        }
        else if (n.isHwMidi)
        {
            for (auto& dev : editor.engine.getHardwareMidiDevices())
            {
                if (dev.engineNodeId == n.engineNodeId)
                {
                    dev.routeX = n.x;
                    dev.routeY = n.y;
                    break;
                }
            }
        }
        else if (!n.isIONode && !n.isHwMidi && n.name.endsWith(" (Pedalboard)"))
        {
            for (auto& b : editor.engine.getBoards())
            {
                if (b.engineNodeId == n.engineNodeId.uid)
                {
                    b.routeX = n.x;
                    b.routeY = n.y;
                    break;
                }
            }
        }
        else if (auto* inst = editor.engine.getPedalInstance (n.engineNodeId))
        {
            inst->routeX = n.x;
            inst->routeY = n.y;
        }
        editor.engine.saveUndoState();
    }

    draggingNodeIdx = -1;
    isPanning = false;
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

void RoutingGraphEditor::RoutingCanvas::mouseMove (const juce::MouseEvent& e)
{
    auto cp = screenToCanvas ((float) e.x, (float) e.y);
    int newHoveredIdx = hitTestConnection (cp);

    // Disable connection hover highlight if mouse is currently over a node or port
    if (hitTestPort (cp).nodeIdx >= 0 || hitTestNode (cp) >= 0)
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

void RoutingGraphEditor::RoutingCanvas::mouseExit (const juce::MouseEvent&)
{
    if (hoveredConnectionIndex != -1)
    {
        hoveredConnectionIndex = -1;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }
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

//==============================================================================
// Drag and Drop support — accept pedal drops from the inventory
//==============================================================================
bool RoutingGraphEditor::RoutingCanvas::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.toString().startsWith ("pedal:");
}

void RoutingGraphEditor::RoutingCanvas::itemDropped (const SourceDetails& details)
{
    auto desc = details.description.toString();
    auto parts = juce::StringArray::fromTokens (desc, ":", "");
    if (parts.size() < 2 || parts[0] != "pedal") return;

    auto pedalName = parts[1];
    auto cp = screenToCanvas ((float)details.localPosition.x, (float)details.localPosition.y);
    editor.addPedalToRoute (pedalName, cp.x, cp.y);
}

void RoutingGraphEditor::addPedalToRoute (const juce::String& pedalName, float canvasX, float canvasY)
{
    for (auto& info : getFactoryPedals())
    {
        if (info.name == pedalName)
        {
            auto processor = info.factory();
            auto nodeId = engine.addPedalOffBoard (std::move (processor));

            if (auto* inst = engine.getPedalInstance (nodeId))
            {
                inst->colour    = info.colour;
                inst->category  = info.category;
                inst->numKnobs  = info.numKnobs;
                inst->boardW    = info.gridW * 100.0f;
                inst->boardH    = info.gridH * 100.0f;
                inst->routeX    = snapToGrid (canvasX);
                inst->routeY    = snapToGrid (canvasY);

                if (info.designFactory)
                {
                    inst->design = info.designFactory();
                    if (auto* proc = dynamic_cast<GraphPedalProcessor*>(engine.getGraph().getNodeForId(nodeId)->getProcessor()))
                        inst->design->effectsGraph = juce::JSON::parse (proc->saveGraph());
                }
                else
                {
                    inst->design = std::make_shared<PedalDesign>();
                    inst->design->name     = inst->name;
                    inst->design->category = inst->category;
                    inst->design->chassisColour = inst->colour;

                    if (auto* proc = dynamic_cast<GraphPedalProcessor*>(engine.getGraph().getNodeForId(nodeId)->getProcessor()))
                    {
                        inst->design->effectsGraph = juce::JSON::parse (proc->saveGraph());

                        float x = 20, y = 40;
                        for (auto* param : proc->getParameters())
                        {
                            if (auto* pf = dynamic_cast<juce::AudioParameterFloat*> (param))
                            {
                                PedalDesign::Control ctrl;
                                ctrl.type      = "knob";
                                ctrl.label     = pf->name;
                                ctrl.controlID = "knob_" + juce::String (inst->design->controls.size() + 1);
                                ctrl.x = x;  ctrl.y = y;
                                ctrl.width = 40;  ctrl.height = 40;
                                inst->design->controls.push_back (ctrl);

                                PedalDesign::Mapping m;
                                m.controlID = ctrl.controlID;
                                m.nodeParam = pf->paramID;
                                inst->design->mappings.push_back (m);

                                x += 50;
                                if (x > 150) { x = 20; y += 60; }
                            }
                        }
                    }
                }
            }

            syncFromEngine();
            return;
        }
    }
}

//==============================================================================
// RouteNodeList — the "Layers" outliner: one clickable row per routing node.
//==============================================================================
struct RoutingGraphEditor::RouteNodeList::Row : public juce::Component
{
    Row (int i, juce::String nm, juce::String ty)
        : idx (i), name (std::move (nm)), type (std::move (ty)) {}

    int idx;
    juce::String name, type;
    std::function<void (int)> onClick;
    bool hovered = false;

    void paint (juce::Graphics& g) override
    {
        if (hovered)
        {
            g.setColour (PedalForgeLookAndFeel::accent.withAlpha (0.12f));
            g.fillRect (getLocalBounds());
        }
        auto b = getLocalBounds().reduced (8, 2);
        g.setColour (PedalForgeLookAndFeel::textPrimary);
        g.setFont (juce::FontOptions (12.5f).withStyle ("Bold"));
        g.drawText (name, b.removeFromTop (b.getHeight() / 2), juce::Justification::bottomLeft, true);
        g.setColour (PedalForgeLookAndFeel::textMuted);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (type, b, juce::Justification::topLeft, true);
        g.setColour (PedalForgeLookAndFeel::gridLine);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void mouseEnter (const juce::MouseEvent&) override { hovered = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick (idx); }
};

RoutingGraphEditor::RouteNodeList::RouteNodeList()
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
}

void RoutingGraphEditor::RouteNodeList::resized()
{
    viewport.setBounds (getLocalBounds());
    const int w = viewport.getMaximumVisibleWidth();
    int y = 0;
    for (auto* r : rows) { r->setBounds (0, y, w, 36); y += 36; }
    content.setSize (w, y);
}

void RoutingGraphEditor::RouteNodeList::refresh (const std::vector<RoutingNode>& nodeList)
{
    rows.clear();
    content.removeAllChildren();
    for (int i = 0; i < (int) nodeList.size(); ++i)
    {
        const auto& n = nodeList[(size_t) i];
        const juce::String type = n.isHwMidi ? "MIDI Device" : (n.isIONode ? "I/O" : "Pedal");
        auto* row = new Row (i, n.name, type);
        row->onClick = [this] (int idx) { if (onNodeClicked) onNodeClicked (idx); };
        rows.add (row);
        content.addAndMakeVisible (row);
    }
    resized();
    repaint();
}
