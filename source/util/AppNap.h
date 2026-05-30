#pragma once

namespace pf
{
    /** Opt the process out of macOS "App Nap".
     *
     * App Nap throttles timers, lowers I/O priority, and coalesces work for
     * apps whose window isn't focused. For a real-time audio app that's wrong
     * twice over:
     *   - it can glitch/throttle audio when you tab away while playing, and
     *   - it throttles our automation bridge timer, which made headless
     *     testing flaky (some runs never picked up the queued command).
     *
     * Holds a process-wide NSProcessInfo activity for the app's lifetime.
     * No-op on non-Apple platforms. Safe to call more than once.
     */
    void disableAppNap();
}
