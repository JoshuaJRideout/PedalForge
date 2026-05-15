#include "AssetLibraryPanel.h"
#include "LookAndFeel.h"

AssetLibraryPanel::AssetLibraryPanel()
{
    tabs.setOutline (0);
    tabs.setTabBarDepth (30);
    
    auto bg = PedalForgeLookAndFeel::bgMid;
    
    // Add the tabs
    tabs.addTab ("Pedals", bg, &pedalPalette, false);
    
    // Placeholders
    tabs.addTab ("Parts", bg, &componentsTab, false);
    tabs.addTab ("Nodes", bg, &nodesTab, false);
    tabs.addTab ("IRs", bg, &irsTab, false);
    tabs.addTab ("Images", bg, &imagesTab, false);

    addAndMakeVisible (tabs);
}

AssetLibraryPanel::~AssetLibraryPanel() = default;

void AssetLibraryPanel::paint (juce::Graphics& g)
{
    g.fillAll (PedalForgeLookAndFeel::bgMid);
    
    g.setColour (PedalForgeLookAndFeel::gridLine);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());
}

void AssetLibraryPanel::resized()
{
    tabs.setBounds (getLocalBounds().withWidth (getWidth() - 1));
}
