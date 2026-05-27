#pragma once

#include <juce_core/juce_core.h>
#include "SecondaryDisplay.h"

//==============================================================================
/**
 * Turing 3.5" Smart Screen V2 (USB35INCHIPSV2) display driver.
 *
 * Implements the Revision B serial protocol:
 *   - 115200 baud, RTS/CTS flow control
 *   - 10-byte command frames: [cmd, 8 payload bytes, cmd-repeat]
 *   - DISPLAY_BITMAP (0xCC) + raw big-endian RGB565 pixels
 *   - HELLO (0xCA "HELLO") for handshake
 *
 * Owns a background thread that drains a 1-deep frame queue and writes
 * bytes to the serial port. The message thread is never blocked on I/O.
 * Reconnects on unplug/replug by re-scanning for the device port.
 *
 * Native resolution: 480×320 (landscape). User-set Rotation is applied
 * as a final juce::Image transform before encoding.
 */
class TuringDisplay : public SecondaryDisplay,
                      private juce::Thread
{
public:
    // Turing 3.5" V2 boots in PORTRAIT (320 wide × 480 tall). We render
    // and address the bitmap in that native orientation; user-set Rotation
    // is applied as a final image transform in applyRotation().
    static constexpr int  kNativeWidth  = 320;
    static constexpr int  kNativeHeight = 480;
    static constexpr int  kBaudRate     = 115200;

    TuringDisplay();
    ~TuringDisplay() override;

    /** Try to find + open the Turing display. Returns true on success. */
    bool startConnection();

    /** Close the port and stop the worker thread. */
    void stopConnection();

    //==========================================================================
    // SecondaryDisplay
    juce::String getDisplayID()   const override { return "turing.usb35inchipsv2"; }
    juce::String getDisplayName() const override { return "Turing 3.5\" Display"; }
    juce::Rectangle<int> getNativeBounds() const override { return { 0, 0, kNativeWidth, kNativeHeight }; }
    bool isConnected() const override { return serialFd.load() >= 0; }
    void pushFrame (const juce::Image& frame) override;

private:
    void run() override;

    //==========================================================================
    // Protocol
    static constexpr juce::uint8 CMD_HELLO            = 0xCA;
    static constexpr juce::uint8 CMD_SET_ORIENTATION  = 0xCB;
    static constexpr juce::uint8 CMD_DISPLAY_BITMAP   = 0xCC;
    static constexpr juce::uint8 CMD_SET_LIGHTING     = 0xCD;
    static constexpr juce::uint8 CMD_SET_BRIGHTNESS   = 0xCE;

    static constexpr juce::uint8 ORIENTATION_PORTRAIT  = 0x0;
    static constexpr juce::uint8 ORIENTATION_LANDSCAPE = 0x1;

    bool openPort (const juce::String& devicePath);
    void closePort();
    bool sendHandshake();
    bool setHardwareOrientation();
    bool sendImage (const juce::Image& src);
    bool writeCommand (juce::uint8 cmd, const juce::uint8 payload[8]);
    bool writeBytes (const void* data, size_t length);

    juce::Image applyRotation (const juce::Image& src) const;
    static juce::String findDevicePath();

    //==========================================================================
    // State
    std::atomic<int> serialFd { -1 };    // POSIX file descriptor; -1 = closed
    juce::CriticalSection bufferLock;
    juce::Image pendingFrame;            // protected by bufferLock
    std::atomic<bool> frameAvailable { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuringDisplay)
};
