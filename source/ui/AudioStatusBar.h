#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class PedalForgeProcessor;

//==============================================================================
/**
 * Always-visible status bar at the bottom of PluginEditor showing the
 * audio I/O state the user cares about while playing:
 *
 *   ▓░░ in L/R   Scarlett 2i2  44.1k/128   [─master─]  [MUTE]   ░▓░ out L/R   ⚙
 *
 * - Stereo input meters (peak), right-click → per-channel input gain
 * - Device name + sample rate + buffer size
 * - Master volume slider + mute toggle
 * - Stereo output meters (peak), right-click → per-channel output gain
 * - Settings cog opens the in-app audio settings dialog (Standalone only)
 *
 * Polls the processor's atomic peak/level state ~30 Hz. Read-only — does
 * not interfere with the audio thread.
 */
class AudioStatusBar : public juce::Component,
                       private juce::Timer
{
public:
    explicit AudioStatusBar (PedalForgeProcessor& proc);
    ~AudioStatusBar() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    /** Push device label from the standalone shell (e.g. "Scarlett 2i2
        44.1k/128"). Empty string hides the label. */
    void setDeviceLabel (const juce::String& label);

private:
    void timerCallback() override;
    void openInputGainMenu();
    void openOutputGainMenu();
    void openAudioSettings();

    PedalForgeProcessor& processor;

    juce::Slider masterSlider;
    juce::TextButton muteBtn { "MUTE" };
    juce::TextButton settingsBtn { juce::CharPointer_UTF8 ("\xe2\x9a\x99") }; // ⚙

    juce::Rectangle<int> inputMeterArea;
    juce::Rectangle<int> outputMeterArea;
    juce::String deviceLabel;

    // Smoothed display values (the atomic peaks fluctuate fast).
    float displayedInputPeak [2] { 0.0f, 0.0f };
    float displayedOutputPeak[2] { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioStatusBar)
};
