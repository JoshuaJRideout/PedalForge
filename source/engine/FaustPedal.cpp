#include "FaustPedal.h"

// Include the FAUST base classes from our architecture
// (These are defined in the generated headers via pedalforge.arch)

//==============================================================================
// ParameterMapper: a FAUST UI subclass that captures parameter declarations
//==============================================================================
class FaustPedal::ParameterMapper : public UI
{
public:
    ParameterMapper (FaustPedal& owner) : owner_ (owner) {}

    void openTabBox (const char*) override {}
    void openHorizontalBox (const char*) override {}
    void openVerticalBox (const char*) override {}
    void closeBox() override {}

    void addButton (const char* label, FAUSTFLOAT* zone) override
    {
        addParameter (label, zone, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    void addCheckButton (const char* label, FAUSTFLOAT* zone) override
    {
        addParameter (label, zone, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    void addVerticalSlider (const char* label, FAUSTFLOAT* zone,
                            FAUSTFLOAT init, FAUSTFLOAT min,
                            FAUSTFLOAT max, FAUSTFLOAT step) override
    {
        addParameter (label, zone, init, min, max, step);
    }

    void addHorizontalSlider (const char* label, FAUSTFLOAT* zone,
                              FAUSTFLOAT init, FAUSTFLOAT min,
                              FAUSTFLOAT max, FAUSTFLOAT step) override
    {
        addParameter (label, zone, init, min, max, step);
    }

    void addNumEntry (const char* label, FAUSTFLOAT* zone,
                      FAUSTFLOAT init, FAUSTFLOAT min,
                      FAUSTFLOAT max, FAUSTFLOAT step) override
    {
        addParameter (label, zone, init, min, max, step);
    }

    void addHorizontalBargraph (const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT) override {}
    void addVerticalBargraph (const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT) override {}

    void declare (FAUSTFLOAT*, const char*, const char*) override {}

private:
    void addParameter (const char* label, FAUSTFLOAT* zone,
                       float defaultVal, float minVal, float maxVal, float step)
    {
        std::string name (label);
        owner_.parameterZones[name] = zone;

        // Create a safe parameter ID from the label
        auto paramId = juce::String (label).removeCharacters (" /()[]{}").toLowerCase();

        auto* param = new juce::AudioParameterFloat (
            juce::ParameterID { paramId, 1 },
            juce::String (label),
            juce::NormalisableRange<float> (minVal, maxVal, step),
            defaultVal);

        owner_.juceParams[name] = param;
        owner_.addParameter (param);
    }

    FaustPedal& owner_;
};

//==============================================================================
FaustPedal::FaustPedal (const juce::String& name, DspFactory factory)
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      pedalName (name),
      dspFactory (std::move (factory))
{
    // Create the FAUST DSP instance
    faustDsp.reset (dspFactory());

    // Discover and register parameters
    auto mapper = std::make_unique<ParameterMapper> (*this);
    faustDsp->buildUserInterface (mapper.get());

    numFaustInputs  = faustDsp->getNumInputs();
    numFaustOutputs = faustDsp->getNumOutputs();
}

FaustPedal::~FaustPedal()
{
}

//==============================================================================
void FaustPedal::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    faustDsp->init (static_cast<int> (sampleRate));
}

void FaustPedal::releaseResources()
{
}

bool FaustPedal::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Accept stereo or mono
    auto mainOutput = layouts.getMainOutputChannelSet();
    auto mainInput  = layouts.getMainInputChannelSet();

    if (mainOutput != juce::AudioChannelSet::stereo() &&
        mainOutput != juce::AudioChannelSet::mono())
        return false;

    return mainInput == mainOutput;
}

void FaustPedal::processBlock (juce::AudioBuffer<float>& buffer,
                                juce::MidiBuffer& /*midiMessages*/)
{
    // Sync JUCE parameter values → FAUST zones
    for (auto& [name, zone] : parameterZones)
    {
        auto it = juceParams.find (name);
        if (it != juceParams.end())
            *zone = it->second->get();
    }

    int numSamples  = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    // FAUST expects float** arrays for input and output
    // For in-place processing, we use the same buffer pointers
    if (numChannels >= numFaustInputs && numChannels >= numFaustOutputs)
    {
        // FAUST compute() takes float** but JUCE returns float*const*.
        // The const_cast is safe — FAUST only writes through the pointers.
        auto channelPtrs = const_cast<float**> (buffer.getArrayOfWritePointers());
        faustDsp->compute (numSamples, channelPtrs, channelPtrs);
    }
    else
    {
        // Mismatched channel count — process what we can
        // Create temp arrays matching FAUST expectations
        std::vector<float*> inputs (numFaustInputs, nullptr);
        std::vector<float*> outputs (numFaustOutputs, nullptr);

        for (int i = 0; i < std::min (numChannels, numFaustInputs); ++i)
            inputs[i] = buffer.getWritePointer (i);
        for (int i = 0; i < std::min (numChannels, numFaustOutputs); ++i)
            outputs[i] = buffer.getWritePointer (i);

        // Fill any missing inputs/outputs with silence buffers
        std::vector<std::vector<float>> silenceBufs;
        for (int i = numChannels; i < std::max (numFaustInputs, numFaustOutputs); ++i)
        {
            silenceBufs.emplace_back (numSamples, 0.0f);
            if (i < numFaustInputs)  inputs[i]  = silenceBufs.back().data();
            if (i < numFaustOutputs) outputs[i] = silenceBufs.back().data();
        }

        faustDsp->compute (numSamples, inputs.data(), outputs.data());
    }
}

//==============================================================================
void FaustPedal::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = std::make_unique<juce::DynamicObject>();

    for (auto& [name, param] : juceParams)
        state->setProperty (juce::String (name), (double) param->get());

    auto json = juce::JSON::toString (juce::var (state.release()));
    destData.replaceAll (json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void FaustPedal::setStateInformation (const void* data, int sizeInBytes)
{
    auto json = juce::String::fromUTF8 (static_cast<const char*> (data), sizeInBytes);
    auto parsed = juce::JSON::parse (json);

    if (auto* obj = parsed.getDynamicObject())
    {
        for (auto& [name, param] : juceParams)
        {
            auto key = juce::String (name);
            if (obj->hasProperty (key))
                param->setValueNotifyingHost (
                    param->getNormalisableRange().convertTo0to1 (
                        static_cast<float> ((double) obj->getProperty (key))));
        }
    }
}
