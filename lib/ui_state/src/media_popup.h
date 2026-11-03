#pragma once
// Model of the media popup (task 106, docs/ui/widgets-new.md 4.1). Pure logic with a millisecond clock.
// Volume steps are optimistic at once and sent once, kMediaSendDebounceMs after the last step (like the
// climate target). Mute is sent at once. The popup closes on close / tap outside / long push, or after
// kMediaIdleCloseMs without input (UNVERIFIED: the spec has no idle time; keeps the device able to sleep).
#include <cstdint>
#include <functional>
#include <string>

namespace uistate {

constexpr uint32_t kMediaSendDebounceMs = 1000;
constexpr uint32_t kMediaIdleCloseMs = 30000;
constexpr int kMediaVolumeStep = 5;

enum class MediaSend { Volume, Mute };
// Volume: value = percent 0..100. Mute: value = 1 (mute) / 0 (unmute). Called with the player's entity id.
using MediaSink = std::function<void(const std::string &entityId, MediaSend what, int value)>;

class MediaPopup {
 public:
  void setSink(MediaSink s) { sink_ = std::move(s); }

  // volumePct: current volume (-1 unknown). tab/widget identify the widget.
  void open(const std::string &entityId, int tab, int widget, int volumePct, bool muted, uint32_t now);
  bool isOpen() const { return open_; }
  const std::string &entityId() const { return id_; }
  int tab() const { return tab_; }
  int widget() const { return widget_; }

  // Each returns true if the shown value changed. All count as input (restart the idle timer).
  bool step(int direction, uint32_t now);  // +1 / -1: +-5 %, clamped 0..100
  bool setPct(int pct, uint32_t now);
  bool tapBar(double fraction, uint32_t now);
  bool toggleMute(uint32_t now);
  void poke(uint32_t now) { if (open_) lastInput_ = now; }
  void cancel() { open_ = false; dirty_ = false; unconfirmed_ = false; }  // closes without sending
  void done(uint32_t now);                                                 // flushes an unsent volume, closes
  // Sends a due volume, closes after the idle time. Returns true if the popup closed.
  bool tick(uint32_t now);

  // The volume / mute to show while a change is unsent or unconfirmed; -1 / false = use the real state.
  int volumeOverride() const { return (dirty_ || unconfirmed_) ? value_ : -1; }
  bool muteOverridden() const { return muteUnconfirmed_; }
  bool mutedOverride() const { return mutedValue_; }
  // The driver confirmed (or failed) the last send: drop the optimistic values.
  void clearOptimistic() { unconfirmed_ = false; muteUnconfirmed_ = false; }
  bool sendPending() const { return dirty_; }

 private:
  void flush();

  MediaSink sink_;
  bool open_ = false, dirty_ = false, unconfirmed_ = false, muteUnconfirmed_ = false, mutedValue_ = false;
  std::string id_;
  int tab_ = 0, widget_ = 0, value_ = 0;
  uint32_t lastInput_ = 0, lastStep_ = 0;
};

}  // namespace uistate
