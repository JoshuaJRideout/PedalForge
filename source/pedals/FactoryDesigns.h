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

    /** Spawn a Knob NODE bound to a target node's parameter, and place its twin
        control on the faceplate. This is the honest "control = node" pattern:
        the knob's output is WIRED into the target param's CV input — exactly what
        a user does by hand in the FX tab. The knob's range + default are taken
        from the target param (set the target param to the intended default
        BEFORE calling this), so the knob sweeps the param's natural range and
        starts where the pedal wants it. Returns the new knob node id.

        Requires the target node to already be in the graph (addNode auto-exposes
        its "<param>_cv" input, which this wires into). */
    inline int addBoundKnob (DSPGraph& g, PedalDesign& d,
                             const juce::String& label, float faceX, float faceY,
                             int targetNodeID, const juce::String& paramId,
                             float faceW = 42.0f, float faceH = 42.0f)
    {
        auto* target = g.getNode (targetNodeID);
        if (target == nullptr) return -1;
        auto* tp = target->getParam (paramId);
        if (tp == nullptr) return -1;

        const float pmin = tp->minVal, pmax = tp->maxVal;
        const float def  = tp->get();   // current value = the intended default
        const float norm = (pmax > pmin) ? juce::jlimit (0.0f, 1.0f, (def - pmin) / (pmax - pmin)) : 0.0f;

        int knobID = g.addNode (createNodeByType ("ctrl_knob"));
        if (auto* k = g.getNode (knobID))
        {
            k->setName (label);
            if (auto* p = k->getParam ("min"))   p->set (pmin);
            if (auto* p = k->getParam ("max"))   p->set (pmax);
            if (auto* p = k->getParam ("value")) p->set (norm);
            k->visualX = target->visualX;
            k->visualY = target->visualY - 140.0f;
        }

        // Wire knob.out -> target's "<paramId>_cv" input (auto-exposed by addNode).
        int cvPort = -1;
        const auto& ins = target->getInputPorts();
        for (int i = 0; i < (int) ins.size(); ++i)
            if (ins[(size_t) i].name == paramId + "_cv") { cvPort = i; break; }
        if (cvPort >= 0)
            g.connect (knobID, 0, targetNodeID, cvPort);

        // Faceplate twin, bonded by the auto_node_<id> convention.
        PedalDesign::Control c;
        c.type = "knob"; c.width = faceW; c.height = faceH;
        c.x = faceX; c.y = faceY; c.label = label;
        c.controlID = "auto_node_" + juce::String (knobID);
        c.defaultValue = norm;
        d.controls.push_back (c);
        d.mappings.push_back ({ c.controlID, juce::String (knobID) + "_value" });
        return knobID;
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
        
        // DSP Graph
        DSPGraph graph;
        int inID  = graph.addNode (createNodeByType ("audio_input"));
        int seqID = graph.addNode (createNodeByType ("grid_sequencer"));
        int outID = graph.addNode (createNodeByType ("audio_output"));
        
        graph.getNode (inID)->visualX = 80.0f;
        graph.getNode (inID)->visualY = 200.0f;
        
        graph.getNode (seqID)->visualX = 290.0f;
        graph.getNode (seqID)->visualY = 200.0f;
        
        graph.getNode (outID)->visualX = 500.0f;
        graph.getNode (outID)->visualY = 200.0f;
        
        graph.connect (inID, 0, outID, 0);
        graph.connect (inID, 1, outID, 1);
        
        d->effectsGraph = graph.toJSON();
        
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
        
        // Add single unified dynamic display component that draws and manages the entire step grid
        PedalDesign::Control gridDisplay;
        gridDisplay.type = "dynamic_display";
        gridDisplay.controlID = "grid_display";
        gridDisplay.x = 10.0f;
        gridDisplay.y = 10.0f;
        gridDisplay.width = 980.0f;
        gridDisplay.height = 580.0f;
        gridPage.controls.push_back(gridDisplay);
        
        d->canvasPages.push_back(gridPage);

        // ── Chassis control mappings ─────────────────────────────────────
        // Knobs control Track 0 parameters on the grid_sequencer node (ID 2)
        d->mappings.push_back({"k_swing",  "2_tr0_swing"});   // Track 0 swing amount
        d->mappings.push_back({"k_div",    "2_tr0_div"});     // Track 0 clock division
        d->mappings.push_back({"k_glitch", "2_tr0_glitch"});  // Track 0 glitch probability
        d->mappings.push_back({"k_vel",    "2_tr0_val2"});    // Track 0 velocity (val2)
        d->mappings.push_back({"k_steps",  "2_tr0_len"});     // Track 0 sequence length

        // Footswitches
        d->mappings.push_back({"sw_play",  "2_run"});           // Run/Stop toggle
        d->mappings.push_back({"sw_tap",   "2_tap"});           // Tap tempo trigger
        d->mappings.push_back({"sw_clr",   "2_clear"});         // Clear selected track
        d->mappings.push_back({"sw_track", "2_track_advance"}); // Cycle to next track

        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createMidiEditor()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "MIDI Editor";
        d->category = "MIDI & CV";
        d->tags = { "midi", "daw", "sequencer", "piano-roll", "advanced" };
        d->chassisW = 280.0f;
        d->chassisH = 340.0f;
        d->chassisColour = juce::Colour(0xFF1E1B4B); // Deep Dark Indigo
        
        // DSP Graph
        DSPGraph graph;
        int inID  = graph.addNode (createNodeByType ("audio_input"));
        int seqID = graph.addNode (createNodeByType ("midi_editor"));
        int outID = graph.addNode (createNodeByType ("audio_output"));
        
        graph.getNode (inID)->visualX = 80.0f;
        graph.getNode (inID)->visualY = 200.0f;
        
        graph.getNode (seqID)->visualX = 290.0f;
        graph.getNode (seqID)->visualY = 200.0f;
        
        graph.getNode (outID)->visualX = 500.0f;
        graph.getNode (outID)->visualY = 200.0f;
        
        graph.connect (inID, 0, outID, 0);
        graph.connect (inID, 1, outID, 1);
        
        d->effectsGraph = graph.toJSON();
        
        // Map DSP controls directly to the chassis
        PedalDesign::Control screen;
        screen.type = "text_screen";
        screen.width = 250; screen.height = 60;
        screen.x = 15.0f; screen.y = 15.0f;
        screen.controlID = "screen_1";
        d->controls.push_back(screen);

        float knobY1 = 100.0f;
        
        PedalDesign::Control k1; k1.type = "knob"; k1.controlID = "k_bpm"; k1.label = "BPM"; k1.x = 25.0f; k1.y = knobY1; k1.width = 50; k1.height = 50; d->controls.push_back(k1);
        PedalDesign::Control k2; k2.type = "knob"; k2.controlID = "k_chan"; k2.label = "Channel"; k2.x = 115.0f; k2.y = knobY1; k2.width = 50; k2.height = 50; d->controls.push_back(k2);
        PedalDesign::Control k3; k3.type = "knob"; k3.controlID = "k_snap"; k3.label = "Snap"; k3.x = 205.0f; k3.y = knobY1; k3.width = 50; k3.height = 50; d->controls.push_back(k3);

        // Footswitches
        PedalDesign::Control swPlay; swPlay.type = "footswitch"; swPlay.controlID = "sw_play"; swPlay.label = "RUN"; swPlay.x = 35.0f; swPlay.y = 250.0f; swPlay.width = 60; swPlay.height = 60; d->controls.push_back(swPlay);
        PedalDesign::Control swClr; swClr.type = "footswitch"; swClr.controlID = "sw_clr"; swClr.label = "CLEAR"; swClr.x = 185.0f; swClr.y = 250.0f; swClr.width = 60; swClr.height = 60; d->controls.push_back(swClr);

        // Button to launch the visual arpeggiator/piano-roll grid editor
        PedalDesign::Control btnOverlay;
        btnOverlay.type = "overlay_launcher"; btnOverlay.width = 160; btnOverlay.height = 30;
        btnOverlay.x = 60.0f; btnOverlay.y = 180.0f;
        btnOverlay.label = "OPEN PIANO ROLL"; btnOverlay.controlID = "btn_grid";
        btnOverlay.overlayPage = "MIDI Editor Grid";
        d->controls.push_back(btnOverlay);

        // Grid Overlay Screen Configuration
        PedalDesign::CanvasPage gridPage;
        gridPage.pageName = "MIDI Editor Grid";
        gridPage.width = 1000.0f;
        gridPage.height = 600.0f;
        gridPage.backgroundColour = juce::Colour(0xFF110B1C); // Very dark indigo
        
        // Add single unified dynamic display component that draws and manages the entire midi editor grid
        PedalDesign::Control gridDisplay;
        gridDisplay.type = "custom_display"; // custom_display triggers our dynamic display as well!
        gridDisplay.controlID = "midi_editor_display";
        gridDisplay.x = 10.0f;
        gridDisplay.y = 10.0f;
        gridDisplay.width = 980.0f;
        gridDisplay.height = 580.0f;
        gridPage.controls.push_back(gridDisplay);
        
        d->canvasPages.push_back(gridPage);

        // ── Chassis control mappings ─────────────────────────────────────
        d->mappings.push_back({"k_bpm",  "2_bpm"});
        d->mappings.push_back({"k_chan", "2_chan"});
        d->mappings.push_back({"k_snap", "2_snap"});

        d->mappings.push_back({"sw_play",  "2_run"});
        d->mappings.push_back({"sw_clr",   "2_clear"});

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
        
        int inID  = graph.addNode (createNodeByType ("audio_input"));
        int host  = graph.addNode (createNodeByType ("plugin_host"));
        int outID = graph.addNode (createNodeByType ("audio_output"));
        
        graph.connect (inID, 0, host, 0);
        graph.connect (inID, 1, host, 1);
        graph.connect (host, 0, outID, 0);
        graph.connect (host, 1, outID, 1);
        
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

        // ── DSP graph (the SAME thing a user gets by placing a knob and wiring
        //    it in the FX tab — no hidden C++ factory). Input -> Gain -> Output,
        //    with a Gain knob NODE whose output drives the Gain effect's level.
        DSPGraph g;
        int inID    = g.addNode (createNodeByType ("audio_input"));
        int gainID  = g.addNode (createNodeByType ("gain"));
        int knobID  = g.addNode (createNodeByType ("ctrl_knob"));
        int outID   = g.addNode (createNodeByType ("audio_output"));

        // Gain knob spans 0..+24 dB, defaulting to a gentle +6 dB boost.
        if (auto* k = g.getNode (knobID))
        {
            k->setName ("Gain");
            if (auto* p = k->getParam ("min"))   p->set (0.0f);
            if (auto* p = k->getParam ("max"))   p->set (24.0f);
            if (auto* p = k->getParam ("value")) p->set (0.25f);   // 0.25 * 24 = +6 dB
        }

        g.getNode (inID)->visualX   = 60.0f;  g.getNode (inID)->visualY   = 200.0f;
        g.getNode (gainID)->visualX = 340.0f; g.getNode (gainID)->visualY = 200.0f;
        g.getNode (knobID)->visualX = 340.0f; g.getNode (knobID)->visualY = 60.0f;
        g.getNode (outID)->visualX  = 600.0f; g.getNode (outID)->visualY  = 200.0f;

        g.connect (inID, 0, gainID, 0);     // audio in -> gain
        g.connect (gainID, 0, outID, 0);    // gain -> out L
        g.connect (gainID, 0, outID, 1);    // gain -> out R
        g.connect (knobID, 0, gainID, 1);   // Gain knob -> gain's "gain" CV input

        d->effectsGraph = g.toJSON();

        // ── Faceplate Gain knob — the twin of the ctrl_knob node above. Bonded
        //    by the auto_node_<id> convention so they behave as one object.
        PedalDesign::Control knob;
        knob.type = "knob";
        knob.width = 50; knob.height = 50;
        knob.x = d->chassisW / 2.0f - 25.0f;
        knob.y = 50.0f;
        knob.label = "Gain";
        knob.controlID = "auto_node_" + juce::String (knobID);
        knob.defaultValue = 0.25f;
        d->controls.push_back(knob);

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({ knob.controlID, juce::String (knobID) + "_value" });

        // ── Teaching notes — this pedal documents itself on the FX canvas.
        {
            StickyNote n1;
            n1.text = "Clean Boost\nSignal path: Input -> Gain -> Output.";
            n1.bounds = { 40, 300, 250, 70 };
            d->fxNotes.push_back (n1);

            StickyNote n2;
            n2.text = "The 'Gain' knob is a Knob NODE. Its output wire feeds the "
                      "Gain effect's gain input, so the faceplate knob sets the "
                      "boost (0 to +24 dB). Delete the wire and the knob does "
                      "nothing - that's how every control works here.";
            n2.bounds = { 320, 300, 300, 130 };
            d->fxNotes.push_back (n2);
        }

        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createOverdrive()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Overdrive";
        d->category = "Drive";
        d->tags = { "overdrive", "tube", "warm", "crunch", "tutorial" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFFFBBF24);

        addBypassAndLED(*d);

        // Declared graph: In -> Gain -> SoftClip -> ToneStack -> Volume -> Out.
        DSPGraph g;
        int inID   = g.addNode (createNodeByType ("audio_input"));
        int gainID = g.addNode (createNodeByType ("gain"));
        int clipID = g.addNode (createNodeByType ("softclip"));
        int toneID = g.addNode (createNodeByType ("tonestack"));
        int volID  = g.addNode (createNodeByType ("gain"));
        int outID  = g.addNode (createNodeByType ("audio_output"));

        g.getNode (gainID)->setName ("Drive Gain");
        g.getNode (volID)->setName ("Volume");
        g.getNode (gainID)->getParam ("gain")->set (20.0f);
        g.getNode (volID)->getParam ("gain")->set (-6.0f);
        g.getNode (clipID)->getParam ("drive")->set (8.0f);   // fixed clip character

        float gx = 60.0f;
        for (int id : { inID, gainID, clipID, toneID, volID, outID })
        { g.getNode (id)->visualX = gx; g.getNode (id)->visualY = 240.0f; gx += 170.0f; }

        g.connect (inID, 0, gainID, 0);
        g.connect (gainID, 0, clipID, 0);
        g.connect (clipID, 0, toneID, 0);
        g.connect (toneID, 0, volID, 0);
        g.connect (volID, 0, outID, 0);
        g.connect (volID, 0, outID, 1);

        const float cx = d->chassisW / 2.0f - 17.5f;
        addBoundKnob (g, *d, "Gain", cx, 30.0f,  gainID, "gain",   35, 35);
        addBoundKnob (g, *d, "Tone", cx, 75.0f,  toneID, "treble", 35, 35);
        addBoundKnob (g, *d, "Vol",  cx, 120.0f, volID,  "gain",   35, 35);

        d->effectsGraph = g.toJSON();
        d->mappings.push_back ({ "bypass_switch", "bypass" });

        StickyNote n;
        n.text = "Overdrive: Gain -> Soft Clip -> Tone -> Volume.\nEach knob is a "
                 "Knob node whose output is wired into the effect it controls.";
        n.bounds = { 40, 380, 380, 90 };
        d->fxNotes.push_back (n);

        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createDistortion()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Distortion";
        d->category = "Drive";
        d->tags = { "distortion", "high-gain", "tutorial" };
        d->chassisW = 140.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFFF97316);

        addBypassAndLED(*d);

        // Declared graph: In -> Gain -> HardClip -> ToneStack -> Volume -> Out.
        DSPGraph g;
        int inID   = g.addNode (createNodeByType ("audio_input"));
        int gainID = g.addNode (createNodeByType ("gain"));
        int clipID = g.addNode (createNodeByType ("hardclip"));
        int toneID = g.addNode (createNodeByType ("tonestack"));
        int volID  = g.addNode (createNodeByType ("gain"));
        int outID  = g.addNode (createNodeByType ("audio_output"));

        g.getNode (gainID)->setName ("Drive Gain");
        g.getNode (volID)->setName ("Volume");
        g.getNode (gainID)->getParam ("gain")->set (30.0f);
        g.getNode (volID)->getParam ("gain")->set (-10.0f);
        g.getNode (clipID)->getParam ("drive")->set (12.0f);
        g.getNode (clipID)->getParam ("threshold")->set (0.4f);

        float gx = 60.0f;
        for (int id : { inID, gainID, clipID, toneID, volID, outID })
        { g.getNode (id)->visualX = gx; g.getNode (id)->visualY = 240.0f; gx += 170.0f; }

        g.connect (inID, 0, gainID, 0);
        g.connect (gainID, 0, clipID, 0);
        g.connect (clipID, 0, toneID, 0);
        g.connect (toneID, 0, volID, 0);
        g.connect (volID, 0, outID, 0);
        g.connect (volID, 0, outID, 1);

        const float cx = d->chassisW / 2.0f - 17.5f;
        addBoundKnob (g, *d, "Gain", 20.0f, 40.0f, gainID, "gain",   35, 35);
        addBoundKnob (g, *d, "Tone", 80.0f, 40.0f, toneID, "treble", 35, 35);
        addBoundKnob (g, *d, "Vol",  cx,    90.0f, volID,  "gain",   35, 35);

        d->effectsGraph = g.toJSON();
        d->mappings.push_back ({ "bypass_switch", "bypass" });

        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createFuzz()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Fuzz";
        d->category = "Drive";
        d->tags = { "fuzz", "vintage", "tutorial" };
        d->chassisW = 140.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFFDC2626);
        addBypassAndLED(*d);

        // Declared graph: In -> Pre-Gain -> Fuzz -> ToneStack -> Volume -> Out.
        DSPGraph g;
        int inID   = g.addNode (createNodeByType ("audio_input"));
        int gainID = g.addNode (createNodeByType ("gain"));
        int fuzzID = g.addNode (createNodeByType ("fuzz"));
        int toneID = g.addNode (createNodeByType ("tonestack"));
        int volID  = g.addNode (createNodeByType ("gain"));
        int outID  = g.addNode (createNodeByType ("audio_output"));

        g.getNode (gainID)->setName ("Pre-Gain");
        g.getNode (volID)->setName ("Volume");
        g.getNode (gainID)->getParam ("gain")->set (10.0f);
        g.getNode (volID)->getParam ("gain")->set (-12.0f);
        g.getNode (fuzzID)->getParam ("gain")->set (60.0f);
        g.getNode (fuzzID)->getParam ("bias")->set (0.15f);

        float gx = 60.0f;
        for (int id : { inID, gainID, fuzzID, toneID, volID, outID })
        { g.getNode (id)->visualX = gx; g.getNode (id)->visualY = 240.0f; gx += 170.0f; }

        g.connect (inID, 0, gainID, 0);
        g.connect (gainID, 0, fuzzID, 0);
        g.connect (fuzzID, 0, toneID, 0);
        g.connect (toneID, 0, volID, 0);
        g.connect (volID, 0, outID, 0);
        g.connect (volID, 0, outID, 1);

        const float cx = d->chassisW / 2.0f - 20.0f;
        addBoundKnob (g, *d, "Fuzz", 20.0f, 30.0f, fuzzID, "gain",   40, 40);
        addBoundKnob (g, *d, "Tone", 80.0f, 30.0f, toneID, "treble", 40, 40);
        addBoundKnob (g, *d, "Vol",  cx,    85.0f, volID,  "gain",   40, 40);

        d->effectsGraph = g.toJSON();
        d->mappings.push_back ({ "bypass_switch", "bypass" });

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

        // In -> Split; dry to Mix, wet through an LFO-modulated delay; Mix -> Out.
        DSPGraph g;
        int inID    = g.addNode (createNodeByType ("audio_input"));
        int splitID = g.addNode (createNodeByType ("split"));
        int lfoID   = g.addNode (createNodeByType ("lfo"));
        int modID   = g.addNode (createNodeByType ("mod_delay"));
        int mixID   = g.addNode (createNodeByType ("mix"));
        int outID   = g.addNode (createNodeByType ("audio_output"));

        g.getNode (lfoID)->getParam ("rate")->set (1.5f);
        g.getNode (modID)->getParam ("time")->set (0.007f);
        g.getNode (modID)->getParam ("depth")->set (0.003f);
        g.getNode (mixID)->getParam ("mix")->set (0.5f);

        float gx = 60.0f;
        for (int id : { inID, splitID, lfoID, modID, mixID, outID })
        { g.getNode (id)->visualX = gx; g.getNode (id)->visualY = 260.0f; gx += 170.0f; }

        g.connect (inID, 0, splitID, 0);
        g.connect (splitID, 0, mixID, 0);
        g.connect (splitID, 1, modID, 0);
        g.connect (lfoID, 0, modID, 1);
        g.connect (modID, 0, mixID, 1);
        g.connect (mixID, 0, outID, 0);
        g.connect (mixID, 0, outID, 1);

        addBoundKnob (g, *d, "Rate",  25.0f, 30.0f, lfoID, "rate",  30, 30);
        addBoundKnob (g, *d, "Depth", 80.0f, 30.0f, modID, "depth", 30, 30);
        addBoundKnob (g, *d, "Time",  25.0f, 80.0f, modID, "time",  30, 30);
        addBoundKnob (g, *d, "Mix",   80.0f, 80.0f, mixID, "mix",   30, 30);

        d->effectsGraph = g.toJSON();
        d->mappings.push_back ({ "bypass_switch", "bypass" });
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

        DSPGraph g;
        int inID    = g.addNode (createNodeByType ("audio_input"));
        int splitID = g.addNode (createNodeByType ("split"));
        int lfoID   = g.addNode (createNodeByType ("lfo"));
        int phsID   = g.addNode (createNodeByType ("phaser"));
        int mixID   = g.addNode (createNodeByType ("mix"));
        int outID   = g.addNode (createNodeByType ("audio_output"));

        g.getNode (lfoID)->getParam ("rate")->set (0.5f);
        g.getNode (phsID)->getParam ("depth")->set (0.8f);
        g.getNode (mixID)->getParam ("mix")->set (0.5f);

        float gx = 60.0f;
        for (int id : { inID, splitID, lfoID, phsID, mixID, outID })
        { g.getNode (id)->visualX = gx; g.getNode (id)->visualY = 260.0f; gx += 170.0f; }

        g.connect (inID, 0, splitID, 0);
        g.connect (splitID, 0, mixID, 0);
        g.connect (splitID, 1, phsID, 0);
        g.connect (lfoID, 0, phsID, 1);
        g.connect (phsID, 0, mixID, 1);
        g.connect (mixID, 0, outID, 0);
        g.connect (mixID, 0, outID, 1);

        const float cx = d->chassisW / 2.0f - 17.5f;
        addBoundKnob (g, *d, "Rate",  cx, 30.0f,  lfoID, "rate",  35, 35);
        addBoundKnob (g, *d, "Depth", cx, 75.0f,  phsID, "depth", 35, 35);
        addBoundKnob (g, *d, "Mix",   cx, 120.0f, mixID, "mix",   35, 35);

        d->effectsGraph = g.toJSON();
        d->mappings.push_back ({ "bypass_switch", "bypass" });
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

        DSPGraph g;
        int inID    = g.addNode (createNodeByType ("audio_input"));
        int splitID = g.addNode (createNodeByType ("split"));
        int lfoID   = g.addNode (createNodeByType ("lfo"));
        int flgID   = g.addNode (createNodeByType ("flanger"));
        int mixID   = g.addNode (createNodeByType ("mix"));
        int outID   = g.addNode (createNodeByType ("audio_output"));

        g.getNode (lfoID)->getParam ("rate")->set (0.2f);
        g.getNode (flgID)->getParam ("depth")->set (0.9f);
        g.getNode (flgID)->getParam ("feedback")->set (0.6f);
        g.getNode (mixID)->getParam ("mix")->set (0.5f);

        float gx = 60.0f;
        for (int id : { inID, splitID, lfoID, flgID, mixID, outID })
        { g.getNode (id)->visualX = gx; g.getNode (id)->visualY = 260.0f; gx += 170.0f; }

        g.connect (inID, 0, splitID, 0);
        g.connect (splitID, 0, mixID, 0);
        g.connect (splitID, 1, flgID, 0);
        g.connect (lfoID, 0, flgID, 1);
        g.connect (flgID, 0, mixID, 1);
        g.connect (mixID, 0, outID, 0);
        g.connect (mixID, 0, outID, 1);

        addBoundKnob (g, *d, "Rate",  25.0f, 30.0f, lfoID, "rate",     30, 30);
        addBoundKnob (g, *d, "Depth", 80.0f, 30.0f, flgID, "depth",    30, 30);
        addBoundKnob (g, *d, "Fdbk",  25.0f, 80.0f, flgID, "feedback", 30, 30);
        addBoundKnob (g, *d, "Mix",   80.0f, 80.0f, mixID, "mix",      30, 30);

        d->effectsGraph = g.toJSON();
        d->mappings.push_back ({ "bypass_switch", "bypass" });
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

        // Tremolo = audio * a unipolar 0..1 envelope. The LFO is bipolar, so we
        // map it: (LFO + 1) / 2. Add/Divide take their second operand from their
        // second INPUT, so we feed constants in (this is the legible way a user
        // would wire it, and it actually works — unlike a bare value param).
        DSPGraph g;
        int inID  = g.addNode (createNodeByType ("audio_input"));
        int lfoID = g.addNode (createNodeByType ("lfo"));
        int oneID = g.addNode (createNodeByType ("constant"));
        int addID = g.addNode (createNodeByType ("add"));
        int twoID = g.addNode (createNodeByType ("constant"));
        int divID = g.addNode (createNodeByType ("divide"));
        int mulID = g.addNode (createNodeByType ("multiply"));
        int outID = g.addNode (createNodeByType ("audio_output"));

        g.getNode (lfoID)->getParam ("rate")->set (5.0f);
        g.getNode (lfoID)->getParam ("depth")->set (0.8f);   // floor stays above silence
        g.getNode (oneID)->getParam ("value")->set (1.0f);
        g.getNode (twoID)->getParam ("value")->set (2.0f);

        float gx = 60.0f;
        for (int id : { inID, lfoID, oneID, addID, twoID, divID, mulID, outID })
        { g.getNode (id)->visualX = gx; g.getNode (id)->visualY = 260.0f; gx += 150.0f; }

        g.connect (lfoID, 0, addID, 0);   // a = LFO
        g.connect (oneID, 0, addID, 1);   // b = 1   -> LFO + 1
        g.connect (addID, 0, divID, 0);   // a = LFO+1
        g.connect (twoID, 0, divID, 1);   // b = 2   -> (LFO+1)/2  (0..1)
        g.connect (inID, 0, mulID, 0);    // a = audio
        g.connect (divID, 0, mulID, 1);   // b = envelope
        g.connect (mulID, 0, outID, 0);
        g.connect (mulID, 0, outID, 1);

        const float cx = d->chassisW / 2.0f - 20.0f;
        addBoundKnob (g, *d, "Rate",  cx, 40.0f, lfoID, "rate",  40, 40);
        addBoundKnob (g, *d, "Depth", cx, 95.0f, lfoID, "depth", 40, 40);

        d->effectsGraph = g.toJSON();
        d->mappings.push_back ({ "bypass_switch", "bypass" });
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createDelay()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Delay";
        d->category = "Time";
        d->tags = { "delay", "echo", "repeat", "tutorial" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF38BDF8);
        addBypassAndLED(*d);

        // In -> Split; dry to Mix, wet through a feedback Delay; Mix -> Out.
        DSPGraph g;
        int inID    = g.addNode (createNodeByType ("audio_input"));
        int splitID = g.addNode (createNodeByType ("split"));
        int delID   = g.addNode (createNodeByType ("delay"));
        int mixID   = g.addNode (createNodeByType ("mix"));
        int outID   = g.addNode (createNodeByType ("audio_output"));

        g.getNode (delID)->getParam ("time")->set (0.4f);
        g.getNode (delID)->getParam ("feedback")->set (0.4f);
        g.getNode (mixID)->getParam ("mix")->set (0.3f);

        float gx = 60.0f;
        for (int id : { inID, splitID, delID, mixID, outID })
        { g.getNode (id)->visualX = gx; g.getNode (id)->visualY = 240.0f; gx += 170.0f; }

        g.connect (inID, 0, splitID, 0);
        g.connect (splitID, 0, mixID, 0);
        g.connect (splitID, 1, delID, 0);
        g.connect (delID, 0, mixID, 1);
        g.connect (mixID, 0, outID, 0);
        g.connect (mixID, 0, outID, 1);

        const float cx = d->chassisW / 2.0f - 17.5f;
        addBoundKnob (g, *d, "Time", cx, 30.0f,  delID, "time",     35, 35);
        addBoundKnob (g, *d, "Fdbk", cx, 75.0f,  delID, "feedback", 35, 35);
        addBoundKnob (g, *d, "Mix",  cx, 120.0f, mixID, "mix",      35, 35);

        d->effectsGraph = g.toJSON();
        d->mappings.push_back ({ "bypass_switch", "bypass" });
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createReverb()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Reverb";
        d->category = "Time";
        d->tags = { "reverb", "space", "ambient", "tutorial" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF22D3EE);
        addBypassAndLED(*d);

        DSPGraph g;
        int inID  = g.addNode (createNodeByType ("audio_input"));
        int revID = g.addNode (createNodeByType ("reverb"));
        int outID = g.addNode (createNodeByType ("audio_output"));

        g.getNode (revID)->getParam ("size")->set (0.7f);
        g.getNode (revID)->getParam ("mix")->set (0.4f);

        g.getNode (inID)->visualX  = 60.0f;  g.getNode (inID)->visualY  = 240.0f;
        g.getNode (revID)->visualX = 340.0f; g.getNode (revID)->visualY = 240.0f;
        g.getNode (outID)->visualX = 620.0f; g.getNode (outID)->visualY = 240.0f;

        g.connect (inID, 0, revID, 0);
        g.connect (revID, 0, outID, 0);
        g.connect (revID, 0, outID, 1);

        const float cx = d->chassisW / 2.0f - 20.0f;
        addBoundKnob (g, *d, "Size", cx, 40.0f, revID, "size", 40, 40);
        addBoundKnob (g, *d, "Mix",  cx, 95.0f, revID, "mix",  40, 40);

        d->effectsGraph = g.toJSON();
        d->mappings.push_back ({ "bypass_switch", "bypass" });
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createCompressor()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Compressor";
        d->category = "Dynamics";
        d->tags = { "compressor", "dynamics", "sustain", "tutorial" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF34D399);
        addBypassAndLED(*d);

        DSPGraph g;
        int inID   = g.addNode (createNodeByType ("audio_input"));
        int compID = g.addNode (createNodeByType ("compressor"));
        int outID  = g.addNode (createNodeByType ("audio_output"));

        g.getNode (compID)->getParam ("threshold")->set (-20.0f);
        g.getNode (compID)->getParam ("ratio")->set (4.0f);

        g.getNode (inID)->visualX   = 60.0f;  g.getNode (inID)->visualY   = 240.0f;
        g.getNode (compID)->visualX = 340.0f; g.getNode (compID)->visualY = 240.0f;
        g.getNode (outID)->visualX  = 620.0f; g.getNode (outID)->visualY  = 240.0f;

        g.connect (inID, 0, compID, 0);
        g.connect (compID, 0, outID, 0);
        g.connect (compID, 0, outID, 1);

        const float cx = d->chassisW / 2.0f - 20.0f;
        addBoundKnob (g, *d, "Thresh", cx, 40.0f, compID, "threshold", 40, 40);
        addBoundKnob (g, *d, "Ratio",  cx, 95.0f, compID, "ratio",     40, 40);

        d->effectsGraph = g.toJSON();
        d->mappings.push_back ({ "bypass_switch", "bypass" });
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createNoiseGate()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Noise Gate";
        d->category = "Dynamics";
        d->tags = { "noise gate", "dynamics", "tutorial" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF9CA3AF);
        addBypassAndLED(*d);

        DSPGraph g;
        int inID   = g.addNode (createNodeByType ("audio_input"));
        int gateID = g.addNode (createNodeByType ("noisegate"));
        int outID  = g.addNode (createNodeByType ("audio_output"));

        g.getNode (gateID)->getParam ("threshold")->set (-50.0f);

        g.getNode (inID)->visualX   = 60.0f;  g.getNode (inID)->visualY   = 240.0f;
        g.getNode (gateID)->visualX = 340.0f; g.getNode (gateID)->visualY = 240.0f;
        g.getNode (outID)->visualX  = 620.0f; g.getNode (outID)->visualY  = 240.0f;

        g.connect (inID, 0, gateID, 0);
        g.connect (gateID, 0, outID, 0);
        g.connect (gateID, 0, outID, 1);

        addBoundKnob (g, *d, "Thresh", d->chassisW / 2.0f - 22.5f, 50.0f, gateID, "threshold", 45, 45);

        d->effectsGraph = g.toJSON();
        d->mappings.push_back ({ "bypass_switch", "bypass" });
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createParametricEQ()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Parametric EQ";
        d->category = "EQ";
        d->tags = { "eq", "parametric", "filter", "tutorial" };
        d->chassisW = 200.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF60A5FA);
        addBypassAndLED(*d);

        // Three series parametric-EQ bands, each with Freq/Gain/Q knobs.
        DSPGraph g;
        int inID  = g.addNode (createNodeByType ("audio_input"));
        int eqIDs[3];
        const char* bandNames[3] = { "Low", "Mid", "High" };
        const float bandFreq[3]  = { 250.0f, 1000.0f, 4000.0f };
        for (int b = 0; b < 3; ++b)
        {
            eqIDs[b] = g.addNode (createNodeByType ("peq"));
            g.getNode (eqIDs[b])->setName (bandNames[b]);
            g.getNode (eqIDs[b])->getParam ("freq")->set (bandFreq[b]);
        }
        int outID = g.addNode (createNodeByType ("audio_output"));

        g.getNode (inID)->visualX = 60.0f; g.getNode (inID)->visualY = 320.0f;
        for (int b = 0; b < 3; ++b) { g.getNode (eqIDs[b])->visualX = 240.0f + b * 200.0f; g.getNode (eqIDs[b])->visualY = 320.0f; }
        g.getNode (outID)->visualX = 240.0f + 3 * 200.0f; g.getNode (outID)->visualY = 320.0f;

        g.connect (inID, 0, eqIDs[0], 0);
        g.connect (eqIDs[0], 0, eqIDs[1], 0);
        g.connect (eqIDs[1], 0, eqIDs[2], 0);
        g.connect (eqIDs[2], 0, outID, 0);
        g.connect (eqIDs[2], 0, outID, 1);

        for (int b = 0; b < 3; ++b)
        {
            float x = 25.0f + b * 60.0f;
            addBoundKnob (g, *d, "Freq", x, 30.0f, eqIDs[b], "freq", 30, 30);
            addBoundKnob (g, *d, "Gain", x, 75.0f, eqIDs[b], "gain", 30, 30);
            addBoundKnob (g, *d, "Q",    x, 120.0f, eqIDs[b], "q",   30, 30);
        }

        d->effectsGraph = g.toJSON();
        d->mappings.push_back ({ "bypass_switch", "bypass" });
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createToneControl()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Tone Control";
        d->category = "EQ";
        d->tags = { "eq", "tone", "bass", "mid", "treble", "tutorial" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF818CF8);
        addBypassAndLED(*d);

        DSPGraph g;
        int inID   = g.addNode (createNodeByType ("audio_input"));
        int toneID = g.addNode (createNodeByType ("tonestack"));
        int outID  = g.addNode (createNodeByType ("audio_output"));

        g.getNode (inID)->visualX   = 60.0f;  g.getNode (inID)->visualY   = 240.0f;
        g.getNode (toneID)->visualX = 340.0f; g.getNode (toneID)->visualY = 240.0f;
        g.getNode (outID)->visualX  = 620.0f; g.getNode (outID)->visualY  = 240.0f;

        g.connect (inID, 0, toneID, 0);
        g.connect (toneID, 0, outID, 0);
        g.connect (toneID, 0, outID, 1);

        const float cx = d->chassisW / 2.0f - 15.0f;
        addBoundKnob (g, *d, "Bass",   cx, 30.0f,  toneID, "bass",   30, 30);
        addBoundKnob (g, *d, "Mid",    cx, 75.0f,  toneID, "mid",    30, 30);
        addBoundKnob (g, *d, "Treble", cx, 120.0f, toneID, "treble", 30, 30);

        d->effectsGraph = g.toJSON();
        d->mappings.push_back ({ "bypass_switch", "bypass" });
        addStandardPorts (*d);
        return d;
    }

    inline std::shared_ptr<PedalDesign> createCabinetSim()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Cabinet Sim";
        d->category = "Utility";
        d->tags = { "cabinet", "speaker", "ir-lite", "tutorial" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFFFCD34D);
        addBypassAndLED(*d);

        DSPGraph g;
        int inID  = g.addNode (createNodeByType ("audio_input"));
        int cabID = g.addNode (createNodeByType ("cabinet"));
        int outID = g.addNode (createNodeByType ("audio_output"));

        g.getNode (cabID)->getParam ("cutoff")->set (3500.0f);
        g.getNode (cabID)->getParam ("resonance")->set (0.5f);

        g.getNode (inID)->visualX  = 60.0f;  g.getNode (inID)->visualY  = 240.0f;
        g.getNode (cabID)->visualX = 340.0f; g.getNode (cabID)->visualY = 240.0f;
        g.getNode (outID)->visualX = 620.0f; g.getNode (outID)->visualY = 240.0f;

        g.connect (inID, 0, cabID, 0);
        g.connect (cabID, 0, outID, 0);
        g.connect (cabID, 0, outID, 1);

        const float cx = d->chassisW / 2.0f - 20.0f;
        addBoundKnob (g, *d, "Cutoff", cx, 40.0f, cabID, "cutoff",    40, 40);
        addBoundKnob (g, *d, "Res",    cx, 95.0f, cabID, "resonance", 40, 40);

        d->effectsGraph = g.toJSON();
        d->mappings.push_back ({ "bypass_switch", "bypass" });
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
        
        auto c3 = new juce::DynamicObject();
        c3->setProperty("srcNode", 2); c3->setProperty("srcPort", 0);
        c3->setProperty("dstNode", 1); c3->setProperty("dstPort", 1);
        conns.add(c3);

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

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_gain"}); // node 2 = GainNode

        StickyNote note;
        note.text = "Tutorial 1: Hello Gain\n\nThis is the simplest possible guitar pedal! It takes the audio input, runs it through a Gain Node to boost or cut the level, and sends it directly to the output.\n\nTweak the \"Level\" knob on the chassis to control the Gain Node's gain parameter (in decibels).";
        note.bounds = { 300, 100, 280, 175 };
        note.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note);

        StickyNote note2;
        note2.text = "The Gain Node (Node 2)\n\nThis is where the volume change happens! It scales the amplitude of the incoming audio samples. Tweak the \"Level\" knob on the chassis to see the gain parameter adjust in real-time.";
        note2.bounds = { 300, 290, 280, 130 };
        note2.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note2);

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

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_freq"});      // node 2 = LowPassNode
        d->mappings.push_back({"knob_2", "2_q"});

        StickyNote note;
        note.text = "Tutorial 2: Filter Sweep\n\nThis pedal routes the guitar signal through a Low Pass Filter.\n\n- The \"Cutoff\" knob modulates the filter's frequency (freq), which cuts off high-frequency harmonics and makes the sound darker.\n- The \"Reso\" knob modulates the resonance (q), which boosts frequencies near the cutoff point for a vocal, synthesizer-like sweep.";
        note.bounds = { 300, 100, 280, 195 };
        note.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note);

        StickyNote note2;
        note2.text = "Low Pass Filter (Node 2)\n\nThis node represents the core DSP block. By rolling off high frequencies, it mimics the analog tone controls of classic vintage wah-wahs and synths. Tweak Reso to hear the sweep narrow down!";
        note2.bounds = { 300, 310, 280, 130 };
        note2.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note2);

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

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "1_rate"});  // node 1 = LFONode
        d->mappings.push_back({"knob_2", "1_depth"});

        StickyNote note;
        note.text = "Tutorial 3: Tremolo 101\n\nHere, we use a Low Frequency Oscillator (LFO) to automate the guitar's volume, creating a classic tremolo effect.\n\n- The LFO generates a cycling wave.\n- We scale the wave from bipolar (-1 to +1) to unipolar (0 to 1) using the Add (+1) and Divide (/2) nodes.\n- The scaled LFO modulates a Multiply Node, scaling the guitar's audio signal.";
        note.bounds = { 350, 100, 300, 215 };
        note.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note);

        StickyNote note2;
        note2.text = "LFO Bipolar-to-Unipolar Scaling\n\n- Node 1 (LFO) oscillates between -1.0 and +1.0.\n- Node 4 (Add) offsets it by +1.0 (now 0.0 to 2.0).\n- Node 5 (Divide) halves it by 2.0 (now 0.0 to 1.0).\nThis prevents phase inversion during multiplication!";
        note2.bounds = { 350, 330, 300, 130 };
        note2.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note2);

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

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_time"});     // node 2 = DelayNode
        d->mappings.push_back({"knob_2", "2_feedback"});
        d->mappings.push_back({"knob_3", "3_mix"});      // node 3 = MixNode

        StickyNote note;
        note.text = "Tutorial 4: Delay Lab\n\nThis pedal demonstrates parallel signal routing!\n\n- We use a Split Node to send the guitar audio down two parallel paths:\n  1. Dry path (unprocessed).\n  2. Wet path (routed through the Delay Node).\n- We mix these dry and wet signals back together using a Mix Node before sending them to the output.";
        note.bounds = { 400, 100, 300, 215 };
        note.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note);

        StickyNote note2;
        note2.text = "Parallel Dry/Wet Signal Paths\n\n- Node 1 (Split) copies the mono audio input to two independent pathways.\n- Node 2 (Delay) delays the wet path and feeds its output back to create repeating echoes.\n- Node 3 (Mix) mixes the original dry and delayed wet signals safely.";
        note2.bounds = { 400, 330, 300, 130 };
        note2.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note2);

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

        StickyNote note;
        note.text = "Tutorial 5: Mini Synth\n\nThis is a fully playable MIDI synthesizer voice!\n\n- The MIDI In node tracks incoming keyboard notes and gates.\n- The pitch CV controls the Oscillator frequency.\n- The gate trigger fires the ADSR envelope generator.\n- The ADSR envelope controls a VCA (amplifier), shaping the volume dynamics of the oscillator over time.";
        note.bounds = { 450, 100, 300, 235 };
        note.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note);

        StickyNote note2;
        note2.text = "MIDI-to-CV Voice Architecture\n\n- Node 5 (MidiNote) splits input: port 0 outputs frequency CV; port 1 outputs gate triggers.\n- Node 2 (Oscillator) produces tone.\n- Node 4 (ADSR) creates an envelope contour.\n- Node 3 (VCA) multiplies tone by the ADSR contour.";
        note2.bounds = { 450, 350, 300, 130 };
        note2.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note2);

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

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_sensitivity"}); // node 2 = EnvelopeFollower
        d->mappings.push_back({"knob_2", "4_q"});           // node 4 = LowPassNode
        d->mappings.push_back({"knob_3", "3_out_max"});     // node 3 = RangerNode

        StickyNote note;
        note.text = "Tutorial 6: Envelope Filter\n\nAn Auto-Wah! The envelope follower tracks the amplitude/dynamics of your guitar playing.\n\n- A harder pluck generates a higher control voltage.\n- The Ranger node scales this 0..1 voltage up to a musical frequency sweep range (100Hz to 5000Hz).\n- This mapped voltage modulates the Low Pass Filter frequency dynamically, creating a responsive wah sound!";
        note.bounds = { 450, 100, 320, 235 };
        note.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note);

        StickyNote note2;
        note2.text = "Control Voltage (CV) Sweep Path\n\n- Node 3 (EnvelopeFollower) acts as a dynamic level sensor (outputs 0.0 to 1.0).\n- Node 4 (Ranger) maps the 0..1 level up to musical frequencies (100Hz to 5000Hz).\n- Node 5 (LowPass) uses this mapped frequency for dynamic cutoff sweeping.";
        note2.bounds = { 450, 350, 320, 130 };
        note2.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note2);

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

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "3_bpm"}); // node 3 = ClockNode
        d->mappings.push_back({"knob_2", "6_q"});   // node 6 = LowPassNode

        StickyNote note;
        note.text = "Tutorial 7: Step Sequencer Filter\n\nAn automated rhythmic step filter!\n\n- The Clock Node generates steady pulses at a BPM rate.\n- Each pulse advances the 8-step Sequencer to the next step value.\n- These stepped control voltages are remapped by the Ranger and modulate the Low Pass Filter, creating rhythmic stepping filter patterns.";
        note.bounds = { 500, 100, 320, 215 };
        note.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note);

        StickyNote note2;
        note2.text = "Clock & Sequencer Modulations\n\n- Node 3 (Clock) acts as a metronome generator, dispatching steady trigger pulses.\n- Node 4 (Sequencer) advances through its custom sliders on each trigger, outputting stepped control values.\n- Node 5 (Ranger) remaps the steps up to musical lowpass cutoffs (200Hz to 6000Hz).";
        note2.bounds = { 500, 330, 320, 130 };
        note2.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note2);

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

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "4_bpm"});          // node 4 = ClockNode
        d->mappings.push_back({"knob_2", "6_drive"});        // node 6 = SoftClipNode
        d->mappings.push_back({"knob_3", "2_sensitivity"});  // node 2 = EnvelopeFollower

        StickyNote note;
        note.text = "Tutorial 8: Pattern Slicer\n\nAn advanced logic-controlled gated fuzz!\n\n- The Envelope Follower + Comparator detect when you are actively playing a note.\n- The Clock Node generates steady rhythmic gate pulses.\n- We combine these via an AND Gate: slicing is active ONLY when you are actively playing AND the clock pulse is high.\n- The AND Gate output drives a Multiplexer (Mux), switching between Dry and Fuzz paths.";
        note.bounds = { 550, 100, 340, 255 };
        note.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note);

        StickyNote note2;
        note2.text = "Logic Gates & Multiplexing Paths\n\n- Node 3 (EnvelopeFollower) & Node 4 (Comparator) threshold your plucks (outputting 0 or 1 CV).\n- Node 5 (Clock) emits a steady rhythmic pulse gate.\n- Node 6 (ANDGate) combines them: high only when plucking AND clock pulse high.\n- Node 8 (MuxNode) routes Dry (port A) or Fuzz (port B) based on Node 6's gate!";
        note2.bounds = { 550, 370, 340, 140 };
        note2.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note2);

        addStandardPorts (*d);
        return d;
    }

    /** Tutorial 9 — Wave Folder */
    inline std::shared_ptr<PedalDesign> createTutorialWaveFolder()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Wave Folder";
        d->category = "Tutorial";
        d->tags = { "distortion", "scripting", "modulation", "cyberpunk", "lua" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFFC084FC); // pastel purple

        // Title label
        PedalDesign::Control title;
        title.type = "label";
        title.label = "WAVE FOLDER";
        title.controlID = "lbl_title";
        title.x = 10.0f; title.y = 8.0f;
        title.width = 100.0f; title.height = 20.0f;
        d->controls.push_back(title);

        // Drive knob (maps to drive parameter on node 3)
        PedalDesign::Control k1;
        k1.type = "knob";
        k1.width = 30; k1.height = 30;
        k1.x = 15.0f; k1.y = 40.0f;
        k1.label = "Drive";
        k1.controlID = "knob_1";
        d->controls.push_back(k1);

        // Rate knob (maps to rate parameter on node 3)
        PedalDesign::Control k2;
        k2.type = "knob";
        k2.width = 30; k2.height = 30;
        k2.x = 75.0f; k2.y = 40.0f;
        k2.label = "Rate";
        k2.controlID = "knob_2";
        d->controls.push_back(k2);

        // Mix knob (maps to mix parameter on node 3)
        PedalDesign::Control k3;
        k3.type = "knob";
        k3.width = 30; k3.height = 30;
        k3.x = d->chassisW / 2.0f - 15.0f;
        k3.y = 95.0f;
        k3.label = "Mix";
        k3.controlID = "knob_3";
        d->controls.push_back(k3);

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"knob_1", "2_drive"}); // node 2 = Expression
        d->mappings.push_back({"knob_2", "2_rate"});  // node 2 = Expression
        d->mappings.push_back({"knob_3", "2_mix"});   // node 2 = Expression

        StickyNote note;
        note.text = "Tutorial 9: Scriptable Wave Folder\n\nWelcome to scriptable modular synthesis!\n\n- The active DSP of this pedal is driven entirely by a LUA-compiled Expression script.\n- Tweak the knobs on the chassis to see the drive, rate, and mix parameters scale the variables inside the code editor in real-time.\n- Explore the code inside the Code/DSP editor tab and feel free to rewrite the math equations!";
        note.bounds = { 500, 100, 320, 215 };
        note.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note);

        StickyNote note2;
        note2.text = "Mathematical Wavefolding\n\n- folder = sin(in * drive) applies a soft sinusoidal wavefolding shape, creating metallic overtones.\n- carrier = sin(t * rate * 6.28318) generates a continuous time-based modulator using the elapsed time variable t.\n- out = lerp(in, modulator, mix) mixes the clean input dry path with the ring modulated folders.";
        note2.bounds = { 500, 330, 320, 130 };
        note2.colour = juce::Colour(0xFFFFEB3B);
        d->fxNotes.push_back(note2);

        addStandardPorts (*d);
        addBypassAndLED (*d);
        d->isFactory = true;
        return d;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // AETHER RIG — Complete NAM Channel Strip
    // ─────────────────────────────────────────────────────────────────────────────
    inline std::shared_ptr<PedalDesign> createAetherRig()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Aether Rig";
        d->category = "Amp Sim";
        d->chassisW = 300.0f;
        d->chassisH = 280.0f;
        d->chassisColour = juce::Colour(0xFF1A1A2E);  // Deep midnight blue

        addBypassAndLED(*d);

        // ── Row 1: Top knobs (Gate, Drive, Input, Output, Cab Vol) ──
        PedalDesign::Control gateKnob;
        gateKnob.type = "knob"; gateKnob.label = "Gate";
        gateKnob.controlID = "gate_knob";
        gateKnob.x = 15; gateKnob.y = 30; gateKnob.width = 40; gateKnob.height = 40;

        PedalDesign::Control driveKnob;
        driveKnob.type = "knob"; driveKnob.label = "Drive";
        driveKnob.controlID = "drive_knob";
        driveKnob.x = 70; driveKnob.y = 30; driveKnob.width = 40; driveKnob.height = 40;

        PedalDesign::Control inputKnob;
        inputKnob.type = "knob"; inputKnob.label = "Input";
        inputKnob.controlID = "input_knob";
        inputKnob.x = 125; inputKnob.y = 30; inputKnob.width = 40; inputKnob.height = 40;

        PedalDesign::Control outputKnob;
        outputKnob.type = "knob"; outputKnob.label = "Output";
        outputKnob.controlID = "output_knob";
        outputKnob.x = 180; outputKnob.y = 30; outputKnob.width = 40; outputKnob.height = 40;

        PedalDesign::Control cabGainKnob;
        cabGainKnob.type = "knob"; cabGainKnob.label = "Cab Vol";
        cabGainKnob.controlID = "cab_gain_knob";
        cabGainKnob.x = 235; cabGainKnob.y = 30; cabGainKnob.width = 40; cabGainKnob.height = 40;

        // ── Row 2: EQ & Reverb knobs ──
        PedalDesign::Control bassKnob;
        bassKnob.type = "knob"; bassKnob.label = "Bass";
        bassKnob.controlID = "bass_knob";
        bassKnob.x = 15; bassKnob.y = 85; bassKnob.width = 35; bassKnob.height = 35;

        PedalDesign::Control midKnob;
        midKnob.type = "knob"; midKnob.label = "Mid";
        midKnob.controlID = "mid_knob";
        midKnob.x = 60; midKnob.y = 85; midKnob.width = 35; midKnob.height = 35;

        PedalDesign::Control trebleKnob;
        trebleKnob.type = "knob"; trebleKnob.label = "Treble";
        trebleKnob.controlID = "treble_knob";
        trebleKnob.x = 105; trebleKnob.y = 85; trebleKnob.width = 35; trebleKnob.height = 35;

        PedalDesign::Control revMixKnob;
        revMixKnob.type = "knob"; revMixKnob.label = "Reverb Mix";
        revMixKnob.controlID = "rev_mix_knob";
        revMixKnob.x = 170; revMixKnob.y = 85; revMixKnob.width = 35; revMixKnob.height = 35;

        PedalDesign::Control revSizeKnob;
        revSizeKnob.type = "knob"; revSizeKnob.label = "Room Size";
        revSizeKnob.controlID = "rev_size_knob";
        revSizeKnob.x = 215; revSizeKnob.y = 85; revSizeKnob.width = 35; revSizeKnob.height = 35;

        PedalDesign::Control cabMixKnob;
        cabMixKnob.type = "knob"; cabMixKnob.label = "Cab Mix";
        cabMixKnob.controlID = "cab_mix_knob";
        cabMixKnob.x = 260; cabMixKnob.y = 85; cabMixKnob.width = 35; cabMixKnob.height = 35;

        // ── NAM Model display ──
        PedalDesign::Control namDisplay;
        namDisplay.type = "text_screen";
        namDisplay.label = "AETHER RIG\nNo Model Loaded";
        namDisplay.numLines = 2;
        namDisplay.fontSize = 8.0f;
        namDisplay.controlID = "nam_display";
        namDisplay.x = 15; namDisplay.y = 135; namDisplay.width = 130; namDisplay.height = 25;

        // ── IR Cab display ──
        PedalDesign::Control irDisplay;
        irDisplay.type = "text_screen";
        irDisplay.label = "Cabinet IR (Convolver)\nNo IR Loaded";
        irDisplay.numLines = 2;
        irDisplay.fontSize = 8.0f;
        irDisplay.controlID = "ir_display";
        irDisplay.x = 155; irDisplay.y = 135; irDisplay.width = 130; irDisplay.height = 25;

        // ── Action buttons ──
        PedalDesign::Control namBrowseBtn;
        namBrowseBtn.type = "file_loader";
        namBrowseBtn.label = "NAM...";
        namBrowseBtn.controlID = "nam_loader";
        namBrowseBtn.x = 15; namBrowseBtn.y = 168; namBrowseBtn.width = 60; namBrowseBtn.height = 18;

        PedalDesign::Control namLibBtn;
        namLibBtn.type = "library_loader";
        namLibBtn.label = "Library";
        namLibBtn.controlID = "nam_library";
        namLibBtn.libraryCategory = "NAM";
        namLibBtn.x = 80; namLibBtn.y = 168; namLibBtn.width = 60; namLibBtn.height = 18;

        PedalDesign::Control irBrowseBtn;
        irBrowseBtn.type = "file_loader";
        irBrowseBtn.label = "Cab IR...";
        irBrowseBtn.controlID = "ir_loader";
        irBrowseBtn.x = 155; irBrowseBtn.y = 168; irBrowseBtn.width = 60; irBrowseBtn.height = 18;

        PedalDesign::Control irLibBtn;
        irLibBtn.type = "library_loader";
        irLibBtn.label = "Cab Lib";
        irLibBtn.controlID = "ir_library";
        irLibBtn.libraryCategory = "IR_CAB";
        irLibBtn.x = 220; irLibBtn.y = 168; irLibBtn.width = 60; irLibBtn.height = 18;

        // Push all controls
        d->controls.push_back(gateKnob);
        d->controls.push_back(driveKnob);
        d->controls.push_back(inputKnob);
        d->controls.push_back(outputKnob);
        d->controls.push_back(cabGainKnob);
        d->controls.push_back(bassKnob);
        d->controls.push_back(midKnob);
        d->controls.push_back(trebleKnob);
        d->controls.push_back(revMixKnob);
        d->controls.push_back(revSizeKnob);
        d->controls.push_back(cabMixKnob);
        d->controls.push_back(namDisplay);
        d->controls.push_back(irDisplay);
        d->controls.push_back(namBrowseBtn);
        d->controls.push_back(namLibBtn);
        d->controls.push_back(irBrowseBtn);
        d->controls.push_back(irLibBtn);

        // ── Mappings ──
        // Node IDs: 2=gate, 3=softclip, 4=nam, 5=ir, 6=tonestack, 7=reverb
        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"gate_knob",     "2_threshold"});
        d->mappings.push_back({"drive_knob",    "3_drive"});
        d->mappings.push_back({"input_knob",    "4_gain"});
        d->mappings.push_back({"output_knob",   "4_out_level"});
        d->mappings.push_back({"cab_gain_knob", "5_gain"});
        d->mappings.push_back({"cab_mix_knob",  "5_mix"});
        d->mappings.push_back({"bass_knob",     "6_bass"});
        d->mappings.push_back({"mid_knob",      "6_mid"});
        d->mappings.push_back({"treble_knob",   "6_treble"});
        d->mappings.push_back({"rev_mix_knob",  "7_mix"});
        d->mappings.push_back({"rev_size_knob", "7_size"});
        d->mappings.push_back({"nam_display:1", "4_filepath"});
        d->mappings.push_back({"ir_display:1",  "5_filepath"});
        d->mappings.push_back({"nam_loader",    "4_filepath"});
        d->mappings.push_back({"nam_library",   "4_filepath"});
        d->mappings.push_back({"ir_loader",     "5_filepath"});
        d->mappings.push_back({"ir_library",    "5_filepath"});

        // ── Internal graph (JSON) ──
        auto graph = std::make_unique<juce::DynamicObject>();
        juce::Array<juce::var> nodes;

        auto mkNode = [&](int id, const char* type, const char* name) {
            auto* n = new juce::DynamicObject();
            n->setProperty("id", id);
            n->setProperty("type", juce::String(type));
            n->setProperty("name", juce::String(name));
            return n;
        };

        nodes.add(mkNode(0, "audio_input",  "Audio In"));
        nodes.add(mkNode(1, "audio_output", "Audio Out"));

        // Gate (node 2)
        auto* gateN = mkNode(2, "noisegate", "Gate");
        { juce::Array<juce::var> p;
          auto* p1 = new juce::DynamicObject(); p1->setProperty("id", "threshold"); p1->setProperty("value", -50.0); p.add(p1);
          gateN->setProperty("params", p); }
        nodes.add(gateN);

        // Boost/OD (node 3)
        auto* odN = mkNode(3, "softclip", "Boost");
        { juce::Array<juce::var> p;
          auto* p1 = new juce::DynamicObject(); p1->setProperty("id", "drive"); p1->setProperty("value", 1.0); p.add(p1);
          odN->setProperty("params", p); }
        nodes.add(odN);

        // NAM (node 4)
        auto* namN = mkNode(4, "nam", "NAM Amp");
        { juce::Array<juce::var> p;
          auto* p1 = new juce::DynamicObject(); p1->setProperty("id", "gain"); p1->setProperty("value", 0.0); p.add(p1);
          auto* p2 = new juce::DynamicObject(); p2->setProperty("id", "out_level"); p2->setProperty("value", 0.0); p.add(p2);
          namN->setProperty("params", p); }
        nodes.add(namN);

        // IR Cab (node 5)
        auto* irN = mkNode(5, "ir", "Cabinet IR");
        { juce::Array<juce::var> p;
          auto* p1 = new juce::DynamicObject(); p1->setProperty("id", "mix"); p1->setProperty("value", 1.0); p.add(p1);
          auto* p2 = new juce::DynamicObject(); p2->setProperty("id", "gain"); p2->setProperty("value", 1.0); p.add(p2);
          irN->setProperty("params", p); }
        nodes.add(irN);

        // Tone Stack EQ (node 6)
        auto* eqN = mkNode(6, "tonestack", "EQ");
        { juce::Array<juce::var> p;
          auto* p1 = new juce::DynamicObject(); p1->setProperty("id", "bass"); p1->setProperty("value", 0.0); p.add(p1);
          auto* p2 = new juce::DynamicObject(); p2->setProperty("id", "mid"); p2->setProperty("value", 0.0); p.add(p2);
          auto* p3 = new juce::DynamicObject(); p3->setProperty("id", "treble"); p3->setProperty("value", 0.0); p.add(p3);
          eqN->setProperty("params", p); }
        nodes.add(eqN);

        // Reverb (node 7)
        auto* revN = mkNode(7, "reverb", "Reverb");
        { juce::Array<juce::var> p;
          auto* p1 = new juce::DynamicObject(); p1->setProperty("id", "size"); p1->setProperty("value", 0.5); p.add(p1);
          auto* p2 = new juce::DynamicObject(); p2->setProperty("id", "mix"); p2->setProperty("value", 0.0); p.add(p2);
          auto* p3 = new juce::DynamicObject(); p3->setProperty("id", "damping"); p3->setProperty("value", 0.5); p.add(p3);
          revN->setProperty("params", p); }
        nodes.add(revN);

        // Tone Stack EQ Right (node 8)
        auto* eqRN = mkNode(8, "tonestack", "EQ Right");
        { juce::Array<juce::var> p;
          auto* p1 = new juce::DynamicObject(); p1->setProperty("id", "bass"); p1->setProperty("value", 0.0); p.add(p1);
          auto* p2 = new juce::DynamicObject(); p2->setProperty("id", "mid"); p2->setProperty("value", 0.0); p.add(p2);
          auto* p3 = new juce::DynamicObject(); p3->setProperty("id", "treble"); p3->setProperty("value", 0.0); p.add(p3);
          eqRN->setProperty("params", p); }
        nodes.add(eqRN);

        // Reverb Right (node 9)
        auto* revRN = mkNode(9, "reverb", "Reverb Right");
        { juce::Array<juce::var> p;
          auto* p1 = new juce::DynamicObject(); p1->setProperty("id", "size"); p1->setProperty("value", 0.5); p.add(p1);
          auto* p2 = new juce::DynamicObject(); p2->setProperty("id", "mix"); p2->setProperty("value", 0.0); p.add(p2);
          auto* p3 = new juce::DynamicObject(); p3->setProperty("id", "damping"); p3->setProperty("value", 0.5); p.add(p3);
          revRN->setProperty("params", p); }
        nodes.add(revRN);

        // Add / Summer (node 10)
        auto* addN = mkNode(10, "add", "Input Summer");
        { juce::Array<juce::var> p;
          auto* p1 = new juce::DynamicObject(); p1->setProperty("id", "value"); p1->setProperty("value", 0.0); p.add(p1);
          addN->setProperty("params", p); }
        nodes.add(addN);

        graph->setProperty("nodes", nodes);

        // ── Connections ──
        juce::Array<juce::var> conns;
        auto mkConn = [&](int sN, int sP, int dN, int dP) {
            auto* c = new juce::DynamicObject();
            c->setProperty("srcNode", sN); c->setProperty("srcPort", sP);
            c->setProperty("dstNode", dN); c->setProperty("dstPort", dP);
            conns.add(c);
        };

        mkConn(0, 0, 10, 0);  // In L → Sum a
        mkConn(0, 1, 10, 1);  // In R → Sum b
        mkConn(10, 0, 2, 0);  // Sum → Gate
        mkConn(2, 0, 3, 0);   // Gate → Boost
        mkConn(3, 0, 4, 0);   // Boost → NAM
        mkConn(4, 0, 5, 0);   // NAM → IR in_l
        mkConn(4, 0, 5, 1);   // NAM → IR in_r
        mkConn(5, 0, 6, 0);   // IR out_l → EQ L
        mkConn(6, 0, 7, 0);   // EQ L → Reverb L
        mkConn(7, 0, 1, 0);   // Reverb L → Out L
        
        mkConn(5, 1, 8, 0);   // IR out_r → EQ R
        mkConn(8, 0, 9, 0);   // EQ R → Reverb R
        mkConn(9, 0, 1, 1);   // Reverb R → Out R

        graph->setProperty("connections", conns);
        d->effectsGraph = juce::var(graph.release());

        addStandardPorts(*d);
        d->isFactory = true;
        return d;
    }

    inline std::shared_ptr<PedalDesign> createMixerPedal()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Mixer";
        d->category = "Utility";
        d->tags = { "mixer", "utility", "combine", "parallel" };
        d->chassisW = 140.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF4F46E5); // Deep Indigo
        
        addBypassAndLED(*d);

        // Define three knobs
        // 1. Vol 1 (mapped to mixerNode vol1)
        PedalDesign::Control k1;
        k1.type = "knob";
        k1.width = 35; k1.height = 35;
        k1.x = 20.0f; k1.y = 40.0f;
        k1.label = "Vol 1";
        k1.controlID = "vol1_knob";
        d->controls.push_back(k1);

        // 2. Vol 2 (mapped to mixerNode vol2)
        PedalDesign::Control k2;
        k2.type = "knob";
        k2.width = 35; k2.height = 35;
        k2.x = 85.0f; k2.y = 40.0f;
        k2.label = "Vol 2";
        k2.controlID = "vol2_knob";
        d->controls.push_back(k2);

        // 3. Master (mapped to mixerNode master)
        PedalDesign::Control k3;
        k3.type = "knob";
        k3.width = 40; k3.height = 40;
        k3.x = d->chassisW / 2.0f - 20.0f;
        k3.y = 100.0f;
        k3.label = "Master";
        k3.controlID = "master_knob";
        d->controls.push_back(k3);

        // Setup effectsGraph in JSON matching the C++ factory
        auto graph = std::make_unique<juce::DynamicObject>();
        juce::Array<juce::var> nodes;
        
        auto n0 = new juce::DynamicObject(); n0->setProperty("id", 0); n0->setProperty("type", "audio_input"); n0->setProperty("name", "Audio In"); nodes.add(n0);
        auto n1 = new juce::DynamicObject(); n1->setProperty("id", 1); n1->setProperty("type", "audio_output"); n1->setProperty("name", "Audio Out"); nodes.add(n1);
        auto n2 = new juce::DynamicObject(); n2->setProperty("id", 2); n2->setProperty("type", "aux_input"); n2->setProperty("name", "Aux In"); nodes.add(n2);
        
        auto n3 = new juce::DynamicObject(); n3->setProperty("id", 3); n3->setProperty("type", "stereo_mixer"); n3->setProperty("name", "Stereo Mixer");
        juce::Array<juce::var> params;
        auto p1 = new juce::DynamicObject(); p1->setProperty("id", "vol1"); p1->setProperty("value", 0.0); params.add(p1);
        auto p2 = new juce::DynamicObject(); p2->setProperty("id", "vol2"); p2->setProperty("value", 0.0); params.add(p2);
        auto p3 = new juce::DynamicObject(); p3->setProperty("id", "master"); p3->setProperty("value", 0.0); params.add(p3);
        n3->setProperty("params", params);
        nodes.add(n3);
        
        graph->setProperty("nodes", nodes);
        
        // Connections matching createMixerPedal C++
        juce::Array<juce::var> conns;
        auto mkConn = [&](int sN, int sP, int dN, int dP) {
            auto* c = new juce::DynamicObject();
            c->setProperty("srcNode", sN); c->setProperty("srcPort", sP);
            c->setProperty("dstNode", dN); c->setProperty("dstPort", dP);
            conns.add(c);
        };
        
        mkConn(0, 0, 3, 0); // Audio Input L -> Mixer In 1 L
        mkConn(0, 1, 3, 1); // Audio Input R -> Mixer In 1 R
        mkConn(2, 0, 3, 2); // Aux Input L -> Mixer In 2 L
        mkConn(2, 1, 3, 3); // Aux Input R -> Mixer In 2 R
        
        mkConn(3, 0, 1, 0); // Mixer Out L -> Audio Output L
        mkConn(3, 1, 1, 1); // Mixer Out R -> Audio Output R
        
        graph->setProperty("connections", conns);
        d->effectsGraph = juce::var(graph.release());

        // Parameter mappings
        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"vol1_knob", "3_vol1"});
        d->mappings.push_back({"vol2_knob", "3_vol2"});
        d->mappings.push_back({"master_knob", "3_master"});

        addStandardPorts(*d);
        d->isFactory = true;
        return d;
    }

    inline std::shared_ptr<PedalDesign> createMatrixMixerPedal()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Matrix Mixer";
        d->category = "Utility";
        d->tags = { "matrix", "mixer", "utility", "routing", "parallel" };
        d->chassisW = 240.0f;
        d->chassisH = 280.0f;
        d->chassisColour = juce::Colour(0xFF1E293B); // Dark graphite
        
        addBypassAndLED(*d);

        // Define the 4x4 matrix of crosspoint knobs
        float xStart = 25.0f;
        float xGap = 50.0f;
        float yStart = 40.0f;
        float yGap = 35.0f;

        for (int row = 1; row <= 4; ++row)
        {
            for (int col = 1; col <= 4; ++col)
            {
                PedalDesign::Control k;
                k.type = "knob";
                k.width = 24; k.height = 24;
                k.x = xStart + (col - 1) * xGap;
                k.y = yStart + (row - 1) * yGap;
                k.label = juce::String (row) + "->" + juce::String (col);
                k.controlID = "k_g" + juce::String (row) + juce::String (col);
                
                // Unity gain defaults for diagonals, 0 for others
                k.defaultValue = (row == col) ? 1.0f : 0.0f;
                d->controls.push_back(k);

                // Map to matrix mixer (node ID 4)
                d->mappings.push_back ({ k.controlID, "4_g" + juce::String (row) + juce::String (col) });
            }
        }

        // Setup effectsGraph in JSON matching the C++ factory
        auto graph = std::make_unique<juce::DynamicObject>();
        juce::Array<juce::var> nodes;
        
        auto n0 = new juce::DynamicObject(); n0->setProperty("id", 0); n0->setProperty("type", "audio_input"); n0->setProperty("name", "Audio In"); nodes.add(n0);
        auto n1 = new juce::DynamicObject(); n1->setProperty("id", 1); n1->setProperty("type", "audio_output"); n1->setProperty("name", "Audio Out"); nodes.add(n1);
        auto n2 = new juce::DynamicObject(); n2->setProperty("id", 2); n2->setProperty("type", "aux_input"); n2->setProperty("name", "Aux In"); nodes.add(n2);
        auto n3 = new juce::DynamicObject(); n3->setProperty("id", 3); n3->setProperty("type", "aux_output"); n3->setProperty("name", "Aux Out"); nodes.add(n3);
        
        auto n4 = new juce::DynamicObject(); n4->setProperty("id", 4); n4->setProperty("type", "matrix_mixer"); n4->setProperty("name", "Matrix Mixer");
        juce::Array<juce::var> params;
        for (int r = 1; r <= 4; ++r)
        {
            for (int c = 1; c <= 4; ++c)
            {
                auto p = new juce::DynamicObject();
                p->setProperty ("id", "g" + juce::String (r) + juce::String (c));
                p->setProperty ("value", (r == c) ? 1.0 : 0.0);
                params.add (p);
            }
        }
        n4->setProperty("params", params);
        nodes.add(n4);
        
        graph->setProperty("nodes", nodes);
        
        // Connections
        juce::Array<juce::var> conns;
        auto mkConn = [&](int sN, int sP, int dN, int dP) {
            auto* c = new juce::DynamicObject();
            c->setProperty("srcNode", sN); c->setProperty("srcPort", sP);
            c->setProperty("dstNode", dN); c->setProperty("dstPort", dP);
            conns.add(c);
        };
        
        // Connect inputs to Matrix
        mkConn(0, 0, 4, 0); // Main In L -> Matrix In 1
        mkConn(0, 1, 4, 1); // Main In R -> Matrix In 2
        mkConn(2, 0, 4, 2); // Aux In L  -> Matrix In 3
        mkConn(2, 1, 4, 3); // Aux In R  -> Matrix In 4
        
        // Connect Matrix to outputs
        mkConn(4, 0, 1, 0); // Matrix Out 1 -> Main Out L
        mkConn(4, 1, 1, 1); // Matrix Out 2 -> Main Out R
        mkConn(4, 2, 3, 0); // Matrix Out 3 -> Aux Out L
        mkConn(4, 3, 3, 1); // Matrix Out 4 -> Aux Out R
        
        graph->setProperty("connections", conns);
        d->effectsGraph = juce::var(graph.release());

        d->mappings.push_back({"bypass_switch", "bypass"});

        addStandardPorts(*d);
        d->isFactory = true;
        return d;
    }

    inline std::shared_ptr<PedalDesign> createMatrixMixerXLPedal()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Matrix Mixer XL";
        d->category = "Utility";
        d->tags = { "matrix", "mixer", "utility", "routing", "multichannel", "parallel" };
        d->chassisW = 120.0f;
        d->chassisH = 200.0f;
        d->chassisColour = juce::Colour(0xFF0F172A); // Midnight blue-slate
        
        addBypassAndLED(*d);

        // Master volume control knob
        PedalDesign::Control kMaster;
        kMaster.type = "knob";
        kMaster.width = 42; kMaster.height = 42;
        kMaster.x = 39.0f; kMaster.y = 50.0f;
        kMaster.label = "MASTER";
        kMaster.controlID = "k_master";
        kMaster.defaultValue = 1.0f;
        d->controls.push_back (kMaster);
        
        d->mappings.push_back ({ "k_master", "4_master_vol" });

        // Launcher button for overlay grid
        PedalDesign::Control btnOverlay;
        btnOverlay.type = "overlay_launcher";
        btnOverlay.width = 90; btnOverlay.height = 25;
        btnOverlay.x = 15.0f; btnOverlay.y = 110.0f;
        btnOverlay.label = "OPEN MATRIX";
        btnOverlay.controlID = "btn_grid";
        btnOverlay.overlayPage = "Matrix Mixer XL Grid";
        d->controls.push_back (btnOverlay);

        // Create overlay canvas page
        PedalDesign::CanvasPage gridPage;
        gridPage.pageName = "Matrix Mixer XL Grid";
        gridPage.width = 1000.0f;
        gridPage.height = 600.0f;
        gridPage.backgroundColour = juce::Colour(0xFF0B0F19);
        
        PedalDesign::Control gridDisplay;
        gridDisplay.type = "custom_display";
        gridDisplay.controlID = "matrix_mixer_xl_display";
        gridDisplay.x = 10.0f; gridDisplay.y = 10.0f;
        gridDisplay.width = 980.0f; gridDisplay.height = 580.0f;
        gridPage.controls.push_back (gridDisplay);
        
        d->canvasPages.push_back (gridPage);

        // JSON Graph template
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
        
        auto n4 = new juce::DynamicObject();
        n4->setProperty("id", 4);
        n4->setProperty("type", "matrix_mixer_xl");
        n4->setProperty("name", "Matrix Mixer XL");
        
        juce::Array<juce::var> params;
        auto pSize = new juce::DynamicObject(); pSize->setProperty("id", "size"); pSize->setProperty("value", 8.0); params.add(pSize);
        auto pVol = new juce::DynamicObject(); pVol->setProperty("id", "master_vol"); pVol->setProperty("value", 1.0); params.add(pVol);
        n4->setProperty("params", params);
        nodes.add(n4);
        
        graph->setProperty("nodes", nodes);
        
        juce::Array<juce::var> conns;
        for (int ch = 0; ch < 32; ++ch)
        {
            auto* c1 = new juce::DynamicObject();
            c1->setProperty("srcNode", 0); c1->setProperty("srcPort", ch);
            c1->setProperty("dstNode", 4); c1->setProperty("dstPort", ch);
            conns.add(c1);
            
            auto* c2 = new juce::DynamicObject();
            c2->setProperty("srcNode", 4); c2->setProperty("srcPort", ch);
            c2->setProperty("dstNode", 1); c2->setProperty("dstPort", ch);
            conns.add(c2);
        }
        
        graph->setProperty("connections", conns);
        d->effectsGraph = juce::var(graph.release());
        
        d->mappings.push_back({"bypass_switch", "bypass"});
        addStandardPorts(*d);
        d->isFactory = true;
        return d;
    }
}

