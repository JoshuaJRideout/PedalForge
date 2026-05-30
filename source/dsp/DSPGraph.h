#pragma once

#include "DSPNode.h"
#include "DSPNodeLibrary.h"
#include "NAMNode.h"
#include "PluginHostNode.h"
#include "MidiEditorNode.h"
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
    if (type == "nam")          return std::make_unique<NAMNode>("nam", "NAM Amp");
    if (type == "plugin_host")  return std::make_unique<PluginHostNode>();
    if (type == "aux_input")    return std::make_unique<AuxInputNode>();
    if (type == "stereo_mixer") return std::make_unique<StereoMixerNode>();
    if (type == "aux_output")   return std::make_unique<AuxOutputNode>();
    if (type == "matrix_mixer") return std::make_unique<MatrixMixerNode>();
    if (type == "matrix_mixer_xl") return std::make_unique<MatrixMixerXLNode>();

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
    if (type == "fuzz")         return std::make_unique<FuzzNode>();
    if (type == "phaser")       return std::make_unique<PhaserNode>();
    if (type == "flanger")      return std::make_unique<FlangerNode>();
    if (type == "peq")          return std::make_unique<ParametricEQNode>();
    if (type == "cabinet")      return std::make_unique<CabinetSimNode>();
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
    
    // Comparison
    if (type == "cmp_eq")       return std::make_unique<EqualNode>();
    if (type == "cmp_neq")      return std::make_unique<NotEqualNode>();
    if (type == "cmp_gt")       return std::make_unique<GreaterThanNode>();
    if (type == "cmp_lt")       return std::make_unique<LessThanNode>();
    if (type == "cmp_gte")      return std::make_unique<GreaterOrEqualNode>();
    if (type == "cmp_lte")      return std::make_unique<LessOrEqualNode>();
    if (type == "edge_rising")  return std::make_unique<RisingEdgeNode>();
    if (type == "edge_falling") return std::make_unique<FallingEdgeNode>();
    if (type == "change_det")   return std::make_unique<ChangeDetectorNode>();
    if (type == "delta")        return std::make_unique<DeltaNode>();
    
    // Time / Triggers
    if (type == "logic_delay")  return std::make_unique<LogicDelayNode>();
    if (type == "pulse_width")  return std::make_unique<PulseWidthNode>();
    if (type == "one_shot")     return std::make_unique<OneShotNode>();
    if (type == "debounce")     return std::make_unique<DebounceNode>();
    if (type == "blink")        return std::make_unique<BlinkNode>();
    if (type == "ramp")         return std::make_unique<RampNode>();
    if (type == "array")        return std::make_unique<ArrayNode>();
    
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
    if (type == "math_sin")     return std::make_unique<SineNode>();
    if (type == "math_cos")     return std::make_unique<CosineNode>();
    if (type == "math_tan")     return std::make_unique<TangentNode>();
    if (type == "math_sinh")    return std::make_unique<SinhNode>();
    if (type == "math_cosh")    return std::make_unique<CoshNode>();
    if (type == "math_tanh")    return std::make_unique<TanhNode>();
    if (type == "math_lerp")    return std::make_unique<LerpNode>();
    if (type == "math_exp")     return std::make_unique<ExpNode>();
    if (type == "math_log")     return std::make_unique<LogNode>();
    
    // Bitwise / Advanced
    if (type == "bit_and")      return std::make_unique<BitAndNode>();
    if (type == "bit_or")       return std::make_unique<BitOrNode>();
    if (type == "bit_xor")      return std::make_unique<BitXorNode>();
    if (type == "bit_not")      return std::make_unique<BitNotNode>();
    if (type == "bit_shl")      return std::make_unique<BitShiftLeftNode>();
    if (type == "bit_shr")      return std::make_unique<BitShiftRightNode>();
    if (type == "math_smoothstep") return std::make_unique<SmoothStepNode>();
    if (type == "accumulator")  return std::make_unique<AccumulatorNode>();
    
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
    if (type == "grid_sequencer") return std::make_unique<GridSequencerNode>();
    if (type == "midi_editor")  return std::make_unique<MidiEditorNode>();
    
    // Audio Sensors / Advanced Processing
    if (type == "pitch_det")    return std::make_unique<PitchDetectorNode>();
    if (type == "transient_det")return std::make_unique<TransientDetectorNode>();
    if (type == "zero_cross")   return std::make_unique<ZeroCrossingNode>();
    if (type == "pid_ctrl")     return std::make_unique<PIDControllerNode>();
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
    if (type == "ctrl_encoder")    return std::make_unique<EncoderNode>();
    if (type == "ctrl_pan")        return std::make_unique<PanNode>();
    if (type == "ctrl_modwheel")   return std::make_unique<WheelNode>();
    if (type == "ctrl_trim")       return std::make_unique<TrimNode>();
    // Display / Peripherals
    if (type == "disp_led")     return std::make_unique<LEDNode>();
    if (type == "disp_rgb_led") return std::make_unique<RGBLEDNode>();
    if (type == "disp_display") return std::make_unique<DisplayNode>();
    if (type == "disp_vu")      return std::make_unique<VUMeterNode>();
    if (type == "disp_tuner")   return std::make_unique<TunerDisplayNode>();
    if (type == "disp_7seg")    return std::make_unique<SevenSegNode>();
    if (type == "disp_easy")    return std::make_unique<EasyDisplayNode>();
    if (type == "disp_text")    return std::make_unique<TextScreenNode>();
    if (type == "disp_console") return std::make_unique<ConsoleScreenNode>();
    if (type == "disp_scope")   return std::make_unique<OscilloscopeNode>();
    if (type == "disp_pixel")   return std::make_unique<PixelDisplayNode>();
    if (type == "disp_shader")  return std::make_unique<ShaderDisplayNode>();
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
        node->autoExposeParams();
        nodes[id] = std::move (node);
        sortDirty = true;
        return id;
    }

    /** Add a node with a specific, predefined ID. */
    int addNode (std::unique_ptr<DSPNode> node, int id)
    {
        node->setNodeID (id);
        node->autoExposeParams();
        nodes[id] = std::move (node);
        nextID = juce::jmax (nextID, id + 1);
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
        {
            node->autoExposeParams();
            node->prepare (sampleRate, maxBlockSize);
        }

        // Allocate buffers
        int maxPorts = 0;
        int maxNodeInputPorts = 0;
        int maxNodeOutputPorts = 0;
        for (auto& [id, node] : nodes)
        {
            maxPorts += (int)node->getOutputPorts().size();
            maxNodeInputPorts  = juce::jmax (maxNodeInputPorts,  (int)node->getInputPorts().size());
            maxNodeOutputPorts = juce::jmax (maxNodeOutputPorts, (int)node->getOutputPorts().size());
        }
        maxPorts = juce::jmax (maxPorts, 16);

        bufferPool.clear();
        bufferPool.resize (maxPorts);
        for (auto& b : bufferPool)
            b.resize (maxBlockSize, 0.0f);

        // Pre-allocate scratch space for processBlock to avoid audio-thread allocations
        scratchInPtrs.resize (maxNodeInputPorts, nullptr);
        scratchConnected.resize (maxNodeInputPorts, 0);
        scratchOutPtrs.resize (maxNodeOutputPorts, nullptr);
        scratchSilence.resize (maxBlockSize, 0.0f);
        scratchDevNull.resize (maxBlockSize, 0.0f);

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

        // Process nodes in topological order
        for (int nodeID : processingOrder)
        {
            auto* node = getNode (nodeID);
            if (!node) continue;

            // Special case: AudioInputNode — read from all active output channels
            if (node->getType() == "audio_input")
            {
                int numChannelsToCopy = (int)node->getOutputPorts().size();
                for (int ch = 0; ch < numChannelsToCopy; ++ch)
                {
                    auto key = std::make_pair(nodeID, ch);
                    if (portBufferMap.count(key))
                    {
                        if (ch < buffer.getNumChannels())
                            std::copy (buffer.getReadPointer(ch),
                                       buffer.getReadPointer(ch) + numSamples,
                                       bufferPool[portBufferMap[key]].data());
                        else
                            std::fill (bufferPool[portBufferMap[key]].data(),
                                       bufferPool[portBufferMap[key]].data() + numSamples, 0.0f);
                    }
                }
                continue;
            }

            // Special case: AuxInputNode — read from Left and Right channels of secondary input bus (channels 2 & 3)
            if (node->getType() == "aux_input")
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto key = std::make_pair(nodeID, ch);
                    if (portBufferMap.count(key))
                    {
                        int actualCh = ch + 2;
                        if (actualCh < buffer.getNumChannels())
                            std::copy (buffer.getReadPointer(actualCh),
                                       buffer.getReadPointer(actualCh) + numSamples,
                                       bufferPool[portBufferMap[key]].data());
                        else
                            std::fill (bufferPool[portBufferMap[key]].data(),
                                       bufferPool[portBufferMap[key]].data() + numSamples, 0.0f);
                    }
                }
                continue;
            }

            // Gather input buffers — use pre-allocated scratch arrays
            int numInputs = (int)node->getInputPorts().size();
            for (int i = 0; i < numInputs; ++i)
            {
                scratchInPtrs[i] = nullptr;
                scratchConnected[i] = 0;
            }

            for (const auto& conn : connections)
            {
                if (conn.destNodeID == nodeID)
                {
                    auto srcKey = std::make_pair (conn.sourceNodeID, conn.sourcePort);
                    if (portBufferMap.count (srcKey) && conn.destPort < numInputs)
                    {
                        scratchInPtrs[conn.destPort] = bufferPool[portBufferMap[srcKey]].data();
                        scratchConnected[conn.destPort] = 1;   // a wire feeds this port
                    }
                }
            }

            // Use pre-allocated silence for unconnected inputs. (scratchConnected
            // still records which ports were genuinely wired, so a CV source
            // parked at 0 is distinguishable from "no wire".)
            std::fill (scratchSilence.begin(), scratchSilence.begin() + numSamples, 0.0f);
            for (int i = 0; i < numInputs; ++i)
                if (scratchInPtrs[i] == nullptr) scratchInPtrs[i] = scratchSilence.data();

            // Gather output buffer pointers — use pre-allocated scratch arrays
            int numOutputs = (int)node->getOutputPorts().size();
            for (int i = 0; i < numOutputs; ++i)
                scratchOutPtrs[i] = nullptr;
            for (int p = 0; p < numOutputs; ++p)
            {
                auto key = std::make_pair (nodeID, p);
                if (portBufferMap.count (key))
                    scratchOutPtrs[p] = bufferPool[portBufferMap[key]].data();
            }

            // Fallback output buffer — use pre-allocated devNull
            std::fill (scratchDevNull.begin(), scratchDevNull.begin() + numSamples, 0.0f);
            for (int i = 0; i < numOutputs; ++i)
                if (scratchOutPtrs[i] == nullptr) scratchOutPtrs[i] = scratchDevNull.data();

            // Set MIDI buffer on node for MIDI-aware nodes
            node->setMidiBuffer (currentMidiBuffer);

            // Apply block-rate CV modulation (connectivity mask lets a wired CV
            // source override a param authoritatively, even at value 0).
            node->applyControlInputs (scratchInPtrs.data(), numInputs, 0, scratchConnected.data());

            // Process!
            node->process (scratchInPtrs.data(), numInputs, scratchOutPtrs.data(), numOutputs, numSamples);

            // Record live port debug values for UI inspection
            if (node->lastInputValues.size() != (size_t) numInputs)
                node->lastInputValues.resize ((size_t) numInputs, 0.0f);
            for (int i = 0; i < numInputs; ++i)
                node->lastInputValues[(size_t) i] = scratchInPtrs[i] != nullptr ? scratchInPtrs[i][numSamples - 1] : 0.0f;

            if (node->lastOutputValues.size() != (size_t) numOutputs)
                node->lastOutputValues.resize ((size_t) numOutputs, 0.0f);
            for (int i = 0; i < numOutputs; ++i)
                node->lastOutputValues[(size_t) i] = scratchOutPtrs[i] != nullptr ? scratchOutPtrs[i][numSamples - 1] : 0.0f;

            // Special case: AudioOutputNode — write to all active channels
            if (node->getType() == "audio_output")
            {
                int numChannelsToCopy = (int)node->getInputPorts().size();
                for (int ch = 0; ch < numChannelsToCopy; ++ch)
                {
                    if (ch < numInputs && scratchInPtrs[ch] != nullptr && ch < buffer.getNumChannels())
                    {
                        if (hasAudioInput)
                        {
                            std::copy (scratchInPtrs[ch], scratchInPtrs[ch] + numSamples,
                                       buffer.getWritePointer(ch));
                        }
                        else
                        {
                            auto* dest = buffer.getWritePointer(ch);
                            const auto* src = scratchInPtrs[ch];
                            for (int i = 0; i < numSamples; ++i)
                            {
                                dest[i] += src[i];
                            }
                        }
                    }
                }
            }

            // Special case: AuxOutputNode — write to channels 2 and 3 (FX Send)
            if (node->getType() == "aux_output")
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    if (ch < numInputs && scratchInPtrs[ch] != nullptr && (ch + 2) < buffer.getNumChannels())
                    {
                        int actualCh = ch + 2;
                        auto* dest = buffer.getWritePointer(actualCh);
                        const auto* src = scratchInPtrs[ch];
                        for (int i = 0; i < numSamples; ++i)
                        {
                            dest[i] = src[i];
                        }
                    }
                }
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
                            node->autoExposeParams();
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
    std::map<std::pair<int,int>, int> portBufferMap;  // Built once in topologicalSort
    std::vector<const float*> scratchInPtrs;   // Pre-allocated for processBlock
    std::vector<char> scratchConnected;         // Per-input-port: 1 if a wire feeds it
    std::vector<float*> scratchOutPtrs;         // Pre-allocated for processBlock
    std::vector<float> scratchSilence;          // Pre-allocated silence buffer
    std::vector<float> scratchDevNull;          // Pre-allocated devnull buffer
    int nextID = 1;
    bool sortDirty = true;
    double sr = 44100.0;
    int maxBlock = 512;
    juce::MidiBuffer* currentMidiBuffer = nullptr;
    bool hasAudioInput = false;

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

        // Build portBufferMap (only when topology changes, not every audio callback)
        portBufferMap.clear();
        int bufIdx = 0;
        for (int nodeID : processingOrder)
        {
            auto* node = getNode (nodeID);
            if (!node) continue;
            for (int p = 0; p < (int)node->getOutputPorts().size(); ++p)
            {
                portBufferMap[{nodeID, p}] = bufIdx % juce::jmax(1, (int)bufferPool.size());
                bufIdx++;
            }
        }

        // Determine if this graph contains an audio_input node
        hasAudioInput = false;
        for (const auto& [id, node] : nodes)
        {
            if (node && node->getType() == "audio_input")
            {
                hasAudioInput = true;
                break;
            }
        }

        sortDirty = false;
    }
};
