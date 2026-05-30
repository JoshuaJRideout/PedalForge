#include "MidiMonitorMode.h"

//==============================================================================
juce::Colour MidiMonitorMode::colourForMessage (const juce::MidiMessage& m)
{
    if (m.isNoteOnOrOff())              return juce::Colour (0xFF6EE7B7);   // green
    if (m.isController())               return juce::Colour (0xFF60A5FA);   // blue
    if (m.isProgramChange())            return juce::Colour (0xFFFBBF24);   // yellow
    if (m.isPitchWheel())               return juce::Colour (0xFFC084FC);   // purple
    if (m.isAftertouch() ||
        m.isChannelPressure())          return juce::Colour (0xFFF97316);   // orange
    if (m.isSysEx())                    return juce::Colour (0xFFFB7185);   // red
    if (m.isMidiClock() ||
        m.isMidiStart() ||
        m.isMidiContinue() ||
        m.isMidiStop())                 return juce::Colour (0xFF9CA3AF);   // grey
    return juce::Colours::white;
}

juce::String MidiMonitorMode::describeMessage (const juce::MidiMessage& m)
{
    const int ch = m.getChannel();
    if (m.isNoteOn())
        return "Note On  ch" + juce::String (ch) + "  " + juce::MidiMessage::getMidiNoteName (m.getNoteNumber(), true, true, 4)
               + "  vel " + juce::String (m.getVelocity());
    if (m.isNoteOff())
        return "Note Off ch" + juce::String (ch) + "  " + juce::MidiMessage::getMidiNoteName (m.getNoteNumber(), true, true, 4);
    if (m.isController())
        return "CC ch" + juce::String (ch) + "  #" + juce::String (m.getControllerNumber())
               + " = " + juce::String (m.getControllerValue());
    if (m.isProgramChange())
        return "PC ch" + juce::String (ch) + "  prog " + juce::String (m.getProgramChangeNumber());
    if (m.isPitchWheel())
        return "Pitch ch" + juce::String (ch) + "  " + juce::String (m.getPitchWheelValue());
    if (m.isAftertouch())
        return "Aftertouch ch" + juce::String (ch) + "  " + juce::MidiMessage::getMidiNoteName (m.getNoteNumber(), true, true, 4)
               + "  " + juce::String (m.getAfterTouchValue());
    if (m.isChannelPressure())
        return "ChanPress ch" + juce::String (ch) + "  " + juce::String (m.getChannelPressureValue());
    if (m.isSysEx())
        return "SysEx " + juce::String (m.getSysExDataSize()) + " bytes";
    if (m.isMidiClock())               return "Clock";
    if (m.isMidiStart())               return "Start";
    if (m.isMidiContinue())            return "Continue";
    if (m.isMidiStop())                return "Stop";
    if (m.isActiveSense())             return "Active Sense";
    return m.getDescription();
}

//==============================================================================
void MidiMonitorMode::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll (juce::Colours::black);

    if (context.engine == nullptr)
    {
        g.setColour (juce::Colours::darkgrey);
        g.setFont (juce::Font (14.0f));
        g.drawText ("No engine", bounds, juce::Justification::centred);
        return;
    }

    auto events = context.engine->getMidiMonitorEvents();

    // Header band
    constexpr int headerH = 28;
    auto header = bounds.removeFromTop (headerH);
    g.setColour (juce::Colour (0xFF1F2937));
    g.fillRect (header);
    g.setColour (juce::Colour (0xFFE5E7EB));
    g.setFont (juce::Font (16.0f, juce::Font::bold));
    g.drawText ("MIDI Monitor", header.reduced (8, 0), juce::Justification::centredLeft);
    g.setFont (juce::Font (12.0f));
    g.setColour (juce::Colour (0xFF9CA3AF));
    g.drawText (juce::String (events.size()) + " events",
                header.reduced (8, 0), juce::Justification::centredRight);

    if (events.isEmpty())
    {
        g.setColour (juce::Colour (0xFF6B7280));
        g.setFont (juce::Font (14.0f));
        g.drawText ("Waiting for MIDI...", bounds, juce::Justification::centred);
        return;
    }

    // Scrolling event list — newest first.
    constexpr int rowH = 22;
    constexpr int timeW = 70;
    const int maxRows = bounds.getHeight() / rowH;

    juce::Font timeFont (12.0f);
    juce::Font msgFont (13.0f, juce::Font::bold);
    juce::Font srcFont (11.0f);

    int row = 0;
    const juce::Time now = context.lastFrameTime;
    for (int i = events.size() - 1; i >= 0 && row < maxRows; --i, ++row)
    {
        const auto& ev = events.getReference (i);
        auto rowBounds = bounds.removeFromTop (rowH);
        if (row % 2 == 1)
        {
            g.setColour (juce::Colour (0xFF0F172A));
            g.fillRect (rowBounds);
        }

        const double secsAgo = (now - ev.time).inSeconds();
        g.setColour (juce::Colour (0xFF6B7280));
        g.setFont (timeFont);
        const juce::String tStr = secsAgo < 1.0
                                      ? juce::String (int (secsAgo * 1000)) + "ms"
                                      : juce::String (secsAgo, 1) + "s";
        g.drawText (tStr, rowBounds.removeFromLeft (timeW).reduced (6, 0),
                    juce::Justification::centredLeft);

        // optional source label on the right
        if (ev.source.isNotEmpty())
        {
            g.setColour (juce::Colour (0xFF4B5563));
            g.setFont (srcFont);
            const int srcW = juce::jmin (90, rowBounds.getWidth() / 3);
            g.drawText (ev.source, rowBounds.removeFromRight (srcW).reduced (6, 0),
                        juce::Justification::centredRight);
        }

        g.setColour (colourForMessage (ev.message));
        g.setFont (msgFont);
        g.drawText (describeMessage (ev.message), rowBounds.reduced (6, 0),
                    juce::Justification::centredLeft);
    }
}
