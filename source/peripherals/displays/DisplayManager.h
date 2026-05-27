#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>
#include "SecondaryDisplay.h"
#include "DisplayMode.h"

class AudioGraphEngine;

//==============================================================================
/**
 * Owns the connected SecondaryDisplays and an active DisplayMode for each.
 * Drives a render loop on the message thread that paints the active mode
 * into a juce::Image and hands it to the display via pushFrame().
 *
 * One DisplayManager per app (created in PluginEditor or PluginProcessor).
 * Displays attach/detach on hot-plug; modes can be hot-swapped via
 * setActiveMode().
 */
class DisplayManager : private juce::Timer
{
public:
    explicit DisplayManager (AudioGraphEngine& engineRef);
    ~DisplayManager() override;

    //==========================================================================
    /** Add a display. Manager takes ownership. */
    void attachDisplay (std::unique_ptr<SecondaryDisplay> display);

    /** Find a display by ID (returns nullptr if not attached). */
    SecondaryDisplay* findDisplay (const juce::String& displayID);

    /** Convenience for "the single connected display" cases. */
    SecondaryDisplay* getPrimaryDisplay();

    //==========================================================================
    /** Register a mode the manager can switch to. Manager takes ownership. */
    void registerMode (std::unique_ptr<DisplayMode> mode);

    /** Switch the active mode for a given display. Pass empty modeID to
        deactivate. */
    void setActiveMode (const juce::String& displayID, const juce::String& modeID);

    /** Currently active mode for a display, or nullptr if none. */
    DisplayMode* getActiveMode (const juce::String& displayID);

    /** All registered modes — for mode picker UI. */
    const std::vector<std::unique_ptr<DisplayMode>>& getModes() const { return modes; }

private:
    void timerCallback() override;
    void renderForDisplay (SecondaryDisplay& display, DisplayMode& mode);

    AudioGraphEngine& engine;
    std::vector<std::unique_ptr<SecondaryDisplay>> displays;
    std::vector<std::unique_ptr<DisplayMode>> modes;

    // displayID → active modeID
    std::map<juce::String, juce::String> activeModeByDisplay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DisplayManager)
};
