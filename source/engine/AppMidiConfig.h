#pragma once

struct AppMidiConfig
{
    int turingPrevCC = 80;
    int turingNextCC = 81;
    int playModeToggleCC = -1;

    // Navigation — Page & Track buttons
    int pageLeftCC  = -1;   // Switch to previous pedalboard page
    int pageRightCC = -1;   // Switch to next pedalboard page
    int trackLeftCC  = -1;  // Select previous pedal on current page
    int trackRightCC = -1;  // Select next pedal on current page

    enum class NovationMode
    {
        AutoMap,     // Auto-map knobs to focused pedal
        Passthrough, // Pass all unlocked CCs/Notes directly to graph for pedal use
        PresetRecall // Use buttons for presets
    };
    NovationMode novationMode = NovationMode::Passthrough; // Defaulting to Passthrough as user emphasized flexibility
};
