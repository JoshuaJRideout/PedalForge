#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/DSPGraph.h"
#include "../engine/PedalInstance.h"

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
    static BusesProperties createBusesProperties (const juce::String& name)
    {
        if (name == "Matrix Mixer XL")
        {
            BusesProperties buses;
            for (int i = 0; i < 16; ++i)
            {
                buses = buses.withInput  ("Input " + juce::String (i + 1),  juce::AudioChannelSet::stereo(), true);
                buses = buses.withOutput ("Output " + juce::String (i + 1), juce::AudioChannelSet::stereo(), true);
            }
            return buses;
        }

        return BusesProperties()
            .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
            .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
            .withInput  ("FX Return", juce::AudioChannelSet::stereo(), name == "Mixer" || name == "Matrix Mixer")
            .withOutput ("FX Send",   juce::AudioChannelSet::stereo(), name == "Matrix Mixer");
    }

    GraphPedalProcessor (const juce::String& name, const juce::String& graphJSON = {})
        : AudioProcessor (createBusesProperties (name)),
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
        else if (auto* node = dspGraph.getNode(nodeID - 1))
        {
            node->setFilePath(path);
        }
        else if (auto* node = dspGraph.getNode(nodeID + 1))
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

            // For Aether Rig, skip exposing node 8 (EQ Right) and 9 (Reverb Right)
            if (pedalName == "Aether Rig" && (nodeID == 8 || nodeID == 9))
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

    std::shared_ptr<PedalMeters> meters;
    void setMeters (std::shared_ptr<PedalMeters> m) { meters = m; }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        // Sync JUCE parameter values → graph NodeParam atomics
        for (auto& bridge : paramBridge)
            bridge.nodeParam->set (bridge.juceParam->get());

        // For Aether Rig, sync duplicate Right EQ (8) and Right Reverb (9) from Left (6 and 7)
        if (pedalName == "Aether Rig")
        {
            if (auto* eqL = dspGraph.getNode (6))
            {
                if (auto* eqR = dspGraph.getNode (8))
                {
                    if (auto* pBassL = eqL->getParam ("bass"))
                        if (auto* pBassR = eqR->getParam ("bass"))
                            pBassR->set (pBassL->get());
                            
                    if (auto* pMidL = eqL->getParam ("mid"))
                        if (auto* pMidR = eqR->getParam ("mid"))
                            pMidR->set (pMidL->get());
                            
                    if (auto* pTrebleL = eqL->getParam ("treble"))
                        if (auto* pTrebleR = eqR->getParam ("treble"))
                            pTrebleR->set (pTrebleL->get());
                }
            }
            if (auto* revL = dspGraph.getNode (7))
            {
                if (auto* revR = dspGraph.getNode (9))
                {
                    if (auto* pSizeL = revL->getParam ("size"))
                        if (auto* pSizeR = revR->getParam ("size"))
                            pSizeR->set (pSizeL->get());
                            
                    if (auto* pDampL = revL->getParam ("damping"))
                        if (auto* pDampR = revR->getParam ("damping"))
                            pDampR->set (pDampL->get());
                            
                    if (auto* pMixL = revL->getParam ("mix"))
                        if (auto* pMixR = revR->getParam ("mix"))
                            pMixR->set (pMixL->get());
                }
            }
        }

        if (bypassParam == nullptr || !bypassParam->get())
        {
            dspGraph.processBlock (buffer, buffer.getNumSamples(), &midi);
        }

        if (meters != nullptr)
        {
            int numSamples = buffer.getNumSamples();
            int numChans = buffer.getNumChannels();
            if (numSamples > 0)
            {
                meters->outRMS[0].store (buffer.getRMSLevel(0, 0, numSamples));
                if (numChans > 1)
                    meters->outRMS[1].store (buffer.getRMSLevel(1, 0, numSamples));
            }
            
            if (!midi.isEmpty())
                meters->midiOut.store (true);
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
        
        // Audio passes through transparently in stereo
        int inID  = g.addNode (std::make_unique<AudioInputNode>(), 1);
        int seqID = g.addNode (std::make_unique<GridSequencerNode>(), 2);
        int outID = g.addNode (std::make_unique<AudioOutputNode>(), 3);
        
        g.connect (inID, 0, outID, 0);
        g.connect (inID, 1, outID, 1);
        
        proc->rebuildParameters();
        return proc;
    }

    inline std::unique_ptr<GraphPedalProcessor> createMidiEditor()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("MIDI Editor");
        auto& g = proc->getDSPGraph();
        
        int inID  = g.addNode (std::make_unique<AudioInputNode>(), 1);
        int seqID = g.addNode (std::make_unique<MidiEditorNode>(), 2);
        int outID = g.addNode (std::make_unique<AudioOutputNode>(), 3);
        
        g.connect (inID, 0, outID, 0);
        g.connect (inID, 1, outID, 1);
        
        proc->rebuildParameters();
        return proc;
    }

    inline std::unique_ptr<GraphPedalProcessor> createPluginHost()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("VST/AU Host");
        auto& g = proc->getDSPGraph();
        
        int inID  = g.addNode (std::make_unique<AudioInputNode>());
        int host  = g.addNode (std::make_unique<PluginHostNode>());
        int outID = g.addNode (std::make_unique<AudioOutputNode>());
        
        g.connect (inID, 0, host, 0);
        g.connect (inID, 1, host, 1);
        g.connect (host, 0, outID, 0);
        g.connect (host, 1, outID, 1);
        
        proc->rebuildParameters();
        return proc;
    }

    // ─── DRIVE ───────────────────────────────────────────────────────────────────

    // ── HONEST builds (no hidden C++ graph) ──────────────────────────────────
    // Clean Boost, Overdrive, Distortion and Fuzz now declare their full DSP
    // graph + control-surface-node twins in their PedalDesign
    // (FactoryDesigns::create*); the registry builds the processor straight from
    // that declared effectsGraph via processorFromDeclaredGraph(). Their old
    // hand-built C++ graphs that used to live here have been retired.

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
        g.connect (mixID, 0, outID, 1);

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
        g.connect (mixID, 0, outID, 1);

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
        g.connect (mixID, 0, outID, 1);

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
        g.connect (mulID, 0, outID, 1);

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
        g.connect (mixID, 0, outID, 1);

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
        g.connect (revID, 0, outID, 1);

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
        g.connect (compID, 0, outID, 1);

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
        g.connect (gateID, 0, outID, 1);

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
        g.connect (eq3ID, 0, outID, 1);

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
        g.connect (toneID, 0, outID, 1);

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
        g.connect (cabID, 0, outID, 1);

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
        g.connect (namID, 0, outID, 1);

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
        g.connect (inID, 1, irID, 1);
        g.connect (irID, 0, outID, 0);
        g.connect (irID, 1, outID, 1);

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
        g.connect (inID, 1, irID, 1);
        g.connect (irID, 0, outID, 0);
        g.connect (irID, 1, outID, 1);

        proc->rebuildParameters();
        return proc;
    }

    // ─── AETHER RIG ─────────────────────────────────────────────────────────────

    /** Aether Rig: Complete channel strip.
        Input → NoiseGate → SoftClip(Boost) → NAM(Amp) → IR(Cab) → ToneStack(EQ) → Reverb → Output
        
        Node IDs:
          0 = audio_input
          1 = audio_output
          2 = noisegate
          3 = softclip (boost/OD)
          4 = nam (amp engine)
          5 = ir (cab sim)
          6 = tonestack (EQ Left)
          7 = reverb (Reverb Left)
          8 = tonestack (EQ Right)
          9 = reverb (Reverb Right)
          10 = add (summing L+R)
    */
    inline std::unique_ptr<GraphPedalProcessor> createAetherRig()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Aether Rig");
        auto& g = proc->getDSPGraph();

        int inID   = g.addNode (std::make_unique<AudioInputNode>(),        0);
        int outID  = g.addNode (std::make_unique<AudioOutputNode>(),       1);
        int gateID = g.addNode (std::make_unique<NoiseGateNode>(),         2);
        int odID   = g.addNode (std::make_unique<SoftClipNode>(),          3);
        int namID  = g.addNode (std::make_unique<NAMNode>("nam", "NAM Amp"), 4);
        int irID   = g.addNode (std::make_unique<IRNode>(),                5);
        int eqID   = g.addNode (std::make_unique<ToneStackNode>(),         6);
        int revID  = g.addNode (std::make_unique<SchroederReverbNode>(),   7);
        int eqID_R = g.addNode (std::make_unique<ToneStackNode>(),         8);
        int revID_R = g.addNode (std::make_unique<SchroederReverbNode>(),  9);
        int sumID  = g.addNode (std::make_unique<AddNode>(),              10);

        // Sensible defaults
        g.getNode(gateID)->getParam("threshold")->set (-50.0f);
        g.getNode(odID)->getParam("drive")->set (1.0f);      // Bypass-level drive
        g.getNode(revID)->getParam("size")->set (0.5f);
        g.getNode(revID)->getParam("mix")->set (0.0f);        // Reverb off by default
        g.getNode(revID_R)->getParam("size")->set (0.5f);
        g.getNode(revID_R)->getParam("mix")->set (0.0f);

        // Mono chain: In L + In R -> Sum -> Gate -> OD -> NAM
        g.connect (inID,   0, sumID,  0);
        g.connect (inID,   1, sumID,  1);
        g.connect (sumID,  0, gateID, 0);
        g.connect (gateID, 0, odID,   0);
        g.connect (odID,   0, namID,  0);

        // NAM (mono) → IR (stereo in: duplicate to L+R)
        g.connect (namID,  0, irID,   0);   // NAM out → IR in_l
        g.connect (namID,  0, irID,   1);   // NAM out → IR in_r

        // IR Left → EQ Left → Reverb Left → Output L
        g.connect (irID,   0, eqID,   0);   // IR out_l → EQ Left
        g.connect (eqID,   0, revID,  0);   // EQ Left → Reverb Left
        g.connect (revID,  0, outID,  0);   // Reverb Left → Output L

        // IR Right → EQ Right → Reverb Right → Output R
        g.connect (irID,   1, eqID_R,  0);   // IR out_r → EQ Right
        g.connect (eqID_R, 0, revID_R, 0);   // EQ Right → Reverb Right
        g.connect (revID_R,0, outID,  1);   // Reverb Right → Output R

        proc->rebuildParameters();
        return proc;
    }

    // ─── TUTORIAL ────────────────────────────────────────────────────────────────

    /** Tutorial 1 — Hello Gain: The simplest possible pedal.
        Input → Gain → Output */
    inline std::unique_ptr<GraphPedalProcessor> createTutorialHelloGain()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Hello Gain");
        auto& g = proc->getDSPGraph();
        int inID   = g.addNode (std::make_unique<AudioInputNode>(), 1);   // 1
        int gainID = g.addNode (std::make_unique<GainNode>(), 2);         // 2
        int outID  = g.addNode (std::make_unique<AudioOutputNode>(), 3);  // 3
        g.getNode(gainID)->getParam("gain")->set (0.0f);

        // Visual Coordinates
        g.getNode(inID)->visualX = 80.0f;   g.getNode(inID)->visualY = 200.0f;
        g.getNode(gainID)->visualX = 300.0f; g.getNode(gainID)->visualY = 200.0f;
        g.getNode(outID)->visualX = 520.0f;  g.getNode(outID)->visualY = 200.0f;

        g.connect (inID, 0, gainID, 0);
        g.connect (gainID, 0, outID, 0);
        g.connect (gainID, 0, outID, 1);
        proc->rebuildParameters();
        return proc;
    }

    /** Tutorial 2 — Filter Sweep: Low-pass filter with cutoff & resonance.
        Input → LowPass → Output */
    inline std::unique_ptr<GraphPedalProcessor> createTutorialFilterSweep()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Filter Sweep");
        auto& g = proc->getDSPGraph();
        int inID  = g.addNode (std::make_unique<AudioInputNode>(), 1);    // 1
        int lpID  = g.addNode (std::make_unique<LowPassNode>(), 2);       // 2
        int outID = g.addNode (std::make_unique<AudioOutputNode>(), 3);   // 3
        g.getNode(lpID)->getParam("freq")->set (1000.0f);
        g.getNode(lpID)->getParam("q")->set (0.5f);

        // Visual Coordinates
        g.getNode(inID)->visualX = 80.0f;  g.getNode(inID)->visualY = 200.0f;
        g.getNode(lpID)->visualX = 300.0f; g.getNode(lpID)->visualY = 200.0f;
        g.getNode(outID)->visualX = 520.0f; g.getNode(outID)->visualY = 200.0f;

        g.connect (inID, 0, lpID, 0);
        g.connect (lpID, 0, outID, 0);
        g.connect (lpID, 0, outID, 1);
        proc->rebuildParameters();
        return proc;
    }

    /** Tutorial 3 — Tremolo 101: LFO modulating volume.
        Input → Multiply(audio × scaled-LFO) → Output
        LFO → Add(+1) → Divide(/2) → Multiply input 1 */
    inline std::unique_ptr<GraphPedalProcessor> createTutorialTremolo101()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Tremolo 101");
        auto& g = proc->getDSPGraph();
        int lfoID = g.addNode (std::make_unique<LFONode>(), 1);           // 1
        int inID  = g.addNode (std::make_unique<AudioInputNode>(), 2);    // 2
        int mulID = g.addNode (std::make_unique<MultiplyNode>(), 3);      // 3
        int outID = g.addNode (std::make_unique<AudioOutputNode>(), 4);   // 4

        g.getNode(lfoID)->getParam("rate")->set (5.0f);
        g.getNode(lfoID)->getParam("depth")->set (1.0f);

        // Scale bipolar LFO (-1..1) to unipolar (0..1)
        int addID = g.addNode (std::make_unique<AddNode>(), 5);           // 5
        g.getNode(addID)->getParam("value")->set (1.0f);
        int divID = g.addNode (std::make_unique<DivideNode>(), 6);        // 6
        g.getNode(divID)->getParam("value")->set (2.0f);

        // Visual Coordinates
        g.getNode(inID)->visualX = 80.0f;   g.getNode(inID)->visualY = 200.0f;
        g.getNode(lfoID)->visualX = 80.0f;  g.getNode(lfoID)->visualY = 380.0f;
        g.getNode(addID)->visualX = 260.0f; g.getNode(addID)->visualY = 380.0f;
        g.getNode(divID)->visualX = 440.0f; g.getNode(divID)->visualY = 380.0f;
        g.getNode(mulID)->visualX = 300.0f; g.getNode(mulID)->visualY = 200.0f;
        g.getNode(outID)->visualX = 520.0f;  g.getNode(outID)->visualY = 200.0f;

        g.connect (lfoID, 0, addID, 0);
        g.connect (addID, 0, divID, 0);
        g.connect (inID, 0, mulID, 0);
        g.connect (divID, 0, mulID, 1);
        g.connect (mulID, 0, outID, 0);
        g.connect (mulID, 0, outID, 1);

        proc->rebuildParameters();
        return proc;
    }

    /** Tutorial 4 — Delay Lab: Parallel dry/wet with delay & feedback.
        Input → Split → Delay → Mix → Output */
    inline std::unique_ptr<GraphPedalProcessor> createTutorialDelayLab()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Delay Lab");
        auto& g = proc->getDSPGraph();
        int inID    = g.addNode (std::make_unique<AudioInputNode>(), 1);  // 1
        int delayID = g.addNode (std::make_unique<DelayNode>(), 2);       // 2
        int mixID   = g.addNode (std::make_unique<MixNode>(), 3);         // 3
        int splitID = g.addNode (std::make_unique<SplitNode>(), 4);       // 4
        int outID   = g.addNode (std::make_unique<AudioOutputNode>(), 5); // 5

        g.getNode(delayID)->getParam("time")->set (0.4f);
        g.getNode(delayID)->getParam("feedback")->set (0.4f);
        g.getNode(mixID)->getParam("mix")->set (0.3f);

        // Visual Coordinates
        g.getNode(inID)->visualX = 80.0f;     g.getNode(inID)->visualY = 200.0f;
        g.getNode(splitID)->visualX = 220.0f;  g.getNode(splitID)->visualY = 200.0f;
        g.getNode(delayID)->visualX = 380.0f;  g.getNode(delayID)->visualY = 350.0f;
        g.getNode(mixID)->visualX = 540.0f;    g.getNode(mixID)->visualY = 200.0f;
        g.getNode(outID)->visualX = 700.0f;    g.getNode(outID)->visualY = 200.0f;

        g.connect (inID, 0, splitID, 0);
        g.connect (splitID, 0, mixID, 0);      // dry path
        g.connect (splitID, 1, delayID, 0);     // wet path
        g.connect (delayID, 0, mixID, 1);
        g.connect (mixID, 0, outID, 0);
        g.connect (mixID, 0, outID, 1);

        proc->rebuildParameters();
        return proc;
    }

    /** Tutorial 5 — Mini Synth: Basic MIDI synth voice.
        MidiNote → Oscillator → VCA → AudioOutput
        MidiNote gate → ADSR → VCA control */
    inline std::unique_ptr<GraphPedalProcessor> createTutorialMiniSynth()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Mini Synth");
        auto& g = proc->getDSPGraph();
        int outID  = g.addNode (std::make_unique<AudioOutputNode>(), 1);  // 1
        int oscID  = g.addNode (std::make_unique<OscillatorNode>(), 2);   // 2
        int adsrID = g.addNode (std::make_unique<ADSRNode>(), 3);         // 3
        int vcaID  = g.addNode (std::make_unique<VCANode>(), 4);          // 4
        int midiID = g.addNode (std::make_unique<MidiNoteNode>(), 5);     // 5

        g.getNode(adsrID)->getParam("attack")->set (0.01f);
        g.getNode(adsrID)->getParam("decay")->set (0.1f);
        g.getNode(adsrID)->getParam("sustain")->set (0.7f);
        g.getNode(adsrID)->getParam("release")->set (0.3f);

        // Visual Coordinates
        g.getNode(midiID)->visualX = 80.0f;  g.getNode(midiID)->visualY = 200.0f;
        g.getNode(oscID)->visualX = 260.0f;  g.getNode(oscID)->visualY = 100.0f;
        g.getNode(adsrID)->visualX = 260.0f; g.getNode(adsrID)->visualY = 320.0f;
        g.getNode(vcaID)->visualX = 440.0f;  g.getNode(vcaID)->visualY = 200.0f;
        g.getNode(outID)->visualX = 620.0f;  g.getNode(outID)->visualY = 200.0f;

        g.connect (midiID, 0, oscID, 1);   // pitch → osc frequency mod
        g.connect (midiID, 2, adsrID, 0);  // gate → ADSR trigger
        g.connect (oscID, 0, vcaID, 0);    // osc audio → VCA audio in
        g.connect (adsrID, 0, vcaID, 1);   // envelope → VCA control in
        g.connect (vcaID, 0, outID, 0);    // VCA → audio output
        g.connect (vcaID, 0, outID, 1);

        proc->rebuildParameters();
        return proc;
    }

    /** Tutorial 6 — Envelope Filter: Auto-wah filter modulated by guitar amplitude.
        Input → EnvelopeFollower → Ranger → LowPass (freq_cv) */
    inline std::unique_ptr<GraphPedalProcessor> createTutorialEnvelopeFilter()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Envelope Filter");
        auto& g = proc->getDSPGraph();
        int inID    = g.addNode (std::make_unique<AudioInputNode>());       // 0
        int outID   = g.addNode (std::make_unique<AudioOutputNode>());      // 1
        int envID   = g.addNode (std::make_unique<EnvelopeFollowerNode>()); // 2
        int rangeID = g.addNode (std::make_unique<RangerNode>());           // 3
        int lpID    = g.addNode (std::make_unique<LowPassNode>());          // 4

        g.getNode(envID)->getParam("attack")->set (10.0f);
        g.getNode(envID)->getParam("release")->set (80.0f);
        g.getNode(envID)->getParam("sensitivity")->set (1.5f);
        g.getNode(rangeID)->getParam("in_min")->set (0.0f);
        g.getNode(rangeID)->getParam("in_max")->set (1.0f);
        g.getNode(rangeID)->getParam("out_min")->set (100.0f);
        g.getNode(rangeID)->getParam("out_max")->set (5000.0f);
        g.getNode(rangeID)->getParam("curve")->set (0.5f);
        g.getNode(lpID)->getParam("q")->set (2.0f);

        // Visual Coordinates
        g.getNode(inID)->visualX = 80.0f;     g.getNode(inID)->visualY = 200.0f;
        g.getNode(lpID)->visualX = 340.0f;    g.getNode(lpID)->visualY = 200.0f;
        g.getNode(envID)->visualX = 220.0f;   g.getNode(envID)->visualY = 360.0f;
        g.getNode(rangeID)->visualX = 380.0f; g.getNode(rangeID)->visualY = 360.0f;
        g.getNode(outID)->visualX = 560.0f;    g.getNode(outID)->visualY = 200.0f;

        g.connect (inID, 0, lpID, 0);
        g.connect (inID, 0, envID, 0);
        g.connect (envID, 0, rangeID, 0);
        g.connect (rangeID, 0, lpID, 1); // Modulate lowpass freq_cv
        g.connect (lpID, 0, outID, 0);
        g.connect (lpID, 0, outID, 1);

        proc->rebuildParameters();
        return proc;
    }

    /** Tutorial 7 — Step Sequencer Filter: Cutoff sweep automatically cycling through BPM-synced steps.
        Clock → Sequencer → Ranger → LowPass (freq_cv) */
    inline std::unique_ptr<GraphPedalProcessor> createTutorialStepSequencer()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Step Sequencer Filter");
        auto& g = proc->getDSPGraph();
        int inID    = g.addNode (std::make_unique<AudioInputNode>(), 1);       // 1
        int outID   = g.addNode (std::make_unique<AudioOutputNode>(), 2);      // 2
        int clockID = g.addNode (std::make_unique<ClockNode>(), 3);            // 3
        int seqID   = g.addNode (std::make_unique<SequencerNode>(), 4);        // 4
        int rangeID = g.addNode (std::make_unique<RangerNode>(), 5);           // 5
        int lpID    = g.addNode (std::make_unique<LowPassNode>(), 6);          // 6

        g.getNode(clockID)->getParam("bpm")->set (120.0f);
        g.getNode(seqID)->getParam("steps")->set (4.0f);
        g.getNode(seqID)->getParam("s1")->set (0.1f);
        g.getNode(seqID)->getParam("s2")->set (0.6f);
        g.getNode(seqID)->getParam("s3")->set (0.3f);
        g.getNode(seqID)->getParam("s4")->set (0.9f);
        g.getNode(rangeID)->getParam("in_min")->set (0.0f);
        g.getNode(rangeID)->getParam("in_max")->set (1.0f);
        g.getNode(rangeID)->getParam("out_min")->set (200.0f);
        g.getNode(rangeID)->getParam("out_max")->set (6000.0f);
        g.getNode(lpID)->getParam("q")->set (3.0f);

        // Visual Coordinates
        g.getNode(inID)->visualX = 80.0f;     g.getNode(inID)->visualY = 200.0f;
        g.getNode(lpID)->visualX = 320.0f;    g.getNode(lpID)->visualY = 200.0f;
        g.getNode(clockID)->visualX = 80.0f;  g.getNode(clockID)->visualY = 380.0f;
        g.getNode(seqID)->visualX = 260.0f;   g.getNode(seqID)->visualY = 380.0f;
        g.getNode(rangeID)->visualX = 440.0f; g.getNode(rangeID)->visualY = 380.0f;
        g.getNode(outID)->visualX = 600.0f;    g.getNode(outID)->visualY = 200.0f;

        g.connect (clockID, 0, seqID, 0); // clock pulse out -> seq clock in
        g.connect (seqID, 0, rangeID, 0);  // seq out -> ranger in
        g.connect (rangeID, 0, lpID, 1);   // ranger out -> lowpass freq_cv
        g.connect (inID, 0, lpID, 0);
        g.connect (lpID, 0, outID, 0);
        g.connect (lpID, 0, outID, 1);

        proc->rebuildParameters();
        return proc;
    }

    /** Tutorial 8 — Pattern Slicer: Gated fuzz switching synced to clock, active only when playing.
        EnvelopeFollower → Comparator + Clock → AND Gate → Mux selector (Dry/Fuzz paths) */
    inline std::unique_ptr<GraphPedalProcessor> createTutorialPatternSlicer()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Pattern Slicer");
        auto& g = proc->getDSPGraph();
        int inID    = g.addNode (std::make_unique<AudioInputNode>(), 1);       // 1
        int envID   = g.addNode (std::make_unique<EnvelopeFollowerNode>(), 2); // 2
        int compID  = g.addNode (std::make_unique<ComparatorNode>(), 3);       // 3
        int clockID = g.addNode (std::make_unique<ClockNode>(), 4);            // 4
        int andID   = g.addNode (std::make_unique<ANDGateNode>(), 5);          // 5
        int fuzzID  = g.addNode (std::make_unique<SoftClipNode>(), 6);         // 6
        int muxID   = g.addNode (std::make_unique<MuxNode>(), 7);              // 7
        int outID   = g.addNode (std::make_unique<AudioOutputNode>(), 8);      // 8

        g.getNode(compID)->getParam("threshold")->set (0.15f);
        g.getNode(compID)->getParam("mode")->set (0.0f); // 0 = a > b
        g.getNode(clockID)->getParam("bpm")->set (135.0f);
        g.getNode(fuzzID)->getParam("drive")->set (40.0f);
        g.getNode(envID)->getParam("release")->set (100.0f);

        // Visual Coordinates
        g.getNode(inID)->visualX = 80.0f;     g.getNode(inID)->visualY = 200.0f;
        g.getNode(fuzzID)->visualX = 260.0f;  g.getNode(fuzzID)->visualY = 80.0f;
        g.getNode(muxID)->visualX = 440.0f;   g.getNode(muxID)->visualY = 200.0f;
        g.getNode(envID)->visualX = 220.0f;   g.getNode(envID)->visualY = 360.0f;
        g.getNode(compID)->visualX = 380.0f;  g.getNode(compID)->visualY = 360.0f;
        g.getNode(clockID)->visualX = 380.0f; g.getNode(clockID)->visualY = 520.0f;
        g.getNode(andID)->visualX = 540.0f;   g.getNode(andID)->visualY = 440.0f;
        g.getNode(outID)->visualX = 700.0f;   g.getNode(outID)->visualY = 200.0f;

        g.connect (inID, 0, envID, 0);
        g.connect (inID, 0, muxID, 0);   // Dry path to Mux Input A
        g.connect (inID, 0, fuzzID, 0);  // Dry to Fuzz
        g.connect (fuzzID, 0, muxID, 1); // Fuzz to Mux Input B
        g.connect (envID, 0, compID, 0);
        g.connect (compID, 0, andID, 0);
        g.connect (clockID, 0, andID, 1);
        g.connect (andID, 0, muxID, 2);  // AND Gate out -> Mux Selector
        g.connect (muxID, 0, outID, 0);
        g.connect (muxID, 0, outID, 1);

        proc->rebuildParameters();
        return proc;
    }

    /** Tutorial 9 — Wave Folder: Scriptable wavefolding distortion + time-based ring modulation.
        AudioInputNode -> ExpressionNode -> AudioOutputNode */
    inline std::unique_ptr<GraphPedalProcessor> createTutorialWaveFolder()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Wave Folder");
        auto& g = proc->getDSPGraph();
        int inID   = g.addNode (std::make_unique<AudioInputNode>(), 1);      // ID 1
        int exprID = g.addNode (std::make_unique<ExpressionNode>(), 2);      // ID 2
        int outID  = g.addNode (std::make_unique<AudioOutputNode>(), 3);     // ID 3

        // Setup the script on the Expression Node
        auto* node = dynamic_cast<ExpressionNode*> (g.getNode (exprID));
        if (node != nullptr)
        {
            node->setExpression (
                "@inputs in\n"
                "@outputs out\n"
                "@parameters drive rate mix\n"
                "folder = sin(in * drive)\n"
                "carrier = sin(t * rate * 6.28318)\n"
                "modulator = folder * carrier\n"
                "out = lerp(in, modulator, mix)"
            );
            
            // Set defaults
            node->getParam("drive")->set (3.0f);
            node->getParam("rate")->set (5.0f);
            node->getParam("mix")->set (0.5f);
        }

        // Visual Coordinates
        g.getNode (inID)->visualX   = 100.0f; g.getNode (inID)->visualY   = 200.0f;
        g.getNode (exprID)->visualX = 350.0f; g.getNode (exprID)->visualY = 200.0f;
        g.getNode (outID)->visualX  = 600.0f; g.getNode (outID)->visualY  = 200.0f;

        // Connections
        g.connect (inID, 0, exprID, 0); 
        g.connect (exprID, 0, outID, 0); 
        g.connect (exprID, 0, outID, 1); 

        proc->rebuildParameters();
        return proc;
    }

    inline std::unique_ptr<GraphPedalProcessor> createMixerPedal()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Mixer");
        auto& g = proc->getDSPGraph();

        // Add nodes with deterministic IDs to match the mapping!
        int inID    = g.addNode (std::make_unique<AudioInputNode>(),  0);
        int outID   = g.addNode (std::make_unique<AudioOutputNode>(), 1);
        int auxID   = g.addNode (std::make_unique<AuxInputNode>(),    2);
        int mixerID = g.addNode (std::make_unique<StereoMixerNode>(), 3);

        // Visual Coordinates
        g.getNode (inID)->visualX    = 80.0f;  g.getNode (inID)->visualY    = 150.0f;
        g.getNode (auxID)->visualX   = 80.0f;  g.getNode (auxID)->visualY   = 300.0f;
        g.getNode (mixerID)->visualX = 350.0f; g.getNode (mixerID)->visualY = 200.0f;
        g.getNode (outID)->visualX   = 600.0f; g.getNode (outID)->visualY   = 200.0f;

        // Set defaults
        g.getNode (mixerID)->getParam ("vol1")->set (0.0f);     // 0dB
        g.getNode (mixerID)->getParam ("vol2")->set (0.0f);     // 0dB
        g.getNode (mixerID)->getParam ("master")->set (0.0f);   // 0dB

        // Connections
        g.connect (inID,    0, mixerID, 0); // Audio Input L -> Mixer In 1 L
        g.connect (inID,    1, mixerID, 1); // Audio Input R -> Mixer In 1 R
        g.connect (auxID,   0, mixerID, 2); // Aux Input L -> Mixer In 2 L
        g.connect (auxID,   1, mixerID, 3); // Aux Input R -> Mixer In 2 R

        g.connect (mixerID, 0, outID,   0); // Mixer Out L -> Audio Output L
        g.connect (mixerID, 1, outID,   1); // Mixer Out R -> Audio Output R

        proc->rebuildParameters();
        return proc;
    }

    inline std::unique_ptr<GraphPedalProcessor> createMatrixMixerPedal()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Matrix Mixer");
        auto& g = proc->getDSPGraph();

        // Nodes with deterministic IDs to match the visual design mappings
        int inID    = g.addNode (std::make_unique<AudioInputNode>(),  0);
        int outID   = g.addNode (std::make_unique<AudioOutputNode>(), 1);
        int auxInID = g.addNode (std::make_unique<AuxInputNode>(),    2);
        int auxOutID = g.addNode (std::make_unique<AuxOutputNode>(),  3);
        int matrixID = g.addNode (std::make_unique<MatrixMixerNode>(), 4);

        // Visual Coordinates
        g.getNode (inID)->visualX     = 80.0f;  g.getNode (inID)->visualY     = 100.0f;
        g.getNode (auxInID)->visualX  = 80.0f;  g.getNode (auxInID)->visualY  = 300.0f;
        g.getNode (matrixID)->visualX = 350.0f; g.getNode (matrixID)->visualY = 200.0f;
        g.getNode (outID)->visualX    = 620.0f; g.getNode (outID)->visualY    = 100.0f;
        g.getNode (auxOutID)->visualX = 620.0f; g.getNode (auxOutID)->visualY = 300.0f;

        // Connections: Inputs into Matrix Mixer
        g.connect (inID,    0, matrixID, 0); // Main In L -> Matrix In 1
        g.connect (inID,    1, matrixID, 1); // Main In R -> Matrix In 2
        g.connect (auxInID, 0, matrixID, 2); // Aux In L  -> Matrix In 3
        g.connect (auxInID, 1, matrixID, 3); // Aux In R  -> Matrix In 4

        // Connections: Matrix Mixer into Outputs
        g.connect (matrixID, 0, outID,    0); // Matrix Out 1 -> Main Out L
        g.connect (matrixID, 1, outID,    1); // Matrix Out 2 -> Main Out R
        g.connect (matrixID, 2, auxOutID, 0); // Matrix Out 3 -> Aux Out L
        g.connect (matrixID, 3, auxOutID, 1); // Matrix Out 4 -> Aux Out R

        proc->rebuildParameters();
        return proc;
    }

    inline std::unique_ptr<GraphPedalProcessor> createMatrixMixerXLPedal()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Matrix Mixer XL");
        auto& g = proc->getDSPGraph();

        // 32-channel I/O nodes
        int inID     = g.addNode (std::make_unique<AudioInputNode> (32), 0);
        int outID    = g.addNode (std::make_unique<AudioOutputNode> (32), 1);
        int matrixID = g.addNode (std::make_unique<MatrixMixerXLNode>(), 4);

        // Position nodes in graph visualizer
        g.getNode (inID)->visualX     = 80.0f;  g.getNode (inID)->visualY     = 200.0f;
        g.getNode (matrixID)->visualX = 350.0f; g.getNode (matrixID)->visualY = 200.0f;
        g.getNode (outID)->visualX    = 620.0f; g.getNode (outID)->visualY    = 200.0f;

        // Connect 32 channels from input -> matrix -> output
        for (int ch = 0; ch < 32; ++ch)
        {
            g.connect (inID,     ch, matrixID, ch);
            g.connect (matrixID, ch, outID,    ch);
        }

        proc->rebuildParameters();
        return proc;
    }
}

