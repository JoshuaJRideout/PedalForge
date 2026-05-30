#include "SnapshotManager.h"
#include "AppPaths.h"

namespace pf::snapshot
{
//==============================================================================
namespace
{
    // Subdirectories of the data root that travel in a snapshot.
    // Logs and KnownPlugins.xml are explicitly excluded.
    juce::StringArray subdirsToInclude (const ExportOptions& opts)
    {
        juce::StringArray dirs;
        dirs.add ("designs");
        dirs.add ("boards");
        if (opts.includePresets)     dirs.add ("playpresets");
        if (opts.includeControllers) dirs.add ("controllers");
        if (opts.includeAutomations) dirs.add ("automations");
        if (opts.includeImages || opts.includeNAM || opts.includeIR)
            dirs.add ("Library");
        return dirs;
    }

    // Files that should NOT travel even if they live in an included dir.
    bool isExcludedFile (const juce::File& f)
    {
        auto name = f.getFileName();
        if (name == "settings.json")        return true;  // contains OAuth tokens
        if (name == "secrets.json")         return true;  // AI provider API keys — never travel
        if (name.startsWith ("."))           return true;  // hidden / sentinels
        if (name.endsWith   (".tmp"))        return true;  // half-written autosaves
        if (name == "KnownPlugins.xml")      return true;
        return false;
    }

    // Library/* subdirectories filtered by ExportOptions.
    bool shouldIncludeLibrarySub (const juce::String& subdirName, const ExportOptions& opts)
    {
        if (subdirName.equalsIgnoreCase ("Images")) return opts.includeImages;
        if (subdirName.equalsIgnoreCase ("NAM"))    return opts.includeNAM;
        if (subdirName.equalsIgnoreCase ("IR"))     return opts.includeIR;
        // Anything else (e.g. Pedals/) follows the includeImages flag as a safe default
        return opts.includeImages;
    }

    juce::var makeManifest (const ExportOptions& opts, int fileCount, juce::int64 totalBytes)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("schemaVersion", 1);
        obj->setProperty ("createdAt",     juce::Time::getCurrentTime().toISO8601 (true));
        obj->setProperty ("appVersion",    JucePlugin_VersionString);
        obj->setProperty ("note",          opts.authorNote);
        obj->setProperty ("filesIncluded", fileCount);
        obj->setProperty ("totalBytes",    (juce::int64) totalBytes);
        return juce::var (obj);
    }
}

//==============================================================================
ExportResult exportSnapshot (const juce::File& destFile, const ExportOptions& opts)
{
    ExportResult r;
    r.destFile = destFile;

    const auto root = pf::paths::getRoot();
    if (! root.isDirectory())
    {
        r.message = "Data root does not exist: " + root.getFullPathName();
        return r;
    }

    juce::ZipFile::Builder builder;
    auto addedCount = 0;
    juce::int64 totalBytes = 0;

    auto addFile = [&] (const juce::File& f, const juce::String& archivePath)
    {
        if (isExcludedFile (f)) return;
        // ZipFile::Builder takes ownership of the InputStream from the File.
        builder.addFile (f, /*compressionLevel*/ 6, archivePath);
        ++addedCount;
        totalBytes += f.getSize();
    };

    for (const auto& subdir : subdirsToInclude (opts))
    {
        auto dir = root.getChildFile (subdir);
        if (! dir.isDirectory()) continue;

        if (subdir == "Library")
        {
            // Walk each sub of Library and filter by opts
            for (const auto& sub : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findDirectories))
            {
                auto subName = sub.getFile().getFileName();
                if (! shouldIncludeLibrarySub (subName, opts)) continue;
                for (const auto& f : juce::RangedDirectoryIterator (sub.getFile(), true, "*", juce::File::findFiles))
                {
                    auto relPath = f.getFile().getRelativePathFrom (root);
                    addFile (f.getFile(), relPath);
                }
            }
        }
        else
        {
            for (const auto& f : juce::RangedDirectoryIterator (dir, true, "*", juce::File::findFiles))
            {
                auto relPath = f.getFile().getRelativePathFrom (root);
                addFile (f.getFile(), relPath);
            }
        }
    }

    // Write manifest.json. We append it last so its file count / total bytes
    // reflect everything else.
    auto manifestJson = juce::JSON::toString (makeManifest (opts, addedCount, totalBytes), true);
    auto tmpManifest = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("pf_snapshot_manifest_" + juce::Uuid().toString() + ".json");
    tmpManifest.replaceWithText (manifestJson);
    builder.addFile (tmpManifest, 6, "manifest.json");

    // Write the archive.
    destFile.deleteFile();
    juce::FileOutputStream out (destFile);
    if (out.failedToOpen())
    {
        tmpManifest.deleteFile();
        r.message = "Could not open output file: " + destFile.getFullPathName();
        return r;
    }
    double progress = 0.0;
    if (! builder.writeToStream (out, &progress))
    {
        tmpManifest.deleteFile();
        destFile.deleteFile();
        r.message = "Write failed (zip builder rejected stream)";
        return r;
    }
    tmpManifest.deleteFile();

    r.success = true;
    r.filesIncluded = addedCount;
    r.totalBytes = totalBytes;
    r.message = "Wrote " + juce::String (addedCount) + " files ("
                + juce::File::descriptionOfSizeInBytes (totalBytes) + ")";
    return r;
}

//==============================================================================
ImportResult importSnapshotMerge (const juce::File& srcFile, bool overwriteExisting)
{
    ImportResult r;

    if (! srcFile.existsAsFile())
    {
        r.message = "Snapshot file not found: " + srcFile.getFileName();
        return r;
    }

    juce::ZipFile zip (srcFile);
    if (zip.getNumEntries() == 0)
    {
        r.kind = ImportResult::FailedCorrupt;
        r.message = srcFile.getFileName() + ": empty or not a valid zip";
        return r;
    }

    // Minimum schema check — must contain a manifest.json with our schemaVersion.
    auto manifestIndex = -1;
    for (int i = 0; i < zip.getNumEntries(); ++i)
        if (zip.getEntry (i)->filename == "manifest.json") { manifestIndex = i; break; }

    if (manifestIndex < 0)
    {
        r.kind = ImportResult::FailedSchema;
        r.message = srcFile.getFileName() + ": not a PedalForge snapshot (no manifest.json)";
        return r;
    }

    {
        std::unique_ptr<juce::InputStream> ms (zip.createStreamForEntry (manifestIndex));
        if (ms == nullptr)
        {
            r.kind = ImportResult::FailedCorrupt;
            r.message = "Could not read manifest";
            return r;
        }
        auto json = juce::JSON::parse (ms->readEntireStreamAsString());
        if (! json.isObject())
        {
            r.kind = ImportResult::FailedSchema;
            r.message = "Manifest is not valid JSON";
            return r;
        }
        const int schema = (int) json.getProperty ("schemaVersion", 0);
        if (schema < 1 || schema > 1)
        {
            r.kind = ImportResult::FailedSchema;
            r.message = "Unsupported snapshot schema version: " + juce::String (schema)
                      + ". Update PedalForge to load this snapshot.";
            return r;
        }
    }

    // Extract everything into the active data root.
    const auto root = pf::paths::getRoot();
    root.createDirectory();

    int restored = 0;
    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        auto* entry = zip.getEntry (i);
        if (entry == nullptr) continue;
        if (entry->filename == "manifest.json") continue;          // handled above
        if (entry->filename.endsWithChar ('/')) continue;          // dirs only

        auto target = root.getChildFile (entry->filename);
        if (target.existsAsFile() && ! overwriteExisting) continue;
        target.getParentDirectory().createDirectory();

        auto result = zip.uncompressEntry (i, root, /*shouldOverwriteFiles*/ overwriteExisting);
        if (result.wasOk()) ++restored;
    }

    r.kind = ImportResult::Imported;
    r.filesRestored = restored;
    r.message = "Restored " + juce::String (restored) + " files from snapshot";
    return r;
}

} // namespace pf::snapshot
