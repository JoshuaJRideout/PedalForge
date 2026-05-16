#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/AudioGraphEngine.h"

class TuringRenderer : public juce::Timer
{
public:
    TuringRenderer (AudioGraphEngine& engine);
    ~TuringRenderer() override;
    
    void timerCallback() override;

private:
    AudioGraphEngine& engine;
    
    int lastPedalNodeId = -1;
    juce::String lastValuesHash;
    bool lastOrientation = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuringRenderer)
};
