#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
/**
 * Central abstraction for PedalForge's data root directory.
 *
 * The data root is the parent directory holding every piece of user-
 * authored content: pedal designs, boards, presets, NAM models, IRs,
 * images, logs, autosave/recovery files. By default that's
 * `~/Library/Application Support/PedalForge/` on macOS (and the
 * equivalent on Windows/Linux), but the root is **overridable at
 * runtime** so we can:
 *
 *   - Run from a USB-stick snapshot without copying anything to the
 *     host computer's disk (see project_snapshots).
 *   - Autosave / recovery writes always go to the active root, so a
 *     mid-session crash while running off USB writes the recovery file
 *     to USB, not the host install (see task #52).
 *
 * Usage:
 *
 *     // Default — uses the OS app-support dir
 *     auto designs = pf::paths::getDesignsDir();
 *
 *     // Snapshot mount — rebase all reads/writes onto a USB folder
 *     pf::paths::setRootOverride (juce::File ("/Volumes/MyUSB/PedalForge"));
 *     // ... all subsequent getXxxDir() calls now resolve under that root.
 *
 *     pf::paths::clearRootOverride();   // back to default
 *
 * Thread safety: setRootOverride / clearRootOverride mutate an atomic
 * pointer; getters are wait-free. Callers shouldn't change the root
 * while the audio thread is mid-write, but day-to-day reads are safe
 * from any thread.
 */
namespace pf::paths
{
    /** Default app-support root, used when no override is active. */
    inline juce::File defaultRoot()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("PedalForge");
    }

    namespace detail
    {
        // We can't put a juce::File in an atomic, so we store the override
        // path as a juce::String guarded by a critical section. Reads are
        // cheap (one mutex acquisition + a String copy).
        inline juce::CriticalSection& mutex()
        {
            static juce::CriticalSection cs;
            return cs;
        }
        inline juce::String& overridePath()
        {
            static juce::String s;   // empty = no override
            return s;
        }
    }

    /** Set an override root. Pass an empty File to clear. The override
        is process-global and persists until cleared. */
    inline void setRootOverride (const juce::File& root)
    {
        const juce::ScopedLock sl (detail::mutex());
        detail::overridePath() = root.exists() || root.getFullPathName().isNotEmpty()
                                     ? root.getFullPathName() : juce::String();
    }

    inline void clearRootOverride()
    {
        const juce::ScopedLock sl (detail::mutex());
        detail::overridePath() = juce::String();
    }

    /** Current active root — override if set, else default. */
    inline juce::File getRoot()
    {
        juce::String p;
        { const juce::ScopedLock sl (detail::mutex());
          p = detail::overridePath(); }
        return p.isNotEmpty() ? juce::File (p) : defaultRoot();
    }

    inline bool isUsingOverride()
    {
        const juce::ScopedLock sl (detail::mutex());
        return detail::overridePath().isNotEmpty();
    }

    //==========================================================================
    // Subdirectories. All ensure their directory exists on first access.
    inline juce::File makeChild (const juce::String& name)
    {
        auto f = getRoot().getChildFile (name);
        f.createDirectory();
        return f;
    }

    inline juce::File getDesignsDir()      { return makeChild ("designs"); }
    inline juce::File getBoardsDir()       { return makeChild ("boards"); }
    inline juce::File getPlayPresetsDir()  { return makeChild ("playpresets"); }
    inline juce::File getLogsDir()         { return makeChild ("logs"); }
    inline juce::File getRecoveryDir()     { return makeChild ("recovery"); }
    inline juce::File getControllersDir()  { return makeChild ("controllers"); }
    inline juce::File getAutomationsDir()  { return makeChild ("automations"); }
    inline juce::File getSnapshotsDir()    { return makeChild ("snapshots"); }
    inline juce::File getPresetsDir()      { return makeChild ("Presets"); }

    // Scratch working directory for the spawned `claude` AI child process.
    // Lives under our own data root (which the app may read/write WITHOUT any
    // TCC prompt), so confining the child's cwd here keeps its startup
    // filesystem access out of the user's Documents/Desktop/Photos — those
    // accesses would otherwise be attributed to the PedalForge bundle and
    // pop scary permission prompts (task #66).
    inline juce::File getAiScratchDir()    { return makeChild ("ai_scratch"); }

    // Asset library subtree
    inline juce::File getLibraryDir()      { return makeChild ("Library"); }
    inline juce::File getImagesDir()       { auto d = getLibraryDir().getChildFile ("Images");  d.createDirectory(); return d; }
    inline juce::File getNamDir()          { auto d = getLibraryDir().getChildFile ("NAM");     d.createDirectory(); return d; }
    inline juce::File getIrDir()           { auto d = getLibraryDir().getChildFile ("IR");      d.createDirectory(); return d; }

    // Flat files in the root
    inline juce::File getSettingsFile()       { return getRoot().getChildFile ("settings.json"); }
    inline juce::File getFirstRunSentinel()   { return getRoot().getChildFile (".welcomed"); }

    //==========================================================================
    // Portable-path helpers. Store image / NAM / IR / etc. paths inside
    // designs as `$DATAROOT$/Library/Images/foo.png` instead of absolute
    // paths. Then the same design works under any data root — local
    // install, USB-mounted snapshot, another musician's machine, etc.

    inline constexpr const char* kPortableRootToken = "$DATAROOT$";

    /** If the given absolute path is INSIDE the active data root, return
        a portable version using `$DATAROOT$/...`. Paths outside the data
        root are returned unchanged (legacy external references still
        work, they just won't be portable across machines). Empty strings
        pass through. */
    inline juce::String normalizePath (const juce::String& absPath)
    {
        if (absPath.isEmpty() || absPath.startsWith (kPortableRootToken))
            return absPath;
        const auto root = getRoot().getFullPathName();
        if (root.isNotEmpty() && absPath.startsWith (root))
            return juce::String (kPortableRootToken) + absPath.substring (root.length());
        return absPath;
    }

    /** Reverse of normalizePath — turns a `$DATAROOT$/...` portable path
        into an absolute path using the currently-active data root. Paths
        that don't start with the token are returned unchanged. */
    inline juce::String expandPath (const juce::String& path)
    {
        if (path.startsWith (kPortableRootToken))
            return getRoot().getFullPathName() + path.substring ((int) std::strlen (kPortableRootToken));
        return path;
    }
}
