#include "AudioGraphEngine.h"
#include "../dsp/GraphPedalProcessor.h"
#include "MidiRoutingNodes.h"
#include "../pedals/PedalRegistry.h"
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

    // Save the starting state as the initial undo state
    saveUndoState();
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
    autoRebuildIOConnections (numInputChannels, numOutputChannels);
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
    float boardX, float boardY, float boardW, float boardH,
    NodeID customNodeId)
{
    auto nodeId = customNodeId.uid != 0 ? customNodeId : NodeID (nextNodeIndex++);
    if (nodeId.uid >= nextNodeIndex)
        nextNodeIndex = nodeId.uid + 1;

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
    std::unique_ptr<juce::AudioProcessor> processor,
    NodeID customNodeId)
{
    auto nodeId = customNodeId.uid != 0 ? customNodeId : NodeID (nextNodeIndex++);
    if (nodeId.uid >= nextNodeIndex)
        nextNodeIndex = nodeId.uid + 1;

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

    // Set play config and prepare the processor before adding to graph so it gets correct sampleRate and blockSize
    newProcessor->setPlayConfigDetails (
        2, 2, // stereo I/O config
        currentSampleRate,
        currentBlockSize
    );
    newProcessor->prepareToPlay (currentSampleRate, currentBlockSize);

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
        obj->setProperty ("nodeID",    (int) inst.nodeID.uid);
        obj->setProperty ("name",      inst.name);
        obj->setProperty ("category",  inst.category);
        obj->setProperty ("colour",    (juce::int64) inst.colour.getARGB());
        obj->setProperty ("numKnobs",  inst.numKnobs);
        obj->setProperty ("boardX",    inst.boardX);
        obj->setProperty ("boardY",    inst.boardY);
        obj->setProperty ("boardW",    inst.boardW);
        obj->setProperty ("boardH",    inst.boardH);
        obj->setProperty ("bypassed",  inst.bypassed);
        obj->setProperty ("onBoard",   inst.onBoard);
        obj->setProperty ("boardId",   inst.boardId);
        obj->setProperty ("pageIndex", inst.pageIndex);
        obj->setProperty ("rotation",  inst.rotation);
        obj->setProperty ("routeX",    inst.routeX);
        obj->setProperty ("routeY",    inst.routeY);

        if (inst.design != nullptr)
        {
            auto* nonConstSelf = const_cast<AudioGraphEngine*> (this);
            if (auto* node = nonConstSelf->graph.getNodeForId (inst.nodeID))
            {
                if (auto* proc = node->getProcessor())
                {
                    if (auto* gProc = dynamic_cast<GraphPedalProcessor*> (proc))
                    {
                        const_cast<PedalDesign*>(inst.design.get())->effectsGraph = gProc->getDSPGraph().toJSON();
                    }
                }
            }
            obj->setProperty ("design", inst.design->toJSON());
        }

        // Control values
        auto ctrlValObj = std::make_unique<juce::DynamicObject>();
        for (const auto& [cid, val] : inst.controlValues)
            ctrlValObj->setProperty (cid, val);
        obj->setProperty ("controlValues", juce::var (ctrlValObj.release()));

        // Control texts
        auto ctrlTextObj = std::make_unique<juce::DynamicObject>();
        for (const auto& [cid, text] : inst.controlTexts)
            ctrlTextObj->setProperty (cid, text);
        obj->setProperty ("controlTexts", juce::var (ctrlTextObj.release()));

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
    bool wasRestoring = isRestoringState;
    isRestoringState = true;

    auto parsed = juce::JSON::parse (jsonState);
    if (auto* root = parsed.getDynamicObject())
    {
        // 1. Clear all existing active pedals first
        std::vector<NodeID> idsToRemove;
        for (auto& inst : instances)
            idsToRemove.push_back (inst.nodeID);
        for (auto id : idsToRemove)
            removePedal (id);

        // 2. Deserialise board, routing, and play notes
        if (root->hasProperty ("boardNotes"))
            boardNotes = StickyNoteData::fromJSON (root->getProperty ("boardNotes"));
        if (root->hasProperty ("routeNotes"))
            routeNotes = StickyNoteData::fromJSON (root->getProperty ("routeNotes"));
        if (root->hasProperty ("playNotes"))
            playNotes = StickyNoteData::fromJSON (root->getProperty ("playNotes"));

        // 3. Deserialise MIDI config
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

        // 4. Reconstruct pedal instances
        if (root->hasProperty ("pedals"))
        {
            if (auto* pedalArray = root->getProperty ("pedals").getArray())
            {
                for (const auto& pv : *pedalArray)
                {
                    if (auto* obj = pv.getDynamicObject())
                    {
                        NodeID nodeId { (juce::uint32) (int) obj->getProperty ("nodeID") };
                        juce::String name = obj->getProperty ("name").toString();

                        std::shared_ptr<PedalDesign> design = nullptr;
                        std::function<std::unique_ptr<juce::AudioProcessor>()> factoryFn = nullptr;

                        for (const auto& info : getFactoryPedals())
                        {
                            if (info.name == name)
                            {
                                factoryFn = info.factory;
                                break;
                            }
                        }

                        if (obj->hasProperty ("design"))
                        {
                            design = std::make_shared<PedalDesign> (PedalDesign::fromJSON (obj->getProperty ("design")));
                        }
                        else
                        {
                            // Fallback for factory presets
                            for (const auto& info : getFactoryPedals())
                            {
                                if (info.name == name)
                                {
                                    if (info.designFactory)
                                        design = info.designFactory();
                                    break;
                                }
                            }
                        }

                        juce::String jsonGraph;
                        if (design != nullptr && !design->effectsGraph.isVoid())
                            jsonGraph = juce::JSON::toString (design->effectsGraph);

                        std::unique_ptr<juce::AudioProcessor> processor;
                        if (jsonGraph.isNotEmpty())
                        {
                            processor = std::make_unique<GraphPedalProcessor> (name, jsonGraph);
                        }
                        else if (factoryFn != nullptr)
                        {
                            processor = factoryFn();
                        }
                        else
                        {
                            processor = std::make_unique<GraphPedalProcessor> (name, jsonGraph);
                        }
                        
                        // Extract additional fields
                        juce::String boardId = obj->getProperty ("boardId").toString();
                        int pageIndex = (int) obj->getProperty ("pageIndex");
                        float boardX = (float) (double) obj->getProperty ("boardX");
                        float boardY = (float) (double) obj->getProperty ("boardY");
                        float boardW = (float) (double) obj->getProperty ("boardW");
                        float boardH = (float) (double) obj->getProperty ("boardH");
                        bool onBoard = obj->hasProperty ("onBoard") ? (bool) obj->getProperty ("onBoard") : true;

                        NodeID addedId;
                        if (onBoard)
                        {
                            addedId = addPedal (std::move (processor), boardId, pageIndex, boardX, boardY, boardW, boardH, nodeId);
                        }
                        else
                        {
                            addedId = addPedalOffBoard (std::move (processor), nodeId);
                        }

                        if (auto* inst = getPedalInstance (addedId))
                        {
                            if (obj->hasProperty ("category")) inst->category = obj->getProperty ("category").toString();
                            if (obj->hasProperty ("colour")) inst->colour = juce::Colour ((juce::uint32) (juce::int64) obj->getProperty ("colour"));
                            if (obj->hasProperty ("numKnobs")) inst->numKnobs = (int) obj->getProperty ("numKnobs");
                            if (obj->hasProperty ("bypassed")) inst->bypassed = (bool) obj->getProperty ("bypassed");
                            if (obj->hasProperty ("rotation")) inst->rotation = (int) obj->getProperty ("rotation");
                            if (obj->hasProperty ("routeX")) inst->routeX = (float) (double) obj->getProperty ("routeX");
                            if (obj->hasProperty ("routeY")) inst->routeY = (float) (double) obj->getProperty ("routeY");
                            
                            inst->design = design;

                            // Sync bypass parameter in processor if it exists
                            if (auto* node = graph.getNodeForId (addedId))
                            {
                                if (auto* proc = node->getProcessor())
                                {
                                    if (!proc->getParameters().isEmpty())
                                    {
                                        if (auto* bypassParam = proc->getParameters()[0]) // bypass is first parameter in rebuildParameters()
                                            bypassParam->setValueNotifyingHost (inst->bypassed ? 1.0f : 0.0f);
                                    }

                                    if (auto* gProc = dynamic_cast<GraphPedalProcessor*> (proc))
                                    {
                                        if (inst->design != nullptr && inst->design->effectsGraph.isVoid())
                                        {
                                            inst->design->effectsGraph = juce::JSON::parse (gProc->saveGraph());
                                        }
                                    }
                                }
                            }

                            // Restore control values
                            if (obj->hasProperty ("controlValues"))
                            {
                                if (auto* ctrlValObj = obj->getProperty ("controlValues").getDynamicObject())
                                {
                                    for (const auto& prop : ctrlValObj->getProperties())
                                    {
                                        juce::String cid = prop.name.toString();
                                        float val = (float) (double) prop.value;
                                        inst->controlValues[cid] = val;

                                        juce::Logger::writeToLog("AudioGraphEngine: Syncing control " + cid + " to value " + juce::String(val));

                                        // Sync parameter value directly to the processor
                                        if (auto* node = graph.getNodeForId (addedId))
                                        {
                                            if (auto* proc = node->getProcessor())
                                            {
                                                juce::String mappedParamID;
                                                if (inst->design != nullptr)
                                                {
                                                    for (const auto& m : inst->design->mappings)
                                                    {
                                                        if (m.controlID == cid)
                                                        {
                                                            mappedParamID = m.nodeParam;
                                                            break;
                                                        }
                                                    }
                                                }
                                                if (mappedParamID.isNotEmpty())
                                                {
                                                    juce::Logger::writeToLog("AudioGraphEngine: Found mapped parameter ID " + mappedParamID);
                                                    for (auto* param : proc->getParameters())
                                                    {
                                                        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                                                        {
                                                            if (matchMappingParam (mappedParamID, rp->getParameterID()))
                                                            {
                                                                juce::Logger::writeToLog("AudioGraphEngine: Setting parameter " + rp->getParameterID() + " via setValueNotifyingHost");
                                                                rp->setValueNotifyingHost (val);
                                                                juce::Logger::writeToLog("AudioGraphEngine: Parameter set successful");
                                                                break;
                                                            }
                                                        }
                                                    }
                                                }
                                                else
                                                {
                                                    juce::Logger::writeToLog("AudioGraphEngine: No mapped parameter ID found for " + cid);
                                                }
                                            }
                                            else
                                            {
                                                juce::Logger::writeToLog("AudioGraphEngine: WARNING node processor is null for addedId " + juce::String(addedId.uid));
                                            }
                                        }
                                        else
                                        {
                                            juce::Logger::writeToLog("AudioGraphEngine: WARNING node is null for addedId " + juce::String(addedId.uid));
                                        }
                                    }
                                }
                            }

                            // Restore control texts
                            if (obj->hasProperty ("controlTexts"))
                            {
                                if (auto* ctrlTextObj = obj->getProperty ("controlTexts").getDynamicObject())
                                {
                                    for (const auto& prop : ctrlTextObj->getProperties())
                                        inst->controlTexts[prop.name.toString()] = prop.value.toString();
                                }
                            }
                        }
                    }
                }
            }
        }

        // 5. Reconstruct routing connections
        // Clear all current active connections so we don't have duplicates or remnants
        auto currentConnections = graph.getConnections();
        for (auto& conn : currentConnections)
            graph.removeConnection (conn);

        if (root->hasProperty ("connections"))
        {
            if (auto* connArray = root->getProperty ("connections").getArray())
            {
                for (const auto& cv : *connArray)
                {
                    if (auto* co = cv.getDynamicObject())
                    {
                        NodeID srcNode { (juce::uint32) (int) co->getProperty ("srcNode") };
                        int srcChan = (int) co->getProperty ("srcChan");
                        NodeID dstNode { (juce::uint32) (int) co->getProperty ("dstNode") };
                        int dstChan = (int) co->getProperty ("dstChan");

                        // Verify that both nodes actually exist in the graph before connecting
                        if (graph.getNodeForId (srcNode) != nullptr && graph.getNodeForId (dstNode) != nullptr)
                        {
                            graph.addConnection ({ { srcNode, srcChan }, { dstNode, dstChan } });
                        }
                    }
                }
            }
        }
        else
        {
            // If no connections saved (e.g. legacy), restore default passthrough
            connectPassthrough();
        }
    }

    isRestoringState = wasRestoring;

    if (!isRestoringState)
    {
        clearUndoHistory();
        saveUndoState();
    }
}

//==============================================================================
void AudioGraphEngine::autoRebuildIOConnections (int numInputs, int numOutputs)
{
    // Find the first and last pedals on the board
    juce::String targetBoardId = boards.empty() ? "main" : boards.front().id;
    PedalInstance* firstPedal = nullptr;
    PedalInstance* lastPedal = nullptr;
    float minX = 999999.0f;
    float maxX = -999999.0f;

    for (auto& inst : instances)
    {
        if (inst.onBoard && inst.boardId == targetBoardId)
        {
            if (inst.boardX < minX)
            {
                minX = inst.boardX;
                firstPedal = &inst;
            }
            if (inst.boardX > maxX)
            {
                maxX = inst.boardX;
                lastPedal = &inst;
            }
        }
    }

    // Disconnect old audio connections on audioInputNodeID and audioOutputNodeID
    for (auto c : graph.getConnections())
    {
        if (c.source.channelIndex != juce::AudioProcessorGraph::midiChannelIndex)
        {
            if (c.source.nodeID == audioInputNodeID || c.destination.nodeID == audioOutputNodeID)
            {
                graph.removeConnection (c);
            }
        }
    }

    if (firstPedal != nullptr && lastPedal != nullptr)
    {
        // Rebuild from audioInputNodeID to firstPedal
        if (numInputs == 1)
        {
            // Mono input: route hardware input 0 to both L and R of firstPedal
            graph.addConnection ({ { audioInputNodeID, 0 }, { firstPedal->nodeID, 0 } });
            graph.addConnection ({ { audioInputNodeID, 0 }, { firstPedal->nodeID, 1 } });
        }
        else if (numInputs >= 2)
        {
            // Stereo input: route hardware input 0->L (0) and 1->R (1) of firstPedal
            graph.addConnection ({ { audioInputNodeID, 0 }, { firstPedal->nodeID, 0 } });
            graph.addConnection ({ { audioInputNodeID, 1 }, { firstPedal->nodeID, 1 } });
        }

        // Rebuild from lastPedal to audioOutputNodeID
        if (numOutputs == 1)
        {
            // Mono output: route L (0) of lastPedal to hardware output 0
            graph.addConnection ({ { lastPedal->nodeID, 0 }, { audioOutputNodeID, 0 } });
        }
        else if (numOutputs >= 2)
        {
            // Stereo output: route L (0)->0 and R (1)->1 of lastPedal
            graph.addConnection ({ { lastPedal->nodeID, 0 }, { audioOutputNodeID, 0 } });
            graph.addConnection ({ { lastPedal->nodeID, 1 }, { audioOutputNodeID, 1 } });
        }
    }
    else
    {
        // No pedals on the board: do a direct passthrough
        if (numInputs == 1)
        {
            if (numOutputs == 1)
            {
                graph.addConnection ({ { audioInputNodeID, 0 }, { audioOutputNodeID, 0 } });
            }
            else if (numOutputs >= 2)
            {
                graph.addConnection ({ { audioInputNodeID, 0 }, { audioOutputNodeID, 0 } });
                graph.addConnection ({ { audioInputNodeID, 0 }, { audioOutputNodeID, 1 } });
            }
        }
        else if (numInputs >= 2)
        {
            if (numOutputs == 1)
            {
                graph.addConnection ({ { audioInputNodeID, 0 }, { audioOutputNodeID, 0 } });
            }
            else if (numOutputs >= 2)
            {
                graph.addConnection ({ { audioInputNodeID, 0 }, { audioOutputNodeID, 0 } });
                graph.addConnection ({ { audioInputNodeID, 1 }, { audioOutputNodeID, 1 } });
            }
        }
    }
}

//==============================================================================
void AudioGraphEngine::saveUndoState()
{
    if (isRestoringState) return;

    juce::String state = serialise();
    
    // If the state is identical to the current top of the stack, don't push it
    if (!undoStack.empty() && undoStack.back() == state)
        return;

    undoStack.push_back (state);
    redoStack.clear(); // Clear redo stack on any new action

    if (undoStack.size() > maxUndoDepth)
        undoStack.erase (undoStack.begin());
}

bool AudioGraphEngine::undo()
{
    if (undoStack.size() <= 1) // We need at least the initial state + 1 state to undo
        return false;

    // Pop the current state and push it to the redo stack
    juce::String currentState = undoStack.back();
    undoStack.pop_back();
    redoStack.push_back (currentState);

    // Get the previous state
    juce::String targetState = undoStack.back();

    isRestoringState = true;
    deserialise (targetState);
    isRestoringState = false;

    return true;
}

bool AudioGraphEngine::redo()
{
    if (redoStack.empty())
        return false;

    juce::String targetState = redoStack.back();
    redoStack.pop_back();
    undoStack.push_back (targetState);

    isRestoringState = true;
    deserialise (targetState);
    isRestoringState = false;

    return true;
}

void AudioGraphEngine::clearUndoHistory()
{
    undoStack.clear();
    redoStack.clear();
}

