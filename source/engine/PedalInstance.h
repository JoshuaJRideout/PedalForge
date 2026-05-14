#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>

struct PedalDesign;  // forward declaration

//==============================================================================
/**
 * Runtime state for a pedal instance on the board.
 * Tracks grid position, bypass state, and the graph node ID.
 */
struct PedalInstance
{
    juce::AudioProcessorGraph::NodeID nodeID;

    juce::String name;
    juce::String category;      // e.g. "Overdrive", "Delay", "Reverb"
    juce::Colour colour;        // Pedal colour for UI
    int numKnobs = 3;           // Number of knobs to draw on the face

    // Grid position and size
    int gridX = 0;
    int gridY = 0;
    int gridW = 1;              // Default: 1 cell wide
    int gridH = 2;              // Default: 2 cells tall

    // State
    bool bypassed = false;
    int  rotation = 0;          // 0, 90, 180, 270 degrees

    // Custom design
    std::shared_ptr<PedalDesign> design;

    // Live control values for custom designs (controlID → value 0..1)
    std::map<juce::String, float> controlValues;
    std::map<juce::String, juce::String> controlTexts;
};
