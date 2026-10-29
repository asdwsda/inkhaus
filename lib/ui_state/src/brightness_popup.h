#pragma once
// Model of the brightness popup (task 045). Pure logic with a millisecond clock; no drawing.
// The value is optimistic from the first change. Changes are debounced: the last value is sent
// once kSendDebounceMs passed without input. The popup closes on Done, tap outside or after
// kIdleCloseMs without input; closing flushes an unsent value.
#include <cstdint>
#include <functional>
#include <string>

namespace uistate {

constexpr uint32_t kSendDebounceMs = 400;
constexpr uint32_t kIdleCloseMs = 10000;
constexpr int kBrightnessStep = 10;
constexpr int kBrightnessPresets[4] = {25, 50, 75, 100};

// pct 0..100 (0 = off). Called with the light's entity id.
using BrightnessSink = std::function<void(const std::string &entityId, int pct)>;

// HA brightness 0..255 -> percent 0..100 (rounded).
int brightnessToPct(int ha255);

class BrightnessPopup {
 public:
  void setSink(BrightnessSink s) { sink_ = std::move(s); }

  // initialPct: current brightness, 0 if off. tab/widget identify the widget.
  void open(const std::string &entityId, int tab, int widget, int initialPct, uint32_t now);
  bool isOpen() const { return open_; }
  const std::string &entityId() const { return id_; }
  int tab() const { return tab_; }
  int widget() const { return widget_; }
  int value() const { return value_; }  // optimistic value, 0..100

  // Each returns true if the shown value changed. All count as input (restart the idle timer).
  bool step(int direction, uint32_t now);  // direction +1 / -1: +-10 %, clamped
  bool setPct(int pct, uint32_t now);
  bool preset(int index, uint32_t now);    // 0..3 -> 25/50/75/100
  bool off(uint32_t now) { return setPct(0, now); }
  // Tap on the bar at fraction 0..1 of its length -> percent (rounded, clamped).
  bool tapBar(double fraction, uint32_t now);

  void poke(uint32_t now) { if (open_) touchInput(now); }  // input that changes nothing (panel tap)
  void cancel() { open_ = false; dirty_ = false; unconfirmed_ = false; }  // closes without sending
  void done(uint32_t now);  // flushes an unsent value, closes
  // Sends a due value, closes after the idle time. Returns true if the popup closed.
  bool tick(uint32_t now);

  // The value to show on the widget while the popup is open or a send is unconfirmed;
  // -1 = none (use the real state).
  int optimisticPct() const { return (open_ || unconfirmed_) ? value_ : -1; }
  // The driver confirmed (or failed) the last send: drop the optimistic value.
  void clearOptimistic() { unconfirmed_ = false; }
  bool sendPending() const { return dirty_; }

 private:
  void touchInput(uint32_t now) { lastInput_ = now; }
  void flush();

  BrightnessSink sink_;
  bool open_ = false, dirty_ = false, unconfirmed_ = false;
  std::string id_;
  int tab_ = 0, widget_ = 0, value_ = 0;
  uint32_t lastInput_ = 0;
};

}  // namespace uistate
