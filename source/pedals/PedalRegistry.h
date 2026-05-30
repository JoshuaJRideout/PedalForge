#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/GraphPedalProcessor.h"
#include "../util/AppPaths.h"
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
    auto dir = pf::paths::getLibraryDir().getChildFile ("Pedals");
    dir.createDirectory();
    return dir;
}

inline std::shared_ptr<PedalDesign> loadDesignOrDefault(const juce::String& name, std::function<std::shared_ptr<PedalDesign>()> factory);

/** Build a pedal's processor straight from its design's DECLARED effectsGraph —
    the honest path (no hidden C++ GraphPedalFactory graph). Use this as the
    processor factory for any pedal whose design carries a complete graph. */
inline std::unique_ptr<juce::AudioProcessor> processorFromDeclaredGraph (
    const juce::String& name, std::function<std::shared_ptr<PedalDesign>()> designFactory)
{
    auto d = loadDesignOrDefault (name, designFactory);
    return std::make_unique<GraphPedalProcessor> (d->name, juce::JSON::toString (d->effectsGraph));
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
/** Outcome of an import operation. The destFile is non-empty when the
    import succeeded (Created or Updated). The message field carries a
    human-readable description for the failure cases. */
struct ImportResult
{
    enum Kind { Created, Updated, FailedCorrupt, FailedSchema, FailedIO };
    Kind kind = FailedIO;
    juce::File destFile;
    juce::String message;
    juce::String displayName;     // populated for any non-FailedIO case
};

inline ImportResult importPedalDesignFileEx (const juce::File& source)
{
    ImportResult r;
    if (! source.existsAsFile())
    {
        r.kind = ImportResult::FailedIO;
        r.message = "File not found: " + source.getFileName();
        return r;
    }

    PedalDesign design;
    try { design = PedalDesign::loadFromFile (source); }
    catch (...)
    {
        r.kind = ImportResult::FailedCorrupt;
        r.message = source.getFileName() + ": corrupted or unreadable JSON";
        return r;
    }

    // Minimum schema check: must have a name AND either UI controls or
    // scripts that do something. A bare file with just an empty design
    // is almost certainly accidental.
    if (design.name.isEmpty()
        || (design.controls.empty() && design.scripts.empty()
            && design.effectsGraph.isVoid()))
    {
        r.kind = ImportResult::FailedSchema;
        r.message = source.getFileName() + ": missing name or empty design";
        return r;
    }
    r.displayName = design.name;

    auto designsDir = pf::paths::getDesignsDir();

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
                    if (source.copyFileTo (f))
                    {
                        r.kind = ImportResult::Updated;
                        r.destFile = f;
                    }
                    else
                    {
                        r.kind = ImportResult::FailedIO;
                        r.message = "Could not overwrite " + f.getFileName();
                    }
                    return r;
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
        if (suffix > 1000)
        {
            r.kind = ImportResult::FailedIO;
            r.message = "Could not pick a non-colliding filename";
            return r;
        }
    }

    if (source.copyFileTo (target))
    {
        r.kind = ImportResult::Created;
        r.destFile = target;
    }
    else
    {
        r.kind = ImportResult::FailedIO;
        r.message = "Could not write " + target.getFileName();
    }
    return r;
}

/** Back-compat wrapper. Returns the dest file (empty on failure) — matches
    the original signature so existing callers don't need to change yet. */
inline juce::File importPedalDesignFile (const juce::File& source)
{
    return importPedalDesignFileEx (source).destFile;
}

/** Look up a custom pedal design in the designs/ dir by its stable UUID.
    Returns nullptr if none match. Use this instead of name-based lookup so
    user designs whose displayName happens to collide with a factory pedal
    are still uniquely addressable. */
inline juce::File getBoardsDir()
{
    return pf::paths::getBoardsDir();
}

/** Copy a .pfboard file into ~/Library/PedalForge/boards/. Returns the
    destination on success.

    Dedup: if the incoming file carries a pfboardUuid that matches an existing
    .pfboard already in boards/, overwrite that file in place — re-import of
    the same rig should not produce MyRig_2, MyRig_3, etc. Otherwise the
    filename is uniquified for hygiene only. */
inline ImportResult importBoardFileEx (const juce::File& source)
{
    ImportResult r;
    if (! source.existsAsFile())
    {
        r.kind = ImportResult::FailedIO;
        r.message = "File not found: " + source.getFileName();
        return r;
    }

    auto boardsDir   = getBoardsDir();
    auto incomingTxt = source.loadFileAsString();
    auto incomingVar = juce::JSON::parse (incomingTxt);
    if (! incomingVar.isObject())
    {
        r.kind = ImportResult::FailedCorrupt;
        r.message = source.getFileName() + ": not valid JSON";
        return r;
    }

    // Minimum schema check for a board file: must look like a pedal board
    // (some recognisable top-level keys).
    if (! incomingVar.hasProperty ("pfboardUuid")
        && ! incomingVar.hasProperty ("boards")
        && ! incomingVar.hasProperty ("config"))
    {
        r.kind = ImportResult::FailedSchema;
        r.message = source.getFileName() + ": not a recognisable .pfboard";
        return r;
    }

    auto incomingUuid = incomingVar.getProperty ("pfboardUuid", "").toString();
    r.displayName = incomingVar.getProperty ("name", source.getFileNameWithoutExtension()).toString();

    if (incomingUuid.isNotEmpty())
    {
        for (const auto& f : boardsDir.findChildFiles (juce::File::findFiles, false, "*.pfboard"))
        {
            auto existingUuid = juce::JSON::parse (f.loadFileAsString())
                                    .getProperty ("pfboardUuid", "").toString();
            if (existingUuid == incomingUuid)
            {
                if (source.copyFileTo (f))
                {
                    r.kind = ImportResult::Updated;
                    r.destFile = f;
                }
                else
                {
                    r.kind = ImportResult::FailedIO;
                    r.message = "Could not overwrite " + f.getFileName();
                }
                return r;
            }
        }
    }

    // New rig — pick a non-colliding filename.
    auto baseName = source.getFileNameWithoutExtension();
    auto target   = boardsDir.getChildFile (baseName + ".pfboard");

    int suffix = 2;
    while (target.existsAsFile())
    {
        target = boardsDir.getChildFile (baseName + "_" + juce::String (suffix++) + ".pfboard");
        if (suffix > 1000)
        {
            r.kind = ImportResult::FailedIO;
            r.message = "Could not pick a non-colliding filename";
            return r;
        }
    }

    if (source.copyFileTo (target))
    {
        r.kind = ImportResult::Created;
        r.destFile = target;
    }
    else
    {
        r.kind = ImportResult::FailedIO;
        r.message = "Could not write " + target.getFileName();
    }
    return r;
}

/** Back-compat wrapper. */
inline juce::File importBoardFile (const juce::File& source)
{
    return importBoardFileEx (source).destFile;
}

inline std::shared_ptr<PedalDesign> loadCustomPedalDesignByUuid (const juce::String& uuid)
{
    if (uuid.isEmpty()) return nullptr;

    auto designsDir = pf::paths::getDesignsDir();
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
    auto designsDir = pf::paths::getDesignsDir();
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
        // (Hello Gain / Tremolo 101 / Delay Lab removed — they duplicated the
        //  Clean Boost / Tremolo / Delay basics, which now carry teaching notes.)
        { "Filter Sweep", "Tutorial",   1, 2, 2,
          juce::Colour (0xFF93C5FD),    // light blue
          [] { return GraphPedalFactory::createTutorialFilterSweep(); },
          [] { return loadDesignOrDefault("Filter Sweep", FactoryDesigns::createTutorialFilterSweep); } },

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
          [] { return processorFromDeclaredGraph ("Step Sequencer", FactoryDesigns::createStepSequencer); },
          [] { return loadDesignOrDefault("Step Sequencer", FactoryDesigns::createStepSequencer); } },

        { "MIDI Editor", "MIDI & CV", 2, 2, 3,
          juce::Colour (0xFF1E1B4B),    // deep dark indigo
          [] { return processorFromDeclaredGraph ("MIDI Editor", FactoryDesigns::createMidiEditor); },
          [] { return loadDesignOrDefault("MIDI Editor", FactoryDesigns::createMidiEditor); } },

        // ─── DRIVE ─────────────────────────────────────────────────────────
        { "Clean Boost",  "Drive",      1, 2, 1,
          juce::Colour (0xFF4ADE80),    // green
          // Honest build: the processor IS the design's declared effectsGraph
          // (no hidden C++ graph). See FactoryDesigns::createCleanBoost.
          [] { return processorFromDeclaredGraph ("Clean Boost", FactoryDesigns::createCleanBoost); },
          [] { return loadDesignOrDefault("Clean Boost", FactoryDesigns::createCleanBoost); } },

        { "Overdrive",    "Drive",      1, 2, 3,
          juce::Colour (0xFFFBBF24),    // yellow/orange
          [] { return processorFromDeclaredGraph ("Overdrive", FactoryDesigns::createOverdrive); },
          [] { return loadDesignOrDefault("Overdrive", FactoryDesigns::createOverdrive); } },

        { "Distortion",   "Drive",      1, 2, 3,
          juce::Colour (0xFFF97316),    // orange
          [] { return processorFromDeclaredGraph ("Distortion", FactoryDesigns::createDistortion); },
          [] { return loadDesignOrDefault("Distortion", FactoryDesigns::createDistortion); } },

        { "Fuzz",         "Drive",      1, 2, 3,
          juce::Colour (0xFFDC2626),    // red
          [] { return processorFromDeclaredGraph ("Fuzz", FactoryDesigns::createFuzz); },
          [] { return loadDesignOrDefault("Fuzz", FactoryDesigns::createFuzz); } },

        // ─── MODULATION ────────────────────────────────────────────────────
        { "Chorus",       "Modulation", 1, 2, 4,
          juce::Colour (0xFFA78BFA),    // purple
          [] { return processorFromDeclaredGraph ("Chorus", FactoryDesigns::createChorus); },
          [] { return loadDesignOrDefault("Chorus", FactoryDesigns::createChorus); } },

        { "Phaser",       "Modulation", 1, 2, 3,
          juce::Colour (0xFFD946EF),    // fuchsia
          [] { return processorFromDeclaredGraph ("Phaser", FactoryDesigns::createPhaser); },
          [] { return loadDesignOrDefault("Phaser", FactoryDesigns::createPhaser); } },

        { "Flanger",      "Modulation", 1, 2, 4,
          juce::Colour (0xFFEC4899),    // pink
          [] { return processorFromDeclaredGraph ("Flanger", FactoryDesigns::createFlanger); },
          [] { return loadDesignOrDefault("Flanger", FactoryDesigns::createFlanger); } },

        { "Tremolo",      "Modulation", 1, 2, 2,
          juce::Colour (0xFFFB7185),    // light pink
          [] { return processorFromDeclaredGraph ("Tremolo", FactoryDesigns::createTremolo); },
          [] { return loadDesignOrDefault("Tremolo", FactoryDesigns::createTremolo); } },

        // ─── TIME & DYNAMICS ───────────────────────────────────────────────
        { "Delay",        "Time",       1, 2, 3,
          juce::Colour (0xFF38BDF8),    // blue
          [] { return processorFromDeclaredGraph ("Delay", FactoryDesigns::createDelay); },
          [] { return loadDesignOrDefault("Delay", FactoryDesigns::createDelay); } },

        { "Reverb",       "Time",       1, 2, 3,
          juce::Colour (0xFF22D3EE),    // cyan
          [] { return processorFromDeclaredGraph ("Reverb", FactoryDesigns::createReverb); },
          [] { return loadDesignOrDefault("Reverb", FactoryDesigns::createReverb); } },

        { "Compressor",   "Dynamics",   1, 2, 2,
          juce::Colour (0xFF34D399),    // emerald
          [] { return processorFromDeclaredGraph ("Compressor", FactoryDesigns::createCompressor); },
          [] { return loadDesignOrDefault("Compressor", FactoryDesigns::createCompressor); } },

        { "Noise Gate",   "Dynamics",   1, 2, 1,
          juce::Colour (0xFF9CA3AF),    // gray
          [] { return processorFromDeclaredGraph ("Noise Gate", FactoryDesigns::createNoiseGate); },
          [] { return loadDesignOrDefault("Noise Gate", FactoryDesigns::createNoiseGate); } },

        // ─── EQ / FILTER / UTILITY ─────────────────────────────────────────
        { "Parametric EQ","EQ",         2, 2, 9,
          juce::Colour (0xFF60A5FA),    // light blue
          [] { return processorFromDeclaredGraph ("Parametric EQ", FactoryDesigns::createParametricEQ); },
          [] { return loadDesignOrDefault("Parametric EQ", FactoryDesigns::createParametricEQ); } },

        { "Tone Control", "EQ",         1, 2, 1,
          juce::Colour (0xFF818CF8),    // indigo
          [] { return processorFromDeclaredGraph ("Tone Control", FactoryDesigns::createToneControl); },
          [] { return loadDesignOrDefault("Tone Control", FactoryDesigns::createToneControl); } },

        { "Cabinet Sim",  "Utility",    1, 2, 2,
          juce::Colour (0xFFFCD34D),    // amber
          [] { return processorFromDeclaredGraph ("Cabinet Sim", FactoryDesigns::createCabinetSim); },
          [] { return loadDesignOrDefault("Cabinet Sim", FactoryDesigns::createCabinetSim); } },

        { "VST/AU Host",  "Utility",    1, 2, 0,
          juce::Colour (0xFF333333),    // dark gray
          [] { return processorFromDeclaredGraph ("VST/AU Host", FactoryDesigns::createPluginHost); },
          [] { return loadDesignOrDefault("VST/AU Host", FactoryDesigns::createPluginHost); } },

        { "Mixer",        "Utility",    1, 2, 3,
          juce::Colour (0xFF4F46E5),    // deep premium indigo
          [] { return processorFromDeclaredGraph ("Mixer", FactoryDesigns::createMixerPedal); },
          [] { return loadDesignOrDefault("Mixer", FactoryDesigns::createMixerPedal); } },

        { "Matrix Mixer", "Utility",    2, 2, 16,
          juce::Colour (0xFF1E293B),    // dark graphite
          [] { return processorFromDeclaredGraph ("Matrix Mixer", FactoryDesigns::createMatrixMixerPedal); },
          [] { return loadDesignOrDefault("Matrix Mixer", FactoryDesigns::createMatrixMixerPedal); } },

        { "Matrix Mixer XL", "Utility", 1, 2, 2,
          juce::Colour (0xFF1E1B4B),    // Dark indigo/purple
          [] { return processorFromDeclaredGraph ("Matrix Mixer XL", FactoryDesigns::createMatrixMixerXLPedal); },
          [] { return loadDesignOrDefault("Matrix Mixer XL", FactoryDesigns::createMatrixMixerXLPedal); } },

        { "NAM Amp",      "Amp Sim",    1, 2, 2,
          juce::Colour (0xFF333333),    // dark grey
          [] { return processorFromDeclaredGraph ("NAM Amp", FactoryDesigns::createNAMAmp); },
          [] { return loadDesignOrDefault("NAM Amp", FactoryDesigns::createNAMAmp); } },

        { "Aether Rig",   "Amp Sim",    2, 2, 11,
          juce::Colour (0xFF1A1A2E),    // deep midnight blue
          [] { return processorFromDeclaredGraph ("Aether Rig", FactoryDesigns::createAetherRig); },
          [] { return loadDesignOrDefault("Aether Rig", FactoryDesigns::createAetherRig); } },

        { "IR Cabinet",   "Amp/Cab",    1, 2, 2,
          juce::Colour (0xff555555),
          [] { return processorFromDeclaredGraph ("IR Cabinet", FactoryDesigns::createIRLoader); },
          [] { return loadDesignOrDefault("IR Cabinet", FactoryDesigns::createIRLoader); } },

        { "IR Reverb",    "Reverb",     1, 2, 2,
          juce::Colour (0xff4a5d85),
          [] { return processorFromDeclaredGraph ("IR Reverb", FactoryDesigns::createIRReverb); },
          [] { return loadDesignOrDefault("IR Reverb", FactoryDesigns::createIRReverb); } }
    };
    return pedals;
}
