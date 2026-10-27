#include "crash_guard.h"

namespace screen_track {

CrashDecision crashGuard(const CrashState& prev, bool crash_reset) {
    CrashDecision d;
    d.next.magic = kCrashMagic;
    if (!crash_reset) {
        d.next.count = 0;
        return d;
    }
    uint32_t n = (prev.magic == kCrashMagic) ? prev.count : 0;
    if (n < kGiveUpAfter) ++n;  // saturate
    d.next.count = n;
    if (n >= kGiveUpAfter) d.mode = BootMode::GiveUp;
    else if (n >= kFallbackAfter) d.mode = BootMode::StockFallback;
    return d;
}

}  // namespace screen_track
