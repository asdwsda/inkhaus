#pragma once
// Task 120: "ready to send" indicator state (docs/ui/experiments/ready-indicator.md). Pure logic: from the net/session
// facts it derives which status-bar state is shown, with the 400 ms coalescing of a short `ready`, and counts the
// status-region refreshes (budget). The device glue (src/app_frame.cpp) feeds it; nothing here touches hardware.
#include <stdint.h>

namespace app_logic {

// What the status bar shows beyond the classic phases. None = the classic status (scheduled wake, demo, steady state).
enum class ReadyShown : uint8_t { None, NotReady, Ready, Offline };

struct ReadyInputs {
  bool interactive = false;  // interactive wake (rocker/touch/boot); false on scheduled (RTC) wakes: draws nothing
  bool demo = false;         // demo mode: always ready, unchanged
  bool wifiUp = false;
  bool offline = false;      // session gave up (Wi-Fi/HA unreachable) or auth error: taps queue, not ready
  bool fetchDone = false;    // at least one fetch result arrived in this session
  int queuedTaps = 0;        // commands waiting in the session machine
};

struct ReadyView {
  ReadyShown shown = ReadyShown::None;
  bool queued = false;  // hourglass + `queued` label (only while not ready)
  bool operator==(const ReadyView &o) const { return shown == o.shown && queued == o.queued; }
  bool operator!=(const ReadyView &o) const { return !(*this == o); }
};

// Ready is held back this long: shorter than this it is not drawn (a fast network goes connecting -> updated).
constexpr uint32_t kReadyCoalesceMs = 400;

class ReadyTracker {
 public:
  // Call every loop pass with the current facts; returns the view to draw. `changes()` counts view changes (= status
  // region partial refreshes) since reset(); the wake itself (first view) counts as one.
  ReadyView step(uint32_t nowMs, const ReadyInputs &in);
  ReadyView view() const { return view_; }
  int changes() const { return changes_; }
  void reset() { *this = ReadyTracker(); }

 private:
  ReadyView view_;
  bool readySeen_ = false;  // Ready is wanted since readySince_
  uint32_t readySince_ = 0;
  bool wakeDone_ = false;   // after the first fetch the wake sequence is over: manual refresh shows `fetching states`
  int changes_ = 0;
};

// Task 120 ghost-debt rule (architect 2026-11-05): a refresh rect fully inside the status region counts toward ghost
// debt by area only, not as a feedback refresh (not in the partial count, not in the burst count).
struct IndicatorRect { int x, y, w, h; };
inline bool countsByAreaOnly(const IndicatorRect &r, const IndicatorRect &statusRegion) {
  return r.w > 0 && r.h > 0 && r.x >= statusRegion.x && r.y >= statusRegion.y && r.x + r.w <= statusRegion.x + statusRegion.w &&
         r.y + r.h <= statusRegion.y + statusRegion.h;
}

}  // namespace app_logic
