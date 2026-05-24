#pragma once

#include "DSPNode.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <iostream>

struct MidiEditorNote
{
    float startBeat = 0.0f;
    float duration = 1.0f;
    int noteNumber = 60;
    int velocity = 100;
    int channel = 1;
};

struct MidiEditorPreviewEvent
{
    int noteNumber = 60;
    bool isOn = true;
    int velocity = 100;
    int channel = 1;
};

class MidiEditorNode : public DSPNode
{
public:
    MidiEditorNode() : DSPNode ("midi_editor", "MIDI Editor")
    {
        addParam ("bpm", "BPM", 20.0f, 300.0f, 120.0f);
        addParam ("run", "Run/Stop", 0.0f, 1.0f, 0.0f);
        addParam ("clear", "Clear Sequence", 0.0f, 1.0f, 0.0f);
        addParam ("snap", "Quantize Snap", 0.0f, 5.0f, 2.0f); // 0=None, 1=1/4, 2=1/8, 3=1/16, 4=1/32, 5=1/64
        addParam ("chan", "Channel", 1.0f, 16.0f, 1.0f);

        // Preload a simple chord progression (C major arpeggio/chord) as a friendly default
        notes.push_back ({ 0.0f, 1.0f, 60, 100, 1 }); // C4
        notes.push_back ({ 1.0f, 1.0f, 64, 100, 1 }); // E4
        notes.push_back ({ 2.0f, 1.0f, 67, 100, 1 }); // G4
        notes.push_back ({ 3.0f, 1.0f, 72, 100, 1 }); // C5
    }

    void prepare (double sr, int maxBlockSize) override
    {
        DSPNode::prepare (sr, maxBlockSize);
        sampleRate = (float)sr;
        beatPosition = 0.0;
        lastBPM = 120.0f;
        
        bpmParam = getParam ("bpm");
        runParam = getParam ("run");
        clearParam = getParam ("clear");
        snapParam = getParam ("snap");
        chanParam = getParam ("chan");

        // Clear active note tracking states
        activeNotes.clear();
    }

    void process (const float** in, int numIn, float** out, int numOut, int numSamples) override
    {
        juce::ignoreUnused (in, numIn);
        
        // Pass audio through transparently
        for (int i = 0; i < std::min (numIn, numOut); ++i)
        {
            if (in[i] != nullptr && out[i] != nullptr && in[i] != out[i])
                std::copy (in[i], in[i] + numSamples, out[i]);
        }

        // Process manual note preview events
        {
            std::lock_guard<std::mutex> lock (noteMutex);
            if (midiBuffer && !previewEvents.empty())
            {
                for (const auto& ev : previewEvents)
                {
                    if (ev.isOn)
                        midiBuffer->addEvent (juce::MidiMessage::noteOn (ev.channel, ev.noteNumber, (juce::uint8)ev.velocity), 0);
                    else
                        midiBuffer->addEvent (juce::MidiMessage::noteOff (ev.channel, ev.noteNumber), 0);
                }
                previewEvents.clear();
            }
        }

        // Handle clear trigger
        if (clearParam && clearParam->get() > 0.5f)
        {
            std::lock_guard<std::mutex> lock (noteMutex);
            notes.clear();
            clearParam->set (0.0f);
        }

        float bpm = bpmParam ? bpmParam->get() : 120.0f;
        bool isRunning = runParam ? runParam->get() > 0.5f : false;

        if (!isRunning)
        {
            // Stop: Turn off any active notes
            if (midiBuffer && !activeNotes.empty())
            {
                for (const auto& note : activeNotes)
                {
                    midiBuffer->addEvent (juce::MidiMessage::noteOff (note.channel, note.noteNumber), 0);
                }
                activeNotes.clear();
            }
            beatPosition = 0.0;
            return;
        }

        double beatsPerSample = (bpm / 60.0) / sampleRate;
        double blockStartBeat = beatPosition;
        double blockEndBeat = beatPosition + (beatsPerSample * numSamples);

        std::lock_guard<std::mutex> lock (noteMutex);

        if (midiBuffer)
        {
            // 1. Process note-offs for notes that should end in this block
            for (auto it = activeNotes.begin(); it != activeNotes.end(); )
            {
                double noteEndBeat = it->startBeat + it->duration;
                if (noteEndBeat >= blockStartBeat && noteEndBeat < blockEndBeat)
                {
                    int offset = (int)((noteEndBeat - blockStartBeat) / beatsPerSample);
                    offset = juce::jlimit (0, numSamples - 1, offset);
                    midiBuffer->addEvent (juce::MidiMessage::noteOff (it->channel, it->noteNumber), offset);
                    it = activeNotes.erase (it);
                }
                else
                {
                    ++it;
                }
            }

            // 2. Process note-ons for notes that start in this block
            for (const auto& note : notes)
            {
                // Wrap loop at 4 beats for continuous playback of the 1-bar pattern
                double noteStart = note.startBeat;
                
                // Trigger note-on if it lands inside this sample block
                if (noteStart >= blockStartBeat && noteStart < blockEndBeat)
                {
                    int offset = (int)((noteStart - blockStartBeat) / beatsPerSample);
                    offset = juce::jlimit (0, numSamples - 1, offset);
                    
                    midiBuffer->addEvent (juce::MidiMessage::noteOn (note.channel, note.noteNumber, (juce::uint8)note.velocity), offset);
                    activeNotes.push_back (note);
                }
            }
        }

        // Advance visual playhead position
        beatPosition = blockEndBeat;
        
        // Loop boundary at 4 beats for standard 1-bar sequence repeating
        if (beatPosition >= 4.0)
        {
            beatPosition = std::fmod (beatPosition, 4.0);
            
            // Clean up any notes that were left active across the boundary
            if (midiBuffer)
            {
                for (const auto& note : activeNotes)
                {
                    midiBuffer->addEvent (juce::MidiMessage::noteOff (note.channel, note.noteNumber), 0);
                }
            }
            activeNotes.clear();
        }
    }

    // ── Serialization overrides for dynamic persistence ────────────────────
    juce::var toJSON() const override
    {
        auto obj = DSPNode::toJSON();
        if (auto* dict = obj.getDynamicObject())
        {
            std::lock_guard<std::mutex> lock (const_cast<std::mutex&> (noteMutex));
            juce::Array<juce::var> noteArr;
            for (const auto& note : notes)
            {
                auto* no = new juce::DynamicObject();
                no->setProperty ("start", (double)note.startBeat);
                no->setProperty ("duration", (double)note.duration);
                no->setProperty ("pitch", note.noteNumber);
                no->setProperty ("vel", note.velocity);
                no->setProperty ("chan", note.channel);
                noteArr.add (juce::var (no));
            }
            dict->setProperty ("note_events", noteArr);
        }
        return obj;
    }

    void fromJSON (const juce::var& json) override
    {
        DSPNode::fromJSON (json);
        if (auto* dict = json.getDynamicObject())
        {
            if (dict->hasProperty ("note_events"))
            {
                std::lock_guard<std::mutex> lock (noteMutex);
                notes.clear();
                if (auto* arr = dict->getProperty ("note_events").getArray())
                {
                    for (const auto& nv : *arr)
                    {
                        if (auto* no = nv.getDynamicObject())
                        {
                            MidiEditorNote note;
                            note.startBeat = (float)(double)no->getProperty ("start");
                            note.duration = (float)(double)no->getProperty ("duration");
                            note.noteNumber = (int)no->getProperty ("pitch");
                            note.velocity = (int)no->getProperty ("vel");
                            note.channel = (int)no->getProperty ("chan");
                            notes.push_back (note);
                        }
                    }
                }
            }
        }
    }

    // ── Node manipulation API ───────────────────────────────────────────────
    void addNote (const MidiEditorNote& note)
    {
        std::lock_guard<std::mutex> lock (noteMutex);
        notes.push_back (note);
    }

    void removeNote (int idx)
    {
        std::lock_guard<std::mutex> lock (noteMutex);
        if (idx >= 0 && idx < (int)notes.size())
            notes.erase (notes.begin() + idx);
    }

    const std::vector<MidiEditorNote>& getNotes() const { return notes; }
    std::vector<MidiEditorNote>& getNotes() { return notes; }
    std::mutex& getMutex() { return noteMutex; }

    float getPlayheadBeat() const { return (float)beatPosition; }

    void triggerNotePreview (int noteNum, bool isOn, int vel = 100, int chan = 1)
    {
        std::lock_guard<std::mutex> lock (noteMutex);
        previewEvents.push_back ({ noteNum, isOn, vel, chan });
    }

private:
    std::vector<MidiEditorNote> notes;
    std::vector<MidiEditorNote> activeNotes; // active playing notes tracking
    std::vector<MidiEditorPreviewEvent> previewEvents;
    std::mutex noteMutex;

    NodeParam* bpmParam = nullptr;
    NodeParam* runParam = nullptr;
    NodeParam* clearParam = nullptr;
    NodeParam* snapParam = nullptr;
    NodeParam* chanParam = nullptr;

    float sampleRate = 44100.0f;
    double beatPosition = 0.0;
    float lastBPM = 120.0f;
};
