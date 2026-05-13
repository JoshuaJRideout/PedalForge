#pragma once

#include "DSPNode.h"
#include "../engine/FaustTypes.h"
#include <functional>
#include <map>
#include <string>

//==============================================================================
/**
 * Wraps a FAUST dsp object into a DSPNode, so it can be used
 * as a custom block in the Effects Forge node graph.
 *
 * On construction, it discovers FAUST parameters via buildUserInterface
 * and exposes them as NodeParam entries. The graph runtime can then
 * process audio through this node like any built-in DSP block.
 *
 * Usage:
 *   auto node = std::make_unique<FaustDSPNode>("Overdrive",
 *       []() { return new overdrive(); });
 */
class FaustDSPNode : public DSPNode
{
public:
    using DspFactory = std::function<dsp*()>;

    FaustDSPNode (const juce::String& nodeName, DspFactory factory)
        : DSPNode ("faust_custom", nodeName), dspFactory (std::move (factory))
    {
        faustDsp.reset (dspFactory());
        if (faustDsp)
        {
            // Discover I/O
            int numIn  = faustDsp->getNumInputs();
            int numOut = faustDsp->getNumOutputs();

            for (int i = 0; i < numIn; ++i)
                addInput ("in_" + juce::String (i));
            for (int i = 0; i < numOut; ++i)
                addOutput ("out_" + juce::String (i));

            // If no inputs (generator), still add one input for chaining
            if (numIn == 0) addInput ("in");
            if (numOut == 0) addOutput ("out");

            // Discover parameters via a custom UI mapper
            ParamDiscovery discovery (*this);
            faustDsp->buildUserInterface (&discovery);
        }
    }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        DSPNode::prepare (sampleRate, maxBlockSize);
        if (faustDsp)
            faustDsp->init ((int) sampleRate);
    }

    void process (const float** inputBuffers, int numInputChannels,
                  float** outputBuffers, int numOutputChannels,
                  int numSamples) override
    {
        if (!faustDsp) return;

        // Sync NodeParam values → FAUST zones
        for (auto& mapping : paramZones)
        {
            if (mapping.zone && mapping.param)
                *mapping.zone = mapping.param->get();
        }

        int faustIn  = faustDsp->getNumInputs();
        int faustOut = faustDsp->getNumOutputs();

        // Build input pointer array
        std::vector<float*> inPtrs (faustIn);
        for (int i = 0; i < faustIn; ++i)
            inPtrs[i] = (i < numInputChannels)
                         ? const_cast<float*>(inputBuffers[i])
                         : silence.data();

        // Build output pointer array
        std::vector<float*> outPtrs (faustOut);
        for (int i = 0; i < faustOut; ++i)
            outPtrs[i] = (i < numOutputChannels)
                         ? outputBuffers[i]
                         : scratch.data();

        faustDsp->compute (numSamples, inPtrs.data(), outPtrs.data());
    }

    void reset() override
    {
        if (faustDsp)
            faustDsp->instanceClear();
    }

private:
    DspFactory dspFactory;
    std::unique_ptr<dsp> faustDsp;
    std::vector<float> silence, scratch;

    struct ZoneMapping
    {
        FAUSTFLOAT* zone = nullptr;
        NodeParam* param = nullptr;
    };
    std::vector<ZoneMapping> paramZones;

    //==========================================================================
    /** UI subclass that captures FAUST parameter declarations
        and creates corresponding NodeParam entries. */
    class ParamDiscovery : public UI
    {
    public:
        ParamDiscovery (FaustDSPNode& owner) : owner_ (owner) {}

        void openTabBox (const char*) override {}
        void openHorizontalBox (const char*) override {}
        void openVerticalBox (const char*) override {}
        void closeBox() override {}

        void addButton (const char* label, FAUSTFLOAT* zone) override
        {
            addParam (label, zone, 0.0f, 0.0f, 1.0f);
        }

        void addCheckButton (const char* label, FAUSTFLOAT* zone) override
        {
            addParam (label, zone, 0.0f, 0.0f, 1.0f);
        }

        void addVerticalSlider (const char* label, FAUSTFLOAT* zone,
                                FAUSTFLOAT init, FAUSTFLOAT min,
                                FAUSTFLOAT max, FAUSTFLOAT /*step*/) override
        {
            addParam (label, zone, init, min, max);
        }

        void addHorizontalSlider (const char* label, FAUSTFLOAT* zone,
                                  FAUSTFLOAT init, FAUSTFLOAT min,
                                  FAUSTFLOAT max, FAUSTFLOAT /*step*/) override
        {
            addParam (label, zone, init, min, max);
        }

        void addNumEntry (const char* label, FAUSTFLOAT* zone,
                          FAUSTFLOAT init, FAUSTFLOAT min,
                          FAUSTFLOAT max, FAUSTFLOAT /*step*/) override
        {
            addParam (label, zone, init, min, max);
        }

        void addHorizontalBargraph (const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT) override {}
        void addVerticalBargraph (const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT) override {}

    private:
        FaustDSPNode& owner_;

        void addParam (const char* label, FAUSTFLOAT* zone,
                       float init, float min, float max)
        {
            juce::String id = juce::String (label).toLowerCase().replace (" ", "_");
            juce::String name = juce::String (label);

            owner_.addParam (id, name, min, max, init);

            // Find the param we just added
            auto* p = owner_.getParam (id);
            if (p)
            {
                p->set (init);
                owner_.paramZones.push_back ({ zone, p });
            }
        }
    };
};
