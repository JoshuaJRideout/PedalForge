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

/**
 * MIDI Program Change Generator — Sends a program change message when triggered.
 * Connect a pedal button (gate) to trigger, and a control signal to select the program.
 */
class MidiProgramGenNode : public DSPNode
{
public:
    MidiProgramGenNode() : DSPNode ("midi_program_gen", "Program Change Gen")
    {
        addInput ("program", NodePort::Control);  // 0-1 → program 0-127
        addInput ("trigger", NodePort::Gate);      // rising edge sends the message
        addParam ("channel", "Channel", 1.0f, 16.0f, 1.0f);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        if (numIn < 2 || !midiBuffer) return;
        int ch = (int) getParam("channel")->get();

        for (int i = 0; i < n; ++i)
        {
            bool trig = in[1][i] > 0.5f;
            if (trig && !lastTrig)
            {
                int prog = juce::jlimit (0, 127, (int)(in[0][i] * 127.0f));
                midiBuffer->addEvent (juce::MidiMessage::programChange (ch, prog), i);
            }
            lastTrig = trig;
        }
    }
private:
    bool lastTrig = false;
};

/**
 * MIDI Channel Pressure Generator — Sends channel aftertouch from a control signal.
 * Perfect for: Envelope Follower → Pressure Gen = guitar dynamics control aftertouch.
 */
class MidiPressureGenNode : public DSPNode
{
public:
    MidiPressureGenNode() : DSPNode ("midi_pressure_gen", "Pressure Gen")
    {
        addInput ("pressure", NodePort::Control);  // 0-1
        addParam ("channel", "Channel", 1.0f, 16.0f, 1.0f);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        if (numIn < 1 || !midiBuffer) return;
        int ch = (int) getParam("channel")->get();
        
        // Sample at end of block, only send on change
        int val = juce::jlimit (0, 127, (int)(in[0][n - 1] * 127.0f));
        if (val != lastValue)
        {
            midiBuffer->addEvent (juce::MidiMessage::channelPressureChange (ch, val), n - 1);
            lastValue = val;
        }
    }
private:
    int lastValue = -1;
};

/**
 * MIDI Poly Pressure Generator — Sends per-note aftertouch.
 * The note input selects which note, pressure input sets the value.
 */
class MidiPolyPressureGenNode : public DSPNode
{
public:
    MidiPolyPressureGenNode() : DSPNode ("midi_poly_pressure_gen", "Poly Pressure Gen")
    {
        addInput ("note", NodePort::Control);      // 0-1 → note 0-127
        addInput ("pressure", NodePort::Control);  // 0-1
        addParam ("channel", "Channel", 1.0f, 16.0f, 1.0f);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        if (numIn < 2 || !midiBuffer) return;
        int ch = (int) getParam("channel")->get();
        
        int note = juce::jlimit (0, 127, (int)(in[0][n - 1] * 127.0f));
        int val = juce::jlimit (0, 127, (int)(in[1][n - 1] * 127.0f));
        if (val != lastValue || note != lastNote)
        {
            midiBuffer->addEvent (juce::MidiMessage::aftertouchChange (ch, note, val), n - 1);
            lastValue = val;
            lastNote = note;
        }
    }
private:
    int lastValue = -1, lastNote = -1;
};

/**
 * MIDI Pitch Bend Generator — Sends pitch bend from a bipolar control signal (-1 to +1).
 */
class MidiPitchBendGenNode : public DSPNode
{
public:
    MidiPitchBendGenNode() : DSPNode ("midi_pitchbend_gen", "Pitch Bend Gen")
    {
        addInput ("bend", NodePort::Control);  // -1 to +1
        addParam ("channel", "Channel", 1.0f, 16.0f, 1.0f);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        if (numIn < 1 || !midiBuffer) return;
        int ch = (int) getParam("channel")->get();
        
        float bend = juce::jlimit (-1.0f, 1.0f, in[0][n - 1]);
        int val = (int)((bend + 1.0f) * 0.5f * 16383.0f);
        val = juce::jlimit (0, 16383, val);
        if (val != lastValue)
        {
            midiBuffer->addEvent (juce::MidiMessage::pitchWheel (ch, val), n - 1);
            lastValue = val;
        }
    }
private:
    int lastValue = 8192; // center
};

/**
 * MIDI Transport Generator — Sends Start, Stop, Continue messages from gate signals.
 * Connect a footswitch or button to control playback of external MIDI gear.
 */
class MidiTransportGenNode : public DSPNode
{
public:
    MidiTransportGenNode() : DSPNode ("midi_transport_gen", "Transport Gen")
    {
        addInput ("start", NodePort::Gate);
        addInput ("stop", NodePort::Gate);
        addInput ("continue", NodePort::Gate);
    }

    void process (const float** in, int numIn, float**, int, int n) override
    {
        if (!midiBuffer) return;
        
        for (int i = 0; i < n; ++i)
        {
            bool startTrig = (numIn > 0 && in[0]) && in[0][i] > 0.5f;
            bool stopTrig  = (numIn > 1 && in[1]) && in[1][i] > 0.5f;
            bool contTrig  = (numIn > 2 && in[2]) && in[2][i] > 0.5f;

            if (startTrig && !lastStart)
                midiBuffer->addEvent (juce::MidiMessage::midiStart(), i);
            if (stopTrig && !lastStop)
                midiBuffer->addEvent (juce::MidiMessage::midiStop(), i);
            if (contTrig && !lastCont)
                midiBuffer->addEvent (juce::MidiMessage::midiContinue(), i);

            lastStart = startTrig;
            lastStop = stopTrig;
            lastCont = contTrig;
        }
    }
private:
    bool lastStart = false, lastStop = false, lastCont = false;
};
