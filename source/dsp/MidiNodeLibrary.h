#pragma once
#include "DSPNode.h"

//==============================================================================
// ─── ADDITIONAL MIDI NODES (Full Spec Completion) ────────────────────────────
//==============================================================================

/**
 * MIDI Program Change — Outputs program number when a Program Change message is received.
 */
class MidiProgramChangeNode : public DSPNode
{
public:
    MidiProgramChangeNode() : DSPNode ("midi_program", "MIDI Program Change")
    {
        addOutput ("program", NodePort::Midi);
        addOutput ("trigger", NodePort::Gate);
        addParam ("channel", "Channel", 0.0f, 16.0f, 0.0f);
    }

    void process (const float**, int, float** out, int, int n) override
    {
        int ch = (int) getParam("channel")->get();
        bool triggered = false;
        
        if (midiBuffer)
        {
            for (const auto metadata : *midiBuffer)
            {
                auto msg = metadata.getMessage();
                if (ch > 0 && msg.getChannel() != ch) continue;
                if (msg.isProgramChange())
                {
                    currentProgram = msg.getProgramChangeNumber() / 127.0f;
                    triggered = true;
                }
            }
        }
        for (int i = 0; i < n; ++i)
        {
            out[0][i] = currentProgram;
            out[1][i] = triggered ? 1.0f : 0.0f;
        }
    }
private:
    float currentProgram = 0.0f;
};

/**
 * MIDI Channel Pressure (Aftertouch) — Outputs channel pressure as 0-1 control.
 */
class MidiChannelPressureNode : public DSPNode
{
public:
    MidiChannelPressureNode() : DSPNode ("midi_pressure", "MIDI Channel Pressure")
    {
        addOutput ("pressure", NodePort::Midi);
        addParam ("channel", "Channel", 0.0f, 16.0f, 0.0f);
    }

    void process (const float**, int, float** out, int, int n) override
    {
        int ch = (int) getParam("channel")->get();
        if (midiBuffer)
        {
            for (const auto metadata : *midiBuffer)
            {
                auto msg = metadata.getMessage();
                if (ch > 0 && msg.getChannel() != ch) continue;
                if (msg.isChannelPressure())
                    currentPressure = msg.getChannelPressureValue() / 127.0f;
            }
        }
        for (int i = 0; i < n; ++i)
            out[0][i] = currentPressure;
    }
private:
    float currentPressure = 0.0f;
};

/**
 * MIDI Poly Pressure — Outputs per-note aftertouch for a specific note.
 */
class MidiPolyPressureNode : public DSPNode
{
public:
    MidiPolyPressureNode() : DSPNode ("midi_poly_pressure", "MIDI Poly Pressure")
    {
        addOutput ("pressure", NodePort::Midi);
        addParam ("note", "Note", 0.0f, 127.0f, 60.0f);
        addParam ("channel", "Channel", 0.0f, 16.0f, 0.0f);
    }

    void process (const float**, int, float** out, int, int n) override
    {
        int ch = (int) getParam("channel")->get();
        int targetNote = (int) getParam("note")->get();
        if (midiBuffer)
        {
            for (const auto metadata : *midiBuffer)
            {
                auto msg = metadata.getMessage();
                if (ch > 0 && msg.getChannel() != ch) continue;
                if (msg.isAftertouch() && msg.getNoteNumber() == targetNote)
                    currentPressure = msg.getAfterTouchValue() / 127.0f;
            }
        }
        for (int i = 0; i < n; ++i)
            out[0][i] = currentPressure;
    }
private:
    float currentPressure = 0.0f;
};

/**
 * MIDI CC 14-bit — Combines MSB/LSB CC pairs (0-31 + 32-63) for high-resolution control.
 */
class MidiCC14Node : public DSPNode
{
public:
    MidiCC14Node() : DSPNode ("midi_cc14", "MIDI CC 14-bit")
    {
        addOutput ("value", NodePort::Midi);
        addParam ("cc", "CC MSB (0-31)", 0.0f, 31.0f, 1.0f);
        addParam ("channel", "Channel", 0.0f, 16.0f, 0.0f);
    }

    void process (const float**, int, float** out, int, int n) override
    {
        int targetCC = (int) getParam("cc")->get();
        int lsbCC = targetCC + 32;
        int ch = (int) getParam("channel")->get();

        if (midiBuffer)
        {
            for (const auto metadata : *midiBuffer)
            {
                auto msg = metadata.getMessage();
                if (ch > 0 && msg.getChannel() != ch) continue;
                if (msg.isController())
                {
                    if (msg.getControllerNumber() == targetCC)
                        msbValue = msg.getControllerValue();
                    else if (msg.getControllerNumber() == lsbCC)
                        lsbValue = msg.getControllerValue();
                }
            }
        }
        float combined = ((msbValue << 7) | lsbValue) / 16383.0f;
        for (int i = 0; i < n; ++i)
            out[0][i] = combined;
    }
private:
    int msbValue = 0, lsbValue = 0;
};

/**
 * MIDI Song Position — Outputs current song position pointer (0-1 range over 16384 steps).
 */
class MidiSongPositionNode : public DSPNode
{
public:
    MidiSongPositionNode() : DSPNode ("midi_song_pos", "MIDI Song Position")
    {
        addOutput ("position", NodePort::Midi);
        addOutput ("trigger", NodePort::Gate);
    }

    void process (const float**, int, float** out, int, int n) override
    {
        bool triggered = false;
        if (midiBuffer)
        {
            for (const auto metadata : *midiBuffer)
            {
                auto msg = metadata.getMessage();
                if (msg.isSongPositionPointer())
                {
                    currentPos = msg.getSongPositionPointerMidiBeat() / 16383.0f;
                    triggered = true;
                }
            }
        }
        for (int i = 0; i < n; ++i)
        {
            out[0][i] = currentPos;
            out[1][i] = triggered ? 1.0f : 0.0f;
        }
    }
private:
    float currentPos = 0.0f;
};

/**
 * MIDI Transport — Outputs Start, Stop, Continue status as gate signals.
 */
class MidiTransportNode : public DSPNode
{
public:
    MidiTransportNode() : DSPNode ("midi_transport", "MIDI Transport")
    {
        addOutput ("playing", NodePort::Gate);
        addOutput ("start", NodePort::Gate);
        addOutput ("stop", NodePort::Gate);
    }

    void process (const float**, int, float** out, int, int n) override
    {
        bool startPulse = false, stopPulse = false;
        
        if (midiBuffer)
        {
            for (const auto metadata : *midiBuffer)
            {
                auto msg = metadata.getMessage();
                if (msg.isMidiStart() || msg.isMidiContinue())
                {
                    isPlaying = true;
                    startPulse = true;
                }
                else if (msg.isMidiStop())
                {
                    isPlaying = false;
                    stopPulse = true;
                }
            }
        }
        for (int i = 0; i < n; ++i)
        {
            out[0][i] = isPlaying ? 1.0f : 0.0f;
            out[1][i] = startPulse ? 1.0f : 0.0f;
            out[2][i] = stopPulse ? 1.0f : 0.0f;
        }
    }
private:
    bool isPlaying = false;
};

/**
 * MIDI Note Generator — Creates MIDI note-on/off messages from control signals.
 * Essentially a CV-to-MIDI converter.
 */
class MidiNoteGenNode : public DSPNode
{
public:
    MidiNoteGenNode() : DSPNode ("midi_note_gen", "MIDI Note Gen")
    {
        addInput ("note", NodePort::Control);
        addInput ("velocity", NodePort::Control);
        addInput ("gate", NodePort::Gate);
        addParam ("channel", "Channel", 1.0f, 16.0f, 1.0f);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        if (numIn < 3 || !midiBuffer) return;
        int ch = (int) getParam("channel")->get();
        
        for (int i = 0; i < n; ++i)
        {
            bool g = in[2][i] > 0.5f;
            if (g && !lastGate)
            {
                int note = juce::jlimit (0, 127, (int)(in[0][i] * 127.0f));
                float vel = juce::jlimit (0.0f, 1.0f, in[1][i]);
                midiBuffer->addEvent (juce::MidiMessage::noteOn (ch, note, vel), i);
                lastNote = note;
            }
            else if (!g && lastGate)
            {
                midiBuffer->addEvent (juce::MidiMessage::noteOff (ch, lastNote), i);
            }
            lastGate = g;
        }
    }
private:
    bool lastGate = false;
    int lastNote = 0;
};

/**
 * MIDI CC Generator — Creates MIDI CC messages from a control signal.
 */
class MidiCCGenNode : public DSPNode
{
public:
    MidiCCGenNode() : DSPNode ("midi_cc_gen", "MIDI CC Gen")
    {
        addInput ("value", NodePort::Control);
        addParam ("cc", "CC Number", 0.0f, 127.0f, 1.0f);
        addParam ("channel", "Channel", 1.0f, 16.0f, 1.0f);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        if (numIn < 1 || !midiBuffer) return;
        int targetCC = (int) getParam("cc")->get();
        int ch = (int) getParam("channel")->get();
        
        // Only send CC when value changes (sample at end of block to avoid flooding)
        float val = in[0][n - 1];
        int ccVal = juce::jlimit (0, 127, (int)(val * 127.0f));
        if (ccVal != lastCCValue)
        {
            midiBuffer->addEvent (juce::MidiMessage::controllerEvent (ch, targetCC, ccVal), n - 1);
            lastCCValue = ccVal;
        }
    }
private:
    int lastCCValue = -1;
};
