#include "AudioGraphEngine.h"
#include "../dsp/GraphPedalProcessor.h"
#include "MidiRoutingNodes.h"
#include <juce_audio_devices/juce_audio_devices.h>

//==============================================================================
AudioGraphEngine::AudioGraphEngine()
    : graph()
{
    // Configure the graph's bus layout to match our stereo I/O
    graph.enableAllBuses();

    // Create default board
    BoardConfig mainBoard;
    mainBoard.id = "main";
    mainBoard.name = "Main Board";
    mainBoard.engineNodeId = 0x424F0000 + (uint32_t)(mainBoard.id.hashCode() % 0xFFFF);
    boards.push_back (mainBoard);
    
    // Add board node to the graph
    graph.addNode (std::make_unique<BoardMidiReceiverNode>(*this, mainBoard.id), NodeID { mainBoard.engineNodeId });
}

AudioGraphEngine::~AudioGraphEngine()
{
    graph.clear();
}

//==============================================================================
BoardConfig* AudioGraphEngine::getBoard (const juce::String& boardId)
{
    for (auto& b : boards)
        if (b.id == boardId)
            return &b;
    return nullptr;
}

void AudioGraphEngine::addBoard (const BoardConfig& boardIn)
{
    BoardConfig board = boardIn;
    if (board.engineNodeId == 0)
        board.engineNodeId = 0x424F0000 + (uint32_t)(board.id.hashCode() % 0xFFFF);
    boards.push_back (board);
    
    graph.addNode (std::make_unique<BoardMidiReceiverNode>(*this, board.id), NodeID { board.engineNodeId });
}

void AudioGraphEngine::removeBoard (const juce::String& boardId)
{
    auto it = std::find_if (boards.begin(), boards.end(),
                            [&] (const BoardConfig& b) { return b.id == boardId; });
    if (it != boards.end())
    {
        graph.removeNode (NodeID { it->engineNodeId });
        boards.erase (it);
    }
}

//==============================================================================
void AudioGraphEngine::prepare (double sampleRate, int samplesPerBlock,
                                 int numInputChannels, int numOutputChannels)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;
    currentNumChannels = numInputChannels;

    // Set the graph's bus layout to match the host using discrete channels
    juce::AudioProcessor::BusesLayout layout;
    if (numInputChannels > 0)
        layout.inputBuses.add (juce::AudioChannelSet::discreteChannels (numInputChannels));
    
    if (numOutputChannels > 0)
        layout.outputBuses.add (juce::AudioChannelSet::discreteChannels (numOutputChannels));

    graph.setBusesLayout (layout);
    graph.setRateAndBufferSizeDetails (sampleRate, samplesPerBlock);
    graph.prepareToPlay (sampleRate, samplesPerBlock);

    setupIONodes();
}

void BoardMidiReceiverNode::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer& midiMessages)
{
    if (auto* config = engine.getBoard (boardId))
    {
        for (const auto meta : midiMessages)
        {
            auto msg = meta.getMessage();
            if (msg.isController())
            {
                int cc = msg.getControllerNumber();
                int val = msg.getControllerValue();
                
                if (val > 64) // button pressed
                {
                    if (config->prevPageCC != -1 && cc == config->prevPageCC)
                    {
                        if (config->activePage > 0)
                            config->activePage--;
                    }
                    else if (config->nextPageCC != -1 && cc == config->nextPageCC)
                    {
                        if (config->activePage < config->numPages - 1)
                            config->activePage++;
                    }
                }
            }
        }
    }
    midiMessages.clear(); // Consume messages so they don't propagate further
}

void AudioGraphEngine::releaseResources()
{
    graph.releaseResources();
}

void AudioGraphEngine::processBlock (juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midi)
{
    int numSamples = buffer.getNumSamples();
    if (numSamples > 0)
    {
        mainInRMS[0].store (buffer.getRMSLevel(0, 0, numSamples));
        if (buffer.getNumChannels() > 1)
            mainInRMS[1].store (buffer.getRMSLevel(1, 0, numSamples));
    }
    if (!midi.isEmpty())
        mainMidiIn.store (true);

    for (const auto meta : midi)
    {
        auto msg = meta.getMessage();
        if (msg.isController())
        {
            int cc = msg.getControllerNumber();
            int val = msg.getControllerValue();
            
            if (val > 64) // button pressed
            {
                if (appMidiConfig.turingPrevCC != -1 && cc == appMidiConfig.turingPrevCC)
                    cycleTuringPedal(-1);
                else if (appMidiConfig.turingNextCC != -1 && cc == appMidiConfig.turingNextCC)
                    cycleTuringPedal(1);

                // Global page navigation
                if (appMidiConfig.pageLeftCC != -1 && cc == appMidiConfig.pageLeftCC)
                    cyclePage(-1);
                else if (appMidiConfig.pageRightCC != -1 && cc == appMidiConfig.pageRightCC)
                    cyclePage(1);

                // Global track navigation (pedal selection)
                if (appMidiConfig.trackLeftCC != -1 && cc == appMidiConfig.trackLeftCC)
                    cycleTrack(-1);
                else if (appMidiConfig.trackRightCC != -1 && cc == appMidiConfig.trackRightCC)
                    cycleTrack(1);
                    
                // Check board-specific CCs
                for (auto& board : boards)
                {
                    if (board.prevPageCC != -1 && cc == board.prevPageCC)
                    {
                        if (board.activePage > 0)
                            board.activePage--;
                    }
                    else if (board.nextPageCC != -1 && cc == board.nextPageCC)
                    {
                        if (board.activePage < board.numPages - 1)
                            board.activePage++;
                    }
                }
            }
        }
    }
    graph.processBlock (buffer, midi);
    
    if (numSamples > 0)
    {
        mainOutRMS[0].store (buffer.getRMSLevel(0, 0, numSamples));
        if (buffer.getNumChannels() > 1)
            mainOutRMS[1].store (buffer.getRMSLevel(1, 0, numSamples));
    }
}

void AudioGraphEngine::cycleTuringPedal(int dir)
{
    for (auto& board : boards)
    {
        if (board.assignToTuring)
        {
            std::vector<PedalInstance*> boardPedals;
            for (auto& inst : instances)
                if (inst.onBoard && inst.boardId == board.id)
                    boardPedals.push_back(&inst);
            
            if (boardPedals.empty()) continue;
            
            // Sort by page, then Y, then X
            std::sort(boardPedals.begin(), boardPedals.end(), [](PedalInstance* a, PedalInstance* b) {
                if (a->pageIndex != b->pageIndex) return a->pageIndex < b->pageIndex;
                if (a->boardY != b->boardY) return a->boardY < b->boardY;
                return a->boardX < b->boardX;
            });
            
            board.turingPedalIndex = (board.turingPedalIndex + dir) % (int)boardPedals.size();
            if (board.turingPedalIndex < 0) board.turingPedalIndex += (int)boardPedals.size();
        }
    }
}

//==============================================================================
void AudioGraphEngine::cyclePage (int dir)
{
    for (auto& board : boards)
    {
        int newPage = board.activePage + dir;
        if (newPage >= 0 && newPage < board.numPages)
            board.activePage = newPage;
    }
}

void AudioGraphEngine::cycleTrack (int dir)
{
    // Collect pedals on the active page of the first board
    if (boards.empty()) return;
    auto& board = boards.front();

    std::vector<PedalInstance*> pagePedals;
    for (auto& inst : instances)
    {
        if (inst.onBoard && inst.boardId == board.id && inst.pageIndex == board.activePage)
            pagePedals.push_back (&inst);
    }

    if (pagePedals.empty()) return;

    // Sort left-to-right, top-to-bottom
    std::sort (pagePedals.begin(), pagePedals.end(),
               [] (PedalInstance* a, PedalInstance* b) {
                   if (std::abs (a->boardY - b->boardY) > 50.0f) return a->boardY < b->boardY;
                   return a->boardX < b->boardX;
               });

    // Find the currently focused pedal in this list
    auto focusedId = getFocusedPedal();
    int currentIdx = -1;
    for (int i = 0; i < (int) pagePedals.size(); ++i)
    {
        if (pagePedals[(size_t) i]->nodeID == focusedId)
        {
            currentIdx = i;
            break;
        }
    }

    // Cycle to next/prev
    int newIdx;
    if (currentIdx < 0)
        newIdx = (dir > 0) ? 0 : (int) pagePedals.size() - 1;
    else
    {
        newIdx = (currentIdx + dir) % (int) pagePedals.size();
        if (newIdx < 0) newIdx += (int) pagePedals.size();
    }

    setFocusedPedal (pagePedals[(size_t) newIdx]->nodeID);
}

//==============================================================================
void AudioGraphEngine::setupIONodes()
{
    // Don't clear the graph if it already has IO nodes!
    if (graph.getNodeForId (audioInputNodeID) != nullptr)
        return; // already setup

    // Clear existing graph only if we are initializing
    graph.clear();
    instances.clear();

    // Create audio I/O nodes
    auto audioInput = graph.addNode (
        std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));

    auto audioOutput = graph.addNode (
        std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));

    auto midiInput = graph.addNode (
        std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
            juce::AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode));

    auto midiOutput = graph.addNode (
        std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
            juce::AudioProcessorGraph::AudioGraphIOProcessor::midiOutputNode));

    audioInputNodeID  = audioInput->nodeID;
    audioOutputNodeID = audioOutput->nodeID;
    midiInputNodeID   = midiInput->nodeID;
    midiOutputNodeID  = midiOutput->nodeID;

    // Direct pass-through by default (input → output)
    connectPassthrough();
}

//==============================================================================
void AudioGraphEngine::connectPassthrough()
{
    int numCh = std::min(currentNumChannels, graph.getTotalNumOutputChannels());
    for (int ch = 0; ch < numCh; ++ch)
    {
        graph.addConnection ({ { audioInputNodeID,  ch },
                                { audioOutputNodeID, ch } });
    }
}

//==============================================================================
AudioGraphEngine::NodeID AudioGraphEngine::addPedal (
    std::unique_ptr<juce::AudioProcessor> processor,
    const juce::String& boardId, int pageIndex,
    float boardX, float boardY, float boardW, float boardH)
{
    auto nodeId = NodeID (nextNodeIndex++);
    auto* gp = dynamic_cast<GraphPedalProcessor*>(processor.get());
    auto node = graph.addNode (std::move (processor), nodeId);

    if (node == nullptr)
        return {};

    PedalInstance instance;
    instance.nodeID = nodeId;
    instance.boardId = boardId;
    instance.pageIndex = pageIndex;
    instance.boardX = boardX;
    instance.boardY = boardY;
    instance.boardW = boardW;
    instance.boardH = boardH;
    instance.name   = node->getProcessor()->getName();
    
    instance.meters = std::make_shared<PedalMeters>();
    if (gp != nullptr)
        gp->setMeters (instance.meters);

    instances.push_back (instance);

    return nodeId;
}

void AudioGraphEngine::removePedal (NodeID nodeId)
{
    disconnectAll (nodeId);
    graph.removeNode (nodeId);

    instances.remove_if ([nodeId] (const PedalInstance& p) { return p.nodeID == nodeId; });
}

AudioGraphEngine::NodeID AudioGraphEngine::addPedalOffBoard (
    std::unique_ptr<juce::AudioProcessor> processor)
{
    auto nodeId = NodeID (nextNodeIndex++);
    auto* gp = dynamic_cast<GraphPedalProcessor*>(processor.get());
    auto node = graph.addNode (std::move (processor), nodeId);

    if (node == nullptr)
        return {};

    PedalInstance instance;
    instance.nodeID  = nodeId;
    instance.boardX  = 0.0f;
    instance.boardY  = 0.0f;
    instance.boardW  = 100.0f;
    instance.boardH  = 200.0f;
    instance.onBoard = false;
    instance.name    = node->getProcessor()->getName();
    
    instance.meters = std::make_shared<PedalMeters>();
    if (gp != nullptr)
        gp->setMeters (instance.meters);

    instances.push_back (instance);

    return nodeId;
}

void AudioGraphEngine::autoRoutePedal (NodeID newNodeId)
{
    auto* newInst = getPedalInstance(newNodeId);
    if (!newInst || !newInst->onBoard) return;

    // Find the left neighbor
    PedalInstance* leftNeighbor = nullptr;
    float maxLeftX = -9999.0f;

    for (auto& inst : instances)
    {
        if (inst.nodeID != newNodeId && inst.boardId == newInst->boardId && inst.onBoard)
        {
            // Simple heuristic: closest X coordinate that is to the left of us
            if (inst.boardX <= newInst->boardX && inst.boardX > maxLeftX)
            {
                maxLeftX = inst.boardX;
                leftNeighbor = &inst;
            }
        }
    }

    auto outNodeId = getAudioOutputNodeID();
    auto inNodeId = getAudioInputNodeID();

    if (leftNeighbor)
    {
        // Find what the left neighbor's output is connected to
        NodeID destId = outNodeId; // Default to output
        
        // Remove existing audio connections from leftNeighbor
        for (auto c : graph.getConnections())
        {
            if (c.source.nodeID == leftNeighbor->nodeID && c.source.channelIndex != juce::AudioProcessorGraph::midiChannelIndex)
            {
                destId = c.destination.nodeID;
                graph.removeConnection (c);
            }
        }

        // Connect leftNeighbor -> newNode -> destId
        graph.addConnection ({ { leftNeighbor->nodeID, 0 }, { newNodeId, 0 } });
        graph.addConnection ({ { leftNeighbor->nodeID, 1 }, { newNodeId, 1 } });

        if (destId != newNodeId) // just in case
        {
            graph.addConnection ({ { newNodeId, 0 }, { destId, 0 } });
            graph.addConnection ({ { newNodeId, 1 }, { destId, 1 } });
        }
    }
    else
    {
        // No left neighbor, we are the first pedal.
        NodeID destId = outNodeId;
        
        // Find what Audio In is connected to
        for (auto c : graph.getConnections())
        {
            if (c.source.nodeID == inNodeId && c.source.channelIndex != juce::AudioProcessorGraph::midiChannelIndex)
            {
                destId = c.destination.nodeID;
                graph.removeConnection (c);
            }
        }

        // Connect Audio In -> newNode -> destId
        graph.addConnection ({ { inNodeId, 0 }, { newNodeId, 0 } });
        graph.addConnection ({ { inNodeId, 1 }, { newNodeId, 1 } });

        if (destId != newNodeId)
        {
            graph.addConnection ({ { newNodeId, 0 }, { destId, 0 } });
            graph.addConnection ({ { newNodeId, 1 }, { destId, 1 } });
        }
    }
}

void AudioGraphEngine::updatePedalProcessor (NodeID nodeId, std::unique_ptr<juce::AudioProcessor> newProcessor)
{
    // Snapshot connections
    std::vector<juce::AudioProcessorGraph::Connection> connections;
    for (auto c : graph.getConnections())
    {
        if (c.source.nodeID == nodeId || c.destination.nodeID == nodeId)
            connections.push_back (c);
    }

    graph.removeNode (nodeId);

    // Re-add node with exact same ID
    auto node = graph.addNode (std::move (newProcessor), nodeId);

    if (node != nullptr)
    {
        // Restore connections
        for (auto c : connections)
            graph.addConnection (c);
    }
}

//==============================================================================
bool AudioGraphEngine::connect (NodeID sourceNode, int sourceChannel,
                                 NodeID destNode,   int destChannel)
{
    // Smartly auto-remove the default passthrough on the specific channel we are intercepting
    if (sourceNode == audioInputNodeID)
        disconnect (audioInputNodeID, sourceChannel, audioOutputNodeID, sourceChannel);

    if (destNode == audioOutputNodeID)
        disconnect (audioInputNodeID, destChannel, audioOutputNodeID, destChannel);

    return graph.addConnection ({ { sourceNode, sourceChannel },
                                   { destNode,   destChannel } });
}

bool AudioGraphEngine::disconnect (NodeID sourceNode, int sourceChannel,
                                    NodeID destNode,   int destChannel)
{
    return graph.removeConnection ({ { sourceNode, sourceChannel },
                                      { destNode,   destChannel } });
}

void AudioGraphEngine::disconnectAll (NodeID nodeId)
{
    // Snapshot connections first — getConnections() returns by value,
    // but we must not modify the graph while iterating.
    auto connections = graph.getConnections();
    for (auto& conn : connections)
    {
        if (conn.source.nodeID == nodeId || conn.destination.nodeID == nodeId)
            graph.removeConnection (conn);
    }
}

bool AudioGraphEngine::hasConnections (NodeID nodeId) const
{
    for (auto& conn : graph.getConnections())
        if (conn.source.nodeID == nodeId || conn.destination.nodeID == nodeId)
            return true;
    return false;
}

//==============================================================================
// Board-Level Routing (MIDI / Expression connections between pedals)
//==============================================================================
void AudioGraphEngine::addBoardConnection (const BoardRoutingConnection& conn)
{
    // Prevent duplicate connections
    for (const auto& existing : boardConnections)
    {
        if (existing.srcNodeId == conn.srcNodeId && existing.srcPortId == conn.srcPortId &&
            existing.dstNodeId == conn.dstNodeId && existing.dstPortId == conn.dstPortId)
            return;
    }
    boardConnections.push_back (conn);
    if (onBoardConnectionsChanged) onBoardConnectionsChanged();
}

void AudioGraphEngine::removeBoardConnection (NodeID srcNodeId, const juce::String& srcPortId,
                                               NodeID dstNodeId, const juce::String& dstPortId)
{
    boardConnections.erase (
        std::remove_if (boardConnections.begin(), boardConnections.end(),
            [&] (const BoardRoutingConnection& c) {
                return c.srcNodeId == srcNodeId && c.srcPortId == srcPortId &&
                       c.dstNodeId == dstNodeId && c.dstPortId == dstPortId;
            }),
        boardConnections.end());
    if (onBoardConnectionsChanged) onBoardConnectionsChanged();
}

//==============================================================================
// Hardware MIDI Device Enumeration
//==============================================================================
bool AudioGraphEngine::refreshHardwareMidiDevices()
{
    // Re-enumerate system MIDI devices, preserving enabled state for known devices
    auto inputs  = juce::MidiInput::getAvailableDevices();
    auto outputs = juce::MidiOutput::getAvailableDevices();

    // Build new list, carry over enabled/position state for devices we already know
    std::vector<HardwareMidiDevice> newList;

    float nextInputY = 100.0f;
    float nextOutputY = 100.0f;
    
    // Find highest existing Y coordinates to stack new devices below them
    for (const auto& existing : hwMidiDevices)
    {
        if (existing.isInput)
            nextInputY = std::max (nextInputY, existing.routeY + 120.0f);
        else
            nextOutputY = std::max (nextOutputY, existing.routeY + 120.0f);
    }

    for (const auto& dev : inputs)
    {
        HardwareMidiDevice hmd;
        hmd.deviceName = dev.name;
        hmd.isInput    = true;
        // Generate a stable NodeID (0x4D490000 base + hash)
        hmd.engineNodeId.uid = 0x4D490000 + (uint32_t)(dev.name.hashCode() % 0xFFFF);

        // Preserve existing state if we had this device before
        bool found = false;
        for (const auto& existing : hwMidiDevices)
        {
            if (existing.deviceName == dev.name && existing.isInput)
            { 
                hmd.routeX = existing.routeX; 
                hmd.routeY = existing.routeY; 
                hmd.activity = existing.activity;
                hmd.engineNodeId = existing.engineNodeId;
                found = true;
                break; 
            }
        }
        
        if (!found)
        {
            hmd.routeX = 80.0f;
            hmd.routeY = nextInputY;
            nextInputY += 120.0f;
        }

        newList.push_back (hmd);
    }
    for (const auto& dev : outputs)
    {
        HardwareMidiDevice hmd;
        hmd.deviceName = dev.name;
        hmd.isInput    = false;
        // Outputs get a slightly different base to avoid collision if input/output share a name
        hmd.engineNodeId.uid = 0x4D4A0000 + (uint32_t)(dev.name.hashCode() % 0xFFFF);

        bool found = false;
        for (const auto& existing : hwMidiDevices)
        {
            if (existing.deviceName == dev.name && !existing.isInput)
            { 
                hmd.routeX = existing.routeX; 
                hmd.routeY = existing.routeY; 
                hmd.activity = existing.activity;
                hmd.engineNodeId = existing.engineNodeId;
                found = true;
                break; 
            }
        }
        
        if (!found)
        {
            hmd.routeX = 900.0f;
            hmd.routeY = nextOutputY;
            nextOutputY += 120.0f;
        }

        newList.push_back (hmd);
    }

    bool changed = (newList.size() != hwMidiDevices.size());
    if (!changed)
    {
        for (size_t i = 0; i < newList.size(); ++i)
        {
            if (newList[i].deviceName != hwMidiDevices[i].deviceName || 
                newList[i].isInput != hwMidiDevices[i].isInput)
            {
                changed = true;
                break;
            }
        }
    }

    if (changed)
    {
        // Remove old nodes from graph that are no longer present
        for (const auto& existing : hwMidiDevices)
        {
            auto it = std::find_if (newList.begin(), newList.end(), [&](const HardwareMidiDevice& dev) {
                return dev.engineNodeId.uid == existing.engineNodeId.uid;
            });
            if (it == newList.end())
                graph.removeNode (existing.engineNodeId);
        }
        
        // Add new nodes to graph that were not present before
        for (const auto& dev : newList)
        {
            auto it = std::find_if (hwMidiDevices.begin(), hwMidiDevices.end(), [&](const HardwareMidiDevice& existing) {
                return dev.engineNodeId.uid == existing.engineNodeId.uid;
            });
            if (it == hwMidiDevices.end())
            {
                if (dev.isInput)
                    graph.addNode (std::make_unique<HardwareMidiInputNode>(dev.deviceName), dev.engineNodeId);
                else
                    graph.addNode (std::make_unique<HardwareMidiOutputNode>(dev.deviceName), dev.engineNodeId);
            }
        }
    }

    hwMidiDevices = std::move (newList);
    if (changed && onBoardConnectionsChanged)
        onBoardConnectionsChanged();
    return changed;
}

void AudioGraphEngine::injectHardwareMidi (const juce::String& deviceName, const juce::MidiMessage& msg)
{
    // Find the node by name
    for (auto* node : graph.getNodes())
    {
        if (auto* hwNode = dynamic_cast<HardwareMidiInputNode*>(node->getProcessor()))
        {
            if (hwNode->getName() == deviceName)
            {
                hwNode->pushMidiMessage(msg);
                for (auto& hw : hwMidiDevices)
                    if (hw.deviceName == deviceName && hw.isInput)
                        hw.activity->store(true);
                return;
            }
        }
    }
}

void AudioGraphEngine::extractHardwareMidi (const juce::String& deviceName, juce::MidiBuffer& dest)
{
    for (auto* node : graph.getNodes())
    {
        if (auto* hwNode = dynamic_cast<HardwareMidiOutputNode*>(node->getProcessor()))
        {
            if (hwNode->getName() == deviceName)
            {
                hwNode->popCapturedMidi(dest);
                if (!dest.isEmpty())
                {
                    for (auto& hw : hwMidiDevices)
                        if (hw.deviceName == deviceName && !hw.isInput)
                            hw.activity->store(true);
                }
                return;
            }
        }
    }
}

//==============================================================================
PedalInstance* AudioGraphEngine::getPedalInstance (NodeID nodeId)
{
    for (auto& inst : instances)
        if (inst.nodeID == nodeId)
            return &inst;
    return nullptr;
}

//==============================================================================
juce::String AudioGraphEngine::serialise() const
{
    auto root = std::make_unique<juce::DynamicObject>();
    juce::Array<juce::var> pedalArray;

    for (auto& inst : instances)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty ("nodeID",   (int) inst.nodeID.uid);
        obj->setProperty ("name",     inst.name);
        obj->setProperty ("boardX",   inst.boardX);
        obj->setProperty ("boardY",   inst.boardY);
        obj->setProperty ("boardW",   inst.boardW);
        obj->setProperty ("boardH",   inst.boardH);
        obj->setProperty ("bypassed", inst.bypassed);
        pedalArray.add (juce::var (obj.release()));
    }

    root->setProperty ("pedals", pedalArray);

    // Serialise connections
    juce::Array<juce::var> connArray;
    for (auto& conn : graph.getConnections())
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty ("srcNode",  (int) conn.source.nodeID.uid);
        obj->setProperty ("srcChan",  conn.source.channelIndex);
        obj->setProperty ("dstNode",  (int) conn.destination.nodeID.uid);
        obj->setProperty ("dstChan",  conn.destination.channelIndex);
        connArray.add (juce::var (obj.release()));
    }

    root->setProperty ("connections", connArray);

    root->setProperty ("boardNotes", StickyNoteData::toJSON (boardNotes));
    root->setProperty ("routeNotes", StickyNoteData::toJSON (routeNotes));
    root->setProperty ("playNotes", StickyNoteData::toJSON (playNotes));

    // Serialise MIDI config
    auto midiCfgObj = std::make_unique<juce::DynamicObject>();
    midiCfgObj->setProperty ("turingPrevCC", appMidiConfig.turingPrevCC);
    midiCfgObj->setProperty ("turingNextCC", appMidiConfig.turingNextCC);
    midiCfgObj->setProperty ("playModeToggleCC", appMidiConfig.playModeToggleCC);
    midiCfgObj->setProperty ("pageLeftCC", appMidiConfig.pageLeftCC);
    midiCfgObj->setProperty ("pageRightCC", appMidiConfig.pageRightCC);
    midiCfgObj->setProperty ("trackLeftCC", appMidiConfig.trackLeftCC);
    midiCfgObj->setProperty ("trackRightCC", appMidiConfig.trackRightCC);
    midiCfgObj->setProperty ("novationMode", (int) appMidiConfig.novationMode);
    root->setProperty ("midiConfig", juce::var (midiCfgObj.release()));

    return juce::JSON::toString (juce::var (root.release()));
}

void AudioGraphEngine::deserialise (const juce::String& jsonState)
{
    // TODO: Implement full deserialisation in Phase 1E
    auto parsed = juce::JSON::parse (jsonState);
    if (auto* root = parsed.getDynamicObject())
    {
        if (root->hasProperty ("boardNotes"))
            boardNotes = StickyNoteData::fromJSON (root->getProperty ("boardNotes"));
        if (root->hasProperty ("routeNotes"))
            routeNotes = StickyNoteData::fromJSON (root->getProperty ("routeNotes"));
        if (root->hasProperty ("playNotes"))
            playNotes = StickyNoteData::fromJSON (root->getProperty ("playNotes"));

        // Deserialise MIDI config
        if (root->hasProperty ("midiConfig"))
        {
            if (auto* midiCfgObj = root->getProperty ("midiConfig").getDynamicObject())
            {
                if (midiCfgObj->hasProperty ("turingPrevCC"))
                    appMidiConfig.turingPrevCC = (int) midiCfgObj->getProperty ("turingPrevCC");
                if (midiCfgObj->hasProperty ("turingNextCC"))
                    appMidiConfig.turingNextCC = (int) midiCfgObj->getProperty ("turingNextCC");
                if (midiCfgObj->hasProperty ("playModeToggleCC"))
                    appMidiConfig.playModeToggleCC = (int) midiCfgObj->getProperty ("playModeToggleCC");
                if (midiCfgObj->hasProperty ("pageLeftCC"))
                    appMidiConfig.pageLeftCC = (int) midiCfgObj->getProperty ("pageLeftCC");
                if (midiCfgObj->hasProperty ("pageRightCC"))
                    appMidiConfig.pageRightCC = (int) midiCfgObj->getProperty ("pageRightCC");
                if (midiCfgObj->hasProperty ("trackLeftCC"))
                    appMidiConfig.trackLeftCC = (int) midiCfgObj->getProperty ("trackLeftCC");
                if (midiCfgObj->hasProperty ("trackRightCC"))
                    appMidiConfig.trackRightCC = (int) midiCfgObj->getProperty ("trackRightCC");
                if (midiCfgObj->hasProperty ("novationMode"))
                    appMidiConfig.novationMode = (AppMidiConfig::NovationMode) (int) midiCfgObj->getProperty ("novationMode");
            }
        }
    }
}
