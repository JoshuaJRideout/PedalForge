#pragma once

#include "DSPNode.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <algorithm>
#include <iostream>

class GridSequencerNode : public DSPNode
{
public:
    GridSequencerNode() : DSPNode ("grid_sequencer", "Grid Sequencer")
    {
        addParam ("bpm", "BPM", 20.0f, 300.0f, 120.0f);
        addParam ("run", "Run/Stop", 0.0f, 1.0f, 0.0f);

        // Trigger parameters for chassis footswitches
        addParam ("tap", "Tap Tempo", 0.0f, 1.0f, 0.0f);
        addParam ("clear", "Clear Track", 0.0f, 1.0f, 0.0f);
        addParam ("track_advance", "Next Track", 0.0f, 1.0f, 0.0f);

        // Which track is currently selected (read by UI, advanced by track_advance)
        addParam ("sel_track", "Selected Track", 0.0f, 7.0f, 0.0f);
        
        // 8 tracks, each has 32 steps (boolean)
        for (int i = 0; i < 8; ++i)
        {
            juce::String tr = "tr" + juce::String(i);
            addParam (tr + "_div", "Div " + juce::String(i), 0.0f, 7.0f, 2.0f); // 0=1/1, 1=1/2, 2=1/4, 3=1/8, 4=1/16, etc.
            addParam (tr + "_swing", "Swing " + juce::String(i), 0.0f, 1.0f, 0.0f);
            addParam (tr + "_glitch", "Glitch " + juce::String(i), 0.0f, 1.0f, 0.0f);
            addParam (tr + "_len", "Len " + juce::String(i), 1.0f, 32.0f, 16.0f);
            
            // Output types: 0=Note, 1=CC, 2=PC, 3=ExprGate
            addParam (tr + "_mode", "Mode " + juce::String(i), 0.0f, 3.0f, 0.0f);
            addParam (tr + "_val1", "Val1 " + juce::String(i), 0.0f, 127.0f, 60.0f); // Note num or CC num
            addParam (tr + "_val2", "Val2 " + juce::String(i), 0.0f, 127.0f, 100.0f); // Velocity or CC val
            addParam (tr + "_chan", "Chan " + juce::String(i), 1.0f, 16.0f, 1.0f);
            
            for (int s = 0; s < 32; ++s)
            {
                addParam (tr + "_s" + juce::String(s), "Step " + juce::String(s), 0.0f, 1.0f, 0.0f, false);
            }
            
            addOutput ("gate_" + juce::String(i), NodePort::Gate);
        }
    }

    int getCurrentStep (int track) const { return (track >= 0 && track < 8) ? lastStepIndex[track] : -1; }
    
    void prepare (double sr, int maxBlockSize) override
    {
        DSPNode::prepare (sr, maxBlockSize);
        sampleRate = (float)sr;
        beatPosition = 0.0;
        totalSamplesProcessed = 0;
        lastTapSample = -1;
        lastTapState = false;
        lastClearState = false;
        lastTrackAdvState = false;
        selectedTrack = 0;
        
        for (int i = 0; i < 8; ++i)
        {
            lastStepIndex[i] = -1;
            lastNoteTriggered[i] = -1;
            gateActive[i] = false;
        }

        // Cache parameter pointers!
        bpmParam = getParam ("bpm");
        runParam = getParam ("run");
        tapParam = getParam ("tap");
        clearParam = getParam ("clear");
        trackAdvParam = getParam ("track_advance");
        selTrackParam = getParam ("sel_track");

        for (int t = 0; t < 8; ++t)
        {
            juce::String tr = "tr" + juce::String(t);
            cachedTrackParams[t].div  = getParam (tr + "_div");
            cachedTrackParams[t].len  = getParam (tr + "_len");
            cachedTrackParams[t].mode = getParam (tr + "_mode");
            cachedTrackParams[t].val1 = getParam (tr + "_val1");
            cachedTrackParams[t].val2 = getParam (tr + "_val2");
            cachedTrackParams[t].chan = getParam (tr + "_chan");
            
            for (int s = 0; s < 32; ++s)
            {
                cachedTrackParams[t].steps[s] = getParam (tr + "_s" + juce::String(s));
            }
        }
    }
    
    void process (const float** in, int numIn, float** out, int numOut, int numSamples) override
    {
        juce::ignoreUnused (in, numIn);
        
        // ── Handle trigger footswitches (rising-edge detection) ──────────
        handleTriggers();
        
        float bpm = bpmParam != nullptr ? bpmParam->get() : 120.0f;
        bool isRunning = runParam != nullptr ? runParam->get() > 0.5f : false;
        
        // DEBUG: Log run state changes
        if (isRunning != wasRunning)
        {
            std::cerr << "GridSequencer: run=" << (isRunning ? "RUNNING" : "STOPPED") << " bpm=" << bpm << " midiBuffer=" << (midiBuffer != nullptr ? "OK" : "NULL") << std::endl;
            wasRunning = isRunning;
        }
        
        if (!isRunning)
        {
            // Stop: send Note Offs for any active notes
            if (midiBuffer)
            {
                for (int t = 0; t < 8; ++t)
                {
                    if (lastNoteTriggered[t] >= 0)
                    {
                        int ch = cachedTrackParams[t].chan != nullptr ? (int)cachedTrackParams[t].chan->get() : 1;
                        midiBuffer->addEvent (juce::MidiMessage::noteOff (ch, lastNoteTriggered[t]), 0);
                        lastNoteTriggered[t] = -1;
                    }
                    gateActive[t] = false;
                }
            }
            beatPosition = 0.0;
            for (int t = 0; t < 8; ++t)
                lastStepIndex[t] = -1;
                
            // Fill CV gate outputs with 0.0f
            for (int i = 0; i < std::min(numOut, 8); ++i)
            {
                if (out[i] != nullptr)
                    std::fill (out[i], out[i] + numSamples, 0.0f);
            }
            totalSamplesProcessed += numSamples;
            return;
        }

        // Cache parameters once per block to avoid string lookups inside sample loop
        struct TrackParams
        {
            float div;
            float len;
            int mode;
            int val1;
            int val2;
            int chan;
            bool steps[32];
        };
        
        TrackParams cachedTracks[8];
        for (int t = 0; t < 8; ++t)
        {
            const auto& c = cachedTrackParams[t];
            cachedTracks[t].div  = c.div != nullptr ? c.div->get() : 2.0f;
            cachedTracks[t].len  = c.len != nullptr ? c.len->get() : 16.0f;
            cachedTracks[t].mode = c.mode != nullptr ? juce::jlimit (0, 3, (int)c.mode->get()) : 0;
            cachedTracks[t].val1 = c.val1 != nullptr ? juce::jlimit (0, 127, (int)c.val1->get()) : 60;
            cachedTracks[t].val2 = c.val2 != nullptr ? juce::jlimit (0, 127, (int)c.val2->get()) : 100;
            cachedTracks[t].chan = c.chan != nullptr ? juce::jlimit (1, 16, (int)c.chan->get()) : 1;
            
            for (int s = 0; s < 32; ++s)
            {
                cachedTracks[t].steps[s] = c.steps[s] != nullptr ? c.steps[s]->get() > 0.5f : false;
            }
        }

        double beatsPerSample = (bpm / 60.0) / sampleRate;

        for (int s = 0; s < numSamples; ++s)
        {
            beatPosition += beatsPerSample;

            for (int t = 0; t < 8; ++t)
            {
                const auto& tp = cachedTracks[t];
                double stepDuration = 4.0 / std::pow (2.0, juce::jlimit (0, 7, (int)tp.div));
                double rawStep = beatPosition / stepDuration;
                int currentStep = (int)rawStep % juce::jlimit (1, 32, (int)tp.len);

                if (currentStep != lastStepIndex[t])
                {
                    // New step!
                    lastStepIndex[t] = currentStep;

                    // Read step value (0 or 1)
                    bool stepActive = tp.steps[currentStep];

                    // Turn off any previous note on this track
                    if (midiBuffer && lastNoteTriggered[t] >= 0)
                    {
                        midiBuffer->addEvent (juce::MidiMessage::noteOff (tp.chan, lastNoteTriggered[t]), s);
                        lastNoteTriggered[t] = -1;
                    }

                    if (stepActive)
                    {
                        if (tp.mode == 0) // Note Mode
                        {
                            if (midiBuffer)
                            {
                                midiBuffer->addEvent (juce::MidiMessage::noteOn (tp.chan, tp.val1, (juce::uint8)tp.val2), s);
                                lastNoteTriggered[t] = tp.val1;
                                std::cerr << "GridSequencer: NOTE ON track=" << t << " note=" << tp.val1 << " vel=" << tp.val2 << " ch=" << tp.chan << " step=" << currentStep << std::endl;
                            }
                            else
                            {
                                std::cerr << "GridSequencer: midiBuffer is NULL! Cannot send MIDI." << std::endl;
                            }
                            gateActive[t] = false;
                        }
                        else if (tp.mode == 1) // CC Mode
                        {
                            if (midiBuffer)
                                midiBuffer->addEvent (juce::MidiMessage::controllerEvent (tp.chan, tp.val1, tp.val2), s);
                            gateActive[t] = false;
                        }
                        else if (tp.mode == 2) // PC Mode
                        {
                            if (midiBuffer)
                                midiBuffer->addEvent (juce::MidiMessage::programChange (tp.chan, tp.val1), s);
                            gateActive[t] = false;
                        }
                        else if (tp.mode == 3) // CV / Expression Gate Mode
                        {
                            gateActive[t] = true;
                        }
                    }
                    else
                    {
                        gateActive[t] = false;
                    }
                }

                // Output CV value (out[t][s])
                if (t < numOut && out[t] != nullptr)
                {
                    out[t][s] = gateActive[t] ? 1.0f : 0.0f;
                }
            }
        }
        totalSamplesProcessed += numSamples;
    }

private:
    struct TrackParamsCached
    {
        NodeParam* div = nullptr;
        NodeParam* len = nullptr;
        NodeParam* mode = nullptr;
        NodeParam* val1 = nullptr;
        NodeParam* val2 = nullptr;
        NodeParam* chan = nullptr;
        NodeParam* steps[32] = { nullptr };
    };

    TrackParamsCached cachedTrackParams[8];
    NodeParam* bpmParam = nullptr;
    NodeParam* runParam = nullptr;
    NodeParam* tapParam = nullptr;
    NodeParam* clearParam = nullptr;
    NodeParam* trackAdvParam = nullptr;
    NodeParam* selTrackParam = nullptr;

    float sampleRate = 44100.0f;
    double beatPosition = 0.0;
    int lastStepIndex[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    int lastNoteTriggered[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    bool gateActive[8] = { false, false, false, false, false, false, false, false };
    bool wasRunning = false;  // for debug logging

    // Trigger state for footswitches
    bool lastTapState = false;
    bool lastClearState = false;
    bool lastTrackAdvState = false;
    int64_t lastTapSample = -1;       // sample counter at last tap
    int64_t totalSamplesProcessed = 0;
    int selectedTrack = 0;

    /** Detect state changes on tap/clear/track_advance and execute their actions.
        Footswitches are latching toggles (0→1→0→1), so we fire on ANY transition. */
    void handleTriggers()
    {
        // ── Tap Tempo ────────────────────────────────────────────────────
        bool tapNow = tapParam != nullptr ? tapParam->get() > 0.5f : false;
        if (tapNow != lastTapState) // any state change = a tap
        {
            if (lastTapSample >= 0)
            {
                int64_t intervalSamples = totalSamplesProcessed - lastTapSample;
                if (intervalSamples > 0 && sampleRate > 0.0f)
                {
                    double intervalSec = (double)intervalSamples / (double)sampleRate;
                    // Clamp to reasonable range: 0.2s (300 BPM) to 3s (20 BPM)
                    if (intervalSec >= 0.2 && intervalSec <= 3.0)
                    {
                        float newBPM = (float)(60.0 / intervalSec);
                        if (bpmParam) bpmParam->set (newBPM);
                    }
                }
            }
            lastTapSample = totalSamplesProcessed;
        }
        lastTapState = tapNow;

        // ── Clear Track ──────────────────────────────────────────────────
        bool clearNow = clearParam != nullptr ? clearParam->get() > 0.5f : false;
        if (clearNow != lastClearState) // any state change = a clear
        {
            // Clear all 32 steps on the selected track
            for (int s = 0; s < 32; ++s)
            {
                auto* p = cachedTrackParams[selectedTrack].steps[s];
                if (p != nullptr)
                    p->set (0.0f);
            }
        }
        lastClearState = clearNow;

        // ── Track Advance ────────────────────────────────────────────────
        bool advNow = trackAdvParam != nullptr ? trackAdvParam->get() > 0.5f : false;
        if (advNow != lastTrackAdvState) // any state change = advance
        {
            selectedTrack = (selectedTrack + 1) % 8;
            if (selTrackParam) selTrackParam->set ((float)selectedTrack);
        }
        lastTrackAdvState = advNow;
    }
};
