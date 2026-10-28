#pragma once
// Pure parts of sleeping and waking (task 024): wake-reason mapping, which sleep indicator, how to
// sleep (power off / deep sleep, wake sources, BM8563 alarm fields) and the guard that swallows the
// input that woke the device. No Arduino, no hardware access.
#include <cstdint>
#include <optional>

#include "power.h"

namespace app_logic {

// ---- wake-up -------------------------------------------------------------------------------------

enum class ResetKind { PowerOn, DeepSleep, Software, Crash, External, Other };
// What woke the ESP32 from deep sleep (ext0 = touch INT GPIO36, ext1 = rocker push GPIO38).
enum class WakePin { None, Touch, Rocker, Timer, Other };

struct WakeInput {
  ResetKind reset = ResetKind::PowerOn;
  WakePin pin = WakePin::None;  // only meaningful after DeepSleep
  bool rtc_alarm_flag = false;  // BM8563 alarm/timer flag was set at boot
  bool slept_marker = false;    // NVS marker written right before the previous power-off / sleep
};

// PowerOn: alarm flag -> RtcAlarm; else the marker (we powered off) -> Button; else ColdBoot (first
// power-up, flashing, USB plugged in).
// DeepSleep: Touch pin -> Touch, Rocker pin -> Button, Timer (or the alarm flag) -> RtcAlarm, else Unknown.
// Software / Crash / External / Other: ColdBoot (a restart is not a wake from sleep).
power::WakeReason classifyWake(const WakeInput &in);
const char *wakeName(power::WakeReason r);

// ---- sleep indicator -----------------------------------------------------------------------------

enum class Indicator : uint8_t { None = 0, Button = 1, Touch = 2 };
// STYLE.md 5.5: Touch only when the plan wakes on touch and it is not a low-battery sleep.
Indicator indicatorFor(const power::SleepPlan &plan, power::Session session);
const char *indicatorName(Indicator i);
// From a persisted byte; anything unknown is None.
Indicator indicatorFromByte(uint8_t b);

// ---- how to sleep --------------------------------------------------------------------------------

struct AlarmFields {  // BM8563 alarm: minute resolution, matches minute + hour + day of month (UTC)
  int minute = 0, hour = 0, day = 1;
};

constexpr int64_t kMaxAlarmAheadS = 27 * 24 * 3600;  // the BM8563 alarm has no month

// UTC alarm time rounded up to a whole minute -> register fields. None if there is no alarm, the clock
// is unset (now_epoch implausible), the alarm is not in the future, or it is more than kMaxAlarmAheadS away.
std::optional<AlarmFields> alarmFields(std::optional<int64_t> alarm_at, int64_t now_epoch);

enum class SleepMethod { PowerOff, DeepSleep };

struct SleepPath {
  SleepMethod first = SleepMethod::PowerOff;  // PowerOff: release the main power hold (GPIO2)
  bool arm_rtc_alarm = false;                 // set the BM8563 alarm before sleeping
  std::optional<AlarmFields> alarm;
  // Deep-sleep wake sources. For PowerOff these are used when the power did not go away (USB powered).
  bool wake_rocker = true;
  bool wake_touch = false;
  std::optional<int64_t> wake_timer_s;  // ESP32 timer wake-up: stands in for the BM8563 INT (not on a GPIO)
  Indicator indicator = Indicator::None;
  bool low_battery = false;
};

// Low battery overrides everything: power off, rocker only, no alarm, no touch.
SleepPath planSleepPath(const power::SleepPlan &plan, power::Session session, int64_t now_epoch);
const char *methodName(SleepMethod m);

// ---- swallowing the wake input -------------------------------------------------------------------

// The press that woke the device is still down (or its release/click is still to come) when the loop
// starts: it must not reach the UI (a held rocker would even open Settings). Armed with the wake
// reason, fed with the raw "is down" state every loop iteration.
class WakeInputGuard {
 public:
  static constexpr int64_t kSettleMs = 150;  // continuously up this long -> released
  static constexpr int64_t kMaxMs = 3000;    // never block longer than this
  void arm(power::WakeReason r, int64_t now_ms);
  void observe(int64_t now_ms, bool key_down, bool touch_down);
  bool blocksKey() const { return key_; }
  bool blocksTouch() const { return touch_; }

 private:
  bool key_ = false, touch_ = false;
  int64_t armed_ms_ = 0;
  int64_t key_up_since_ = -1, touch_up_since_ = -1;
};

}  // namespace app_logic
