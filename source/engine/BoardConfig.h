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
    
    // Canvas position (in pixels or cells)
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
