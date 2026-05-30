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
