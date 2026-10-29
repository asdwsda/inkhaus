#include "sleep_logic.h"

#include "wall_time.h"

namespace app_logic {

power::WakeReason classifyWake(const WakeInput &in) {
  using R = power::WakeReason;
  switch (in.reset) {
    case ResetKind::PowerOn:
      if (in.rtc_alarm_flag) return R::RtcAlarm;
      return in.slept_marker ? R::Button : R::ColdBoot;
    case ResetKind::DeepSleep:
      switch (in.pin) {
        case WakePin::Touch: return R::Touch;
        case WakePin::Rocker: return R::Button;
        case WakePin::Timer: return R::RtcAlarm;
        case WakePin::None:
          return in.rtc_alarm_flag ? R::RtcAlarm : R::Unknown;
        default: return R::Unknown;
      }
    default: return R::ColdBoot;
  }
}

WakePin pinForExt1(uint64_t) { return WakePin::Rocker; }

const char *wakeName(power::WakeReason r) {
  switch (r) {
    case power::WakeReason::ColdBoot: return "cold_boot";
    case power::WakeReason::Button: return "button";
    case power::WakeReason::RtcAlarm: return "rtc_alarm";
    case power::WakeReason::Touch: return "touch";
    default: return "unknown";
  }
}

Indicator indicatorFor(const power::SleepPlan &plan, power::Session session) {
  if (session == power::Session::LowBattery) return Indicator::Button;
  return plan.touch_wake ? Indicator::Touch : Indicator::Button;
}

const char *indicatorName(Indicator i) {
  switch (i) {
    case Indicator::Button: return "button";
    case Indicator::Touch: return "touch";
    default: return "none";
  }
}

Indicator indicatorFromByte(uint8_t b) {
  return b == uint8_t(Indicator::Button) ? Indicator::Button : b == uint8_t(Indicator::Touch) ? Indicator::Touch : Indicator::None;
}

std::optional<AlarmFields> alarmFields(std::optional<int64_t> alarm_at, int64_t now_epoch) {
  if (!alarm_at || !plausibleEpoch(now_epoch)) return std::nullopt;
  int64_t t = *alarm_at;
  if (t % 60 != 0) t += 60 - (((t % 60) + 60) % 60);
  if (t <= now_epoch || t - now_epoch > kMaxAlarmAheadS) return std::nullopt;
  const Civil c = epochToCivil(t, 0);
  AlarmFields f;
  f.minute = c.minute;
  f.hour = c.hour;
  f.day = c.day;
  return f;
}

SleepPath planSleepPath(const power::SleepPlan &plan, power::Session session, int64_t now_epoch) {
  SleepPath p;
  p.indicator = indicatorFor(plan, session);
  if (session == power::Session::LowBattery) {
    p.low_battery = true;
    p.first = SleepMethod::PowerOff;
    p.wake_rocker = true;
    p.wake_touch = false;
    return p;  // no alarm: waking a flat battery for a refresh only drains it further
  }
  p.first = plan.mode == power::SleepMode::DeepSleep ? SleepMethod::DeepSleep : SleepMethod::PowerOff;
  p.wake_rocker = plan.button_wake;
  p.wake_touch = plan.touch_wake;
  p.alarm = alarmFields(plan.rtc_alarm_at, now_epoch);
  p.arm_rtc_alarm = p.alarm.has_value();
  if (p.alarm) {
    int64_t s = *plan.rtc_alarm_at - now_epoch;
    p.wake_timer_s = s < 1 ? 1 : s;
  }
  return p;
}

const char *methodName(SleepMethod m) { return m == SleepMethod::PowerOff ? "poweroff" : "deepsleep"; }

SleepExec resolveSleepExec(SleepMethod planned, bool power_off_build) {
  SleepExec e;
  e.gpio2_low = planned == SleepMethod::PowerOff && power_off_build;
  e.epd_standby = !e.gpio2_low;  // power gone anyway on the old path
  return e;
}

bool localIdleSleepWanted(bool session_running) { return !session_running; }

power::SleepPlan localIdlePlan(const settings::Settings &s, int64_t now_epoch, int32_t utc_offset_s) {
  power::SleepPlan p = power::plan_sleep(s, power::Session::Interactive, now_epoch, utc_offset_s);
  p.rtc_alarm_at.reset();
  return p;
}

power::SleepPlan planForSleep(const settings::Settings &s, power::Session session, int64_t now_epoch,
                              int32_t utc_offset_s, bool local_idle) {
  return local_idle ? localIdlePlan(s, now_epoch, utc_offset_s)
                    : power::plan_sleep(s, session, now_epoch, utc_offset_s);
}

void WakeInputGuard::arm(power::WakeReason r, int64_t now_ms) {
  key_ = r == power::WakeReason::Button;
  touch_ = r == power::WakeReason::Touch;
  armed_ms_ = now_ms;
  key_up_since_ = touch_up_since_ = -1;
}

void WakeInputGuard::observe(int64_t now_ms, bool key_down, bool touch_down) {
  if (key_) {
    if (key_down) key_up_since_ = -1;
    else if (key_up_since_ < 0) key_up_since_ = now_ms;
    if ((key_up_since_ >= 0 && now_ms - key_up_since_ >= kSettleMs) || now_ms - armed_ms_ >= kMaxMs) key_ = false;
  }
  if (touch_) {
    if (touch_down) touch_up_since_ = -1;
    else if (touch_up_since_ < 0) touch_up_since_ = now_ms;
    if ((touch_up_since_ >= 0 && now_ms - touch_up_since_ >= kSettleMs) || now_ms - armed_ms_ >= kMaxMs) touch_ = false;
  }
}

}  // namespace app_logic
