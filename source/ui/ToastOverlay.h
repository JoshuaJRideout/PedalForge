#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
 * Non-modal toast notifications stacked in the lower-right of the editor.
 *
 * Usage from anywhere in the codebase:
 *
 *     pf::toastInfo  ("Saved as MyRig.pfboard");
 *     pf::toastWarn  ("Audio device disconnected");
 *     pf::toastError ("Couldn't load reverb.nam: file missing");
 *
 * Each call also appends a timestamped line to
 * `~/Library/Logs/PedalForge/log.txt` so users can review history.
 *
 * One ToastOverlay instance is added as a top-most child of PluginEditor.
 * The static singleton hook is set/cleared in its constructor/destructor.
 * If no overlay is alive (e.g. the plugin window is closed in a DAW), the
 * toast call falls through silently — only the log file is written.
 */
class ToastOverlay : public juce::Component,
                     private juce::Timer
{
public:
    enum Severity { Info, Warn, Error };

    ToastOverlay();
    ~ToastOverlay() override;

    /** Show a toast. Safe to call from any thread — re-routes to the
        message thread internally. */
    void post (Severity sev, const juce::String& message);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    /** Global accessor. nullptr if no overlay is currently mounted. */
    static ToastOverlay* getInstance() noexcept { return s_instance; }

private:
    struct Entry
    {
        Severity sev;
        juce::String message;
        juce::int64 createdMs;
        juce::int64 expiresMs;
        float opacity = 1.0f;
    };

    void timerCallback() override;

    std::vector<Entry> entries;
    juce::CriticalSection entriesLock;

    static ToastOverlay* s_instance;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToastOverlay)
};

//==============================================================================
namespace pf
{
    juce::File getLogFile();
    void writeLog (ToastOverlay::Severity sev, const juce::String& message);

    inline void toastInfo  (const juce::String& m) {
        writeLog (ToastOverlay::Info, m);
        if (auto* o = ToastOverlay::getInstance()) o->post (ToastOverlay::Info, m);
    }
    inline void toastWarn  (const juce::String& m) {
        writeLog (ToastOverlay::Warn, m);
        if (auto* o = ToastOverlay::getInstance()) o->post (ToastOverlay::Warn, m);
    }
    inline void toastError (const juce::String& m) {
        writeLog (ToastOverlay::Error, m);
        if (auto* o = ToastOverlay::getInstance()) o->post (ToastOverlay::Error, m);
    }
}
