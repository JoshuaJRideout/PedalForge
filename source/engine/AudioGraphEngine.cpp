#include "AudioGraphEngine.h"

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
    boards.push_back (mainBoard);
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

void AudioGraphEngine::addBoard (const BoardConfig& board)
{
    boards.push_back (board);
}

void AudioGraphEngine::removeBoard (const juce::String& boardId)
{
    boards.erase (std::remove_if (boards.begin(), boards.end(),
                  [&] (const BoardConfig& b) { return b.id == boardId; }),
                  boards.end());
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

void AudioGraphEngine::releaseResources()
{
    graph.releaseResources();
}

void AudioGraphEngine::processBlock (juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midi)
{
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
                    
                // Check board-specific CCs
                for (auto& board : boards)
                {
                    if (board.prevPageCC != -1 && cc == board.prevPageCC)
                    {
                        if (board.activePage > 0)
                            board.activePage--;
                        // Note: To properly update UI, we probably want to dispatch an async message
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
                if (a->gridY != b->gridY) return a->gridY < b->gridY;
                return a->gridX < b->gridX;
            });
            
            board.turingPedalIndex = (board.turingPedalIndex + dir) % (int)boardPedals.size();
            if (board.turingPedalIndex < 0) board.turingPedalIndex += (int)boardPedals.size();
        }
    }
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
    int gridX, int gridY, int gridW, int gridH)
{
    auto nodeId = NodeID (nextNodeIndex++);
    auto node = graph.addNode (std::move (processor), nodeId);

    if (node == nullptr)
        return {};

    PedalInstance instance;
    instance.nodeID = nodeId;
    instance.boardId = boardId;
    instance.pageIndex = pageIndex;
    instance.gridX  = gridX;
    instance.gridY  = gridY;
    instance.gridW  = gridW;
    instance.gridH  = gridH;
    instance.name   = node->getProcessor()->getName();

    instances.push_back (instance);

    return nodeId;
}

void AudioGraphEngine::removePedal (NodeID nodeId)
{
    disconnectAll (nodeId);
    graph.removeNode (nodeId);

    instances.erase (
        std::remove_if (instances.begin(), instances.end(),
                        [nodeId] (const PedalInstance& p) { return p.nodeID == nodeId; }),
        instances.end());
}

AudioGraphEngine::NodeID AudioGraphEngine::addPedalOffBoard (
    std::unique_ptr<juce::AudioProcessor> processor)
{
    auto nodeId = NodeID (nextNodeIndex++);
    auto node = graph.addNode (std::move (processor), nodeId);

    if (node == nullptr)
        return {};

    PedalInstance instance;
    instance.nodeID  = nodeId;
    instance.gridX   = 0;
    instance.gridY   = 0;
    instance.gridW   = 1;
    instance.gridH   = 2;
    instance.onBoard = false;
    instance.name    = node->getProcessor()->getName();

    instances.push_back (instance);

    return nodeId;
}

void AudioGraphEngine::autoRoutePedal (NodeID newNodeId)
{
    auto* newInst = getPedalInstance(newNodeId);
    if (!newInst || !newInst->onBoard) return;

    // Find the left neighbor
    PedalInstance* leftNeighbor = nullptr;
    int maxLeftX = -1;

    for (auto& inst : instances)
    {
        if (inst.nodeID != newNodeId && inst.boardId == newInst->boardId && inst.onBoard)
        {
            // Simple heuristic: closest X coordinate that is to the left of us
            if (inst.gridX <= newInst->gridX && inst.gridX > maxLeftX)
            {
                maxLeftX = inst.gridX;
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
        obj->setProperty ("gridX",    inst.gridX);
        obj->setProperty ("gridY",    inst.gridY);
        obj->setProperty ("gridW",    inst.gridW);
        obj->setProperty ("gridH",    inst.gridH);
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

    return juce::JSON::toString (juce::var (root.release()));
}

void AudioGraphEngine::deserialise (const juce::String& jsonState)
{
    // TODO: Implement full deserialisation in Phase 1E
    juce::ignoreUnused (jsonState);
}
