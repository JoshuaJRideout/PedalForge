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

    /** Stable identity for the inventory. Factory entries get an ID derived
        from their name; user-modified copies live in designs/ with their own
        PedalDesign::uuid and never collide with this. */
    juce::String factoryID() const
    {
        return "factory:" + name.replace (" ", "_").toLowerCase();
    }
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

/** Import a .pfpedal (or .json) file as a custom pedal. Copies it into the
    designs/ directory under the pedal's own name, with a numeric suffix if a
    pedal with that name already exists. Returns the target file on success,
    or an empty File() on parse/write failure.

    Pure utility — does not refresh any UI. Callers should refresh the
    inventory after a successful import. */
inline juce::File importPedalDesignFile (const juce::File& source)
{
    if (! source.existsAsFile()) return {};

    PedalDesign design;
    try { design = PedalDesign::loadFromFile (source); }
    catch (...) { return {}; }

    if (design.name.isEmpty()) return {};

    auto designsDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                          .getChildFile ("PedalForge").getChildFile ("designs");
    designsDir.createDirectory();

    // If a design with the same UUID is already installed, treat this import
    // as an update and overwrite it. Same identity = same pedal, even if the
    // user renamed it locally.
    if (design.uuid.isNotEmpty())
    {
        for (const auto& f : designsDir.findChildFiles (juce::File::findFiles, false, "*.json"))
        {
            try {
                if (PedalDesign::loadFromFile (f).uuid == design.uuid)
                {
                    source.copyFileTo (f);
                    return f;
                }
            } catch (...) {}
        }
    }

    // New design — pick a filename that doesn't collide. Filename uniqueness
    // is just hygiene; identity is the uuid inside the JSON.
    auto baseName = design.name.replace (" ", "_");
    auto target   = designsDir.getChildFile (baseName + ".json");

    int suffix = 2;
    while (target.existsAsFile())
    {
        target = designsDir.getChildFile (baseName + "_" + juce::String (suffix++) + ".json");
        if (suffix > 1000) return {};   // sanity
    }

    return source.copyFileTo (target) ? target : juce::File();
}

/** Look up a custom pedal design in the designs/ dir by its stable UUID.
    Returns nullptr if none match. Use this instead of name-based lookup so
    user designs whose displayName happens to collide with a factory pedal
    are still uniquely addressable. */
inline juce::File getBoardsDir()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("PedalForge").getChildFile ("boards");
    dir.createDirectory();
    return dir;
}

/** Copy a .pfboard file into ~/Library/PedalForge/boards/ for later loading
    via the Library tab. Returns the destination on success.
    Filename is uniquified if a board with the same name already lives there;
    no UUID dedup yet because BoardConfig doesn't carry one (TODO). */
inline juce::File importBoardFile (const juce::File& source)
{
    if (! source.existsAsFile()) return {};

    auto boardsDir = getBoardsDir();
    auto baseName  = source.getFileNameWithoutExtension();
    auto target    = boardsDir.getChildFile (baseName + ".pfboard");

    int suffix = 2;
    while (target.existsAsFile())
    {
        target = boardsDir.getChildFile (baseName + "_" + juce::String (suffix++) + ".pfboard");
        if (suffix > 1000) return {};
    }
    return source.copyFileTo (target) ? target : juce::File();
}

inline std::shared_ptr<PedalDesign> loadCustomPedalDesignByUuid (const juce::String& uuid)
{
    if (uuid.isEmpty()) return nullptr;

    auto designsDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                          .getChildFile ("PedalForge").getChildFile ("designs");
    if (! designsDir.isDirectory()) return nullptr;

    for (const auto& f : designsDir.findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        try {
            auto d = PedalDesign::loadFromFile (f);
            if (d.uuid == uuid) return std::make_shared<PedalDesign> (d);
        } catch (...) {}
    }
    return nullptr;
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

        { "Wave Folder", "Tutorial", 1, 2, 3,
          juce::Colour (0xFFC084FC),    // pastel purple
          [] { return GraphPedalFactory::createTutorialWaveFolder(); },
          [] { return loadDesignOrDefault("Wave Folder", FactoryDesigns::createTutorialWaveFolder); } },

        // ─── MIDI & CV ─────────────────────────────────────────────────────
        { "Step Sequencer", "MIDI & CV", 2, 2, 6,
          juce::Colour (0xFF1A0533),    // deep indigo
          [] { return GraphPedalFactory::createStepSequencer(); },
          [] { return loadDesignOrDefault("Step Sequencer", FactoryDesigns::createStepSequencer); } },

        { "MIDI Editor", "MIDI & CV", 2, 2, 3,
          juce::Colour (0xFF1E1B4B),    // deep dark indigo
          [] { return GraphPedalFactory::createMidiEditor(); },
          [] { return loadDesignOrDefault("MIDI Editor", FactoryDesigns::createMidiEditor); } },

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

        { "Mixer",        "Utility",    1, 2, 3,
          juce::Colour (0xFF4F46E5),    // deep premium indigo
          [] { return GraphPedalFactory::createMixerPedal(); },
          [] { return loadDesignOrDefault("Mixer", FactoryDesigns::createMixerPedal); } },

        { "Matrix Mixer", "Utility",    2, 2, 16,
          juce::Colour (0xFF1E293B),    // dark graphite
          [] { return GraphPedalFactory::createMatrixMixerPedal(); },
          [] { return loadDesignOrDefault("Matrix Mixer", FactoryDesigns::createMatrixMixerPedal); } },

        { "Matrix Mixer XL", "Utility", 1, 2, 2,
          juce::Colour (0xFF1E1B4B),    // Dark indigo/purple
          [] { return GraphPedalFactory::createMatrixMixerXLPedal(); },
          [] { return loadDesignOrDefault("Matrix Mixer XL", FactoryDesigns::createMatrixMixerXLPedal); } },

        { "NAM Amp",      "Amp Sim",    1, 2, 2,
          juce::Colour (0xFF333333),    // dark grey
          [] { return GraphPedalFactory::createNAMAmp(); },
          [] { return loadDesignOrDefault("NAM Amp", FactoryDesigns::createNAMAmp); } },

        { "Aether Rig",   "Amp Sim",    2, 2, 11,
          juce::Colour (0xFF1A1A2E),    // deep midnight blue
          [] { return GraphPedalFactory::createAetherRig(); },
          [] { return loadDesignOrDefault("Aether Rig", FactoryDesigns::createAetherRig); } },

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
