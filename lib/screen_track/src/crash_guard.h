#pragma once
// Decides what to do after a watchdog reset or panic, so a hang or crash in setup can never turn
// into an endless reset loop. The state is kept in RTC memory that survives these resets (the
// caller owns it); a power-on or an external reset clears it.
#include <cstdint>

namespace screen_track {

constexpr uint32_t kCrashMagic = 0xC0A5B007u;
constexpr uint32_t kFallbackAfter = 2;  // consecutive crash resets before the stock start-up
constexpr uint32_t kGiveUpAfter = 4;    // consecutive crash resets before powering off

enum class BootMode {
    Normal,
    StockFallback,  // stock display start-up, full redraw
    GiveUp,         // even the fallback keeps crashing: power off instead of looping
};

struct CrashState {
    uint32_t magic = 0;  // kCrashMagic when `count` is meaningful (RTC memory is garbage at power-on)
    uint32_t count = 0;  // consecutive crash resets
};

struct CrashDecision {
    CrashState next;  // store this back
    BootMode mode = BootMode::Normal;
};

// `crash_reset`: the reset reason was a task/interrupt/RTC watchdog or a panic.
CrashDecision crashGuard(const CrashState& prev, bool crash_reset);

}  // namespace screen_track
