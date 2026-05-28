#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
/**
 * Snapshot import/export (task #62).
 *
 * A `.pfsnapshot` is a zip archive that contains the entire active data
 * root: designs/, boards/, playpresets/, Library/, controllers/, etc. —
 * everything except secrets (Tone3000 OAuth tokens) and logs.
 *
 * Path portability is already handled at the PedalDesign serialization
 * layer via pf::paths::normalizePath — designs reference assets via the
 * `$DATAROOT$/...` placeholder which expands against the active root at
 * load time. That makes snapshots automatically work on any other Mac
 * running PedalForge.
 *
 * v1 supports export + import-merge. Mount-from-USB and replace-mode
 * are queued for v2 (need a small data-root override UX + confirm flow).
 */
namespace pf::snapshot
{
    struct ExportOptions
    {
        bool includeImages = true;
        bool includeNAM    = true;   // can be huge — turn off for "compact" mode
        bool includeIR     = true;
        bool includePresets = true;
        bool includeControllers = true;
        bool includeAutomations = true;
        juce::String authorNote;     // optional, embedded in manifest.json
    };

    struct ExportResult
    {
        bool success = false;
        juce::File destFile;
        juce::String message;
        int filesIncluded = 0;
        juce::int64 totalBytes = 0;
    };

    /** Write a snapshot of the active data root to destFile. */
    ExportResult exportSnapshot (const juce::File& destFile, const ExportOptions& opts = {});

    struct ImportResult
    {
        enum Kind { Imported, FailedCorrupt, FailedSchema, FailedIO };
        Kind kind = FailedIO;
        juce::String message;
        int filesRestored = 0;
    };

    /** Merge a snapshot into the active data root. Files in the snapshot
        that already exist locally are skipped (UUID-based dedup for
        designs/boards, filename for assets) unless `overwriteExisting`
        is true. */
    ImportResult importSnapshotMerge (const juce::File& srcFile,
                                       bool overwriteExisting = false);
}
