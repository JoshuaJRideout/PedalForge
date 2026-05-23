#pragma once

#include "../dsp/PedalDesign.h"
#include "../dsp/DSPGraph.h"
#include <memory>

namespace FactoryDesigns
{
    inline void addBypassAndLED (PedalDesign& d)
    {
        PedalDesign::Control sw;
        sw.type = "footswitch";
        sw.width = 40; sw.height = 40;
        sw.x = d.chassisW / 2.0f - 20.0f;
        sw.y = d.chassisH - 50.0f;
        sw.controlID = "bypass_switch";
        d.controls.push_back(sw);

        PedalDesign::Control led;
        led.type = "led";
        led.width = 12; led.height = 12;
        led.x = d.chassisW / 2.0f - 6.0f;
        led.y = d.chassisH - 85.0f;
        led.controlID = "bypass_led";
        led.customColour = juce::Colours::red;
        d.controls.push_back(led);
    }

    /** Adds MIDI In, two Expression Ins, and MIDI Out ports to any pedal design.
     *  This makes every factory pedal behave like real hardware — it can receive
     *  MIDI CC/PC/Note and expression pedal signals from the Routing Tab, and
     *  can also send MIDI out (e.g. to hardware synths or other pedals). */
    inline void addStandardPorts (PedalDesign& d)
    {
        using K = PedalDesign::RoutingPort::Kind;
        d.routingPorts.push_back ({ K::MidiIn,       "midi_in",    "MIDI In" });
        d.routingPorts.push_back ({ K::ExpressionIn,  "expr_in_1",  "Expression In 1" });
        d.routingPorts.push_back ({ K::ExpressionIn,  "expr_in_2",  "Expression In 2" });
        d.routingPorts.push_back ({ K::MidiOut,       "midi_out",   "MIDI Out" });
    }

    inline std::shared_ptr<PedalDesign> createStepSequencer()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Step Sequencer";
        d->category = "MIDI & CV";
        d->tags = { "midi", "rhythm", "sequencer", "advanced" };
        d->chassisW = 280.0f;
        d->chassisH = 340.0f;
        d->chassisColour = juce::Colour(0xFF1A0533); // Deep Indigo
        
        // Custom Grid Sequencer DSP Node (Type: grid_sequencer)
        
        // We'll map DSP controls directly to the chassis
        PedalDesign::Control screen;
        screen.type = "text_screen";
        screen.width = 250; screen.height = 60;
        screen.x = 15.0f; screen.y = 15.0f;
        screen.controlID = "screen_1";
        d->controls.push_back(screen);

        float knobY1 = 100.0f;
        float knobY2 = 170.0f;
        
        PedalDesign::Control kSwing;
        kSwing.type = "knob"; kSwing.width = 40; kSwing.height = 40;
        kSwing.x = 30.0f; kSwing.y = knobY1;
        kSwing.label = "Swing"; kSwing.controlID = "k_swing";
        d->controls.push_back(kSwing);
        
        PedalDesign::Control kDiv;
        kDiv.type = "knob"; kDiv.width = 40; kDiv.height = 40;
        kDiv.x = 120.0f; kDiv.y = knobY1;
        kDiv.label = "Div"; kDiv.controlID = "k_div";
        d->controls.push_back(kDiv);
        
        PedalDesign::Control kGlitch;
        kGlitch.type = "knob"; kGlitch.width = 40; kGlitch.height = 40;
        kGlitch.x = 210.0f; kGlitch.y = knobY1;
        kGlitch.label = "Glitch"; kGlitch.controlID = "k_glitch";
        d->controls.push_back(kGlitch);
        
        PedalDesign::Control kVel;
        kVel.type = "knob"; kVel.width = 40; kVel.height = 40;
        kVel.x = 75.0f; kVel.y = knobY2;
        kVel.label = "Vel"; kVel.controlID = "k_vel";
        d->controls.push_back(kVel);

        PedalDesign::Control kSteps;
        kSteps.type = "knob"; kSteps.width = 40; kSteps.height = 40;
        kSteps.x = 165.0f; kSteps.y = knobY2;
        kSteps.label = "Steps"; kSteps.controlID = "k_steps";
        d->controls.push_back(kSteps);

        float swY = 240.0f;
        PedalDesign::Control swTap;
        swTap.type = "footswitch"; swTap.width = 40; swTap.height = 40;
        swTap.x = 15.0f; swTap.y = swY;
        swTap.label = "Tap"; swTap.controlID = "sw_tap";
        d->controls.push_back(swTap);

        PedalDesign::Control swPlay;
        swPlay.type = "footswitch"; swPlay.width = 40; swPlay.height = 40;
        swPlay.x = 80.0f; swPlay.y = swY;
        swPlay.label = "Play"; swPlay.controlID = "sw_play";
        d->controls.push_back(swPlay);

        PedalDesign::Control swClr;
        swClr.type = "footswitch"; swClr.width = 40; swClr.height = 40;
        swClr.x = 145.0f; swClr.y = swY;
        swClr.label = "Clr"; swClr.controlID = "sw_clr";
        d->controls.push_back(swClr);

        PedalDesign::Control swTrack;
        swTrack.type = "footswitch"; swTrack.width = 40; swTrack.height = 40;
        swTrack.x = 210.0f; swTrack.y = swY;
        swTrack.label = "Track"; swTrack.controlID = "sw_track";
        d->controls.push_back(swTrack);
        
        PedalDesign::Control btnOverlay;
        btnOverlay.type = "overlay_launcher"; btnOverlay.width = 160; btnOverlay.height = 25;
        btnOverlay.x = 60.0f; btnOverlay.y = 300.0f;
        btnOverlay.label = "OPEN GRID"; btnOverlay.controlID = "btn_grid";
        btnOverlay.overlayPage = "grid_editor";
        d->controls.push_back(btnOverlay);
        
        // Define Grid Editor Overlay Page
        PedalDesign::CanvasPage gridPage;
        gridPage.pageName = "grid_editor";
        gridPage.width = 1000.0f;
        gridPage.height = 600.0f;
        gridPage.backgroundColour = juce::Colour(0xFF110B1C); // Very dark indigo
        
        // Add 8 tracks * 32 steps grid
        float startX = 120.0f;
        float startY = 80.0f;
        float stepSpacingX = 25.0f;
        float stepSpacingY = 50.0f;
        
        for (int tr = 0; tr < 8; ++tr)
        {
            // Track label
            PedalDesign::Control lbl;
            lbl.type = "label";
            lbl.x = 20.0f; lbl.y = startY + tr * stepSpacingY;
            lbl.width = 80.0f; lbl.height = 20.0f;
            lbl.label = "Track " + juce::String(tr + 1);
            lbl.controlID = "lbl_tr" + juce::String(tr);
            gridPage.controls.push_back(lbl);
            
            for (int s = 0; s < 32; ++s)
            {
                PedalDesign::Control stepBtn;
                stepBtn.type = "led_toggle";
                stepBtn.x = startX + s * stepSpacingX;
                stepBtn.y = startY + tr * stepSpacingY;
                stepBtn.width = 16.0f; stepBtn.height = 16.0f;
                stepBtn.controlID = "tr" + juce::String(tr) + "_s" + juce::String(s);
                
                // Color code by beats (4 steps = 1 beat)
                if ((s / 4) % 2 == 0)
                    stepBtn.customColour = juce::Colour(0xFF8B5CF6); // Purple
                else
                    stepBtn.customColour = juce::Colour(0xFF6366F1); // Indigo
                    
                gridPage.controls.push_back(stepBtn);
                
                // Add DSP Mapping
                d->mappings.push_back({ stepBtn.controlID, "2_" + stepBtn.controlID });
            }
        }
        
        d->canvasPages.push_back(gridPage);

        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createPluginHost()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "VST/AU Host";
        d->category = "Utility";
        d->chassisW = 200.0f;
        d->chassisH = 260.0f;
        d->chassisColour = juce::Colour(0xFF333333); // Dark Gray

        // DSP Graph
        DSPGraph graph;
        
        int inL = graph.addNode(createNodeByType("audio_input"));
        graph.getNode(inL)->getParam("channel")->set(1.0f);
        
        int inR = graph.addNode(createNodeByType("audio_input"));
        graph.getNode(inR)->getParam("channel")->set(2.0f);
        
        int host = graph.addNode(createNodeByType("plugin_host"));
        
        int outL = graph.addNode(createNodeByType("audio_output"));
        graph.getNode(outL)->getParam("channel")->set(1.0f);
        
        int outR = graph.addNode(createNodeByType("audio_output"));
        graph.getNode(outR)->getParam("channel")->set(2.0f);
        
        graph.connect(inL, 0, host, 0);
        graph.connect(inR, 0, host, 1);
        graph.connect(host, 0, outL, 0);
        graph.connect(host, 1, outR, 0);
        
        d->effectsGraph = graph.toJSON();

        // Canvas Page for Plugin Editor
        PedalDesign::CanvasPage editorPage;
        editorPage.pageName = "PluginEditorPage";
        editorPage.width = 800.0f;
        editorPage.height = 600.0f;
        editorPage.backgroundColour = juce::Colours::transparentBlack;
        
        PedalDesign::Control pluginView;
        pluginView.type = "plugin_editor";
        pluginView.controlID = "plugin_view_control";
        pluginView.x = 0;
        pluginView.y = 0;
        pluginView.width = 800;
        pluginView.height = 600;
        
        editorPage.controls.push_back(pluginView);
        d->canvasPages.push_back(editorPage);

        // Pedal Chassis Controls
        PedalDesign::Control loadBtn;
        loadBtn.type = "plugin_browser";
        loadBtn.width = 160; loadBtn.height = 40;
        loadBtn.x = 20.0f; loadBtn.y = 20.0f;
        loadBtn.label = "Load VST/AU";
        loadBtn.controlID = "file_btn";
        d->controls.push_back(loadBtn);
        
        d->mappings.push_back({"file_btn", juce::String(host) + "_filepath"});
        d->mappings.push_back({"plugin_view_control", juce::String(host) + "_editor"});

        PedalDesign::Control openBtn;
        openBtn.type = "overlay_launcher";
        openBtn.width = 160; openBtn.height = 40;
        openBtn.x = 20.0f; openBtn.y = 80.0f;
        openBtn.label = "Open Editor";
        openBtn.controlID = "open_btn";
        openBtn.overlayPage = "PluginEditorPage";
        d->controls.push_back(openBtn);

        addBypassAndLED(*d);
        addStandardPorts(*d);
        d->isFactory = true;

        return d;
    }

    inline std::shared_ptr<PedalDesign> createCleanBoost()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Clean Boost";
        d->category = "Drive";
        d->tags = { "boost", "clean", "gain", "simple", "beginner" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF4ADE80);
        
        addBypassAndLED(*d);

        PedalDesign::Control knob;
        knob.type = "knob";
        knob.width = 45; knob.height = 45;
        knob.x = d->chassisW / 2.0f - 22.5f;
        knob.y = 40.0f;
        knob.label = "Gain";
        knob.controlID = "knob_1";
        d->controls.push_back(knob);

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_gain"}); // 2 is GainNode
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createOverdrive()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Overdrive";
        d->category = "Drive";
        d->tags = { "overdrive", "tube", "warm", "crunch" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFFFBBF24);
        
        addBypassAndLED(*d);

        float y = 30.0f;
        float gap = 45.0f;
        for (int i=0; i<3; ++i)
        {
            PedalDesign::Control knob;
            knob.type = "knob";
            knob.width = 35; knob.height = 35;
            knob.x = d->chassisW / 2.0f - 17.5f;
            knob.y = y + i * gap;
            knob.controlID = "knob_" + juce::String(i+1);
            d->controls.push_back(knob);
        }
        d->controls[2].label = "Gain";
        d->controls[3].label = "Tone";
        d->controls[4].label = "Vol";

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_gain"}); // GainNode pre
        d->mappings.push_back({"knob_2", "4_treble"}); // ToneStackNode
        d->mappings.push_back({"knob_3", "5_gain"}); // GainNode post
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createDistortion()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Distortion";
        d->category = "Drive";
        d->chassisW = 140.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFFF97316);
        
        addBypassAndLED(*d);

        float y = 40.0f;
        for (int i=0; i<3; ++i)
        {
            PedalDesign::Control knob;
            knob.type = "knob";
            knob.width = 35; knob.height = 35;
            knob.x = 20.0f + (i % 2) * 60.0f;
            knob.y = y + (i / 2) * 50.0f;
            if (i == 2) knob.x = d->chassisW / 2.0f - 17.5f; // Center third knob
            knob.controlID = "knob_" + juce::String(i+1);
            d->controls.push_back(knob);
        }
        d->controls[2].label = "Gain";
        d->controls[3].label = "Tone";
        d->controls[4].label = "Vol";

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_gain"}); // GainNode pre
        d->mappings.push_back({"knob_2", "4_treble"}); // ToneStackNode
        d->mappings.push_back({"knob_3", "5_gain"}); // GainNode post
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createFuzz()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Fuzz";
        d->category = "Drive";
        d->chassisW = 140.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFFDC2626);
        addBypassAndLED(*d);

        for (int i=0; i<3; ++i)
        {
            PedalDesign::Control knob;
            knob.type = "knob";
            knob.width = 40; knob.height = 40;
            knob.x = 20.0f + (i % 2) * 60.0f;
            knob.y = 30.0f + (i / 2) * 55.0f;
            if (i == 2) knob.x = d->chassisW / 2.0f - 20.0f;
            knob.controlID = "knob_" + juce::String(i+1);
            d->controls.push_back(knob);
        }
        d->controls[2].label = "Fuzz";
        d->controls[3].label = "Tone";
        d->controls[4].label = "Vol";

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "3_gain"}); // FuzzNode gain
        d->mappings.push_back({"knob_2", "4_treble"}); // ToneStackNode
        d->mappings.push_back({"knob_3", "5_gain"}); // GainNode post
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createChorus()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Chorus";
        d->category = "Modulation";
        d->chassisW = 140.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFFA78BFA);
        addBypassAndLED(*d);

        for (int i=0; i<4; ++i)
        {
            PedalDesign::Control knob;
            knob.type = "knob";
            knob.width = 30; knob.height = 30;
            knob.x = 25.0f + (i % 2) * 55.0f;
            knob.y = 30.0f + (i / 2) * 50.0f;
            knob.controlID = "knob_" + juce::String(i+1);
            d->controls.push_back(knob);
        }
        d->controls[2].label = "Rate";
        d->controls[3].label = "Depth";
        d->controls[4].label = "Time";
        d->controls[5].label = "Mix";

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "3_rate"}); // LFONode rate
        d->mappings.push_back({"knob_2", "4_depth"}); // ModDelay depth
        d->mappings.push_back({"knob_3", "4_time"}); // ModDelay time
        d->mappings.push_back({"knob_4", "5_mix"}); // MixNode mix
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createPhaser()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Phaser";
        d->category = "Modulation";
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFFD946EF);
        addBypassAndLED(*d);

        for (int i=0; i<3; ++i)
        {
            PedalDesign::Control knob;
            knob.type = "knob";
            knob.width = 35; knob.height = 35;
            knob.x = d->chassisW / 2.0f - 17.5f;
            knob.y = 30.0f + i * 45.0f;
            knob.controlID = "knob_" + juce::String(i+1);
            d->controls.push_back(knob);
        }
        d->controls[2].label = "Rate";
        d->controls[3].label = "Depth";
        d->controls[4].label = "Mix";

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "3_rate"}); // LFONode rate
        d->mappings.push_back({"knob_2", "4_depth"}); // PhaserNode depth
        d->mappings.push_back({"knob_3", "5_mix"}); // MixNode mix
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createFlanger()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Flanger";
        d->category = "Modulation";
        d->chassisW = 140.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFFEC4899);
        addBypassAndLED(*d);

        for (int i=0; i<4; ++i)
        {
            PedalDesign::Control knob;
            knob.type = "knob";
            knob.width = 30; knob.height = 30;
            knob.x = 25.0f + (i % 2) * 55.0f;
            knob.y = 30.0f + (i / 2) * 50.0f;
            knob.controlID = "knob_" + juce::String(i+1);
            d->controls.push_back(knob);
        }
        d->controls[2].label = "Rate";
        d->controls[3].label = "Depth";
        d->controls[4].label = "Fdbk";
        d->controls[5].label = "Mix";

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "3_rate"}); // LFONode rate
        d->mappings.push_back({"knob_2", "4_depth"}); // FlangerNode depth
        d->mappings.push_back({"knob_3", "4_feedback"}); // FlangerNode feedback
        d->mappings.push_back({"knob_4", "5_mix"}); // MixNode mix
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createTremolo()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Tremolo";
        d->category = "Modulation";
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFFFB7185);
        addBypassAndLED(*d);

        for (int i=0; i<2; ++i)
        {
            PedalDesign::Control knob;
            knob.type = "knob";
            knob.width = 40; knob.height = 40;
            knob.x = d->chassisW / 2.0f - 20.0f;
            knob.y = 40.0f + i * 55.0f;
            knob.controlID = "knob_" + juce::String(i+1);
            d->controls.push_back(knob);
        }
        d->controls[2].label = "Rate";
        d->controls[3].label = "Depth";

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_rate"}); // LFONode rate
        d->mappings.push_back({"knob_2", "2_depth"}); // LFONode depth
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createDelay()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Delay";
        d->category = "Time";
        d->tags = { "delay", "echo", "repeat" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF38BDF8);
        addBypassAndLED(*d);

        for (int i=0; i<3; ++i)
        {
            PedalDesign::Control knob;
            knob.type = "knob";
            knob.width = 35; knob.height = 35;
            knob.x = d->chassisW / 2.0f - 17.5f;
            knob.y = 30.0f + i * 45.0f;
            knob.controlID = "knob_" + juce::String(i+1);
            d->controls.push_back(knob);
        }
        d->controls[2].label = "Time";
        d->controls[3].label = "Fdbk";
        d->controls[4].label = "Mix";

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "3_time"}); // DelayNode time
        d->mappings.push_back({"knob_2", "3_feedback"}); // DelayNode feedback
        d->mappings.push_back({"knob_3", "4_mix"}); // MixNode mix
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createReverb()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Reverb";
        d->category = "Time";
        d->tags = { "reverb", "space", "ambient" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF22D3EE);
        addBypassAndLED(*d);

        for (int i=0; i<2; ++i)
        {
            PedalDesign::Control knob;
            knob.type = "knob";
            knob.width = 40; knob.height = 40;
            knob.x = d->chassisW / 2.0f - 20.0f;
            knob.y = 40.0f + i * 55.0f;
            knob.controlID = "knob_" + juce::String(i+1);
            d->controls.push_back(knob);
        }
        d->controls[2].label = "Size";
        d->controls[3].label = "Mix";

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_size"}); // Reverb size
        d->mappings.push_back({"knob_2", "2_mix"}); // Reverb mix
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createCompressor()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Compressor";
        d->category = "Dynamics";
        d->tags = { "compressor", "dynamics", "sustain" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF34D399);
        addBypassAndLED(*d);

        for (int i=0; i<2; ++i)
        {
            PedalDesign::Control knob;
            knob.type = "knob";
            knob.width = 40; knob.height = 40;
            knob.x = d->chassisW / 2.0f - 20.0f;
            knob.y = 40.0f + i * 55.0f;
            knob.controlID = "knob_" + juce::String(i+1);
            d->controls.push_back(knob);
        }
        d->controls[2].label = "Thresh";
        d->controls[3].label = "Ratio";

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_threshold"}); 
        d->mappings.push_back({"knob_2", "2_ratio"}); 
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createNoiseGate()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Noise Gate";
        d->category = "Dynamics";
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF9CA3AF);
        addBypassAndLED(*d);

        PedalDesign::Control knob;
        knob.type = "knob";
        knob.width = 45; knob.height = 45;
        knob.x = d->chassisW / 2.0f - 22.5f;
        knob.y = 50.0f;
        knob.label = "Thresh";
        knob.controlID = "knob_1";
        d->controls.push_back(knob);

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_threshold"}); 
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createParametricEQ()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Parametric EQ";
        d->category = "EQ";
        d->chassisW = 200.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF60A5FA);
        addBypassAndLED(*d);

        for (int band=0; band<3; ++band)
        {
            for (int p=0; p<3; ++p)
            {
                PedalDesign::Control knob;
                knob.type = "knob";
                knob.width = 30; knob.height = 30;
                knob.x = 25.0f + band * 60.0f;
                knob.y = 30.0f + p * 45.0f;
                knob.controlID = "knob_" + juce::String(band*3 + p + 1);
                d->controls.push_back(knob);

                if (p == 0) d->controls.back().label = "Freq";
                if (p == 1) d->controls.back().label = "Gain";
                if (p == 2) d->controls.back().label = "Q";

                juce::String nodeID = juce::String(2 + band); // eq1 is 2, eq2 is 3, eq3 is 4
                juce::String paramID = (p == 0) ? "freq" : (p == 1) ? "gain" : "q";
                d->mappings.push_back({knob.controlID, nodeID + "_" + paramID});
            }
        }

        d->mappings.push_back({"bypass_switch", "bypass"});
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createToneControl()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Tone Control";
        d->category = "EQ";
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF818CF8);
        addBypassAndLED(*d);

        for (int i=0; i<3; ++i)
        {
            PedalDesign::Control knob;
            knob.type = "knob";
            knob.width = 30; knob.height = 30;
            knob.x = d->chassisW / 2.0f - 15.0f;
            knob.y = 30.0f + i * 45.0f;
            knob.controlID = "knob_" + juce::String(i+1);
            d->controls.push_back(knob);
        }
        d->controls[2].label = "Bass";
        d->controls[3].label = "Mid";
        d->controls[4].label = "Treble";

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_bass"}); 
        d->mappings.push_back({"knob_2", "2_mid"}); 
        d->mappings.push_back({"knob_3", "2_treble"}); 
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createCabinetSim()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Cabinet Sim";
        d->category = "Utility";
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFFFCD34D);
        addBypassAndLED(*d);

        for (int i=0; i<2; ++i)
        {
            PedalDesign::Control knob;
            knob.type = "knob";
            knob.width = 40; knob.height = 40;
            knob.x = d->chassisW / 2.0f - 20.0f;
            knob.y = 40.0f + i * 55.0f;
            knob.controlID = "knob_" + juce::String(i+1);
            d->controls.push_back(knob);
        }
        d->controls[2].label = "Cutoff";
        d->controls[3].label = "Res";

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_cutoff"}); 
        d->mappings.push_back({"knob_2", "2_resonance"}); 
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createNAMAmp()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "NAM Amp";
        d->category = "Amp Sim";
        d->chassisW = 160.0f;
        d->chassisH = 220.0f;
        d->chassisColour = juce::Colour(0xFF333333); // Dark Grey
        
        addBypassAndLED(*d);

        PedalDesign::Control inKnob, outKnob;
        inKnob.type = "knob";
        inKnob.width = 40; inKnob.height = 40;
        inKnob.x = 20.0f; inKnob.y = 40.0f;
        inKnob.label = "Input";
        inKnob.controlID = "in_knob";
        
        outKnob.type = "knob";
        outKnob.width = 40; outKnob.height = 40;
        outKnob.x = 100.0f; outKnob.y = 40.0f;
        outKnob.label = "Output";
        outKnob.controlID = "out_knob";

        PedalDesign::Control fileBtn;
        fileBtn.type = "file_loader";
        fileBtn.width = 55; fileBtn.height = 20;
        fileBtn.x = 15.0f; fileBtn.y = 110.0f;
        fileBtn.label = "Browse...";
        fileBtn.controlID = "nam_loader";

        PedalDesign::Control libBtn;
        libBtn.type = "library_loader";
        libBtn.width = 55; libBtn.height = 20;
        libBtn.x = 85.0f; libBtn.y = 110.0f;
        libBtn.label = "Library";
        libBtn.controlID = "nam_library";
        libBtn.libraryCategory = "NAM";

        PedalDesign::Control display;
        display.type = "text_screen";
        display.label = "NAM Amp\nNo Model Loaded";
        display.numLines = 2;
        display.fontSize = 8.0f;
        display.controlID = "nam_display";
        display.x = 20.0f; display.y = 80.0f; display.width = 120.0f; display.height = 25.0f;

        d->controls.push_back(inKnob);
        d->controls.push_back(outKnob);
        d->controls.push_back(display);
        d->controls.push_back(fileBtn);
        d->controls.push_back(libBtn);

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"in_knob", "2_gain"}); // 2 is NAMNode
        d->mappings.push_back({"out_knob", "2_out_level"});
        d->mappings.push_back({"nam_display:1", "2_filepath"});
        d->mappings.push_back({"nam_loader", "2_filepath"});
        d->mappings.push_back({"nam_library", "2_filepath"});

        auto graph = std::make_unique<juce::DynamicObject>();
        juce::Array<juce::var> nodes;
        
        auto n0 = new juce::DynamicObject();
        n0->setProperty("id", 0);
        n0->setProperty("type", "audio_input");
        n0->setProperty("name", "Audio In");
        nodes.add(n0);

        auto n1 = new juce::DynamicObject();
        n1->setProperty("id", 1);
        n1->setProperty("type", "audio_output");
        n1->setProperty("name", "Audio Out");
        nodes.add(n1);

        auto n2 = new juce::DynamicObject();
        n2->setProperty("id", 2);
        n2->setProperty("type", "nam");
        n2->setProperty("name", "NAM Amp");
        
        juce::Array<juce::var> params;
        auto p1 = new juce::DynamicObject(); p1->setProperty("id", "gain"); p1->setProperty("value", 0.0);
        auto p2 = new juce::DynamicObject(); p2->setProperty("id", "out_level"); p2->setProperty("value", 0.0);
        params.add(p1); params.add(p2);
        n2->setProperty("params", params);
        nodes.add(n2);
        
        graph->setProperty("nodes", nodes);
        
        juce::Array<juce::var> conns;
        auto c1 = new juce::DynamicObject();
        c1->setProperty("srcNode", 0); c1->setProperty("srcPort", 0);
        c1->setProperty("dstNode", 2); c1->setProperty("dstPort", 0);
        conns.add(c1);
        
        auto c2 = new juce::DynamicObject();
        c2->setProperty("srcNode", 2); c2->setProperty("srcPort", 0);
        c2->setProperty("dstNode", 1); c2->setProperty("dstPort", 0);
        conns.add(c2);

        graph->setProperty("connections", conns);
        d->effectsGraph = juce::var(graph.release());

        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createIRLoader()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "IR Cabinet";
        d->category = "Amp/Cab";
        d->chassisColour = juce::Colour(0xff555555);
        d->chassisW = 140;
        d->chassisH = 220;

        PedalDesign::Control mix; mix.type = "knob"; mix.label = "Mix"; mix.controlID = "mix"; mix.x = 20; mix.y = 20;
        PedalDesign::Control gain; gain.type = "knob"; gain.label = "Gain"; gain.controlID = "gain"; gain.x = 80; gain.y = 20;

        PedalDesign::Control fileBtn;
        fileBtn.type = "file_loader";
        fileBtn.label = "Browse...";
        fileBtn.controlID = "ir_loader";
        fileBtn.x = 20; fileBtn.y = 120; fileBtn.width = 100; fileBtn.height = 30;

        PedalDesign::Control libBtn;
        libBtn.type = "library_loader";
        libBtn.label = "Library";
        libBtn.controlID = "ir_library";
        libBtn.libraryCategory = "IR_CAB";
        libBtn.x = 20; libBtn.y = 160; libBtn.width = 100; libBtn.height = 30;

        PedalDesign::Control display;
        display.type = "text_screen";
        display.label = "No Cab Loaded";
        display.controlID = "ir_display";
        display.x = 20; display.y = 80; display.width = 100; display.height = 30;

        d->controls.push_back(mix);
        d->controls.push_back(gain);
        d->controls.push_back(display);
        d->controls.push_back(fileBtn);
        d->controls.push_back(libBtn);

        d->mappings.push_back({"mix", "2_mix"});
        d->mappings.push_back({"gain", "2_gain"});
        d->mappings.push_back({"ir_display", "2_filepath"});
        d->mappings.push_back({"ir_loader", "2_filepath"});
        d->mappings.push_back({"ir_library", "2_filepath"});

        auto graph = std::make_unique<juce::DynamicObject>();
        juce::Array<juce::var> nodes;
        
        auto n0 = new juce::DynamicObject(); n0->setProperty("id", 0); n0->setProperty("type", "audio_input"); n0->setProperty("name", "Audio In"); nodes.add(n0);
        auto n1 = new juce::DynamicObject(); n1->setProperty("id", 1); n1->setProperty("type", "audio_output"); n1->setProperty("name", "Audio Out"); nodes.add(n1);
        
        auto n2 = new juce::DynamicObject(); n2->setProperty("id", 2); n2->setProperty("type", "ir"); n2->setProperty("name", "IR Loader");
        juce::Array<juce::var> params;
        auto p1 = new juce::DynamicObject(); p1->setProperty("id", "mix"); p1->setProperty("value", 1.0); params.add(p1);
        auto p2 = new juce::DynamicObject(); p2->setProperty("id", "gain"); p2->setProperty("value", 1.0); params.add(p2);
        n2->setProperty("params", params);
        nodes.add(n2);
        
        graph->setProperty("nodes", nodes);
        
        juce::Array<juce::var> conns;
        auto c1 = new juce::DynamicObject(); c1->setProperty("srcNode", 0); c1->setProperty("srcPort", 0); c1->setProperty("dstNode", 2); c1->setProperty("dstPort", 0); conns.add(c1); // Audio In -> IR In L
        auto c2 = new juce::DynamicObject(); c2->setProperty("srcNode", 0); c2->setProperty("srcPort", 0); c2->setProperty("dstNode", 2); c2->setProperty("dstPort", 1); conns.add(c2); // Audio In -> IR In R
        auto c3 = new juce::DynamicObject(); c3->setProperty("srcNode", 2); c3->setProperty("srcPort", 0); c3->setProperty("dstNode", 1); c3->setProperty("dstPort", 0); conns.add(c3); // IR Out L -> Audio Out
        graph->setProperty("connections", conns);
        d->effectsGraph = juce::var(graph.release());

        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createIRReverb()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "IR Reverb";
        d->category = "Reverb";
        d->chassisColour = juce::Colour(0xff4a5d85);
        d->chassisW = 140;
        d->chassisH = 220;

        PedalDesign::Control mix; mix.type = "knob"; mix.label = "Mix"; mix.controlID = "mix"; mix.x = 20; mix.y = 20;
        PedalDesign::Control gain; gain.type = "knob"; gain.label = "Gain"; gain.controlID = "gain"; gain.x = 80; gain.y = 20;

        PedalDesign::Control display;
        display.type = "text_screen";
        display.label = "No IR Loaded";
        display.controlID = "ir_display";
        display.x = 20; display.y = 80; display.width = 100; display.height = 30;

        PedalDesign::Control fileBtn;
        fileBtn.type = "file_loader";
        fileBtn.label = "Browse...";
        fileBtn.controlID = "ir_loader";
        fileBtn.x = 20; fileBtn.y = 120; fileBtn.width = 100; fileBtn.height = 30;

        PedalDesign::Control libBtn;
        libBtn.type = "library_loader";
        libBtn.label = "Library";
        libBtn.controlID = "ir_library";
        libBtn.libraryCategory = "IR_REV";
        libBtn.x = 20; libBtn.y = 160; libBtn.width = 100; libBtn.height = 30;

        d->controls.push_back(mix);
        d->controls.push_back(gain);
        d->controls.push_back(display);
        d->controls.push_back(fileBtn);
        d->controls.push_back(libBtn);

        d->mappings.push_back({"mix", "2_mix"});
        d->mappings.push_back({"gain", "2_gain"});
        d->mappings.push_back({"ir_display", "2_filepath"});
        d->mappings.push_back({"ir_loader", "2_filepath"});
        d->mappings.push_back({"ir_library", "2_filepath"});

        auto graph = std::make_unique<juce::DynamicObject>();
        juce::Array<juce::var> nodes;
        
        auto n0 = new juce::DynamicObject(); n0->setProperty("id", 0); n0->setProperty("type", "audio_input"); n0->setProperty("name", "Audio In"); nodes.add(n0);
        auto n1 = new juce::DynamicObject(); n1->setProperty("id", 1); n1->setProperty("type", "audio_output"); n1->setProperty("name", "Audio Out"); nodes.add(n1);
        
        auto n2 = new juce::DynamicObject(); n2->setProperty("id", 2); n2->setProperty("type", "ir"); n2->setProperty("name", "IR Loader");
        juce::Array<juce::var> params;
        auto p1 = new juce::DynamicObject(); p1->setProperty("id", "mix"); p1->setProperty("value", 0.5); params.add(p1);
        auto p2 = new juce::DynamicObject(); p2->setProperty("id", "gain"); p2->setProperty("value", 1.0); params.add(p2);
        n2->setProperty("params", params);
        nodes.add(n2);
        
        graph->setProperty("nodes", nodes);
        
        juce::Array<juce::var> conns;
        auto c1 = new juce::DynamicObject(); c1->setProperty("srcNode", 0); c1->setProperty("srcPort", 0); c1->setProperty("dstNode", 2); c1->setProperty("dstPort", 0); conns.add(c1); // Audio In -> IR In L
        auto c2 = new juce::DynamicObject(); c2->setProperty("srcNode", 0); c2->setProperty("srcPort", 0); c2->setProperty("dstNode", 2); c2->setProperty("dstPort", 1); conns.add(c2); // Audio In -> IR In R
        auto c3 = new juce::DynamicObject(); c3->setProperty("srcNode", 2); c3->setProperty("srcPort", 0); c3->setProperty("dstNode", 1); c3->setProperty("dstPort", 0); conns.add(c3); // IR Out L -> Audio Out
        graph->setProperty("connections", conns);
        d->effectsGraph = juce::var(graph.release());

        addStandardPorts (*d);
        return d;
    }

    // ─── TUTORIAL ────────────────────────────────────────────────────────────────

    /** Tutorial 1 — Hello Gain */
    inline std::shared_ptr<PedalDesign> createTutorialHelloGain()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Hello Gain";
        d->category = "Tutorial";
        d->tags = { "beginner", "gain", "basics", "first-pedal" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF6EE7B7); // mint green

        // Title label
        PedalDesign::Control title;
        title.type = "label";
        title.label = "HELLO GAIN";
        title.controlID = "lbl_title";
        title.x = 10.0f; title.y = 8.0f;
        title.width = 100.0f; title.height = 20.0f;
        d->controls.push_back(title);

        // Level knob
        PedalDesign::Control knob;
        knob.type = "knob";
        knob.width = 45; knob.height = 45;
        knob.x = d->chassisW / 2.0f - 22.5f;
        knob.y = 50.0f;
        knob.label = "Level";
        knob.controlID = "knob_1";
        d->controls.push_back(knob);

        addBypassAndLED(*d);

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_gain"}); // node 2 = GainNode
        addStandardPorts (*d);
        return d;
    }

    /** Tutorial 2 — Filter Sweep */
    inline std::shared_ptr<PedalDesign> createTutorialFilterSweep()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Filter Sweep";
        d->category = "Tutorial";
        d->tags = { "filter", "lowpass", "cutoff", "resonance", "beginner" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF93C5FD); // light blue

        // Title label
        PedalDesign::Control title;
        title.type = "label";
        title.label = "FILTER SWEEP";
        title.controlID = "lbl_title";
        title.x = 5.0f; title.y = 8.0f;
        title.width = 110.0f; title.height = 20.0f;
        d->controls.push_back(title);

        // Cutoff knob
        PedalDesign::Control k1;
        k1.type = "knob";
        k1.width = 40; k1.height = 40;
        k1.x = d->chassisW / 2.0f - 20.0f;
        k1.y = 40.0f;
        k1.label = "Cutoff";
        k1.controlID = "knob_1";
        d->controls.push_back(k1);

        // Resonance knob
        PedalDesign::Control k2;
        k2.type = "knob";
        k2.width = 40; k2.height = 40;
        k2.x = d->chassisW / 2.0f - 20.0f;
        k2.y = 95.0f;
        k2.label = "Reso";
        k2.controlID = "knob_2";
        d->controls.push_back(k2);

        addBypassAndLED(*d);

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_freq"});      // node 2 = LowPassNode
        d->mappings.push_back({"knob_2", "2_q"});
        addStandardPorts (*d);
        return d;
    }

    /** Tutorial 3 — Tremolo 101 */
    inline std::shared_ptr<PedalDesign> createTutorialTremolo101()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Tremolo 101";
        d->category = "Tutorial";
        d->tags = { "modulation", "lfo", "tremolo", "beginner" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFFFDA4AF); // salmon pink

        // Title label
        PedalDesign::Control title;
        title.type = "label";
        title.label = "TREMOLO 101";
        title.controlID = "lbl_title";
        title.x = 5.0f; title.y = 8.0f;
        title.width = 110.0f; title.height = 20.0f;
        d->controls.push_back(title);

        // Rate knob
        PedalDesign::Control k1;
        k1.type = "knob";
        k1.width = 40; k1.height = 40;
        k1.x = d->chassisW / 2.0f - 20.0f;
        k1.y = 40.0f;
        k1.label = "Rate";
        k1.controlID = "knob_1";
        d->controls.push_back(k1);

        // Depth knob
        PedalDesign::Control k2;
        k2.type = "knob";
        k2.width = 40; k2.height = 40;
        k2.x = d->chassisW / 2.0f - 20.0f;
        k2.y = 95.0f;
        k2.label = "Depth";
        k2.controlID = "knob_2";
        d->controls.push_back(k2);

        addBypassAndLED(*d);

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "1_rate"});  // node 1 = LFONode
        d->mappings.push_back({"knob_2", "1_depth"});
        addStandardPorts (*d);
        return d;
    }

    /** Tutorial 4 — Delay Lab */
    inline std::shared_ptr<PedalDesign> createTutorialDelayLab()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Delay Lab";
        d->category = "Tutorial";
        d->tags = { "delay", "parallel", "split", "mix", "signal-flow" };
        d->chassisW = 140.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF7DD3FC); // sky blue

        // Title label
        PedalDesign::Control title;
        title.type = "label";
        title.label = "DELAY LAB";
        title.controlID = "lbl_title";
        title.x = 20.0f; title.y = 8.0f;
        title.width = 100.0f; title.height = 20.0f;
        d->controls.push_back(title);

        // Time knob
        PedalDesign::Control k1;
        k1.type = "knob";
        k1.width = 35; k1.height = 35;
        k1.x = d->chassisW / 2.0f - 17.5f;
        k1.y = 30.0f;
        k1.label = "Time";
        k1.controlID = "knob_1";
        d->controls.push_back(k1);

        // Feedback knob
        PedalDesign::Control k2;
        k2.type = "knob";
        k2.width = 35; k2.height = 35;
        k2.x = d->chassisW / 2.0f - 17.5f;
        k2.y = 75.0f;
        k2.label = "Fdbk";
        k2.controlID = "knob_2";
        d->controls.push_back(k2);

        // Mix knob
        PedalDesign::Control k3;
        k3.type = "knob";
        k3.width = 35; k3.height = 35;
        k3.x = d->chassisW / 2.0f - 17.5f;
        k3.y = 120.0f;
        k3.label = "Mix";
        k3.controlID = "knob_3";
        d->controls.push_back(k3);

        addBypassAndLED(*d);

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_time"});     // node 2 = DelayNode
        d->mappings.push_back({"knob_2", "2_feedback"});
        d->mappings.push_back({"knob_3", "3_mix"});      // node 3 = MixNode
        addStandardPorts (*d);
        return d;
    }

    /** Tutorial 5 — Mini Synth */
    inline std::shared_ptr<PedalDesign> createTutorialMiniSynth()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Mini Synth";
        d->category = "Tutorial";
        d->tags = { "synth", "midi", "oscillator", "adsr", "envelope" };
        d->chassisW = 160.0f;
        d->chassisH = 220.0f;
        d->chassisColour = juce::Colour(0xFFC084FC); // purple

        // Title label
        PedalDesign::Control title;
        title.type = "label";
        title.label = "MINI SYNTH";
        title.controlID = "lbl_title";
        title.x = 25.0f; title.y = 8.0f;
        title.width = 110.0f; title.height = 20.0f;
        d->controls.push_back(title);

        // Attack knob
        PedalDesign::Control kA;
        kA.type = "knob";
        kA.width = 35; kA.height = 35;
        kA.x = 20.0f; kA.y = 40.0f;
        kA.label = "Attack";
        kA.controlID = "knob_1";
        d->controls.push_back(kA);

        // Decay knob
        PedalDesign::Control kD;
        kD.type = "knob";
        kD.width = 35; kD.height = 35;
        kD.x = 100.0f; kD.y = 40.0f;
        kD.label = "Decay";
        kD.controlID = "knob_2";
        d->controls.push_back(kD);

        // Sustain knob
        PedalDesign::Control kS;
        kS.type = "knob";
        kS.width = 35; kS.height = 35;
        kS.x = 20.0f; kS.y = 100.0f;
        kS.label = "Sustain";
        kS.controlID = "knob_3";
        d->controls.push_back(kS);

        // Release knob
        PedalDesign::Control kR;
        kR.type = "knob";
        kR.width = 35; kR.height = 35;
        kR.x = 100.0f; kR.y = 100.0f;
        kR.label = "Release";
        kR.controlID = "knob_4";
        d->controls.push_back(kR);

        // No bypass switch for synth — it generates audio, doesn't process it

        d->mappings.push_back({"knob_1", "3_attack"});   // node 3 = ADSRNode
        d->mappings.push_back({"knob_2", "3_decay"});
        d->mappings.push_back({"knob_3", "3_sustain"});
        d->mappings.push_back({"knob_4", "3_release"});
        addStandardPorts (*d);
        return d;
    }

    /** Tutorial 6 — Envelope Filter */
    inline std::shared_ptr<PedalDesign> createTutorialEnvelopeFilter()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Envelope Filter";
        d->category = "Tutorial";
        d->tags = { "filter", "envelope", "modulation", "sensors", "auto-wah" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFFA7F3D0); // pastel mint/emerald

        // Title label
        PedalDesign::Control title;
        title.type = "label";
        title.label = "ENV FILTER";
        title.controlID = "lbl_title";
        title.x = 10.0f; title.y = 8.0f;
        title.width = 100.0f; title.height = 20.0f;
        d->controls.push_back(title);

        // Sens knob
        PedalDesign::Control k1;
        k1.type = "knob";
        k1.width = 35; k1.height = 35;
        k1.x = 20.0f; k1.y = 40.0f;
        k1.label = "Sens";
        k1.controlID = "knob_1";
        d->controls.push_back(k1);

        // Reso knob
        PedalDesign::Control k2;
        k2.type = "knob";
        k2.width = 35; k2.height = 35;
        k2.x = 80.0f; k2.y = 40.0f;
        k2.label = "Reso";
        k2.controlID = "knob_2";
        d->controls.push_back(k2);

        // Range knob (Range maps to the Ranger's out_max parameter)
        PedalDesign::Control k3;
        k3.type = "knob";
        k3.width = 35; k3.height = 35;
        k3.x = d->chassisW / 2.0f - 17.5f;
        k3.y = 95.0f;
        k3.label = "Range";
        k3.controlID = "knob_3";
        d->controls.push_back(k3);

        addBypassAndLED(*d);

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_sensitivity"}); // node 2 = EnvelopeFollower
        d->mappings.push_back({"knob_2", "4_q"});           // node 4 = LowPassNode
        d->mappings.push_back({"knob_3", "3_out_max"});     // node 3 = RangerNode
        addStandardPorts (*d);
        return d;
    }

    /** Tutorial 7 — Step Sequencer Filter */
    inline std::shared_ptr<PedalDesign> createTutorialStepSequencer()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Step Sequencer Filter";
        d->category = "Tutorial";
        d->tags = { "filter", "sequencer", "clock", "rhythm", "timing" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF93C5FD); // pastel blue

        // Title label
        PedalDesign::Control title;
        title.type = "label";
        title.label = "STEP FILTER";
        title.controlID = "lbl_title";
        title.x = 10.0f; title.y = 8.0f;
        title.width = 100.0f; title.height = 20.0f;
        d->controls.push_back(title);

        // Speed knob
        PedalDesign::Control k1;
        k1.type = "knob";
        k1.width = 35; k1.height = 35;
        k1.x = 20.0f; k1.y = 60.0f;
        k1.label = "Speed";
        k1.controlID = "knob_1";
        d->controls.push_back(k1);

        // Reso knob
        PedalDesign::Control k2;
        k2.type = "knob";
        k2.width = 35; k2.height = 35;
        k2.x = 80.0f; k2.y = 60.0f;
        k2.label = "Reso";
        k2.controlID = "knob_2";
        d->controls.push_back(k2);

        addBypassAndLED(*d);

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_bpm"}); // node 2 = ClockNode
        d->mappings.push_back({"knob_2", "5_q"});   // node 5 = LowPassNode
        addStandardPorts (*d);
        return d;
    }

    /** Tutorial 8 — Pattern Slicer */
    inline std::shared_ptr<PedalDesign> createTutorialPatternSlicer()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Pattern Slicer";
        d->category = "Tutorial";
        d->tags = { "logic", "gating", "switching", "fuzz", "comparator" };
        d->chassisW = 140.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFFFDE047); // pastel yellow

        // Title label
        PedalDesign::Control title;
        title.type = "label";
        title.label = "PATTERN SLICER";
        title.controlID = "lbl_title";
        title.x = 10.0f; title.y = 8.0f;
        title.width = 120.0f; title.height = 20.0f;
        d->controls.push_back(title);

        // Speed knob
        PedalDesign::Control k1;
        k1.type = "knob";
        k1.width = 35; k1.height = 35;
        k1.x = 20.0f; k1.y = 40.0f;
        k1.label = "Speed";
        k1.controlID = "knob_1";
        d->controls.push_back(k1);

        // Fuzz knob
        PedalDesign::Control k2;
        k2.type = "knob";
        k2.width = 35; k2.height = 35;
        k2.x = 85.0f; k2.y = 40.0f;
        k2.label = "Fuzz";
        k2.controlID = "knob_2";
        d->controls.push_back(k2);

        // Sens knob
        PedalDesign::Control k3;
        k3.type = "knob";
        k3.width = 35; k3.height = 35;
        k3.x = d->chassisW / 2.0f - 17.5f;
        k3.y = 95.0f;
        k3.label = "Sens";
        k3.controlID = "knob_3";
        d->controls.push_back(k3);

        addBypassAndLED(*d);

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "4_bpm"});          // node 4 = ClockNode
        d->mappings.push_back({"knob_2", "6_drive"});        // node 6 = SoftClipNode
        d->mappings.push_back({"knob_3", "2_sensitivity"});  // node 2 = EnvelopeFollower
        addStandardPorts (*d);
        return d;
    }
}
