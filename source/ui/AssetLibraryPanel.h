#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PedalPalette.h"

// Forward declarations for future tabs
class ComponentLibrary;
class NodeLibrary;
class ImageLibrary;

//==============================================================================
/**
 * A tabbed library manager that replaces the simple PedalPalette.
 * Contains tabs for Pedals, Components, Nodes, IRs, Images, and Audio.
 */
class AssetLibraryPanel : public juce::Component
{
public:
    AssetLibraryPanel();
    ~AssetLibraryPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Access to the pedal palette for drag/drop
    PedalPalette& getPedalPalette() { return pedalPalette; }

private:
    juce::TabbedComponent tabs { juce::TabbedButtonBar::Orientation::TabsAtTop };

    PedalPalette pedalPalette;
    
    // Placeholder components for other tabs
    juce::Component componentsTab;
    juce::Component nodesTab;
    juce::Component irsTab;
    juce::Component imagesTab;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AssetLibraryPanel)
};
