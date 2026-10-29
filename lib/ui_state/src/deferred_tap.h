#pragma once
// A tap on a dimmable light waits for the release (short tap = toggle) or the long press (opens the
// brightness popup); if neither arrives within kDeferredTapMs it counts as a tap (task 045). Pure logic.
#include <cstdint>

#include "ui_layout.h"

namespace uistate {

constexpr uint32_t kDeferredTapMs = 1500;

class DeferredTap {
 public:
  struct Result {
    enum Kind { None, Tap, LongPress } kind = None;
    ui::Point point;
  };
  void press(ui::Point p, uint32_t now) { armed_ = true; point_ = p; at_ = now; }
  Result release() { return fire(Result::Tap); }
  Result longPress() { return fire(Result::LongPress); }
  Result tick(uint32_t now) { return armed_ && now - at_ > kDeferredTapMs ? fire(Result::Tap) : Result(); }
  void cancel() { armed_ = false; }
  bool armed() const { return armed_; }

 private:
  Result fire(Result::Kind k) {
    Result r;
    if (!armed_) return r;
    armed_ = false;
    r.kind = k;
    r.point = point_;
    return r;
  }
  bool armed_ = false;
  ui::Point point_;
  uint32_t at_ = 0;
};

}  // namespace uistate
