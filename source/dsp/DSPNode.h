#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <string>
#include <atomic>
#include <cmath>

//==============================================================================
/**
 * A named, rangeable parameter for a DSP node.
 * Values are atomic for thread-safe audio-thread access.
 */
struct NodeParam
{
    juce::String id;
    juce::String name;
    float minVal = 0.0f;
    float maxVal = 1.0f;
    float defaultVal = 0.5f;
    std::atomic<float> value { 0.5f };

    NodeParam() = default;
    NodeParam (const juce::String& pid, const juce::String& pname,
              float mn, float mx, float def)
        : id (pid), name (pname), minVal (mn), maxVal (mx), defaultVal (def)
    { value.store (def); }

    // Non-copyable due to atomic, but movable
    NodeParam (NodeParam&& o) noexcept
        : id (std::move(o.id)), name (std::move(o.name)),
          minVal (o.minVal), maxVal (o.maxVal), defaultVal (o.defaultVal)
    { value.store (o.value.load()); }

    NodeParam& operator= (NodeParam&& o) noexcept
    {
        id = std::move(o.id); name = std::move(o.name);
        minVal = o.minVal; maxVal = o.maxVal; defaultVal = o.defaultVal;
        value.store (o.value.load());
        return *this;
    }

    float get() const { return value.load (std::memory_order_relaxed); }
    void set (float v) { value.store (juce::jlimit (minVal, maxVal, v), std::memory_order_relaxed); }
    float getNormalized() const { return (get() - minVal) / (maxVal - minVal); }
    void setNormalized (float n) { set (minVal + n * (maxVal - minVal)); }
};

//==============================================================================
/**
 * Port descriptor for a DSP node.
 */
struct NodePort
{
    enum Type { Audio, Control };

    juce::String name;
    Type type = Audio;
    int channels = 1; // 1 = mono, 2 = stereo
};

//==============================================================================
/**
 * Abstract base class for all DSP processing blocks.
 *
 * Each node has:
 *   - Named input and output ports
 *   - Named parameters with ranges
 *   - prepare() / process() / reset() lifecycle
 *
 * Nodes process mono or stereo audio.  The graph runtime manages
 * buffer routing between connected nodes.
 */
class DSPNode
{
public:
    DSPNode (const juce::String& nodeType, const juce::String& displayName)
        : type (nodeType), name (displayName) {}

    virtual ~DSPNode() = default;

    //==========================================================================
    /** Called before audio processing begins. */
    virtual void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        blockSize = maxBlockSize;
    }

    /** Process audio. inputBuffers and outputBuffers are arrays of channel pointers.
        numInputs/numOutputs correspond to port count. */
    virtual void process (const float** inputBuffers, int numInputChannels,
                          float** outputBuffers, int numOutputChannels,
                          int numSamples) = 0;

    /** Reset internal state (clear delay lines, etc.). */
    virtual void reset() {}

    //==========================================================================
    const juce::String& getType() const { return type; }
    const juce::String& getName() const { return name; }
    void setName (const juce::String& n) { name = n; }

    int getNodeID() const { return nodeID; }
    void setNodeID (int id) { nodeID = id; }

    //==========================================================================
    const std::vector<NodePort>& getInputPorts()  const { return inputPorts; }
    const std::vector<NodePort>& getOutputPorts() const { return outputPorts; }

    std::vector<NodeParam>& getParams() { return params; }
    const std::vector<NodeParam>& getParams() const { return params; }

    NodeParam* getParam (const juce::String& paramId)
    {
        for (auto& p : params)
            if (p.id == paramId) return &p;
        return nullptr;
    }

    //==========================================================================
    /** Serialize node state to JSON. */
    juce::var toJSON() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("type", type);
        obj->setProperty ("name", name);
        obj->setProperty ("id", nodeID);

        juce::Array<juce::var> paramArray;
        for (const auto& p : params)
        {
            auto* po = new juce::DynamicObject();
            po->setProperty ("id", p.id);
            po->setProperty ("value", (double) p.get());
            paramArray.add (juce::var (po));
        }
        obj->setProperty ("params", paramArray);
        if (filePath.isNotEmpty())
            obj->setProperty ("filePath", filePath);
        return juce::var (obj);
    }

    /** Restore parameter values from JSON. */
    void fromJSON (const juce::var& json)
    {
        if (auto* obj = json.getDynamicObject())
        {
            name = obj->getProperty ("name").toString();
            if (obj->hasProperty ("filePath"))
                setFilePath (obj->getProperty ("filePath").toString());
                
            if (auto* arr = obj->getProperty ("params").getArray())
            {
                for (const auto& pv : *arr)
                {
                    if (auto* po = pv.getDynamicObject())
                    {
                        juce::String pid = po->getProperty ("id").toString();
                        float val = (float)(double) po->getProperty ("value");
                        if (auto* p = getParam (pid))
                            p->set (val);
                    }
                }
            }
        }
    }

    /** Set the MIDI buffer for this processing block (called by DSPGraph). */
    void setMidiBuffer (juce::MidiBuffer* midi) { midiBuffer = midi; }

    virtual void setFilePath (const juce::String& path) { filePath = path; }
    virtual juce::String getFilePath() const { return filePath; }

protected:
    juce::MidiBuffer* midiBuffer = nullptr;
    juce::String filePath;
    void addInput  (const juce::String& n, NodePort::Type t = NodePort::Audio, int ch = 1)
    { inputPorts.push_back ({ n, t, ch }); }

    void addOutput (const juce::String& n, NodePort::Type t = NodePort::Audio, int ch = 1)
    { outputPorts.push_back ({ n, t, ch }); }

    void addParam (const juce::String& id, const juce::String& pname,
                   float mn, float mx, float def)
    { params.emplace_back (id, pname, mn, mx, def); }

    double sr = 44100.0;
    int blockSize = 512;

private:
    juce::String type;
    juce::String name;
    int nodeID = -1;

    std::vector<NodePort> inputPorts;
    std::vector<NodePort> outputPorts;
    std::vector<NodeParam> params;
};
