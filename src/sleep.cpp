#include "sleep.h"

#include <M5Unified.h>
#include <Preferences.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_system.h>

#include <string>

#include "boot.h"
#include "wakelog.h"

namespace sleepwake {

namespace {

// M5Paper pins. M5Unified has no power-hold pin for the original M5Paper (its pin table and
// Power_Class.inl:642-651 give only the touch INT as wake pin); GPIO2 is "M5EPD_MAIN_PWR_PIN"
// (M5GFX.cpp:1644, driven high at autodetect, boot.cpp kPinPwr).
constexpr gpio_num_t kPinMainPower = GPIO_NUM_2;
constexpr gpio_num_t kPinTouchInt = GPIO_NUM_36;   // GT911 INT, low = touch (Power_Class.inl:644)
constexpr gpio_num_t kPinRockerPush = GPIO_NUM_38;  // BtnB, external pull-up, low = pressed
constexpr uint32_t kPowerOffWaitMs = 400;           // still running after this: the board is on USB

const char *kNs = "slp";
const char *kKeyMarker = "m", *kKeyInd = "ind";

app_logic::Indicator g_stored = app_logic::Indicator::None;

app_logic::ResetKind resetKind() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return app_logic::ResetKind::PowerOn;
    case ESP_RST_DEEPSLEEP: return app_logic::ResetKind::DeepSleep;
    case ESP_RST_SW: return app_logic::ResetKind::Software;
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return app_logic::ResetKind::Crash;
    case ESP_RST_EXT: return app_logic::ResetKind::External;
    default: return app_logic::ResetKind::Other;
  }
}

const char *resetName(app_logic::ResetKind k) {
  using K = app_logic::ResetKind;
  switch (k) {
    case K::PowerOn: return "poweron";
    case K::DeepSleep: return "deepsleep";
    case K::Software: return "software";
    case K::Crash: return "crash";
    case K::External: return "external";
    default: return "other";
  }
}

// Only meaningful after deep sleep. ext0 = touch INT, ext1 = rocker push, timer = the stand-in for
// the RTC alarm (see enter()).
app_logic::WakePin wakePin() {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT0: return app_logic::WakePin::Touch;
    case ESP_SLEEP_WAKEUP_EXT1:
      return app_logic::pinForExt1(esp_sleep_get_ext1_wakeup_status());  // empty mask = rocker too
    case ESP_SLEEP_WAKEUP_TIMER: return app_logic::WakePin::Timer;
    case ESP_SLEEP_WAKEUP_UNDEFINED: return app_logic::WakePin::None;
    default: return app_logic::WakePin::Other;
  }
}

const char *pinName(app_logic::WakePin p) {
  using P = app_logic::WakePin;
  switch (p) {
    case P::Touch: return "touch";
    case P::Rocker: return "rocker";
    case P::Timer: return "timer";
    case P::Other: return "other";
    default: return "none";
  }
}

// GT911 holds its INT low until the touch data is read; reading it releases the line (the same
// thing M5Unified does before a touch-wake sleep, M5Unified.inl:3325-3335).
void releaseTouchInt() {
  pinMode(kPinTouchInt, INPUT);
  for (int i = 0; i < 10 && digitalRead(kPinTouchInt) == LOW; ++i) {
    lgfx::touch_point_t tp;
    M5.Display.getTouchRaw(&tp, 1);
    delay(5);
  }
}

}  // namespace

power::WakeReason detect() {
  // A deep sleep held GPIO2 high with the pad hold; release it so that a later power-off can work.
  gpio_deep_sleep_hold_dis();
  gpio_hold_dis(kPinMainPower);

  app_logic::WakeInput in;
  in.reset = resetKind();
  in.pin = in.reset == app_logic::ResetKind::DeepSleep ? wakePin() : app_logic::WakePin::None;
  in.rtc_alarm_flag = M5.Rtc.isEnabled() && M5.Rtc.getIRQstatus();
  uint8_t ind = 0;
  Preferences p;
  if (p.begin(kNs, false)) {
    in.slept_marker = p.getUChar(kKeyMarker, 0) != 0;
    ind = p.getUChar(kKeyInd, 0);
    p.putUChar(kKeyMarker, 0);
    p.end();
  }
  // Nothing may fire during the session: the next alarm is planned at its end.
  if (M5.Rtc.isEnabled()) {
    M5.Rtc.disableIRQ();
    M5.Rtc.clearIRQ();
  }
  const power::WakeReason r = app_logic::classifyWake(in);
  // The first frame only keeps the indicator of the previous sleep for a scheduled wake that
  // follows a sleep of ours; every other start clears it.
  g_stored = (r == power::WakeReason::RtcAlarm) ? app_logic::indicatorFromByte(ind) : app_logic::Indicator::None;
  wakelog::logf("WAKE: reason=%s reset=%s pin=%s rtc_flag=%d marker=%d indicator_on_panel=%s\n", app_logic::wakeName(r),
                resetName(in.reset), pinName(in.pin), in.rtc_alarm_flag ? 1 : 0, in.slept_marker ? 1 : 0,
                app_logic::indicatorName(g_stored));
  return r;
}

app_logic::Indicator storedIndicator() { return g_stored; }

void enter(const power::SleepPlan &plan, power::Session session, int64_t nowEpoch, int32_t batteryMv) {
  const app_logic::SleepPath sp = app_logic::planSleepPath(plan, session, nowEpoch);

  std::string wake;
  auto add = [&wake](const char *w) { wake += wake.empty() ? w : std::string("+") + w; };
  if (sp.wake_rocker) add("rocker");
  if (sp.wake_touch) add("touch");
  if (sp.arm_rtc_alarm) add("rtc");
  wakelog::logf("SLEEP: mode=%s alarm_at=%lld indicator=%s wake=%s battery_mv=%d awake_ms=%u low_battery=%d\n",
                app_logic::methodName(sp.first), (long long)(sp.arm_rtc_alarm ? *plan.rtc_alarm_at : 0),
                app_logic::indicatorName(sp.indicator), wake.empty() ? "none" : wake.c_str(), (int)batteryMv,
                (unsigned)millis(), sp.low_battery ? 1 : 0);

  wakelog::append(session, nowEpoch, batteryMv);  // no-op unless the debug_log setting is on

  Preferences p;
  if (p.begin(kNs, false)) {
    p.putUChar(kKeyInd, uint8_t(sp.indicator));
    p.putUChar(kKeyMarker, 1);
    p.end();
  }

  // BM8563 alarm: matches minute + hour + day of month, UTC (the RTC is written in UTC by the NTP code).
  // On battery its INT line switches the board on again (M5EPD shutdown(seconds) relies on this; the
  // schematic is not in M5Unified: it has no RTC INT pin for the M5Paper, Power_Class.inl:642-651).
  if (M5.Rtc.isEnabled()) {
    M5.Rtc.disableIRQ();
    M5.Rtc.clearIRQ();
    if (sp.alarm) {
      m5::rtc_date_t d;
      d.date = int8_t(sp.alarm->day);
      m5::rtc_time_t t(int8_t(sp.alarm->hour), int8_t(sp.alarm->minute), 0);
      const int rc = M5.Rtc.setAlarmIRQ(d, t);
      Serial.printf("SLEEP: rtc_alarm_set=%d at=%02d-%02d:%02d\n", rc > 0 ? 1 : 0, sp.alarm->day, sp.alarm->hour,
                    sp.alarm->minute);
    }
  }

  const app_logic::SleepExec ex = app_logic::resolveSleepExec(sp.first,
#ifdef SLEEP_POWER_OFF
                                                              true
#else
                                                              false
#endif
  );
  if (ex.gpio2_low) {
    wakelog::setSleptPath("poweroff");
    Serial.println("SLEEP: path=poweroff (GPIO2 low)");
    Serial.flush();
    gpio_hold_dis(kPinMainPower);
    pinMode(kPinMainPower, OUTPUT);
    digitalWrite(kPinMainPower, LOW);
    delay(kPowerOffWaitMs);
    // Still running: USB supplies the board. Take the power hold back and sleep with the same wake
    // sources instead (rocker, and the timer in place of the RTC alarm).
    digitalWrite(kPinMainPower, HIGH);
    wakelog::setSleptPath("deepsleep_fallback");
    Serial.println("SLEEP: path=deepsleep_fallback (still powered: USB)");
  } else {
    // Task 035: GPIO2 stays high through deep sleep. IT8951 in STANDBY (image memory kept). The GT911
    // stays awake: its wake needs INT driven by the host and GPIO36 is input-only (see SleepExec).
    wakelog::setSleptPath("deepsleep_hold");
    const bool epdOk = ex.epd_standby && boot::controllerStandby();
    Serial.printf("SLEEP: path=deepsleep_hold epd=%s touch=awake\n", epdOk ? "standby" : "active");
  }
  // Deep sleep. GPIO2 keeps the board powered on battery: hold it high through the sleep.
  digitalWrite(kPinMainPower, HIGH);
  gpio_hold_en(kPinMainPower);
  gpio_deep_sleep_hold_en();
  // On battery the pad hold alone did not survive deep sleep (task 035 device evidence): also make
  // GPIO2 (RTC_GPIO12) an RTC output held high, and keep the RTC peripheral domain powered.
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  rtc_gpio_init(GPIO_NUM_2);
  rtc_gpio_set_direction(GPIO_NUM_2, RTC_GPIO_MODE_OUTPUT_ONLY);
  rtc_gpio_set_level(GPIO_NUM_2, 1);
  rtc_gpio_hold_en(GPIO_NUM_2);
  Serial.println("SLEEP: hold=rtc");

  if (sp.wake_touch) {
    releaseTouchInt();
    esp_sleep_enable_ext0_wakeup(kPinTouchInt, 0);
  }
  if (sp.wake_rocker) {
    // A pressed rocker would wake us at once.
    for (uint32_t t = millis(); digitalRead(kPinRockerPush) == LOW && millis() - t < 1500;) delay(10);
    // ESP32 ext1 has no "any low": one pin with ALL_LOW is "this pin low" (ext0 is taken by the touch INT).
    esp_sleep_enable_ext1_wakeup(1ULL << kPinRockerPush, ESP_EXT1_WAKEUP_ALL_LOW);
  }
  if (sp.wake_timer_s) esp_sleep_enable_timer_wakeup(uint64_t(*sp.wake_timer_s) * 1000000ULL);
  Serial.flush();
  esp_deep_sleep_start();
}

}  // namespace sleepwake
