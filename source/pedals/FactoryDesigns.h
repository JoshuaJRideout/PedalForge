#pragma once

#include "../dsp/PedalDesign.h"
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

    inline std::shared_ptr<PedalDesign> createCleanBoost()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Clean Boost";
        d->category = "Drive";
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
        return d;
    }

    inline std::shared_ptr<PedalDesign> createOverdrive()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Overdrive";
        d->category = "Drive";
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
        return d;
    }

    inline std::shared_ptr<PedalDesign> createDelay()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Delay";
        d->category = "Time";
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
        return d;
    }

    inline std::shared_ptr<PedalDesign> createReverb()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Reverb";
        d->category = "Time";
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
        return d;
    }

    inline std::shared_ptr<PedalDesign> createCompressor()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Compressor";
        d->category = "Dynamics";
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

        d->controls.push_back(inKnob);
        d->controls.push_back(outKnob);
        d->controls.push_back(fileBtn);
        d->controls.push_back(libBtn);

        d->mappings.push_back({"bypass_switch", "bypass"});
        d->mappings.push_back({"in_knob", "2_gain"}); // 2 is NAMNode
        d->mappings.push_back({"out_knob", "2_out_level"});
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
        libBtn.x = 20; libBtn.y = 160; libBtn.width = 100; libBtn.height = 30;

        d->controls.push_back(mix);
        d->controls.push_back(gain);
        d->controls.push_back(fileBtn);
        d->controls.push_back(libBtn);

        d->mappings.push_back({"mix", "2_mix"});
        d->mappings.push_back({"gain", "2_gain"});
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

        PedalDesign::Control fileBtn;
        fileBtn.type = "file_loader";
        fileBtn.label = "Browse...";
        fileBtn.controlID = "ir_loader";
        fileBtn.x = 20; fileBtn.y = 120; fileBtn.width = 100; fileBtn.height = 30;

        PedalDesign::Control libBtn;
        libBtn.type = "library_loader";
        libBtn.label = "Library";
        libBtn.controlID = "ir_library";
        libBtn.x = 20; libBtn.y = 160; libBtn.width = 100; libBtn.height = 30;

        d->controls.push_back(mix);
        d->controls.push_back(gain);
        d->controls.push_back(fileBtn);
        d->controls.push_back(libBtn);

        d->mappings.push_back({"mix", "2_mix"});
        d->mappings.push_back({"gain", "2_gain"});
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

        return d;
    }
}
