#include "brightness_popup.h"

#include <cmath>

namespace uistate {

namespace {
int clampPct(int p) { return p < 0 ? 0 : p > 100 ? 100 : p; }
}  // namespace

int brightnessToPct(int ha255) {
  if (ha255 <= 0) return 0;
  return clampPct(int(std::lround(ha255 * 100.0 / 255.0)));
}

void BrightnessPopup::open(const std::string &entityId, int tab, int widget, int initialPct, uint32_t now) {
  id_ = entityId;
  tab_ = tab;
  widget_ = widget;
  value_ = clampPct(initialPct);
  open_ = true;
  dirty_ = false;
  unconfirmed_ = false;
  lastInput_ = now;
}

bool BrightnessPopup::setPct(int pct, uint32_t now) {
  if (!open_) return false;
  touchInput(now);
  pct = clampPct(pct);
  if (pct == value_) return false;
  value_ = pct;
  dirty_ = true;
  return true;
}

bool BrightnessPopup::step(int direction, uint32_t now) {
  if (!open_) return false;
  // Steps land on multiples of 10 (e.g. 37 -> 40 up, 30 down).
  int v = value_;
  if (direction > 0) v = (v / kBrightnessStep + 1) * kBrightnessStep;
  else if (direction < 0) v = ((v + kBrightnessStep - 1) / kBrightnessStep - 1) * kBrightnessStep;
  return setPct(v, now);
}

bool BrightnessPopup::preset(int index, uint32_t now) {
  if (index < 0 || index > 3) return false;
  return setPct(kBrightnessPresets[index], now);
}

bool BrightnessPopup::tapBar(double fraction, uint32_t now) {
  if (fraction < 0) fraction = 0;
  if (fraction > 1) fraction = 1;
  return setPct(int(std::lround(fraction * 100.0)), now);
}

void BrightnessPopup::flush() {
  if (!dirty_) return;
  dirty_ = false;
  unconfirmed_ = true;
  if (sink_) sink_(id_, value_);
}

void BrightnessPopup::done(uint32_t now) {
  if (!open_) return;
  (void)now;
  flush();
  open_ = false;
}

bool BrightnessPopup::tick(uint32_t now) {
  if (!open_) return false;
  if (dirty_ && now - lastInput_ >= kSendDebounceMs) flush();
  if (now - lastInput_ >= kIdleCloseMs) {
    flush();
    open_ = false;
    return true;
  }
  return false;
}

}  // namespace uistate
