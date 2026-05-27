#include "DisplayManager.h"

DisplayManager::DisplayManager (AudioGraphEngine& engineRef)
    : engine (engineRef)
{
    startTimerHz (15);  // baseline render rate; modes can request higher in future
}

DisplayManager::~DisplayManager()
{
    stopTimer();
}

//==============================================================================
void DisplayManager::attachDisplay (std::unique_ptr<SecondaryDisplay> display)
{
    if (display != nullptr)
        displays.push_back (std::move (display));
}

SecondaryDisplay* DisplayManager::findDisplay (const juce::String& displayID)
{
    for (auto& d : displays)
        if (d->getDisplayID() == displayID)
            return d.get();
    return nullptr;
}

SecondaryDisplay* DisplayManager::getPrimaryDisplay()
{
    return displays.empty() ? nullptr : displays.front().get();
}

//==============================================================================
void DisplayManager::registerMode (std::unique_ptr<DisplayMode> mode)
{
    if (mode != nullptr)
        modes.push_back (std::move (mode));
}

void DisplayManager::setActiveMode (const juce::String& displayID, const juce::String& modeID)
{
    if (auto* prevMode = getActiveMode (displayID))
        prevMode->onDeactivate();

    activeModeByDisplay[displayID] = modeID;

    if (auto* newMode = getActiveMode (displayID))
    {
        DisplayContext ctx;
        ctx.engine = &engine;
        ctx.lastFrameTime = juce::Time::getCurrentTime();
        if (auto* d = findDisplay (displayID))
            ctx.deviceConnected = d->isConnected();
        newMode->onActivate (ctx);
    }
}

DisplayMode* DisplayManager::getActiveMode (const juce::String& displayID)
{
    auto it = activeModeByDisplay.find (displayID);
    if (it == activeModeByDisplay.end()) return nullptr;
    for (auto& m : modes)
        if (m->getModeID() == it->second)
            return m.get();
    return nullptr;
}

//==============================================================================
void DisplayManager::timerCallback()
{
    for (auto& display : displays)
    {
        auto it = activeModeByDisplay.find (display->getDisplayID());
        if (it == activeModeByDisplay.end()) continue;

        DisplayMode* mode = nullptr;
        for (auto& m : modes)
            if (m->getModeID() == it->second) { mode = m.get(); break; }

        if (mode != nullptr)
            renderForDisplay (*display, *mode);
    }
}

void DisplayManager::renderForDisplay (SecondaryDisplay& display, DisplayMode& mode)
{
    const auto bounds = display.getRenderBounds();
    if (bounds.isEmpty()) return;

    juce::Image frame (juce::Image::RGB, bounds.getWidth(), bounds.getHeight(), true);
    juce::Graphics g (frame);
    g.fillAll (juce::Colours::black);

    DisplayContext ctx;
    ctx.engine = &engine;
    ctx.lastFrameTime = juce::Time::getCurrentTime();
    ctx.deviceConnected = display.isConnected();

    mode.renderInto (g, bounds, ctx);

    display.pushFrame (frame);
}
