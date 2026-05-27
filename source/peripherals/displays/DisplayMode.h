#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class AudioGraphEngine;

//==============================================================================
/**
 * Abstract base for anything that can be shown on a SecondaryDisplay.
 *
 * Modes paint into an arbitrary bounds rectangle. The same mode renders
 * identically on a Turing screen, an HDMI bar display, or in an in-app
 * preview Component, because every mode is itself a juce::Component.
 *
 * Concrete modes (initial):
 *   - PedalViewerMode   active pedal mirror (deferred — TuringRenderer
 *                       still owns this for now)
 *   - MidiMonitorMode   live scrolling MIDI feed
 *   - IdleClockMode     date/time/logo
 *
 * Modes get a TuringContext (engine refs, time, etc.) to subscribe to
 * data without owning singletons themselves.
 */
struct DisplayContext
{
    AudioGraphEngine* engine = nullptr;
    juce::Time        lastFrameTime;
    bool              deviceConnected = false;
};


class DisplayMode : public juce::Component
{
public:
    ~DisplayMode() override = default;

    /** Stable mode identifier, e.g. "pedal_viewer", "midi_monitor". */
    virtual juce::String getModeID() const = 0;

    /** Human-readable mode name shown in the mode picker. */
    virtual juce::String getModeDisplayName() const = 0;

    /** Mode renders into the given bounds. Default just calls paint() with
        bounds set to the component. Subclasses override paint() — the
        juce::Component mechanism does the rest. */
    virtual void renderInto (juce::Graphics& g, juce::Rectangle<int> bounds, const DisplayContext& ctx)
    {
        setBounds (bounds);
        context = ctx;
        paint (g);
    }

    /** Hz at which the DisplayManager should request fresh frames for this
        mode. Most modes are happy at 15 Hz; tuner/metronome later may want
        higher. Override as needed. */
    virtual int getPreferredFPS() const { return 15; }

    /** Aspect-ratio hint so the per-display mode picker can filter to
        sensible modes. */
    enum class AspectHint { Any, Portrait, Landscape, Bar };
    virtual AspectHint getPreferredAspect() const { return AspectHint::Any; }

    /** Called when this mode becomes active on a display. Mode can wire
        up any subscriptions / timers here. */
    virtual void onActivate (const DisplayContext&) {}

    /** Called when this mode is swapped out for another. */
    virtual void onDeactivate() {}

protected:
    DisplayContext context;
};
