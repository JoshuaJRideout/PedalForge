#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>

struct PedalDesign;  // forward declaration

#include <atomic>

struct PedalMeters
{
    std::atomic<float> outRMS[2] { {0.0f}, {0.0f} };
    std::atomic<bool> midiOut { false };
};

//==============================================================================
/**
 * Runtime state for a pedal instance on the board.
 * Tracks grid position, bypass state, and the graph node ID.
 */
struct PedalInstance
{
    juce::AudioProcessorGraph::NodeID nodeID;
    
    std::shared_ptr<PedalMeters> meters;

    juce::String name;
    juce::String category;      // e.g. "Overdrive", "Delay", "Reverb"
    juce::Colour colour;        // Pedal colour for UI
    int numKnobs = 3;           // Number of knobs to draw on the face

    // Board position and size (in pixels)
    float boardX = 0.0f;
    float boardY = 0.0f;
    float boardW = 100.0f;
    float boardH = 200.0f;

    // State
    bool bypassed = false;
    bool onBoard  = true;       // false = exists in engine but not placed on board grid
    juce::String boardId;       // Empty = default main board
    int pageIndex = 0;          // Page within the board
    int  rotation = 0;          // 0, 90, 180, 270 degrees

    // Routing-tab canvas position (persisted across tab switches)
    float routeX = -1.0f;      // -1 = not yet placed (use auto-layout)
    float routeY = -1.0f;

    // Custom design
    std::shared_ptr<PedalDesign> design;

    // Live control values for custom designs (controlID → value 0..1)
    std::map<juce::String, float> controlValues;
    std::map<juce::String, juce::String> controlTexts;
    std::map<juce::String, std::vector<float>> controlData;
};
