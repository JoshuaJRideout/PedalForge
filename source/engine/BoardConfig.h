#pragma once
#include <juce_core/juce_core.h>

struct BoardConfig
{
    juce::String id;
    juce::String name;
    int cols = 8;
    int rows = 4;
    int numPages = 1;
    int activePage = 0;
    int displayIndex = -1; // -1 = main window, >= 0 external monitors
    
    // Virtual ID for routing connections
    uint32_t engineNodeId = 0;
    
    // Position on the Routing Canvas (for routing hardware MIDI to the board)
    float routeX = 100.0f;
    float routeY = 100.0f;
    
    // Canvas position (in pixels or cells) for its own internal grid view
    int canvasX = 0;
    int canvasY = 0;
    
    // Turing Display Settings
    bool assignToTuring = false;
    bool turingHorizontal = false;
    int turingPedalIndex = 0; // The currently active pedal index for the Turing display
    
    // MIDI Assignments (-1 means unassigned)
    int prevPageCC = -1;
    int nextPageCC = -1;
};
