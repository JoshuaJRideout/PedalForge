#pragma once

#include "DSPNode.h"
#include <juce_core/juce_core.h>

class GridSequencerNode : public DSPNode
{
public:
    GridSequencerNode() : DSPNode ("grid_sequencer", "Grid Sequencer")
    {
        addParam ("bpm", "BPM", 20.0f, 300.0f, 120.0f);
        addParam ("run", "Run/Stop", 0.0f, 1.0f, 0.0f);
        
        // 8 tracks, each has 32 steps (boolean)
        for (int i = 0; i < 8; ++i)
        {
            juce::String tr = "tr" + juce::String(i);
            addParam (tr + "_div", "Div " + juce::String(i), 0.0f, 7.0f, 2.0f); // 0=1/1, 1=1/2, 2=1/4, 3=1/8, 4=1/16, etc.
            addParam (tr + "_swing", "Swing " + juce::String(i), 0.0f, 1.0f, 0.0f);
            addParam (tr + "_glitch", "Glitch " + juce::String(i), 0.0f, 1.0f, 0.0f);
            addParam (tr + "_len", "Len " + juce::String(i), 1.0f, 32.0f, 16.0f);
            
            // Output types: 0=Note, 1=CC, 2=PC, 3=ExprGate
            addParam (tr + "_mode", "Mode " + juce::String(i), 0.0f, 3.0f, 0.0f);
            addParam (tr + "_val1", "Val1 " + juce::String(i), 0.0f, 127.0f, 60.0f); // Note num or CC num
            addParam (tr + "_val2", "Val2 " + juce::String(i), 0.0f, 127.0f, 100.0f); // Velocity or CC val
            addParam (tr + "_chan", "Chan " + juce::String(i), 1.0f, 16.0f, 1.0f);
            
            for (int s = 0; s < 32; ++s)
            {
                addParam (tr + "_s" + juce::String(s), "Step " + juce::String(s), 0.0f, 1.0f, 0.0f, false);
            }
            
            addOutput ("gate_" + juce::String(i), NodePort::Gate);
        }
    }
    
    void process (const float** in, int numIn, float** out, int numOut, int numSamples) override
    {
        // Calculate step progression
        float bpm = getParam("bpm")->get();
        bool isRunning = getParam("run")->get() > 0.5f;
        
        // TODO: Full sequencing logic. For now, it's a skeleton.
        if (isRunning)
        {
            timeInSeconds += (float)numSamples / sampleRate;
            // ... beat logic
        }
        else
        {
            timeInSeconds = 0.0f;
        }
        
        // Output zero to all gates
        for (int i = 0; i < std::min(numOut, 8); ++i)
        {
            if (out[i] != nullptr)
                std::fill (out[i], out[i] + numSamples, 0.0f);
        }
    }

private:
    float timeInSeconds = 0.0f;
    float sampleRate = 44100.0f;
};
