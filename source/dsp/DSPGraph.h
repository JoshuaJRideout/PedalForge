#pragma once

#include "DSPNode.h"
#include "DSPNodeLibrary.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <map>
#include <memory>
#include <algorithm>

//==============================================================================
/**
 * A connection between two node ports in the graph.
 */
struct NodeConnection
{
    int sourceNodeID;
    int sourcePort;
    int destNodeID;
    int destPort;
};

//==============================================================================
/**
 * Factory for creating DSP nodes by type string.
 */
inline std::unique_ptr<DSPNode> createNodeByType (const juce::String& type)
{
    if (type == "audio_input")  return std::make_unique<AudioInputNode>();
    if (type == "audio_output") return std::make_unique<AudioOutputNode>();
    if (type == "midi_input")   return std::make_unique<MidiInputNode>();
    if (type == "midi_output")  return std::make_unique<MidiOutputNode>();
    if (type == "oscillator")   return std::make_unique<OscillatorNode>();
    if (type == "noise")        return std::make_unique<NoiseNode>();
    if (type == "adsr")         return std::make_unique<ADSRNode>();
    if (type == "ar_env")       return std::make_unique<ARNode>();
    if (type == "svf")          return std::make_unique<SVFNode>();
    if (type == "ladder_filter")return std::make_unique<LadderFilterNode>();
    if (type == "vca")          return std::make_unique<VCANode>();
    if (type == "voice_alloc")  return std::make_unique<VoiceAllocatorNode>();
    if (type == "glide")        return std::make_unique<GlideNode>();

    if (type == "ir")           return std::make_unique<IRNode>();
    if (type == "sampler")      return std::make_unique<SamplerNode>();
    if (type == "ram")          return std::make_unique<RamNode>();

    if (type == "gain")         return std::make_unique<GainNode>();
    if (type == "mix")          return std::make_unique<MixNode>();
    if (type == "split")        return std::make_unique<SplitNode>();
    if (type == "lowpass")      return std::make_unique<LowPassNode>();
    if (type == "highpass")     return std::make_unique<HighPassNode>();
    if (type == "allpass")      return std::make_unique<AllPassNode>();
    if (type == "tonestack")    return std::make_unique<ToneStackNode>();
    if (type == "softclip")     return std::make_unique<SoftClipNode>();
    if (type == "hardclip")     return std::make_unique<HardClipNode>();
    if (type == "lfo")          return std::make_unique<LFONode>();
    if (type == "delay")        return std::make_unique<DelayNode>();
    if (type == "mod_delay")    return std::make_unique<ModDelayNode>();
    if (type == "compressor")   return std::make_unique<CompressorNode>();
    if (type == "noisegate")    return std::make_unique<NoiseGateNode>();
    if (type == "reverb")       return std::make_unique<SchroederReverbNode>();
    // Logic (Wiremod)
    if (type == "and_gate")     return std::make_unique<ANDGateNode>();
    if (type == "or_gate")      return std::make_unique<ORGateNode>();
    if (type == "not_gate")     return std::make_unique<NOTGateNode>();
    if (type == "nand_gate")    return std::make_unique<NANDGateNode>();
    if (type == "nor_gate")     return std::make_unique<NORGateNode>();
    if (type == "xor_gate")     return std::make_unique<XORGateNode>();
    if (type == "xnor_gate")    return std::make_unique<XNORGateNode>();
    if (type == "buffer")       return std::make_unique<BufferNode>();
    if (type == "pulse")        return std::make_unique<PulseNode>();
    if (type == "gate_buffer")  return std::make_unique<GateBufferNode>();
    if (type == "sr_latch")     return std::make_unique<SRLatchNode>();
    if (type == "d_latch")      return std::make_unique<DLatchNode>();
    if (type == "d_ff")         return std::make_unique<DFlipFlopNode>();
    if (type == "t_ff")         return std::make_unique<TFlipFlopNode>();
    if (type == "jk_ff")        return std::make_unique<JKFlipFlopNode>();
    if (type == "demux")        return std::make_unique<DemuxNode>();
    if (type == "priority")     return std::make_unique<PriorityNode>();
    
    if (type == "comparator")   return std::make_unique<ComparatorNode>();
    if (type == "latch")        return std::make_unique<LatchNode>();
    if (type == "mux")          return std::make_unique<MuxNode>();
    if (type == "constant")     return std::make_unique<ConstantNode>();
    // Math
    if (type == "add")          return std::make_unique<AddNode>();
    if (type == "subtract")     return std::make_unique<SubtractNode>();
    if (type == "multiply")     return std::make_unique<MultiplyNode>();
    if (type == "divide")       return std::make_unique<DivideNode>();
    if (type == "modulo")       return std::make_unique<ModuloNode>();
    if (type == "round")        return std::make_unique<RoundNode>();
    if (type == "floor")        return std::make_unique<FloorNode>();
    if (type == "ceiling")      return std::make_unique<CeilingNode>();
    if (type == "sqrt")         return std::make_unique<SquareRootNode>();
    if (type == "power")        return std::make_unique<PowerNode>();
    if (type == "min")          return std::make_unique<MinNode>();
    if (type == "max")          return std::make_unique<MaxNode>();
    if (type == "sign")         return std::make_unique<SignNode>();
    if (type == "reciprocal")   return std::make_unique<ReciprocalNode>();
    if (type == "increment")    return std::make_unique<IncrementNode>();
    if (type == "decrement")    return std::make_unique<DecrementNode>();
    if (type == "average")      return std::make_unique<AverageNode>();
    
    if (type == "ranger")       return std::make_unique<RangerNode>();
    if (type == "smooth")       return std::make_unique<SmoothNode>();
    if (type == "clamp")        return std::make_unique<ClampNode>();
    if (type == "abs")          return std::make_unique<AbsNode>();
    if (type == "negate")       return std::make_unique<NegateNode>();
    // Sensors / Timing
    if (type == "env_follower") return std::make_unique<EnvelopeFollowerNode>();
    if (type == "sample_hold")  return std::make_unique<SampleHoldNode>();
    if (type == "clock")        return std::make_unique<ClockNode>();
    if (type == "counter")      return std::make_unique<CounterNode>();
    if (type == "sequencer")    return std::make_unique<SequencerNode>();
    // Scripting
    if (type == "expression")   return std::make_unique<ExpressionNode>();
    // MIDI
    if (type == "midi_note")    return std::make_unique<MidiNoteNode>();
    if (type == "midi_cc")      return std::make_unique<MidiCCNode>();
    if (type == "midi_pitchbend") return std::make_unique<MidiPitchBendNode>();
    if (type == "midi_clock")   return std::make_unique<MidiClockNode>();
    if (type == "midi_program") return std::make_unique<MidiProgramChangeNode>();
    if (type == "midi_pressure") return std::make_unique<MidiChannelPressureNode>();
    if (type == "midi_poly_pressure") return std::make_unique<MidiPolyPressureNode>();
    if (type == "midi_cc14")    return std::make_unique<MidiCC14Node>();
    if (type == "midi_song_pos") return std::make_unique<MidiSongPositionNode>();
    if (type == "midi_transport") return std::make_unique<MidiTransportNode>();
    if (type == "midi_note_gen") return std::make_unique<MidiNoteGenNode>();
    if (type == "midi_cc_gen")  return std::make_unique<MidiCCGenNode>();
    if (type == "midi_program_gen") return std::make_unique<MidiProgramGenNode>();
    if (type == "midi_pressure_gen") return std::make_unique<MidiPressureGenNode>();
    if (type == "midi_poly_pressure_gen") return std::make_unique<MidiPolyPressureGenNode>();
    if (type == "midi_pitchbend_gen") return std::make_unique<MidiPitchBendGenNode>();
    if (type == "midi_transport_gen") return std::make_unique<MidiTransportGenNode>();
    // Control Surface
    if (type == "ctrl_knob")    return std::make_unique<KnobNode>();
    if (type == "ctrl_fader")   return std::make_unique<FaderNode>();
    if (type == "ctrl_button")  return std::make_unique<ButtonNode>();
    if (type == "ctrl_toggle")  return std::make_unique<ToggleNode>();
    if (type == "ctrl_selector") return std::make_unique<SelectorNode>();
    if (type == "ctrl_xy")      return std::make_unique<XYPadNode>();
    // Display / Peripherals
    if (type == "disp_led")     return std::make_unique<LEDNode>();
    if (type == "disp_rgb_led") return std::make_unique<RGBLEDNode>();
    if (type == "disp_display") return std::make_unique<DisplayNode>();
    if (type == "disp_vu")      return std::make_unique<VUMeterNode>();
    if (type == "disp_tuner")   return std::make_unique<TunerDisplayNode>();
    if (type == "disp_7seg")    return std::make_unique<SevenSegNode>();
    if (type == "disp_text")    return std::make_unique<TextScreenNode>();
    if (type == "disp_console") return std::make_unique<ConsoleScreenNode>();
    if (type == "disp_scope")   return std::make_unique<OscilloscopeNode>();
    if (type == "disp_pixel")   return std::make_unique<PixelDisplayNode>();
    if (type == "disp_indicator") return std::make_unique<IndicatorNode>();
    if (type == "disp_sound")   return std::make_unique<SoundEmitterNode>();
    // I/O Peripherals
    if (type == "io_expression") return std::make_unique<ExpressionPedalNode>();
    if (type == "io_footswitch") return std::make_unique<FootswitchNode>();
    if (type == "io_cv_in")     return std::make_unique<CVInputNode>();
    if (type == "io_cv_out")    return std::make_unique<CVOutputNode>();
    return nullptr;
}

//==============================================================================
/**
 * The DSP graph runtime.
 *
 * Manages a collection of DSPNodes and connections between them.
 * Performs topological sorting and buffer routing for audio processing.
 */
class DSPGraph
{
public:
    DSPGraph() = default;

    /** Clear all nodes and connections. */
    void clear()
    {
        nodes.clear();
        connections.clear();
        sortDirty = true;
        nextID = 0;
    }

    //==========================================================================
    // Node management

    /** Add a node to the graph. Returns the assigned node ID. */
    int addNode (std::unique_ptr<DSPNode> node)
    {
        int id = nextID++;
        node->setNodeID (id);
        nodes[id] = std::move (node);
        sortDirty = true;
        return id;
    }

    /** Remove a node and all its connections. */
    void removeNode (int nodeID)
    {
        connections.erase (
            std::remove_if (connections.begin(), connections.end(),
                [nodeID](const NodeConnection& c) {
                    return c.sourceNodeID == nodeID || c.destNodeID == nodeID;
                }),
            connections.end());
        nodes.erase (nodeID);
        sortDirty = true;
    }

    /** Get a node by ID. */
    DSPNode* getNode (int nodeID)
    {
        auto it = nodes.find (nodeID);
        return it != nodes.end() ? it->second.get() : nullptr;
    }

    const std::map<int, std::unique_ptr<DSPNode>>& getNodes() const { return nodes; }

    //==========================================================================
    // Connection management

    /** Connect source node's output port to dest node's input port.
        Returns false if types are incompatible or connection is invalid. */
    bool connect (int srcID, int srcPort, int dstID, int dstPort)
    {
        // Validate nodes exist
        auto srcIt = nodes.find (srcID);
        auto dstIt = nodes.find (dstID);
        if (srcIt == nodes.end() || dstIt == nodes.end())
            return false;

        // Validate port indices
        auto& srcPorts = srcIt->second->getOutputPorts();
        auto& dstPorts = dstIt->second->getInputPorts();
        if (srcPort >= (int)srcPorts.size() || dstPort >= (int)dstPorts.size())
            return false;

        // Enforce type compatibility
        if (!NodePort::areCompatible (srcPorts[srcPort].type, dstPorts[dstPort].type))
            return false;

        // Don't allow duplicate connections
        for (const auto& c : connections)
            if (c.sourceNodeID == srcID && c.sourcePort == srcPort &&
                c.destNodeID == dstID && c.destPort == dstPort)
                return false;

        connections.push_back ({ srcID, srcPort, dstID, dstPort });
        sortDirty = true;
        return true;
    }

    /** Disconnect a specific connection. */
    bool disconnect (int srcID, int srcPort, int dstID, int dstPort)
    {
        auto it = std::remove_if (connections.begin(), connections.end(),
            [=](const NodeConnection& c) {
                return c.sourceNodeID == srcID && c.sourcePort == srcPort &&
                       c.destNodeID == dstID && c.destPort == dstPort;
            });
        if (it == connections.end()) return false;
        connections.erase (it, connections.end());
        sortDirty = true;
        return true;
    }

    const std::vector<NodeConnection>& getConnections() const { return connections; }

    //==========================================================================
    // Audio processing

    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        maxBlock = maxBlockSize;

        for (auto& [id, node] : nodes)
            node->prepare (sampleRate, maxBlockSize);

        // Allocate buffers
        int maxPorts = 0;
        for (auto& [id, node] : nodes)
            maxPorts += (int)node->getOutputPorts().size();
        maxPorts = juce::jmax (maxPorts, 16);

        bufferPool.clear();
        bufferPool.resize (maxPorts);
        for (auto& b : bufferPool)
            b.resize (maxBlockSize, 0.0f);

        topologicalSort();
    }

    /** Process audio through the graph (multi-channel version).
        AudioInputNode/AudioOutputNode read/write to specific channels based on their 'channel' param. */
    void processBlock (juce::AudioBuffer<float>& buffer, int numSamples,
                       juce::MidiBuffer* midi = nullptr)
    {
        currentMidiBuffer = midi;
        if (sortDirty) topologicalSort();

        // Clear all internal buffers
        for (auto& b : bufferPool)
            std::fill (b.begin(), b.begin() + numSamples, 0.0f);

        // Assign a buffer index to each node's output port
        std::map<std::pair<int,int>, int> portBufferMap;
        int bufIdx = 0;
        for (int nodeID : processingOrder)
        {
            auto* node = getNode (nodeID);
            if (!node) continue;
            for (int p = 0; p < (int)node->getOutputPorts().size(); ++p)
            {
                portBufferMap[{nodeID, p}] = bufIdx % (int)bufferPool.size();
                bufIdx++;
            }
        }

        // Process nodes in topological order
        for (int nodeID : processingOrder)
        {
            auto* node = getNode (nodeID);
            if (!node) continue;

            // Special case: AudioInputNode — read from selected channel
            if (node->getType() == "audio_input")
            {
                auto key = std::make_pair(nodeID, 0);
                if (portBufferMap.count(key))
                {
                    auto* inNode = dynamic_cast<AudioInputNode*>(node);
                    int ch = inNode ? (inNode->getSelectedChannel() - 1) : 0;
                    if (ch < buffer.getNumChannels())
                        std::copy (buffer.getReadPointer(ch),
                                   buffer.getReadPointer(ch) + numSamples,
                                   bufferPool[portBufferMap[key]].data());
                    else
                        std::fill (bufferPool[portBufferMap[key]].data(),
                                   bufferPool[portBufferMap[key]].data() + numSamples, 0.0f);
                }
                continue;
            }

            // Gather input buffers
            int numInputs = (int)node->getInputPorts().size();
            std::vector<const float*> inPtrs (numInputs, nullptr);

            for (const auto& conn : connections)
            {
                if (conn.destNodeID == nodeID)
                {
                    auto srcKey = std::make_pair (conn.sourceNodeID, conn.sourcePort);
                    if (portBufferMap.count (srcKey) && conn.destPort < numInputs)
                        inPtrs[conn.destPort] = bufferPool[portBufferMap[srcKey]].data();
                }
            }

            // Use silence for unconnected inputs
            std::vector<float> silence (numSamples, 0.0f);
            for (auto& p : inPtrs)
                if (p == nullptr) p = silence.data();

            // Gather output buffer pointers
            int numOutputs = (int)node->getOutputPorts().size();
            std::vector<float*> outPtrs (numOutputs, nullptr);
            for (int p = 0; p < numOutputs; ++p)
            {
                auto key = std::make_pair (nodeID, p);
                if (portBufferMap.count (key))
                    outPtrs[p] = bufferPool[portBufferMap[key]].data();
            }

            // Fallback output buffer
            std::vector<float> devNull (numSamples, 0.0f);
            for (auto& p : outPtrs)
                if (p == nullptr) p = devNull.data();

            // Set MIDI buffer on node for MIDI-aware nodes
            node->setMidiBuffer (currentMidiBuffer);

            // Process!
            node->process (inPtrs.data(), numInputs, outPtrs.data(), numOutputs, numSamples);

            // Special case: AudioOutputNode — write to selected channel
            if (node->getType() == "audio_output" && !inPtrs.empty())
            {
                auto* outNode = dynamic_cast<AudioOutputNode*>(node);
                int ch = outNode ? (outNode->getSelectedChannel() - 1) : 0;
                if (ch < buffer.getNumChannels())
                    std::copy (inPtrs[0], inPtrs[0] + numSamples,
                               buffer.getWritePointer(ch));
            }
        }

        currentMidiBuffer = nullptr;
    }

    /** Convenience: mono in/out wrapper (backward compatible). */
    void processBlock (const float* input, float* output, int numSamples,
                       juce::MidiBuffer* midi = nullptr)
    {
        // Wrap mono pointers into a temporary buffer
        juce::AudioBuffer<float> tempBuffer (1, numSamples);
        std::copy (input, input + numSamples, tempBuffer.getWritePointer(0));
        processBlock (tempBuffer, numSamples, midi);
        std::copy (tempBuffer.getReadPointer(0), tempBuffer.getReadPointer(0) + numSamples, output);
    }

    void reset()
    {
        for (auto& [id, node] : nodes)
            node->reset();
    }

    //==========================================================================
    // Serialization

    juce::var toJSON() const
    {
        auto* root = new juce::DynamicObject();

        juce::Array<juce::var> nodeArray;
        for (const auto& [id, node] : nodes)
            nodeArray.add (node->toJSON());
        root->setProperty ("nodes", nodeArray);

        juce::Array<juce::var> connArray;
        for (const auto& c : connections)
        {
            auto* co = new juce::DynamicObject();
            co->setProperty ("srcNode", c.sourceNodeID);
            co->setProperty ("srcPort", c.sourcePort);
            co->setProperty ("dstNode", c.destNodeID);
            co->setProperty ("dstPort", c.destPort);
            connArray.add (juce::var (co));
        }
        root->setProperty ("connections", connArray);
        return juce::var (root);
    }

    void fromJSON (const juce::var& json)
    {
        nodes.clear();
        connections.clear();

        if (auto* root = json.getDynamicObject())
        {
            if (auto* nodeArr = root->getProperty("nodes").getArray())
            {
                for (const auto& nv : *nodeArr)
                {
                    if (auto* no = nv.getDynamicObject())
                    {
                        juce::String type = no->getProperty("type").toString();
                        int id = (int) no->getProperty("id");
                        auto node = createNodeByType (type);
                        if (node)
                        {
                            node->setNodeID (id);
                            node->fromJSON (nv);
                            nextID = juce::jmax (nextID, id + 1);
                            nodes[id] = std::move (node);
                        }
                    }
                }
            }

            if (auto* connArr = root->getProperty("connections").getArray())
            {
                for (const auto& cv : *connArr)
                {
                    if (auto* co = cv.getDynamicObject())
                    {
                        connections.push_back ({
                            (int) co->getProperty ("srcNode"),
                            (int) co->getProperty ("srcPort"),
                            (int) co->getProperty ("dstNode"),
                            (int) co->getProperty ("dstPort")
                        });
                    }
                }
            }
        }

        sortDirty = true;
    }

    /** Get the current MIDI buffer (valid only during processBlock). */
    juce::MidiBuffer* getCurrentMidiBuffer() { return currentMidiBuffer; }

private:
    std::map<int, std::unique_ptr<DSPNode>> nodes;
    std::vector<NodeConnection> connections;
    std::vector<int> processingOrder;
    std::vector<std::vector<float>> bufferPool;
    int nextID = 1;
    bool sortDirty = true;
    double sr = 44100.0;
    int maxBlock = 512;
    juce::MidiBuffer* currentMidiBuffer = nullptr;

    /** Topological sort using Kahn's algorithm. */
    void topologicalSort()
    {
        processingOrder.clear();

        // Build adjacency and in-degree
        std::map<int, int> inDegree;
        std::map<int, std::vector<int>> adj;
        for (auto& [id, _] : nodes) { inDegree[id] = 0; adj[id] = {}; }

        for (const auto& c : connections)
        {
            adj[c.sourceNodeID].push_back (c.destNodeID);
            inDegree[c.destNodeID]++;
        }

        // Start with nodes that have no incoming connections
        std::vector<int> queue;
        for (auto& [id, deg] : inDegree)
            if (deg == 0) queue.push_back (id);

        while (!queue.empty())
        {
            int curr = queue.back();
            queue.pop_back();
            processingOrder.push_back (curr);

            for (int next : adj[curr])
            {
                inDegree[next]--;
                if (inDegree[next] == 0)
                    queue.push_back (next);
            }
        }

        sortDirty = false;
    }
};
