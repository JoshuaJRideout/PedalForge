#include "AppNap.h"

#if defined(__APPLE__)
#import <Foundation/Foundation.h>

namespace pf
{
    void disableAppNap()
    {
        static id<NSObject> token = nil;
        if (token != nil) return;   // already held

        // UserInitiated => the system treats us as doing real work, so we are
        // never App-Napped. AllowingIdleSystemSleep keeps normal display/system
        // sleep behaviour (we only want to stop the per-app throttling, not
        // pin the whole machine awake). LatencyCritical stops timer coalescing,
        // which matters for both audio and the 3 Hz automation-bridge poll.
        NSActivityOptions opts = (NSActivityOptions)
            (NSActivityUserInitiatedAllowingIdleSystemSleep | NSActivityLatencyCritical);

        token = [[NSProcessInfo processInfo] beginActivityWithOptions: opts
                                                               reason: @"PedalForge real-time audio + automation bridge"];
        [token retain];   // JUCE compiles .mm without ARC — hold it for the app lifetime
    }
}
#else
namespace pf { void disableAppNap() {} }
#endif
