#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/DSPGraph.h"

//==============================================================================
/**
 * Wraps a DSPGraph into a juce::AudioProcessor so it can be used
 * on the pedalboard alongside FaustPedal instances.
 *
 * The graph can be built programmatically or loaded from JSON.
 * Parameters from the graph's nodes are exposed as JUCE AudioParameters
 * for host automation and pedal UI mapping.
 */
class GraphPedalProcessor : public juce::AudioProcessor
{
public:
    GraphPedalProcessor (const juce::String& name, const juce::String& graphJSON = {})
        : AudioProcessor (BusesProperties()
                          .withInput  ("Input",    juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",   juce::AudioChannelSet::stereo(), true)
                          .withInput  ("FX Return", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("FX Send",   juce::AudioChannelSet::stereo(), false)),
          pedalName (name)
    {
        if (graphJSON.isNotEmpty())
            loadGraph (graphJSON);
    }

    ~GraphPedalProcessor() override = default;

    //==========================================================================
    DSPGraph& getDSPGraph() { return dspGraph; }
    const DSPGraph& getDSPGraph() const { return dspGraph; }

    void loadGraph (const juce::String& jsonString)
    {
        auto parsed = juce::JSON::parse (jsonString);
        dspGraph.fromJSON (parsed);
        rebuildParameters();
    }

    void setNodeFilePath(int nodeID, const juce::String& path)
    {
        if (auto* node = dspGraph.getNode(nodeID))
        {
            node->setFilePath(path);
        }
    }

    juce::String saveGraph() const
    {
        return juce::JSON::toString (dspGraph.toJSON());
    }

    /** Rebuild JUCE parameters from the current graph nodes.
        Creates AudioParameterFloat objects that the host/UI can discover. */
    void rebuildParameters()
    {
        paramBridge.clear();

        bypassParam = new juce::AudioParameterBool ({"bypass", 1}, "Bypass", false);
        addParameter (bypassParam);

        for (const auto& [nodeID, node] : dspGraph.getNodes())
        {
            // Skip I/O nodes — they have no user-facing params
            if (node->getType() == "audio_input" || node->getType() == "audio_output")
                continue;

            for (auto& param : const_cast<DSPNode*>(node.get())->getParams())
            {
                juce::String fullID = juce::String (nodeID) + "_" + param.id;
                juce::String displayName = node->getName() + " " + param.name;

                auto* juceParam = new juce::AudioParameterFloat (
                    juce::ParameterID { fullID, 1 },
                    displayName,
                    juce::NormalisableRange<float> (param.minVal, param.maxVal),
                    param.get());

                addParameter (juceParam); // AudioProcessor takes ownership
                paramBridge.push_back ({ juceParam, &param });
            }
        }
    }

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        dspGraph.prepare (sampleRate, samplesPerBlock);
    }

    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        // Support any layout up to 32 channels
        int totalIn = 0;
        for (auto& bus : layouts.inputBuses)
            totalIn += bus.size();

        int totalOut = 0;
        for (auto& bus : layouts.outputBuses)
            totalOut += bus.size();

        if (totalIn > 32 || totalOut > 32)
            return false;

        return true;
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        // Sync JUCE parameter values → graph NodeParam atomics
        for (auto& bridge : paramBridge)
            bridge.nodeParam->set (bridge.juceParam->get());

        // Check the graph to see if it's Mono or Stereo
        int maxInChannel = -1;
        int maxOutChannel = -1;
        for (const auto& [id, node] : dspGraph.getNodes())
        {
            if (node->getType() == "audio_input")
            {
                if (auto* param = node->getParam("channel"))
                    maxInChannel = juce::jmax(maxInChannel, (int)param->get() - 1);
            }
            if (node->getType() == "audio_output")
            {
                if (auto* param = node->getParam("channel"))
                    maxOutChannel = juce::jmax(maxOutChannel, (int)param->get() - 1);
            }
        }

        int numChans = buffer.getNumChannels();

        // If graph is Mono Input but we have Stereo (or more) data, mix to Mono L
        if (maxInChannel == 0 && numChans > 1)
        {
            auto* writeL = buffer.getWritePointer(0);
            for (int ch = 1; ch < numChans; ++ch)
            {
                auto* readR = buffer.getReadPointer(ch);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    writeL[i] += readR[i];
            }
            // Normalize gain
            float gain = 1.0f / (float)numChans;
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                writeL[i] *= gain;
        }

        if (bypassParam != nullptr && bypassParam->get())
        {
            // If bypassed, do not run DSP graph, and output original input.
            // Wait, since we mix Mono L above, the buffer has the input already.
            // Just copy it to output channels if needed below.
        }
        else
        {
            dspGraph.processBlock (buffer, buffer.getNumSamples(), &midi);
        }

        // If graph is Mono Output but we are outputting to Stereo (or more), copy L to all
        if (maxOutChannel == 0 && numChans > 1)
        {
            auto* readL = buffer.getReadPointer(0);
            for (int ch = 1; ch < numChans; ++ch)
            {
                auto* writeCh = buffer.getWritePointer(ch);
                if (readL != writeCh)
                    std::copy(readL, readL + buffer.getNumSamples(), writeCh);
            }
        }
    }

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    const juce::String getName() const override { return pedalName; }
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int   getNumPrograms() override { return 1; }
    int   getCurrentProgram() override { return 0; }
    void  setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void  changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        auto json = saveGraph();
        destData.append (json.toRawUTF8(), json.getNumBytesAsUTF8());
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        juce::String json = juce::String::fromUTF8 (static_cast<const char*>(data), sizeInBytes);
        loadGraph (json);
    }

private:
    juce::String pedalName;
    DSPGraph dspGraph;

    juce::AudioParameterBool* bypassParam = nullptr;

    /** Bridge between JUCE parameters and graph NodeParam atomics. */
    struct ParamBridge
    {
        juce::AudioParameterFloat* juceParam = nullptr;
        NodeParam* nodeParam = nullptr;
    };
    std::vector<ParamBridge> paramBridge;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphPedalProcessor)
};

//==============================================================================
/**
 * Helper to build common pedal graphs programmatically.
 */
namespace GraphPedalFactory
{
    // ─── MIDI & CV ───────────────────────────────────────────────────────────────

    inline std::unique_ptr<GraphPedalProcessor> createStepSequencer()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Step Sequencer");
        auto& g = proc->getDSPGraph();
        
        // Audio just passes through transparently
        int inID  = g.addNode (std::make_unique<AudioInputNode>());
        int outID = g.addNode (std::make_unique<AudioOutputNode>());
        g.connect (inID, 0, outID, 0);
        
        // Add the Grid Sequencer node
        g.addNode (std::make_unique<GridSequencerNode>());
        
        proc->rebuildParameters();
        return proc;
    }

    inline std::unique_ptr<GraphPedalProcessor> createPluginHost()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("VST/AU Host");
        auto& g = proc->getDSPGraph();
        
        int inL = g.addNode (std::make_unique<AudioInputNode>());
        g.getNode(inL)->getParam("channel")->set(1.0f);
        
        int inR = g.addNode (std::make_unique<AudioInputNode>());
        g.getNode(inR)->getParam("channel")->set(2.0f);
        
        int host = g.addNode (std::make_unique<PluginHostNode>());
        
        int outL = g.addNode (std::make_unique<AudioOutputNode>());
        g.getNode(outL)->getParam("channel")->set(1.0f);
        
        int outR = g.addNode (std::make_unique<AudioOutputNode>());
        g.getNode(outR)->getParam("channel")->set(2.0f);
        
        g.connect (inL, 0, host, 0);
        g.connect (inR, 0, host, 1);
        g.connect (host, 0, outL, 0);
        g.connect (host, 1, outR, 0);
        
        proc->rebuildParameters();
        return proc;
    }

    // ─── DRIVE ───────────────────────────────────────────────────────────────────

    /** 1. Clean Boost: Input → Gain → Output */
    inline std::unique_ptr<GraphPedalProcessor> createCleanBoost()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Clean Boost");
        auto& g = proc->getDSPGraph();
        int inID   = g.addNode (std::make_unique<AudioInputNode>());
        int gainID = g.addNode (std::make_unique<GainNode>());
        int outID  = g.addNode (std::make_unique<AudioOutputNode>());
        g.getNode(gainID)->getParam("gain")->set (6.0f);
        g.connect (inID, 0, gainID, 0);
        g.connect (gainID, 0, outID, 0);
        proc->rebuildParameters();
        return proc;
    }

    /** 2. Overdrive: Input → Gain → SoftClip → ToneStack → Gain(vol) → Output */
    inline std::unique_ptr<GraphPedalProcessor> createOverdrive()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Overdrive");
        auto& g = proc->getDSPGraph();
        int inID    = g.addNode (std::make_unique<AudioInputNode>());
        int gainID  = g.addNode (std::make_unique<GainNode>());
        int clipID  = g.addNode (std::make_unique<SoftClipNode>());
        int toneID  = g.addNode (std::make_unique<ToneStackNode>());
        int volID   = g.addNode (std::make_unique<GainNode>());
        int outID   = g.addNode (std::make_unique<AudioOutputNode>());

        g.getNode(gainID)->setName ("Drive Gain");
        g.getNode(volID)->setName ("Volume");
        g.getNode(gainID)->getParam("gain")->set (20.0f);
        g.getNode(volID)->getParam("gain")->set (-6.0f);
        g.getNode(clipID)->getParam("drive")->set (8.0f);

        g.connect (inID, 0, gainID, 0);
        g.connect (gainID, 0, clipID, 0);
        g.connect (clipID, 0, toneID, 0);
        g.connect (toneID, 0, volID, 0);
        g.connect (volID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    /** 3. Distortion: Input → Gain → HardClip → ToneStack → Gain(vol) → Output */
    inline std::unique_ptr<GraphPedalProcessor> createDistortion()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Distortion");
        auto& g = proc->getDSPGraph();
        int inID    = g.addNode (std::make_unique<AudioInputNode>());
        int gainID  = g.addNode (std::make_unique<GainNode>());
        int clipID  = g.addNode (std::make_unique<HardClipNode>());
        int toneID  = g.addNode (std::make_unique<ToneStackNode>());
        int volID   = g.addNode (std::make_unique<GainNode>());
        int outID   = g.addNode (std::make_unique<AudioOutputNode>());

        g.getNode(gainID)->setName ("Drive Gain");
        g.getNode(volID)->setName ("Volume");
        g.getNode(gainID)->getParam("gain")->set (30.0f);
        g.getNode(volID)->getParam("gain")->set (-10.0f);
        g.getNode(clipID)->getParam("drive")->set (12.0f);
        g.getNode(clipID)->getParam("threshold")->set (0.4f);

        g.connect (inID, 0, gainID, 0);
        g.connect (gainID, 0, clipID, 0);
        g.connect (clipID, 0, toneID, 0);
        g.connect (toneID, 0, volID, 0);
        g.connect (volID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    /** 4. Fuzz: Input → Gain → FuzzNode → ToneStack → Gain(vol) → Output */
    inline std::unique_ptr<GraphPedalProcessor> createFuzz()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Fuzz");
        auto& g = proc->getDSPGraph();
        int inID    = g.addNode (std::make_unique<AudioInputNode>());
        int gainID  = g.addNode (std::make_unique<GainNode>());
        int fuzzID  = g.addNode (std::make_unique<FuzzNode>());
        int toneID  = g.addNode (std::make_unique<ToneStackNode>());
        int volID   = g.addNode (std::make_unique<GainNode>());
        int outID   = g.addNode (std::make_unique<AudioOutputNode>());

        g.getNode(gainID)->setName ("Pre-Gain");
        g.getNode(volID)->setName ("Volume");
        g.getNode(gainID)->getParam("gain")->set (10.0f);
        g.getNode(volID)->getParam("gain")->set (-12.0f);
        g.getNode(fuzzID)->getParam("gain")->set (60.0f);
        g.getNode(fuzzID)->getParam("bias")->set (0.15f);

        g.connect (inID, 0, gainID, 0);
        g.connect (gainID, 0, fuzzID, 0);
        g.connect (fuzzID, 0, toneID, 0);
        g.connect (toneID, 0, volID, 0);
        g.connect (volID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    // ─── MODULATION ──────────────────────────────────────────────────────────────

    /** 5. Chorus: Input → Split → ModDelay(LFO) → Mix → Output */
    inline std::unique_ptr<GraphPedalProcessor> createChorus()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Chorus");
        auto& g = proc->getDSPGraph();
        int inID     = g.addNode (std::make_unique<AudioInputNode>());
        int splitID  = g.addNode (std::make_unique<SplitNode>());
        int lfoID    = g.addNode (std::make_unique<LFONode>());
        int modDelID = g.addNode (std::make_unique<ModDelayNode>());
        int mixID    = g.addNode (std::make_unique<MixNode>());
        int outID    = g.addNode (std::make_unique<AudioOutputNode>());

        g.getNode(lfoID)->getParam("rate")->set (1.5f);
        g.getNode(modDelID)->getParam("time")->set (0.007f);
        g.getNode(modDelID)->getParam("depth")->set (0.003f);
        g.getNode(mixID)->getParam("mix")->set (0.5f);

        g.connect (inID, 0, splitID, 0);
        g.connect (splitID, 0, mixID, 0);
        g.connect (splitID, 1, modDelID, 0);
        g.connect (lfoID, 0, modDelID, 1);
        g.connect (modDelID, 0, mixID, 1);
        g.connect (mixID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    /** 6. Phaser: Input → Split → Phaser(LFO) → Mix → Output */
    inline std::unique_ptr<GraphPedalProcessor> createPhaser()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Phaser");
        auto& g = proc->getDSPGraph();
        int inID    = g.addNode (std::make_unique<AudioInputNode>());
        int splitID = g.addNode (std::make_unique<SplitNode>());
        int lfoID   = g.addNode (std::make_unique<LFONode>());
        int phsID   = g.addNode (std::make_unique<PhaserNode>());
        int mixID   = g.addNode (std::make_unique<MixNode>());
        int outID   = g.addNode (std::make_unique<AudioOutputNode>());

        g.getNode(lfoID)->getParam("rate")->set (0.5f);
        g.getNode(phsID)->getParam("depth")->set (0.8f);
        g.getNode(mixID)->getParam("mix")->set (0.5f);

        g.connect (inID, 0, splitID, 0);
        g.connect (splitID, 0, mixID, 0);
        g.connect (splitID, 1, phsID, 0);
        g.connect (lfoID, 0, phsID, 1);
        g.connect (phsID, 0, mixID, 1);
        g.connect (mixID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    /** 7. Flanger: Input → Split → Flanger(LFO) → Mix → Output */
    inline std::unique_ptr<GraphPedalProcessor> createFlanger()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Flanger");
        auto& g = proc->getDSPGraph();
        int inID    = g.addNode (std::make_unique<AudioInputNode>());
        int splitID = g.addNode (std::make_unique<SplitNode>());
        int lfoID   = g.addNode (std::make_unique<LFONode>());
        int flgID   = g.addNode (std::make_unique<FlangerNode>());
        int mixID   = g.addNode (std::make_unique<MixNode>());
        int outID   = g.addNode (std::make_unique<AudioOutputNode>());

        g.getNode(lfoID)->getParam("rate")->set (0.2f);
        g.getNode(flgID)->getParam("depth")->set (0.9f);
        g.getNode(flgID)->getParam("feedback")->set (0.6f);
        g.getNode(mixID)->getParam("mix")->set (0.5f);

        g.connect (inID, 0, splitID, 0);
        g.connect (splitID, 0, mixID, 0);
        g.connect (splitID, 1, flgID, 0);
        g.connect (lfoID, 0, flgID, 1);
        g.connect (flgID, 0, mixID, 1);
        g.connect (mixID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    /** 8. Tremolo: Input → Multiply(LFO) → Output */
    inline std::unique_ptr<GraphPedalProcessor> createTremolo()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Tremolo");
        auto& g = proc->getDSPGraph();
        int inID   = g.addNode (std::make_unique<AudioInputNode>());
        int lfoID  = g.addNode (std::make_unique<LFONode>());
        int mulID  = g.addNode (std::make_unique<MultiplyNode>());
        int outID  = g.addNode (std::make_unique<AudioOutputNode>());

        g.getNode(lfoID)->getParam("rate")->set (5.0f);
        g.getNode(lfoID)->getParam("depth")->set (1.0f);

        // LFO goes 0..1, we want 1..0 or similar.
        // Actually, just multiply the audio by LFO. LFO outputs bipolar (-1 to 1).
        // Let's map it 0 to 1 for amplitude mod.
        int addID = g.addNode (std::make_unique<AddNode>());
        g.getNode(addID)->getParam("value")->set(1.0f);
        int divID = g.addNode (std::make_unique<DivideNode>());
        g.getNode(divID)->getParam("value")->set(2.0f);

        g.connect (lfoID, 0, addID, 0);
        g.connect (addID, 0, divID, 0); // now LFO is 0 to 1
        
        g.connect (inID, 0, mulID, 0);
        g.connect (divID, 0, mulID, 1);
        g.connect (mulID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    // ─── TIME & DYNAMICS ────────────────────────────────────────────────────────

    /** 9. Delay: Input → Split → Delay (w/ feedback) → Mix → Output */
    inline std::unique_ptr<GraphPedalProcessor> createDelay()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Delay");
        auto& g = proc->getDSPGraph();
        int inID    = g.addNode (std::make_unique<AudioInputNode>());
        int splitID = g.addNode (std::make_unique<SplitNode>());
        int delayID = g.addNode (std::make_unique<DelayNode>());
        int mixID   = g.addNode (std::make_unique<MixNode>());
        int outID   = g.addNode (std::make_unique<AudioOutputNode>());

        g.getNode(delayID)->getParam("time")->set(0.4f);
        g.getNode(delayID)->getParam("feedback")->set(0.4f);
        g.getNode(mixID)->getParam("mix")->set(0.3f);

        g.connect (inID, 0, splitID, 0);
        g.connect (splitID, 0, mixID, 0);
        g.connect (splitID, 1, delayID, 0);
        g.connect (delayID, 0, mixID, 1);
        g.connect (mixID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    /** 10. Reverb: Input → Reverb → Output */
    inline std::unique_ptr<GraphPedalProcessor> createReverb()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Reverb");
        auto& g = proc->getDSPGraph();
        int inID  = g.addNode (std::make_unique<AudioInputNode>());
        int revID = g.addNode (std::make_unique<SchroederReverbNode>());
        int outID = g.addNode (std::make_unique<AudioOutputNode>());
        
        g.getNode(revID)->getParam("size")->set(0.7f);
        g.getNode(revID)->getParam("mix")->set(0.4f);

        g.connect (inID, 0, revID, 0);
        g.connect (revID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    /** 11. Compressor: Input → Compressor → Output */
    inline std::unique_ptr<GraphPedalProcessor> createCompressor()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Compressor");
        auto& g = proc->getDSPGraph();
        int inID   = g.addNode (std::make_unique<AudioInputNode>());
        int compID = g.addNode (std::make_unique<CompressorNode>());
        int outID  = g.addNode (std::make_unique<AudioOutputNode>());
        
        g.getNode(compID)->getParam("threshold")->set(-20.0f);
        g.getNode(compID)->getParam("ratio")->set(4.0f);

        g.connect (inID, 0, compID, 0);
        g.connect (compID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    /** 12. Noise Gate: Input → NoiseGate → Output */
    inline std::unique_ptr<GraphPedalProcessor> createNoiseGate()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Noise Gate");
        auto& g = proc->getDSPGraph();
        int inID   = g.addNode (std::make_unique<AudioInputNode>());
        int gateID = g.addNode (std::make_unique<NoiseGateNode>());
        int outID  = g.addNode (std::make_unique<AudioOutputNode>());
        
        g.getNode(gateID)->getParam("threshold")->set(-50.0f);

        g.connect (inID, 0, gateID, 0);
        g.connect (gateID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    // ─── EQ / FILTER ────────────────────────────────────────────────────────────

    /** 13. Parametric EQ: Input → PEQ1 → PEQ2 → PEQ3 → Output */
    inline std::unique_ptr<GraphPedalProcessor> createParametricEQ()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Parametric EQ");
        auto& g = proc->getDSPGraph();
        int inID   = g.addNode (std::make_unique<AudioInputNode>());
        int eq1ID  = g.addNode (std::make_unique<ParametricEQNode>());
        int eq2ID  = g.addNode (std::make_unique<ParametricEQNode>());
        int eq3ID  = g.addNode (std::make_unique<ParametricEQNode>());
        int outID  = g.addNode (std::make_unique<AudioOutputNode>());

        g.getNode(eq1ID)->setName("Low");
        g.getNode(eq2ID)->setName("Mid");
        g.getNode(eq3ID)->setName("High");

        g.getNode(eq1ID)->getParam("freq")->set(250.0f);
        g.getNode(eq2ID)->getParam("freq")->set(1000.0f);
        g.getNode(eq3ID)->getParam("freq")->set(4000.0f);

        g.connect (inID, 0, eq1ID, 0);
        g.connect (eq1ID, 0, eq2ID, 0);
        g.connect (eq2ID, 0, eq3ID, 0);
        g.connect (eq3ID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    /** 14. Tone Control: Input → ToneStack → Output */
    inline std::unique_ptr<GraphPedalProcessor> createToneControl()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Tone Control");
        auto& g = proc->getDSPGraph();
        int inID   = g.addNode (std::make_unique<AudioInputNode>());
        int toneID = g.addNode (std::make_unique<ToneStackNode>());
        int outID  = g.addNode (std::make_unique<AudioOutputNode>());
        
        g.connect (inID, 0, toneID, 0);
        g.connect (toneID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    // ─── UTILITY ─────────────────────────────────────────────────────────────────

    /** 15. Cabinet Sim: Input → CabinetSim → Output */
    inline std::unique_ptr<GraphPedalProcessor> createCabinetSim()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Cabinet Sim");
        auto& g = proc->getDSPGraph();
        int inID   = g.addNode (std::make_unique<AudioInputNode>());
        int cabID  = g.addNode (std::make_unique<CabinetSimNode>());
        int outID  = g.addNode (std::make_unique<AudioOutputNode>());
        
        g.getNode(cabID)->getParam("cutoff")->set(3500.0f);
        g.getNode(cabID)->getParam("resonance")->set(0.5f);

        g.connect (inID, 0, cabID, 0);
        g.connect (cabID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    inline std::unique_ptr<GraphPedalProcessor> createNAMAmp()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("NAM Amp");
        auto& g = proc->getDSPGraph();
        int inID   = g.addNode (std::make_unique<AudioInputNode>());
        int namID  = g.addNode (std::make_unique<NAMNode>("nam", "NAM Amp"));
        int outID  = g.addNode (std::make_unique<AudioOutputNode>());

        g.connect (inID, 0, namID, 0);
        g.connect (namID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    inline std::unique_ptr<GraphPedalProcessor> createIRLoader()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("IR Cabinet");
        auto& g = proc->getDSPGraph();
        int inID   = g.addNode (std::make_unique<AudioInputNode>());
        int irID   = g.addNode (std::make_unique<IRNode>());
        int outID  = g.addNode (std::make_unique<AudioOutputNode>());

        g.connect (inID, 0, irID, 0);
        g.connect (inID, 0, irID, 1);
        g.connect (irID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    inline std::unique_ptr<GraphPedalProcessor> createIRReverb()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("IR Reverb");
        auto& g = proc->getDSPGraph();
        int inID   = g.addNode (std::make_unique<AudioInputNode>());
        int irID   = g.addNode (std::make_unique<IRNode>());
        int outID  = g.addNode (std::make_unique<AudioOutputNode>());

        g.getNode(irID)->getParam("mix")->set(0.5f);

        g.connect (inID, 0, irID, 0);
        g.connect (inID, 0, irID, 1);
        g.connect (irID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }
}

