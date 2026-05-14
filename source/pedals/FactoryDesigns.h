#pragma once

#include "../dsp/PedalDesign.h"

namespace FactoryDesigns
{
    inline std::shared_ptr<PedalDesign> createCleanBoostDesign()
    {
        auto d = std::make_shared<PedalDesign>();
        d->name = "Clean Boost";
        d->category = "Drive";
        d->chassisW = 150.0f;
        d->chassisH = 220.0f;
        d->chassisColour = juce::Colour(0xFF4ADE80); // Green
        
        // Footswitch
        PedalDesign::Control sw;
        sw.type = "footswitch";
        sw.width = 40; sw.height = 40;
        sw.x = d->chassisW / 2.0f - 20.0f;
        sw.y = 160.0f;
        sw.controlID = "bypass_switch";
        d->controls.push_back(sw);

        // LED
        PedalDesign::Control led;
        led.type = "led";
        led.width = 16; led.height = 16;
        led.x = d->chassisW / 2.0f - 8.0f;
        led.y = 130.0f;
        led.controlID = "bypass_led";
        led.customColour = juce::Colours::red;
        d->controls.push_back(led);

        // Knob (Gain)
        PedalDesign::Control knob;
        knob.type = "knob";
        knob.width = 50; knob.height = 50;
        knob.x = d->chassisW / 2.0f - 25.0f;
        knob.y = 40.0f;
        knob.label = "Gain";
        knob.controlID = "knob_1";
        d->controls.push_back(knob);

        // Mapping (nodeID 2 is usually the first node added after Input)
        // Wait, the param ID from the DSPGraph is typically "2_gain" since input is 1.
        PedalDesign::Mapping m;
        m.controlID = "knob_1";
        m.nodeParam = "2_gain";
        d->mappings.push_back(m);

        return d;
    }

    // You can add the rest here later...
}
