#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/PluginHostNode.h"

class PluginScannerWindow : public juce::DocumentWindow
{
public:
    PluginScannerWindow() 
        : juce::DocumentWindow("Plugin Scanner", juce::Colours::darkgrey, juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        
        juce::File dmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("PedalForge_DMP");
        
        scanner = std::make_unique<juce::PluginListComponent>(
            PluginHostNode::getFormatManager(),
            PluginHostNode::getKnownPluginList(),
            dmp, nullptr, true);
            
        setContentNonOwned(scanner.get(), true);
        centreWithSize(800, 600);
        setVisible(true);
    }
    
    void closeButtonPressed() override
    {
        PluginHostNode::saveKnownPluginList();
        delete this;
    }

private:
    std::unique_ptr<juce::PluginListComponent> scanner;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginScannerWindow)
};
