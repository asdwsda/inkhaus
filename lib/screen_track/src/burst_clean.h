#pragma once
// Task 114: clean the screen after a burst of taps (factory-app style). Pure decision logic; the device code is
// in src/present.cpp. Once more than `n` fast partial refreshes happened since the last full refresh and the
// user has been idle for `idle_ms`, one full refresh of the whole frame runs. Any full refresh resets the count.
#include <stdint.h>

#include "tab_clean.h"

namespace screen_track {

// How the burst clean wipes: reuses task 099's plan (INIT clear first, then GC16 or GL16 full).
enum class BurstMode : uint8_t { GC16, GL16, InitGL16 };

constexpr TabCleanPlan burstPlan(BurstMode m) {
    return m == BurstMode::GL16 ? TabCleanPlan{false, FullWave::GL16}
         : m == BurstMode::InitGL16 ? TabCleanPlan{true, FullWave::GL16}
                                    : TabCleanPlan{false, FullWave::GC16};
}

inline const char* burstModeName(BurstMode m) {
    return m == BurstMode::GL16 ? "gl16" : m == BurstMode::InitGL16 ? "init+gl16" : "gc16";
}

// "gc16" | "gl16" | "init+gl16"; false (and `out` untouched) for anything else.
inline bool parseBurstMode(const char* s, BurstMode& out) {
    auto eq = [](const char* a, const char* b) {
        while (*a && *a == *b) { ++a; ++b; }
        return *a == *b;
    };
    if (eq(s, "gc16")) out = BurstMode::GC16;
    else if (eq(s, "gl16")) out = BurstMode::GL16;
    else if (eq(s, "init+gl16")) out = BurstMode::InitGL16;
    else return false;
    return true;
}

struct BurstConfig {
    int n = 4;                 // clean when MORE than n partials happened; negative = off
    uint32_t idle_ms = 2000;   // no user input for this long
    BurstMode mode = BurstMode::GC16;
};

// Where the app is: anything that must not be cleaned under.
struct BurstContext {
    bool popup = false;        // history / brightness / notifications popup
    bool settings = false;
    bool ota = false;
    bool tap_pending = false;  // a tap is armed / being deferred
    bool scheduled = false;    // scheduled wake session (nothing interactive)
};

class BurstTracker {
public:
    void onPartial() { ++partials_; }
    void onFull() { partials_ = 0; }  // any full refresh, whatever its reason
    int partials() const { return partials_; }
    // `idle_ms`: time since the last user input (the same clock as task 095).
    bool due(const BurstConfig& c, uint32_t idle_ms, const BurstContext& x) const {
        if (c.n < 0) return false;
        if (x.popup || x.settings || x.ota || x.tap_pending || x.scheduled) return false;
        return partials_ > c.n && idle_ms >= c.idle_ms;
    }
private:
    int partials_ = 0;
};

}  // namespace screen_track
