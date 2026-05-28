#include "ToastOverlay.h"
#include "../util/AppPaths.h"

ToastOverlay* ToastOverlay::s_instance = nullptr;

//==============================================================================
namespace pf
{
    juce::File getLogFile()
    {
        return pf::paths::getLogsDir().getChildFile ("log.txt");
    }

    void writeLog (ToastOverlay::Severity sev, const juce::String& message)
    {
        const char* tag = "INFO ";
        if (sev == ToastOverlay::Warn)  tag = "WARN ";
        if (sev == ToastOverlay::Error) tag = "ERROR";
        auto line = "[" + juce::Time::getCurrentTime().toString (true, true, false)
                    + "] " + tag + " " + message + "\n";
        getLogFile().appendText (line);
    }
}

//==============================================================================
ToastOverlay::ToastOverlay()
{
    setInterceptsMouseClicks (false, false);
    setAlwaysOnTop (true);
    s_instance = this;
    startTimerHz (20);
}

ToastOverlay::~ToastOverlay()
{
    if (s_instance == this) s_instance = nullptr;
    stopTimer();
}

//==============================================================================
void ToastOverlay::post (Severity sev, const juce::String& message)
{
    if (! juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        juce::Component::SafePointer<ToastOverlay> safe (this);
        juce::MessageManager::callAsync ([safe, sev, message]
        {
            if (auto* o = safe.getComponent()) o->post (sev, message);
        });
        return;
    }

    Entry e;
    e.sev = sev;
    e.message = message;
    e.createdMs = juce::Time::currentTimeMillis();
    // Errors stick around longer than info; users need time to read them.
    const int lifetimeMs = (sev == Error) ? 8000 : (sev == Warn) ? 5500 : 3500;
    e.expiresMs = e.createdMs + lifetimeMs;
    {
        const juce::ScopedLock sl (entriesLock);
        entries.push_back (std::move (e));
        // Cap visible toasts; older drop off if many stack up.
        while (entries.size() > 6)
            entries.erase (entries.begin());
    }
    setInterceptsMouseClicks (true, false);
    repaint();
}

void ToastOverlay::timerCallback()
{
    const auto now = juce::Time::currentTimeMillis();
    bool changed = false;
    {
        const juce::ScopedLock sl (entriesLock);
        for (auto& e : entries)
        {
            const auto remaining = e.expiresMs - now;
            if (remaining < 400)
            {
                const float newOp = juce::jlimit (0.0f, 1.0f, (float) remaining / 400.0f);
                if (std::abs (newOp - e.opacity) > 0.01f) { e.opacity = newOp; changed = true; }
            }
        }
        const auto before = entries.size();
        entries.erase (
            std::remove_if (entries.begin(), entries.end(),
                            [now] (const Entry& e) { return e.expiresMs <= now; }),
            entries.end());
        if (entries.size() != before) changed = true;
        if (entries.empty()) setInterceptsMouseClicks (false, false);
    }
    if (changed) repaint();
}

//==============================================================================
void ToastOverlay::mouseDown (const juce::MouseEvent& e)
{
    // Click on any toast dismisses just that toast.
    const juce::ScopedLock sl (entriesLock);
    if (entries.empty()) return;

    // Walk the layout from the most-recent toast (bottom) up.
    const int pad = 12;
    const int gap = 8;
    const int toastW = juce::jmin (380, getWidth() - pad * 2);
    int y = getHeight() - pad;
    for (int i = (int) entries.size() - 1; i >= 0; --i)
    {
        const int toastH = juce::roundToInt (juce::Font (14.0f).getHeight() * 2.4f);
        const juce::Rectangle<int> bounds (getWidth() - pad - toastW, y - toastH, toastW, toastH);
        if (bounds.contains (e.getPosition()))
        {
            entries.erase (entries.begin() + i);
            if (entries.empty()) setInterceptsMouseClicks (false, false);
            repaint();
            return;
        }
        y -= toastH + gap;
    }
}

//==============================================================================
void ToastOverlay::paint (juce::Graphics& g)
{
    const juce::ScopedLock sl (entriesLock);
    if (entries.empty()) return;

    const int pad = 12;
    const int gap = 8;
    const int toastW = juce::jmin (380, getWidth() - pad * 2);

    juce::Font msgFont (14.0f);

    int y = getHeight() - pad;
    for (int i = (int) entries.size() - 1; i >= 0; --i)
    {
        const auto& e = entries[(size_t) i];
        const int toastH = juce::roundToInt (msgFont.getHeight() * 2.4f);
        y -= toastH;
        juce::Rectangle<int> bounds (getWidth() - pad - toastW, y, toastW, toastH);

        juce::Colour bg, accent, text;
        switch (e.sev)
        {
            case Info:  bg = juce::Colour (0xE6111827); accent = juce::Colour (0xFF60A5FA); text = juce::Colour (0xFFE5E7EB); break;
            case Warn:  bg = juce::Colour (0xE61F1404); accent = juce::Colour (0xFFFBBF24); text = juce::Colour (0xFFFEF3C7); break;
            case Error:
            default:    bg = juce::Colour (0xE61F0408); accent = juce::Colour (0xFFEF4444); text = juce::Colour (0xFFFECACA); break;
        }
        bg     = bg.withAlpha     (bg.getFloatAlpha()     * e.opacity);
        accent = accent.withAlpha (accent.getFloatAlpha() * e.opacity);
        text   = text.withAlpha   (text.getFloatAlpha()   * e.opacity);

        g.setColour (bg);
        g.fillRoundedRectangle (bounds.toFloat(), 6.0f);
        g.setColour (accent);
        g.fillRect (bounds.removeFromLeft (4));
        g.setColour (text);
        g.setFont (msgFont);
        g.drawFittedText (e.message, bounds.reduced (10, 0), juce::Justification::centredLeft, 2);

        y -= gap;
    }
}

void ToastOverlay::resized() {}
