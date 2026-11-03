#pragma once
// Task 099: tab-change cleanup variants (pure selection logic; the device code is in src/present.cpp).
//   0 = current: GC16 full (white fill first only when the panel content is unknown)
//   1 = IT8951 INIT clear, then GC16 full
//   2 = IT8951 INIT clear, then GL16 full
//   3 = GL16 full only
#include <stdint.h>

namespace screen_track {

enum class FullWave : uint8_t { GC16, GL16 };

struct TabCleanPlan {
    bool init;      // wipe the panel with the INIT waveform (mode 0) first
    FullWave full;  // waveform of the full-screen refresh
};

constexpr int kTabCleanVariants = 4;

constexpr bool tabCleanValid(int v) { return v >= 0 && v < kTabCleanVariants; }

constexpr TabCleanPlan tabCleanPlan(int v) {
    return v == 1 ? TabCleanPlan{true, FullWave::GC16}
         : v == 2 ? TabCleanPlan{true, FullWave::GL16}
         : v == 3 ? TabCleanPlan{false, FullWave::GL16}
                  : TabCleanPlan{false, FullWave::GC16};  // 0 and anything invalid: current behavior
}

// The INIT/GL16 path only runs for a user-visible tab change: a full frame, the tab differs from the
// last shown one, a known previous tab, the panel content is known (no forced white fill) and the
// frame is not a popup close. Taps, popups and scheduled wakes never qualify.
constexpr bool isTabChange(bool fullFrame, bool popupClose, bool forced, int lastTab, int tab) {
    return fullFrame && !popupClose && !forced && lastTab >= 0 && tab >= 0 && tab != lastTab;
}

}  // namespace screen_track
