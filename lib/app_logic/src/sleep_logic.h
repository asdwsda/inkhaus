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

// ext1 wake: the rocker push is the only EXT1 source, so the status mask is not needed; an empty mask
// (seen on the device) is still the rocker. Kept as a function so the rule is tested.
WakePin pinForExt1(uint64_t status_mask);

// ---- sleep indicator -----------------------------------------------------------------------------

// STYLE.md 5.5: a single Zz badge for every wake mode and low-battery sleep. The value is persisted
// (1 = a badge is on the panel).
enum class Indicator : uint8_t { None = 0, Badge = 1 };
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

// What enter() does for a planned method (task 035). A PowerOff plan (Button mode, low battery) uses
// ESP32 deep sleep with the GPIO2 power hold kept ("deepsleep_hold") unless the build has
// -DSLEEP_POWER_OFF (the old GPIO2-low path, kept for testing). The IT8951 goes to standby before
// every real sleep. The GT911 is never put to sleep: its wake needs a pulse on INT (GPIO36), which is
// input-only on the ESP32, so a sleeping GT911 could not be brought back.
struct SleepExec {
  bool gpio2_low = false;    // old path: release the main power hold, fall back to deep sleep on USB
  bool epd_standby = true;   // IT8951 STANDBY (keeps image memory) before sleeping
};
SleepExec resolveSleepExec(SleepMethod planned, bool power_off_build);

// ---- local idle sleep (no session: demo mode, config-error screen) --------------------------------

// A local idle timer drives sleep only when no session machine runs (it issues the Sleep command
// otherwise).
bool localIdleSleepWanted(bool session_running);
// Interactive plan from the settings, but never an RTC alarm: nothing to refresh.
// The plan for the sleep about to happen, always from the settings in effect NOW (g_settings), never
// from a copy taken when the session started (task 039 bug: the session machine keeps the boot-time copy).
power::SleepPlan planForSleep(const settings::Settings &s, power::Session session, int64_t now_epoch,
                              int32_t utc_offset_s, bool local_idle);
power::SleepPlan localIdlePlan(const settings::Settings &s, int64_t now_epoch, int32_t utc_offset_s);

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
