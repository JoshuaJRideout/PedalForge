#pragma once

#include "DSPNode.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>

//==============================================================================
class RamNode : public DSPNode
{
public:
    RamNode() : DSPNode ("ram", "RAM / Delay Line")
    {
        addInput ("address_read", NodePort::Control);
        addInput ("address_write", NodePort::Control);
        addInput ("data_in", NodePort::Audio);
        addInput ("write_en", NodePort::Control);
        
        addOutput ("data_out", NodePort::Audio);
        
        addParam ("size", "Max Samples", 1.0f, 48000.0f * 10.0f, 48000.0f); // 1 sec default
        
        // 10 seconds at 48kHz
        memory.resize (480000, 0.0f);
    }

    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;

        float maxAddr = getParam("size")->get() - 1.0f;
        int maxIndex = (int)memory.size() - 1;

        for (int i = 0; i < n; ++i)
        {
            float readAddr = (numIn > 0 && in[0] != nullptr) ? in[0][i] : 0.0f;
            float writeAddr = (numIn > 1 && in[1] != nullptr) ? in[1][i] : 0.0f;
            float dataIn = (numIn > 2 && in[2] != nullptr) ? in[2][i] : 0.0f;
            bool writeEn = (numIn > 3 && in[3] != nullptr) && in[3][i] > 0.5f;

            int readIdx = juce::jlimit (0, maxIndex, (int)juce::jmin (readAddr, maxAddr));
            int writeIdx = juce::jlimit (0, maxIndex, (int)juce::jmin (writeAddr, maxAddr));

            out[0][i] = memory[readIdx];

            if (writeEn)
                memory[writeIdx] = dataIn;
        }
    }
private:
    std::vector<float> memory;
};

//==============================================================================
class IRNode : public DSPNode
{
public:
    IRNode() : DSPNode ("ir", "IR Convolution")
    {
        addInput ("in_l", NodePort::Audio);
        addInput ("in_r", NodePort::Audio);
        addOutput ("out_l", NodePort::Audio);
        addOutput ("out_r", NodePort::Audio);
        addParam ("mix", "Mix", 0.0f, 1.0f, 1.0f);
        addParam ("gain", "Gain", 0.0f, 10.0f, 1.0f);
    }
    
    void prepare (double sampleRate, int maxBlockSize) override
    {
        DSPNode::prepare (sampleRate, maxBlockSize);
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };
        convL.prepare (spec);
        convR.prepare (spec);
        
        if (filePath.isNotEmpty() && !irLoaded)
            setFilePath (filePath);
    }
    
    void reset() override
    {
        convL.reset();
        convR.reset();
    }
    
    void setFilePath (const juce::String& path) override
    {
        filePath = path;
        irLoaded = false;
        isTrueStereo = false;
        
        juce::File f(path);
        if (! f.existsAsFile()) return;
        
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (f));
        
        if (! reader) return;
        
        int numChannels = reader->numChannels;
        juce::AudioBuffer<float> fileBuffer (numChannels, (int)reader->lengthInSamples);
        reader->read (&fileBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
        
        if (numChannels == 4)
        {
            // True Stereo: Ch 0=LL, 1=LR, 2=RL, 3=RR
            isTrueStereo = true;
            juce::AudioBuffer<float> bufL (2, fileBuffer.getNumSamples());
            bufL.copyFrom (0, 0, fileBuffer, 0, 0, fileBuffer.getNumSamples());
            bufL.copyFrom (1, 0, fileBuffer, 1, 0, fileBuffer.getNumSamples());
            
            juce::AudioBuffer<float> bufR (2, fileBuffer.getNumSamples());
            bufR.copyFrom (0, 0, fileBuffer, 2, 0, fileBuffer.getNumSamples());
            bufR.copyFrom (1, 0, fileBuffer, 3, 0, fileBuffer.getNumSamples());
            
            convL.loadImpulseResponse (std::move (bufL), reader->sampleRate, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::yes, juce::dsp::Convolution::Normalise::yes);
            convR.loadImpulseResponse (std::move (bufR), reader->sampleRate, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::yes, juce::dsp::Convolution::Normalise::yes);
        }
        else if (numChannels == 1)
        {
            // Mono: duplicate to L and R
            juce::AudioBuffer<float> buf (2, fileBuffer.getNumSamples());
            buf.copyFrom (0, 0, fileBuffer, 0, 0, fileBuffer.getNumSamples());
            buf.copyFrom (1, 0, fileBuffer, 0, 0, fileBuffer.getNumSamples());
            convL.loadImpulseResponse (std::move (buf), reader->sampleRate, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::yes, juce::dsp::Convolution::Normalise::yes);
        }
        else
        {
            // Stereo (or 3 ch fallback to stereo)
            juce::AudioBuffer<float> buf (2, fileBuffer.getNumSamples());
            buf.copyFrom (0, 0, fileBuffer, 0, 0, fileBuffer.getNumSamples());
            buf.copyFrom (1, 0, fileBuffer, 1, 0, fileBuffer.getNumSamples());
            convL.loadImpulseResponse (std::move (buf), reader->sampleRate, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::yes, juce::dsp::Convolution::Normalise::yes);
        }
        
        irLoaded = true;
    }

    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0 || numIn == 0) return;
        
        float mix = getParam("mix")->get();
        float gain = getParam("gain")->get();
        
        const float* inL = (numIn > 0 && in[0]) ? in[0] : nullptr;
        const float* inR = (numIn > 1 && in[1]) ? in[1] : inL; // fallback to mono input
        
        if (!inL) return; // Need at least one input
        if (!inR) inR = inL;

        // We need scratch buffers because juce::dsp::ProcessContextReplacing overwrites the block
        juce::AudioBuffer<float> dryBuffer (2, n);
        dryBuffer.copyFrom (0, 0, inL, n);
        dryBuffer.copyFrom (1, 0, inR, n);

        if (! irLoaded)
        {
            // Just pass through if no IR loaded
            for (int i = 0; i < n; ++i)
            {
                if (numOut > 0 && out[0]) out[0][i] = dryBuffer.getReadPointer(0)[i] * gain;
                if (numOut > 1 && out[1]) out[1][i] = dryBuffer.getReadPointer(1)[i] * gain;
            }
            return;
        }

        if (isTrueStereo)
        {
            // TRUE STEREO PROCESSING
            // Engine L processes Input L, outputs LL and LR
            juce::AudioBuffer<float> procL (2, n);
            procL.copyFrom (0, 0, inL, n);
            procL.copyFrom (1, 0, inL, n);
            
            // Engine R processes Input R, outputs RL and RR
            juce::AudioBuffer<float> procR (2, n);
            procR.copyFrom (0, 0, inR, n);
            procR.copyFrom (1, 0, inR, n);
            
            juce::dsp::AudioBlock<float> blockL (procL);
            juce::dsp::ProcessContextReplacing<float> ctxL (blockL);
            convL.process (ctxL);
            
            juce::dsp::AudioBlock<float> blockR (procR);
            juce::dsp::ProcessContextReplacing<float> ctxR (blockR);
            convR.process (ctxR);
            
            // Output L = LL + RL
            // Output R = LR + RR
            for (int i = 0; i < n; ++i)
            {
                float wetL = procL.getReadPointer(0)[i] + procR.getReadPointer(0)[i];
                float wetR = procL.getReadPointer(1)[i] + procR.getReadPointer(1)[i];
                
                if (numOut > 0 && out[0]) out[0][i] = (inL[i] * (1.0f - mix) + wetL * mix) * gain;
                if (numOut > 1 && out[1]) out[1][i] = (inR[i] * (1.0f - mix) + wetR * mix) * gain;
            }
        }
        else
        {
            // MONO/STEREO PROCESSING
            juce::AudioBuffer<float> procBuf (2, n);
            procBuf.copyFrom (0, 0, inL, n);
            procBuf.copyFrom (1, 0, inR, n);
            
            juce::dsp::AudioBlock<float> block (procBuf);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            convL.process (ctx);
            
            for (int i = 0; i < n; ++i)
            {
                float wetL = procBuf.getReadPointer(0)[i];
                float wetR = procBuf.getReadPointer(1)[i];
                
                if (numOut > 0 && out[0]) out[0][i] = (inL[i] * (1.0f - mix) + wetL * mix) * gain;
                if (numOut > 1 && out[1]) out[1][i] = (inR[i] * (1.0f - mix) + wetR * mix) * gain;
            }
        }
    }
private:
    juce::dsp::Convolution convL;
    juce::dsp::Convolution convR;
    bool irLoaded = false;
    bool isTrueStereo = false;
};

//==============================================================================
class SamplerNode : public DSPNode
{
public:
    SamplerNode() : DSPNode ("sampler", "File Sampler")
    {
        addInput ("trigger", NodePort::Control);
        addOutput ("out", NodePort::Audio);
        addParam ("pitch", "Pitch", 0.1f, 4.0f, 1.0f); // playback speed
        addParam ("loop", "Loop", 0.0f, 1.0f, 0.0f);
    }
    
    void setFilePath (const juce::String& path) override
    {
        filePath = path;
        juce::File f(path);
        if (f.existsAsFile())
        {
            juce::AudioFormatManager formatManager;
            formatManager.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (f));
            
            if (reader)
            {
                sampleBuffer.setSize (1, (int)reader->lengthInSamples);
                reader->read (&sampleBuffer, 0, (int)reader->lengthInSamples, 0, true, false);
                hasSample = true;
            }
        }
    }

    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        
        if (!hasSample)
        {
            for (int i = 0; i < n; ++i) out[0][i] = 0.0f;
            return;
        }
        
        float pitch = getParam("pitch")->get();
        bool loop = getParam("loop")->get() > 0.5f;
        int maxSample = sampleBuffer.getNumSamples() - 1;
        const float* sampleData = sampleBuffer.getReadPointer(0);

        for (int i = 0; i < n; ++i)
        {
            bool trigger = (numIn > 0 && in[0] != nullptr) && in[0][i] > 0.5f;
            
            if (trigger && !lastTrigger)
            {
                playPos = 0.0;
                playing = true;
            }
            lastTrigger = trigger;
            
            if (playing)
            {
                int posInt = (int)playPos;
                float frac = (float)(playPos - posInt);
                
                if (posInt >= maxSample)
                {
                    if (loop) playPos -= maxSample;
                    else { playing = false; out[0][i] = 0.0f; continue; }
                    posInt = (int)playPos;
                }
                
                float val1 = sampleData[posInt];
                float val2 = (posInt + 1 <= maxSample) ? sampleData[posInt + 1] : 0.0f;
                out[0][i] = val1 + frac * (val2 - val1); // linear interpolation
                
                playPos += pitch;
            }
            else
            {
                out[0][i] = 0.0f;
            }
        }
    }
private:
    juce::AudioBuffer<float> sampleBuffer;
    bool hasSample = false;
    double playPos = 0.0;
    bool playing = false;
    bool lastTrigger = false;
};

//==============================================================================
class RisingEdgeNode : public DSPNode
{
public:
    RisingEdgeNode() : DSPNode ("edge_rising", "Rising Edge")
    {
        addInput ("in", NodePort::Control);
        addOutput ("out", NodePort::Gate);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
        {
            float val = (numIn > 0 && in[0] != nullptr) ? in[0][i] : 0.0f;
            out[0][i] = (val > 0.0f && lastVal <= 0.0f) ? 1.0f : 0.0f;
            lastVal = val;
        }
    }
    void reset() override { lastVal = 0.0f; }
private:
    float lastVal = 0.0f;
};

//==============================================================================
class FallingEdgeNode : public DSPNode
{
public:
    FallingEdgeNode() : DSPNode ("edge_falling", "Falling Edge")
    {
        addInput ("in", NodePort::Control);
        addOutput ("out", NodePort::Gate);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
        {
            float val = (numIn > 0 && in[0] != nullptr) ? in[0][i] : 0.0f;
            out[0][i] = (val <= 0.0f && lastVal > 0.0f) ? 1.0f : 0.0f;
            lastVal = val;
        }
    }
    void reset() override { lastVal = 0.0f; }
private:
    float lastVal = 0.0f;
};

//==============================================================================
class ChangeDetectorNode : public DSPNode
{
public:
    ChangeDetectorNode() : DSPNode ("change_det", "Change Detector")
    {
        addInput ("in", NodePort::Control);
        addOutput ("out", NodePort::Gate);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
        {
            float val = (numIn > 0 && in[0] != nullptr) ? in[0][i] : 0.0f;
            out[0][i] = (val != lastVal) ? 1.0f : 0.0f;
            lastVal = val;
        }
    }
    void reset() override { lastVal = 0.0f; }
private:
    float lastVal = 0.0f;
};

//==============================================================================
class DeltaNode : public DSPNode
{
public:
    DeltaNode() : DSPNode ("delta", "Delta")
    {
        addInput ("in", NodePort::Control);
        addOutput ("out", NodePort::Control);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
        {
            float val = (numIn > 0 && in[0] != nullptr) ? in[0][i] : 0.0f;
            out[0][i] = val - lastVal;
            lastVal = val;
        }
    }
    void reset() override { lastVal = 0.0f; }
private:
    float lastVal = 0.0f;
};

//==============================================================================
// TIME & TRIGGERS
class LogicDelayNode : public DSPNode
{
public:
    LogicDelayNode() : DSPNode ("logic_delay", "Logic Delay")
    {
        addInput ("in", NodePort::Control);
        addOutput ("out", NodePort::Control);
        addParam ("delay_ms", "Delay (ms)", 0.0f, 5000.0f, 100.0f);
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        DSPNode::prepare (sampleRate, maxBlockSize);
        sr = sampleRate;
        buffer.resize ((int)(sampleRate * 5.0), 0.0f); // 5 sec max
        writePos = 0;
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        float delaySamples = getParam("delay_ms")->get() * (float)sr * 0.001f;
        int dSamples = juce::jlimit (0, (int)buffer.size() - 1, (int)delaySamples);
        
        for (int i = 0; i < n; ++i)
        {
            float val = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            buffer[writePos] = val;
            
            int readPos = writePos - dSamples;
            if (readPos < 0) readPos += buffer.size();
            
            out[0][i] = buffer[readPos];
            
            writePos = (writePos + 1) % buffer.size();
        }
    }
private:
    std::vector<float> buffer;
    int writePos = 0;
    double sr = 44100.0;
};

class PulseWidthNode : public DSPNode
{
public:
    PulseWidthNode() : DSPNode ("pulse_width", "Pulse Width")
    {
        addInput ("trig", NodePort::Gate);
        addOutput ("out", NodePort::Gate);
        addParam ("width_ms", "Width (ms)", 1.0f, 5000.0f, 100.0f);
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        DSPNode::prepare (sampleRate, maxBlockSize);
        sr = sampleRate;
        samplesRemaining = 0;
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        float widthSamples = getParam("width_ms")->get() * (float)sr * 0.001f;
        
        for (int i = 0; i < n; ++i)
        {
            float trig = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            if (trig > 0.0f && lastTrig <= 0.0f)
                samplesRemaining = (int)widthSamples;
            lastTrig = trig;
            
            if (samplesRemaining > 0)
            {
                out[0][i] = 1.0f;
                samplesRemaining--;
            }
            else
            {
                out[0][i] = 0.0f;
            }
        }
    }
private:
    int samplesRemaining = 0;
    float lastTrig = 0.0f;
    double sr = 44100.0;
};

class OneShotNode : public DSPNode
{
public:
    OneShotNode() : DSPNode ("one_shot", "One-Shot")
    {
        addInput ("trig", NodePort::Gate);
        addOutput ("out", NodePort::Gate);
        addParam ("width_ms", "Width (ms)", 1.0f, 5000.0f, 100.0f);
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        DSPNode::prepare (sampleRate, maxBlockSize);
        sr = sampleRate;
        samplesRemaining = 0;
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        float widthSamples = getParam("width_ms")->get() * (float)sr * 0.001f;
        
        for (int i = 0; i < n; ++i)
        {
            float trig = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            // Only re-trigger if NOT currently playing
            if (trig > 0.0f && lastTrig <= 0.0f && samplesRemaining <= 0)
                samplesRemaining = (int)widthSamples;
            lastTrig = trig;
            
            if (samplesRemaining > 0)
            {
                out[0][i] = 1.0f;
                samplesRemaining--;
            }
            else
            {
                out[0][i] = 0.0f;
            }
        }
    }
private:
    int samplesRemaining = 0;
    float lastTrig = 0.0f;
    double sr = 44100.0;
};

class DebounceNode : public DSPNode
{
public:
    DebounceNode() : DSPNode ("debounce", "Debounce")
    {
        addInput ("in", NodePort::Control);
        addOutput ("out", NodePort::Control);
        addParam ("time_ms", "Time (ms)", 1.0f, 1000.0f, 50.0f);
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        DSPNode::prepare (sampleRate, maxBlockSize);
        sr = sampleRate;
        samplesRemaining = 0;
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        float timeSamples = getParam("time_ms")->get() * (float)sr * 0.001f;
        
        for (int i = 0; i < n; ++i)
        {
            float val = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            
            if (val != pendingVal)
            {
                pendingVal = val;
                samplesRemaining = (int)timeSamples;
            }
            
            if (samplesRemaining > 0)
            {
                samplesRemaining--;
                if (samplesRemaining == 0)
                    currentVal = pendingVal;
            }
            
            out[0][i] = currentVal;
        }
    }
private:
    float currentVal = 0.0f;
    float pendingVal = 0.0f;
    int samplesRemaining = 0;
    double sr = 44100.0;
};

class BlinkNode : public DSPNode
{
public:
    BlinkNode() : DSPNode ("blink", "Blink")
    {
        addInput ("en", NodePort::Gate);
        addOutput ("out", NodePort::Gate);
        addParam ("rate_hz", "Rate (Hz)", 0.1f, 50.0f, 2.0f);
        addParam ("duty", "Duty %", 1.0f, 99.0f, 50.0f);
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        DSPNode::prepare (sampleRate, maxBlockSize);
        sr = sampleRate;
        phase = 0.0f;
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        float rate = getParam("rate_hz")->get();
        float duty = getParam("duty")->get() * 0.01f;
        float inc = rate / (float)sr;
        
        for (int i = 0; i < n; ++i)
        {
            float en = (numIn > 0 && in[0]) ? in[0][i] : 1.0f;
            if (en > 0.5f)
            {
                out[0][i] = (phase < duty) ? 1.0f : 0.0f;
                phase += inc;
                if (phase >= 1.0f) phase -= 1.0f;
            }
            else
            {
                out[0][i] = 0.0f;
                phase = 0.0f;
            }
        }
    }
private:
    float phase = 0.0f;
    double sr = 44100.0;
};

class RampNode : public DSPNode
{
public:
    RampNode() : DSPNode ("ramp", "Ramp")
    {
        addInput ("en", NodePort::Gate);
        addOutput ("out", NodePort::Control);
        addParam ("rise_ms", "Rise (ms)", 1.0f, 10000.0f, 1000.0f);
        addParam ("fall_ms", "Fall (ms)", 1.0f, 10000.0f, 1000.0f);
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        DSPNode::prepare (sampleRate, maxBlockSize);
        sr = sampleRate;
        currentVal = 0.0f;
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        float riseInc = 1.0f / (getParam("rise_ms")->get() * (float)sr * 0.001f);
        float fallInc = 1.0f / (getParam("fall_ms")->get() * (float)sr * 0.001f);
        
        for (int i = 0; i < n; ++i)
        {
            float en = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            if (en > 0.5f)
            {
                currentVal += riseInc;
                if (currentVal > 1.0f) currentVal = 1.0f;
            }
            else
            {
                currentVal -= fallInc;
                if (currentVal < 0.0f) currentVal = 0.0f;
            }
            out[0][i] = currentVal;
        }
    }
private:
    float currentVal = 0.0f;
    double sr = 44100.0;
};

//==============================================================================
// ARRAYS
class ArrayNode : public DSPNode
{
public:
    ArrayNode() : DSPNode ("array", "Array")
    {
        addInput ("index", NodePort::Control);
        addInput ("val_in", NodePort::Control);
        addInput ("set", NodePort::Gate);
        addInput ("push", NodePort::Gate);
        addInput ("pop", NodePort::Gate);
        addInput ("clear", NodePort::Gate);
        
        addOutput ("val_out", NodePort::Control);
        addOutput ("length", NodePort::Control);
    }
    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0) return;
        for (int i = 0; i < n; ++i)
        {
            float index  = (numIn > 0 && in[0]) ? in[0][i] : 0.0f;
            float val_in = (numIn > 1 && in[1]) ? in[1][i] : 0.0f;
            bool  set    = (numIn > 2 && in[2]) && (in[2][i] > 0.5f);
            bool  push   = (numIn > 3 && in[3]) && (in[3][i] > 0.5f);
            bool  pop    = (numIn > 4 && in[4]) && (in[4][i] > 0.5f);
            bool  clear  = (numIn > 5 && in[5]) && (in[5][i] > 0.5f);
            
            if (clear && !lastClear) data.clear();
            
            if (push && !lastPush) data.push_back(val_in);
            
            if (pop && !lastPop && !data.empty()) data.pop_back();
            
            int idx = (int)index;
            if (set && !lastSet && idx >= 0 && idx < data.size()) {
                data[idx] = val_in;
            }
            
            lastSet = set;
            lastPush = push;
            lastPop = pop;
            lastClear = clear;
            
            if (idx >= 0 && idx < data.size()) {
                out[0][i] = data[idx];
            } else {
                out[0][i] = 0.0f;
            }
            
            if (numOut > 1 && out[1]) {
                out[1][i] = (float)data.size();
            }
        }
    }
    
    void reset() override
    {
        data.clear();
        lastSet = false;
        lastPush = false;
        lastPop = false;
        lastClear = false;
    }
    
private:
    std::vector<float> data;
    bool lastSet = false;
    bool lastPush = false;
    bool lastPop = false;
    bool lastClear = false;
};
