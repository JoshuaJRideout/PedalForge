#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <string>
#include <atomic>
#include <cmath>
#include <set>

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
    std::atomic<float> baseValue { 0.5f };
    float modValue { 0.0f };
    bool isModulated { false };
    bool modulatable { true };

    NodeParam() = default;
    NodeParam (const juce::String& pid, const juce::String& pname,
              float mn, float mx, float def, bool isMod = true)
        : id (pid), name (pname), minVal (mn), maxVal (mx), defaultVal (def), modulatable (isMod)
    { baseValue.store (def); }

    // Non-copyable due to atomic, but movable
    NodeParam (NodeParam&& o) noexcept
        : id (std::move(o.id)), name (std::move(o.name)),
          minVal (o.minVal), maxVal (o.maxVal), defaultVal (o.defaultVal),
          modValue (o.modValue), isModulated (o.isModulated),
          modulatable (o.modulatable)
    { baseValue.store (o.baseValue.load()); }

    NodeParam& operator= (NodeParam&& o) noexcept
    {
        id = std::move(o.id); name = std::move(o.name);
        minVal = o.minVal; maxVal = o.maxVal; defaultVal = o.defaultVal;
        modValue = o.modValue; isModulated = o.isModulated;
        modulatable = o.modulatable;
        baseValue.store (o.baseValue.load());
        return *this;
    }

    float get() const { return isModulated ? modValue : baseValue.load (std::memory_order_relaxed); }
    void set (float v) { baseValue.store (juce::jlimit (minVal, maxVal, v), std::memory_order_relaxed); }
    float getNormalized() const { return (get() - minVal) / (maxVal - minVal); }
    void setNormalized (float n) { set (minVal + n * (maxVal - minVal)); }

    /** Apply a CV modulation value non-destructively. */
    void applyModulation (float cv)
    {
        if (cv != 0.0f)
        {
            isModulated = true;
            modValue = juce::jlimit (minVal, maxVal, cv);
        }
        else
        {
            isModulated = false;
        }
    }
};

//==============================================================================
/**
 * Port descriptor for a DSP node.
 */
struct NodePort
{
    enum Type { Audio, Control, Midi, Gate };

    juce::String name;
    Type type = Audio;
    int channels = 1; // 1 = mono, 2 = stereo
    bool isParameterCV = false;

    /** Check if two port types are compatible for connection. */
    static bool areCompatible (Type src, Type dst)
    {
        if (src == dst) return true;
        // Control and Gate are interchangeable (gate is a subset of control)
        if ((src == Control && dst == Gate) || (src == Gate && dst == Control)) return true;
        // Audio can accept Control (for CV-to-audio modulation)
        if (src == Control && dst == Audio) return true;
        return false;
    }
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

    float visualX = -1.0f;
    float visualY = -1.0f;

    std::vector<float> lastInputValues;
    std::vector<float> lastOutputValues;

    //==========================================================================
    /** Called before audio processing begins. */
    virtual void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        blockSize = maxBlockSize;
        lastInputValues.assign (inputPorts.size(), 0.0f);
        lastOutputValues.assign (outputPorts.size(), 0.0f);
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

    virtual bool isDisplayNode() const { return false; }
    virtual juce::String getDisplayType() const { return ""; }
    virtual float getDisplayValue() const { return 0.0f; }
    virtual const std::vector<float>* getPixelData() const { return nullptr; }

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
    /** Auto-generate Control input ports for all parameters.
        Called automatically by the graph runtime before first process.
        Each parameter gets a corresponding `paramId_cv` Control input port
        appended after the node's explicitly declared ports. */
    void autoExposeParams()
    {
        if (paramsExposed) return;
        paramsExposed = true;

        paramPortStartIndex = (int) getInputPorts().size();

        for (const auto& p : params)
        {
            if (p.modulatable)
                addInput (p.id + "_cv", NodePort::Control, 1, true);
        }
    }

    /** Read auto-generated CV input ports and apply them to parameters.
        The graph runtime calls this once per block, before process().
        When a CV port receives a non-zero signal, it overrides the
        parameter's knob value for that sample block.

        @param inputBuffers  The input buffer array from process()
        @param numInputs     Number of input buffers
        @param sampleIndex   Current sample index (use 0 for block-rate)
    */
    void applyControlInputs (const float** inputBuffers, int numInputs, int sampleIndex)
    {
        if (paramPortStartIndex < 0) return;

        int portIdx = paramPortStartIndex;
        for (auto& p : params)
        {
            if (p.modulatable)
            {
                if (portIdx < numInputs && inputBuffers[portIdx] != nullptr)
                    p.applyModulation (inputBuffers[portIdx][sampleIndex]);
                else
                    p.applyModulation (0.0f);
                
                portIdx++;
            }
        }
    }

    /** Check if params have been auto-exposed. */
    bool areParamsExposed() const { return paramsExposed; }

    //==========================================================================
    /** Serialize node state to JSON. */
    virtual juce::var toJSON() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("type", type);
        obj->setProperty ("name", name);
        obj->setProperty ("id", nodeID);
        if (visualX >= 0.0f) obj->setProperty ("visualX", (double) visualX);
        if (visualY >= 0.0f) obj->setProperty ("visualY", (double) visualY);

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
    virtual void fromJSON (const juce::var& json)
    {
        if (auto* obj = json.getDynamicObject())
        {
            name = obj->getProperty ("name").toString();
            if (obj->hasProperty ("filePath"))
                setFilePath (obj->getProperty ("filePath").toString());

            if (obj->hasProperty ("visualX"))
                visualX = (float)(double) obj->getProperty ("visualX");
            if (obj->hasProperty ("visualY"))
                visualY = (float)(double) obj->getProperty ("visualY");
                
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
    void addInput  (const juce::String& n, NodePort::Type t = NodePort::Audio, int ch = 1, bool isParamCV = false)
    { inputPorts.push_back ({ n, t, ch, isParamCV }); }

    void addOutput (const juce::String& n, NodePort::Type t = NodePort::Audio, int ch = 1)
    { outputPorts.push_back ({ n, t, ch, false }); }

    void addParam (const juce::String& id, const juce::String& pname,
                   float mn, float mx, float def, bool isMod = true)
    { params.emplace_back (id, pname, mn, mx, def, isMod); }

    void clearInputs()  { inputPorts.clear(); paramsExposed = false; paramPortStartIndex = -1; }
    void clearOutputs() { outputPorts.clear(); }
    void clearParams()  { params.clear(); paramsExposed = false; paramPortStartIndex = -1; }

    double sr = 44100.0;
    int blockSize = 512;

private:
    juce::String type;
    juce::String name;
    int nodeID = -1;

    std::vector<NodePort> inputPorts;
    std::vector<NodePort> outputPorts;
    std::vector<NodeParam> params;

    bool paramsExposed = false;
    int paramPortStartIndex = -1;  // index of first auto-generated CV port
};
