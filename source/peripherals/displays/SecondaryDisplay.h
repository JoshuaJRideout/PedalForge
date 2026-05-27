#pragma once

#include <juce_graphics/juce_graphics.h>

//==============================================================================
/**
 * Abstract base for a secondary display attached to PedalForge.
 *
 * Two concrete kinds (initially):
 *   - TuringDisplay  — USB-serial Turing 3.5" Smart Screen V2 (480x320, ~15 Hz)
 *   - HdmiDisplay    — juce::Component on a real monitor (future)
 *
 * Modes (PedalViewerMode, MidiMonitorMode, IdleClockMode, TunerMode, ...)
 * paint juce::Images at the display's natural orientation. The display
 * applies its rotation (0/90/180/270) as the FINAL transform before the
 * pixels hit the wire / window. Mode code is rotation-agnostic.
 */
class SecondaryDisplay
{
public:
    enum class Rotation { Deg0 = 0, Deg90 = 90, Deg180 = 180, Deg270 = 270 };

    virtual ~SecondaryDisplay() = default;

    //==========================================================================
    /** Stable identifier for this display, used by DisplayManager and any
        Companion pedal pointing at it. e.g. "turing.usb35inchipsv2". */
    virtual juce::String getDisplayID() const = 0;

    /** Human-readable name shown in pickers. */
    virtual juce::String getDisplayName() const = 0;

    /** Natural pixel size at the device's hardware orientation (before
        user rotation). For Turing 3.5" this is 480x320. */
    virtual juce::Rectangle<int> getNativeBounds() const = 0;

    /** Effective bounds after rotation — modes render into this. */
    juce::Rectangle<int> getRenderBounds() const
    {
        auto n = getNativeBounds();
        return (rotation == Rotation::Deg90 || rotation == Rotation::Deg270)
                   ? juce::Rectangle<int>(0, 0, n.getHeight(), n.getWidth())
                   : n;
    }

    /** Is the display physically connected and responsive? */
    virtual bool isConnected() const = 0;

    /** Push a freshly-painted frame to the device.
        Concrete subclasses are responsible for any rotation transform
        and protocol encoding. Safe to call from the message thread —
        actual I/O happens on a worker thread inside the subclass. */
    virtual void pushFrame (const juce::Image& frame) = 0;

    //==========================================================================
    Rotation getRotation() const  { return rotation; }
    void setRotation (Rotation r) { rotation = r; }

protected:
    Rotation rotation = Rotation::Deg0;
};
