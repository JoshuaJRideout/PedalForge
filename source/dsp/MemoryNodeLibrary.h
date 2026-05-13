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
        addInput ("in", NodePort::Audio);
        addOutput ("out", NodePort::Audio);
        addParam ("mix", "Mix", 0.0f, 1.0f, 1.0f);
    }
    
    void prepare (double sampleRate, int maxBlockSize) override
    {
        DSPNode::prepare (sampleRate, maxBlockSize);
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 1 };
        conv.prepare (spec);
        
        if (filePath.isNotEmpty() && !irLoaded)
            setFilePath (filePath);
    }
    
    void reset() override
    {
        conv.reset();
    }
    
    void setFilePath (const juce::String& path) override
    {
        filePath = path;
        juce::File f(path);
        if (f.existsAsFile())
        {
            conv.loadImpulseResponse (f, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes, 0);
            irLoaded = true;
        }
    }

    void process (const float** in, int numIn, float** out, int numOut, int n) override
    {
        if (numOut == 0 || numIn == 0 || in[0] == nullptr) return;
        
        float mix = getParam("mix")->get();

        // Convolution requires AudioBlock
        juce::AudioBuffer<float> buffer (1, n);
        buffer.copyFrom (0, 0, in[0], n);
        
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        
        conv.process (context);
        
        for (int i = 0; i < n; ++i)
        {
            out[0][i] = in[0][i] * (1.0f - mix) + buffer.getReadPointer(0)[i] * mix;
        }
    }
private:
    juce::dsp::Convolution conv;
    bool irLoaded = false;
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
