#pragma once
// Task 127 (docs/ui/wake-updating.md, variant B): after an interactive wake every state widget shows a greyed
// "not fresh yet" look until the first fetch result arrives (or the session gives up). Pure logic: from the same
// facts as the ready indicator it derives the `stale` flag and counts the refreshes the look costs (budget).
// The device glue (src/app_frame.cpp) feeds it; nothing here touches hardware.
#include <stdint.h>

#include "ready_state.h"

namespace app_logic {

enum class StaleEnd : uint8_t { None, Fresh, Timeout };

class WakeUpdating {
 public:
  // Call every loop pass with the facts of the ready indicator. Returns the stale flag to draw.
  // Starts at most once per session: on the first step of an interactive, non-demo wake that is not already offline.
  // Ends (for good) with the first fetch result (Fresh) or when the session gives up (Timeout).
  // fetchFailed: the first fetch result was a failure; that ends the look as Timeout (not Fresh).
  bool step(const ReadyInputs &in, bool fetchFailed = false);
  bool stale() const { return stale_; }
  StaleEnd ended() const { return end_; }
  // Refreshes caused by the look: 1 when it started (one partial for the union rect, or part of the first full draw),
  // +1 when it ended (one refresh). Never more than 2 per session, however many widgets trickle in.
  int lookRefreshes() const { return refreshes_; }
  // A widget tapped while updating leaves the grey look for the rest of the wake (no re-grey after its confirm).
  void exclude(int tab, int widget);
  bool excluded(int tab, int widget) const;
  void reset() { *this = WakeUpdating(); }

 private:
  bool started_ = false;
  bool stale_ = false;
  StaleEnd end_ = StaleEnd::None;
  int refreshes_ = 0;
  static constexpr int kMaxExcluded = 16;
  int exTab_[kMaxExcluded] = {};
  int exWidget_[kMaxExcluded] = {};
  int exCount_ = 0;
};

// The end refresh: when more than this many entity values changed at once the end refresh is a GC16 full
// (STYLE.md 10) instead of one partial. Removing the stale look itself is not a changed widget.
constexpr int kStaleEndFullChanged = 6;
inline bool staleEndIsFull(int changedValues) { return changedValues > kStaleEndFullChanged; }

// Grey of a pixel under the stale look: black becomes OFF grey (#888), white stays white, greys (anti-aliasing) map
// linearly in between. Used by the renderer on the 8-bit canvas.
constexpr uint8_t kStaleGrey = 0x88;
inline uint8_t staleGrey(uint8_t v) {
  return uint8_t(255 - ((255 - int(v)) * (255 - int(kStaleGrey))) / 255);
}

}  // namespace app_logic
