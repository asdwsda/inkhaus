#include "media_popup.h"

#include <cmath>

namespace uistate {

void MediaPopup::open(const std::string &entityId, int tab, int widget, int volumePct, bool muted, uint32_t now) {
  open_ = true;
  dirty_ = unconfirmed_ = muteUnconfirmed_ = false;
  id_ = entityId;
  tab_ = tab;
  widget_ = widget;
  value_ = volumePct < 0 ? 0 : volumePct;
  mutedValue_ = muted;
  lastInput_ = lastStep_ = now;
}

bool MediaPopup::setPct(int pct, uint32_t now) {
  if (!open_) return false;
  pct = pct < 0 ? 0 : pct > 100 ? 100 : pct;
  lastInput_ = now;
  if (pct == value_) return false;
  value_ = pct;
  dirty_ = true;
  lastStep_ = now;
  return true;
}

bool MediaPopup::step(int direction, uint32_t now) {
  if (!open_) return false;
  // Align to the 5 % grid, so 47 % + 1 step is 50 %, not 52 %.
  int target = value_ + direction * kMediaVolumeStep;
  if (value_ % kMediaVolumeStep != 0)
    target = direction > 0 ? (value_ / kMediaVolumeStep + 1) * kMediaVolumeStep : (value_ / kMediaVolumeStep) * kMediaVolumeStep;
  return setPct(target, now);
}

bool MediaPopup::tapBar(double fraction, uint32_t now) { return setPct(int(std::lround(fraction * 100.0)), now); }

bool MediaPopup::toggleMute(uint32_t now) {
  if (!open_) return false;
  lastInput_ = now;
  mutedValue_ = !mutedValue_;
  muteUnconfirmed_ = true;
  if (sink_) sink_(id_, MediaSend::Mute, mutedValue_ ? 1 : 0);
  return true;
}

void MediaPopup::flush() {
  if (!dirty_) return;
  dirty_ = false;
  unconfirmed_ = true;
  if (sink_) sink_(id_, MediaSend::Volume, value_);
}

void MediaPopup::done(uint32_t) {
  if (!open_) return;
  flush();
  open_ = false;
}

bool MediaPopup::tick(uint32_t now) {
  if (!open_) return false;
  if (dirty_ && int32_t(now - lastStep_) >= int32_t(kMediaSendDebounceMs)) flush();
  if (int32_t(now - lastInput_) >= int32_t(kMediaIdleCloseMs)) {
    done(now);
    return true;
  }
  return false;
}

}  // namespace uistate
