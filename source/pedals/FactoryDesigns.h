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

        d->mappings.push_back({"knob_1", "2_gain"}); // GainNode pre
        d->mappings.push_back({"knob_2", "4_tone"}); // ToneStackNode
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

        d->mappings.push_back({"knob_1", "2_gain"}); // GainNode pre
        d->mappings.push_back({"knob_2", "4_tone"}); // ToneStackNode
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

        d->mappings.push_back({"knob_1", "3_gain"}); // FuzzNode gain
        d->mappings.push_back({"knob_2", "4_tone"}); // ToneStackNode
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

        d->mappings.push_back({"knob_1", "2_cutoff"}); 
        d->mappings.push_back({"knob_2", "2_resonance"}); 
        return d;
    }
}
