#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/GraphPedalProcessor.h"
#include "FactoryDesigns.h"
#include <functional>
#include <vector>

//==============================================================================
/**
 * Registry of all available factory pedal types.
 * Used by the palette UI to populate the pedalboard menu.
 */
struct PedalInfo
{
    juce::String name;
    juce::String category;
    int gridW, gridH;
    int numKnobs;
    juce::Colour colour;
    std::function<std::unique_ptr<juce::AudioProcessor>()> factory;
    std::function<std::shared_ptr<PedalDesign>()> designFactory;
};

inline juce::File getPedalLibraryDir()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("PedalForge")
        .getChildFile("Library")
        .getChildFile("Pedals");
    dir.createDirectory();
    return dir;
}

inline std::shared_ptr<PedalDesign> loadDesignOrDefault(const juce::String& name, std::function<std::shared_ptr<PedalDesign>()> factory)
{
    juce::File overrideFile = getPedalLibraryDir().getChildFile(name + ".json");
    if (overrideFile.existsAsFile())
    {
        try {
            auto d = std::make_shared<PedalDesign>(PedalDesign::loadFromFile(overrideFile));
            d->name = name;
            return d;
        } catch (...) {}
    }
    return factory();
}

inline std::shared_ptr<PedalDesign> loadCustomPedalDesign(const juce::String& name)
{
    auto designsDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                          .getChildFile ("PedalForge").getChildFile ("designs");
    
    auto file = designsDir.getChildFile (name.replace (" ", "_") + ".json");
    if (file.existsAsFile())
    {
        try {
            return std::make_shared<PedalDesign>(PedalDesign::loadFromFile(file));
        } catch (...) {}
    }
    
    file = designsDir.getChildFile (name + ".json");
    if (file.existsAsFile())
    {
        try {
            return std::make_shared<PedalDesign>(PedalDesign::loadFromFile(file));
        } catch (...) {}
    }
    
    for (const auto& f : designsDir.findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        try {
            auto d = PedalDesign::loadFromFile(f);
            if (d.name == name) return std::make_shared<PedalDesign>(d);
        } catch (...) {}
    }
    
    return nullptr;
}

inline std::vector<PedalInfo> getFactoryPedals()
{
    std::vector<PedalInfo> pedals = {
        // ─── TUTORIAL ──────────────────────────────────────────────────────
        { "Hello Gain",   "Tutorial",   1, 2, 1,
          juce::Colour (0xFF6EE7B7),    // mint green
          [] { return GraphPedalFactory::createTutorialHelloGain(); },
          [] { return loadDesignOrDefault("Hello Gain", FactoryDesigns::createTutorialHelloGain); } },

        { "Filter Sweep", "Tutorial",   1, 2, 2,
          juce::Colour (0xFF93C5FD),    // light blue
          [] { return GraphPedalFactory::createTutorialFilterSweep(); },
          [] { return loadDesignOrDefault("Filter Sweep", FactoryDesigns::createTutorialFilterSweep); } },

        { "Tremolo 101",  "Tutorial",   1, 2, 2,
          juce::Colour (0xFFFDA4AF),    // salmon pink
          [] { return GraphPedalFactory::createTutorialTremolo101(); },
          [] { return loadDesignOrDefault("Tremolo 101", FactoryDesigns::createTutorialTremolo101); } },

        { "Delay Lab",    "Tutorial",   1, 2, 3,
          juce::Colour (0xFF7DD3FC),    // sky blue
          [] { return GraphPedalFactory::createTutorialDelayLab(); },
          [] { return loadDesignOrDefault("Delay Lab", FactoryDesigns::createTutorialDelayLab); } },

        { "Mini Synth",   "Tutorial",   1, 2, 4,
          juce::Colour (0xFFC084FC),    // purple
          [] { return GraphPedalFactory::createTutorialMiniSynth(); },
          [] { return loadDesignOrDefault("Mini Synth", FactoryDesigns::createTutorialMiniSynth); } },

        { "Envelope Filter", "Tutorial", 1, 2, 3,
          juce::Colour (0xFFA7F3D0),    // pastel mint
          [] { return GraphPedalFactory::createTutorialEnvelopeFilter(); },
          [] { return loadDesignOrDefault("Envelope Filter", FactoryDesigns::createTutorialEnvelopeFilter); } },

        { "Step Sequencer Filter", "Tutorial", 1, 2, 2,
          juce::Colour (0xFF93C5FD),    // pastel blue
          [] { return GraphPedalFactory::createTutorialStepSequencer(); },
          [] { return loadDesignOrDefault("Step Sequencer Filter", FactoryDesigns::createTutorialStepSequencer); } },

        { "Pattern Slicer", "Tutorial", 1, 2, 3,
          juce::Colour (0xFFFDE047),    // pastel yellow
          [] { return GraphPedalFactory::createTutorialPatternSlicer(); },
          [] { return loadDesignOrDefault("Pattern Slicer", FactoryDesigns::createTutorialPatternSlicer); } },

        // ─── MIDI & CV ─────────────────────────────────────────────────────
        { "Step Sequencer", "MIDI & CV", 2, 2, 6,
          juce::Colour (0xFF1A0533),    // deep indigo
          [] { return GraphPedalFactory::createStepSequencer(); },
          [] { return loadDesignOrDefault("Step Sequencer", FactoryDesigns::createStepSequencer); } },

        // ─── DRIVE ─────────────────────────────────────────────────────────
        { "Clean Boost",  "Drive",      1, 2, 1,
          juce::Colour (0xFF4ADE80),    // green
          [] { return GraphPedalFactory::createCleanBoost(); },
          [] { return loadDesignOrDefault("Clean Boost", FactoryDesigns::createCleanBoost); } },

        { "Overdrive",    "Drive",      1, 2, 3,
          juce::Colour (0xFFFBBF24),    // yellow/orange
          [] { return GraphPedalFactory::createOverdrive(); },
          [] { return loadDesignOrDefault("Overdrive", FactoryDesigns::createOverdrive); } },

        { "Distortion",   "Drive",      1, 2, 3,
          juce::Colour (0xFFF97316),    // orange
          [] { return GraphPedalFactory::createDistortion(); },
          [] { return loadDesignOrDefault("Distortion", FactoryDesigns::createDistortion); } },

        { "Fuzz",         "Drive",      1, 2, 3,
          juce::Colour (0xFFDC2626),    // red
          [] { return GraphPedalFactory::createFuzz(); },
          [] { return loadDesignOrDefault("Fuzz", FactoryDesigns::createFuzz); } },

        // ─── MODULATION ────────────────────────────────────────────────────
        { "Chorus",       "Modulation", 1, 2, 4,
          juce::Colour (0xFFA78BFA),    // purple
          [] { return GraphPedalFactory::createChorus(); },
          [] { return loadDesignOrDefault("Chorus", FactoryDesigns::createChorus); } },

        { "Phaser",       "Modulation", 1, 2, 3,
          juce::Colour (0xFFD946EF),    // fuchsia
          [] { return GraphPedalFactory::createPhaser(); },
          [] { return loadDesignOrDefault("Phaser", FactoryDesigns::createPhaser); } },

        { "Flanger",      "Modulation", 1, 2, 4,
          juce::Colour (0xFFEC4899),    // pink
          [] { return GraphPedalFactory::createFlanger(); },
          [] { return loadDesignOrDefault("Flanger", FactoryDesigns::createFlanger); } },

        { "Tremolo",      "Modulation", 1, 2, 2,
          juce::Colour (0xFFFB7185),    // light pink
          [] { return GraphPedalFactory::createTremolo(); },
          [] { return loadDesignOrDefault("Tremolo", FactoryDesigns::createTremolo); } },

        // ─── TIME & DYNAMICS ───────────────────────────────────────────────
        { "Delay",        "Time",       1, 2, 3,
          juce::Colour (0xFF38BDF8),    // blue
          [] { return GraphPedalFactory::createDelay(); },
          [] { return loadDesignOrDefault("Delay", FactoryDesigns::createDelay); } },

        { "Reverb",       "Time",       1, 2, 3,
          juce::Colour (0xFF22D3EE),    // cyan
          [] { return GraphPedalFactory::createReverb(); },
          [] { return loadDesignOrDefault("Reverb", FactoryDesigns::createReverb); } },

        { "Compressor",   "Dynamics",   1, 2, 2,
          juce::Colour (0xFF34D399),    // emerald
          [] { return GraphPedalFactory::createCompressor(); },
          [] { return loadDesignOrDefault("Compressor", FactoryDesigns::createCompressor); } },

        { "Noise Gate",   "Dynamics",   1, 2, 1,
          juce::Colour (0xFF9CA3AF),    // gray
          [] { return GraphPedalFactory::createNoiseGate(); },
          [] { return loadDesignOrDefault("Noise Gate", FactoryDesigns::createNoiseGate); } },

        // ─── EQ / FILTER / UTILITY ─────────────────────────────────────────
        { "Parametric EQ","EQ",         2, 2, 9,
          juce::Colour (0xFF60A5FA),    // light blue
          [] { return GraphPedalFactory::createParametricEQ(); },
          [] { return loadDesignOrDefault("Parametric EQ", FactoryDesigns::createParametricEQ); } },

        { "Tone Control", "EQ",         1, 2, 1,
          juce::Colour (0xFF818CF8),    // indigo
          [] { return GraphPedalFactory::createToneControl(); },
          [] { return loadDesignOrDefault("Tone Control", FactoryDesigns::createToneControl); } },

        { "Cabinet Sim",  "Utility",    1, 2, 2,
          juce::Colour (0xFFFCD34D),    // amber
          [] { return GraphPedalFactory::createCabinetSim(); },
          [] { return loadDesignOrDefault("Cabinet Sim", FactoryDesigns::createCabinetSim); } },

        { "VST/AU Host",  "Utility",    1, 2, 0,
          juce::Colour (0xFF333333),    // dark gray
          [] { return GraphPedalFactory::createPluginHost(); },
          [] { return loadDesignOrDefault("VST/AU Host", FactoryDesigns::createPluginHost); } },

        { "NAM Amp",      "Amp Sim",    1, 2, 2,
          juce::Colour (0xFF333333),    // dark grey
          [] { return GraphPedalFactory::createNAMAmp(); },
          [] { return loadDesignOrDefault("NAM Amp", FactoryDesigns::createNAMAmp); } },

        { "IR Cabinet",   "Amp/Cab",    1, 2, 2,
          juce::Colour (0xff555555),
          [] { return GraphPedalFactory::createIRLoader(); },
          [] { return loadDesignOrDefault("IR Cabinet", FactoryDesigns::createIRLoader); } },

        { "IR Reverb",    "Reverb",     1, 2, 2,
          juce::Colour (0xff4a5d85),
          [] { return GraphPedalFactory::createIRReverb(); },
          [] { return loadDesignOrDefault("IR Reverb", FactoryDesigns::createIRReverb); } }
    };
    return pedals;
}
