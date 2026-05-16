#include "AudioGraphEngine.h"

//==============================================================================
AudioGraphEngine::AudioGraphEngine()
    : graph()
{
    // Configure the graph's bus layout to match our stereo I/O
    graph.enableAllBuses();
}

AudioGraphEngine::~AudioGraphEngine()
{
    graph.clear();
}

//==============================================================================
void AudioGraphEngine::prepare (double sampleRate, int samplesPerBlock,
                                 int numInputChannels, int numOutputChannels)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;
    currentNumChannels = numInputChannels;

    // Set the graph's bus layout to match the host
    juce::AudioProcessor::BusesLayout layout;
    if (numInputChannels == 2)
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
    else
        layout.inputBuses.add (juce::AudioChannelSet::mono());

    if (numOutputChannels == 2)
        layout.outputBuses.add (juce::AudioChannelSet::stereo());
    else
        layout.outputBuses.add (juce::AudioChannelSet::mono());

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
    graph.processBlock (buffer, midi);
}

//==============================================================================
void AudioGraphEngine::setupIONodes()
{
    // Clear existing graph
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
    for (int ch = 0; ch < currentNumChannels; ++ch)
    {
        graph.addConnection ({ { audioInputNodeID,  ch },
                                { audioOutputNodeID, ch } });
    }
}

//==============================================================================
AudioGraphEngine::NodeID AudioGraphEngine::addPedal (
    std::unique_ptr<juce::AudioProcessor> processor,
    int gridX, int gridY, int gridW, int gridH)
{
    auto nodeId = NodeID (nextNodeIndex++);
    auto node = graph.addNode (std::move (processor), nodeId);

    if (node == nullptr)
        return {};

    PedalInstance instance;
    instance.nodeID = nodeId;
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
    // Auto-remove default passthrough on first manual connection
    for (int ch = 0; ch < 4; ++ch)
        disconnect (audioInputNodeID, ch, audioOutputNodeID, ch);

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
