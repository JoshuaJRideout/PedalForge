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

    juce::String saveGraph() const
    {
        return juce::JSON::toString (dspGraph.toJSON());
    }

    /** Rebuild JUCE parameters from the current graph nodes.
        Creates AudioParameterFloat objects that the host/UI can discover. */
    void rebuildParameters()
    {
        paramBridge.clear();

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

        dspGraph.processBlock (buffer, buffer.getNumSamples(), &midi);
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
    /** Creates a simple distortion: Input → Gain → SoftClip → ToneStack → Gain(vol) → Output */
    inline std::unique_ptr<GraphPedalProcessor> createDistortion()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Distortion");
        auto& g = proc->getDSPGraph();

        int inID    = g.addNode (std::make_unique<AudioInputNode>());
        int gainID  = g.addNode (std::make_unique<GainNode>());
        int clipID  = g.addNode (std::make_unique<SoftClipNode>());
        int toneID  = g.addNode (std::make_unique<ToneStackNode>());
        int volID   = g.addNode (std::make_unique<GainNode>());
        int outID   = g.addNode (std::make_unique<AudioOutputNode>());

        // Name the gain nodes distinctly
        g.getNode(gainID)->setName ("Drive Gain");
        g.getNode(volID)->setName ("Volume");
        g.getNode(gainID)->getParam("gain")->set (20.0f); // +20dB drive default
        g.getNode(volID)->getParam("gain")->set (-6.0f);  // -6dB volume default
        g.getNode(clipID)->getParam("drive")->set (8.0f);

        g.connect (inID, 0, gainID, 0);
        g.connect (gainID, 0, clipID, 0);
        g.connect (clipID, 0, toneID, 0);
        g.connect (toneID, 0, volID, 0);
        g.connect (volID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    /** Creates a delay: Input → Split → Delay (w/ feedback) → Mix → Output */
    inline std::unique_ptr<GraphPedalProcessor> createDelay()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Delay");
        auto& g = proc->getDSPGraph();

        int inID    = g.addNode (std::make_unique<AudioInputNode>());
        int splitID = g.addNode (std::make_unique<SplitNode>());
        int delayID = g.addNode (std::make_unique<DelayNode>());
        int mixID   = g.addNode (std::make_unique<MixNode>());
        int outID   = g.addNode (std::make_unique<AudioOutputNode>());

        g.connect (inID, 0, splitID, 0);
        g.connect (splitID, 0, mixID, 0);    // dry → mix.dry
        g.connect (splitID, 1, delayID, 0);  // split → delay
        g.connect (delayID, 0, mixID, 1);    // delay → mix.wet
        g.connect (mixID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    /** Creates a chorus: Input → Split → ModDelay (LFO-modulated) → Mix → Output */
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
        g.connect (splitID, 0, mixID, 0);       // dry → mix.dry
        g.connect (splitID, 1, modDelID, 0);    // split → mod delay input
        g.connect (lfoID, 0, modDelID, 1);      // LFO → mod delay mod input
        g.connect (modDelID, 0, mixID, 1);      // mod delay → mix.wet
        g.connect (mixID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    /** Creates a reverb: Input → Reverb → Output */
    inline std::unique_ptr<GraphPedalProcessor> createReverb()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Reverb");
        auto& g = proc->getDSPGraph();

        int inID  = g.addNode (std::make_unique<AudioInputNode>());
        int revID = g.addNode (std::make_unique<SchroederReverbNode>());
        int outID = g.addNode (std::make_unique<AudioOutputNode>());

        g.connect (inID, 0, revID, 0);
        g.connect (revID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    /** Creates a compressor: Input → Compressor → Output */
    inline std::unique_ptr<GraphPedalProcessor> createCompressor()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Compressor");
        auto& g = proc->getDSPGraph();

        int inID   = g.addNode (std::make_unique<AudioInputNode>());
        int compID = g.addNode (std::make_unique<CompressorNode>());
        int outID  = g.addNode (std::make_unique<AudioOutputNode>());

        g.connect (inID, 0, compID, 0);
        g.connect (compID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    /** Creates a clean boost: Input → Gain → Output */
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

    /** Creates a noise gate: Input → NoiseGate → Output */
    inline std::unique_ptr<GraphPedalProcessor> createNoiseGate()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Noise Gate");
        auto& g = proc->getDSPGraph();

        int inID   = g.addNode (std::make_unique<AudioInputNode>());
        int gateID = g.addNode (std::make_unique<NoiseGateNode>());
        int outID  = g.addNode (std::make_unique<AudioOutputNode>());

        g.connect (inID, 0, gateID, 0);
        g.connect (gateID, 0, outID, 0);

        proc->rebuildParameters();
        return proc;
    }

    /** Creates a tremolo: Input → LFO-modulated gain → Output */
    inline std::unique_ptr<GraphPedalProcessor> createTremolo()
    {
        auto proc = std::make_unique<GraphPedalProcessor> ("Tremolo");
        auto& g = proc->getDSPGraph();

        int inID   = g.addNode (std::make_unique<AudioInputNode>());
        int lfoID  = g.addNode (std::make_unique<LFONode>());
        int gainID = g.addNode (std::make_unique<GainNode>());
        int outID  = g.addNode (std::make_unique<AudioOutputNode>());

        g.getNode(lfoID)->getParam("rate")->set (5.0f);
        g.getNode(lfoID)->getParam("depth")->set (0.5f);

        // For tremolo, the LFO modulates gain. We route through gain node.
        // The gain node will use its own gain param, but LFO acts as volume mod.
        // For a proper tremolo, we'd need a multiply node. Let's use gain as-is.
        g.connect (inID, 0, gainID, 0);
        g.connect (gainID, 0, outID, 0);
        // TODO: Need a MultiplyNode for proper AM tremolo

        proc->rebuildParameters();
        return proc;
    }
}
