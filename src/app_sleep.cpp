// Sleep paths: sleep screen, goToSleep, manual and low-battery sleep. Split from main.cpp (task 124e).
#include "app_internal.h"
#include "app_loop.h"

// ---- sleep (task 024) ----

// Task 101: composes the sleep screen (lib/sleep_screen) into the canvas from what this wake knows.
static void composeSleepScreen(const app_logic::SleepPath &pp) {
  const int64_t epoch = session_glue::wallClock();
  const config::SleepScreenConfig &sc = g_cfg.cfg.sleepScreen;
  sleepscreen::Inputs in;
  in.landscape = landscape();
  in.wakeEpoch = epoch;
  in.utcOffsetS = utcOffsetAt(epoch);
  in.updatedEpoch = g_drv && g_drv->lastOkEpoch() ? g_drv->lastOkEpoch() : g_restoredEpoch;
  in.nextUpdateEpoch = pp.next_update_at ? *pp.next_update_at : 0;
  in.batteryPct = batteryPercent();
  in.batteryMv = batteryMv();
  in.charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  int32_t t10 = 0, h10 = 0;
  in.haveClimate = report_glue::climate(t10, h10);  // the SHT30 reading of this wake, never an older one
  in.tempX10 = t10;
  in.humX10 = h10;
  in.sleepMode = g_settings.sleep_mode;
  in.weatherConfigured = !sc.weather.empty();
  in.weather = in.weatherConfigured ? g_app.store().get(sc.weather) : nullptr;
  in.calendarConfigured = !sc.calendar.empty();
  in.calendar = in.calendarConfigured ? g_app.store().get(sc.calendar) : nullptr;
  in.notificationsConfigured = sc.notifications;
  in.notificationCount = g_notifValid ? g_notifCount : 0;
  in.notificationTitle = g_notifTitle;
  const sleepscreen::View v = sleepscreen::compose(in);
  render::drawSleepScreen(present::canvas(), v);
  present::setLogTab(g_app.shownTab());
  boot::log("sleep_screen", v.layout.blocks.size());
}

// Shared tail of every sleep: indicator frame (not for the low-battery notice), Wi-Fi off, controller
// idle, then the real sleep. Returns only if sleeping failed.
void goToSleep(power::SleepPlan plan, power::Session session, bool localIdle) {
  uint32_t now = millis();
  const bool low = session == power::Session::LowBattery;
  session_glue::ntpAbort();
  ota::beforeSleep();  // task 087: a deliberate sleep after the first frame is not a crash
  if (!low) {
    historyShutdown();
    g_app.closeNotif();  // task 103
    g_app.closeMedia(now);  // task 106: flush the unsent volume
    g_app.closeAlarm();  // task 108: clears the typed code
    g_app.closeConfirm();
    g_app.closeBrightness(now);  // flush the unsent value; the frozen screen never shows the popup (brightness.md rule 5)
    // Settings auto-leave before sleeping (settings.md rule 5). Whether or not Settings was open now,
    // the plan is made again from the settings in effect now: the session machine's plan was made
    // from the copy taken when the session started (task 039 bug).
    if (settings_flow::active()) handleSettingsResult(settings_flow::tick(now + 3600u * 1000u), now);
    const int64_t epoch = session_glue::wallClock();
    plan = app_logic::planForSleep(g_settings, session, epoch, utcOffsetAt(epoch), localIdle);
  }
  bool pillOnPanel = true;
  if (!low && present::framesOk()) {
    const int64_t pe = session_glue::wallClock();
    const app_logic::SleepPath pp = app_logic::planSleepPath(plan, session, pe);
    if (g_settings.sleep_screen) {
      // Task 101: the dedicated sleep screen is the last frame, no pill. Always one GC16 full (scheduled wake:
      // the deferred session draws it now; interactive: Mode::Full), then the flag that makes the next wake
      // replace it by one full.
      composeSleepScreen(pp);
      now = millis();
      if (g_scheduled) {
        const uint32_t t0 = millis();
        if (present::finishScheduled(true))
          wakelog::refreshed(millis() - t0);
      } else {
        timedShow(present::Mode::Full, now);
      }
      present::setSleepScreenOnPanel(true);
      pillOnPanel = false;
    } else {
      // Task 092: a scheduled wake draws nothing until here (see present::setWakePolicy), then one GC16 full.
      if (!g_scheduled) present::prepareSleep();  // ghost debt first, the indicator is drawn last (STYLE.md 5.5)
      // STYLE.md 5.6: the pill shows the sleep mode and the next scheduled update, from the plan about to run
      g_status.sleepKind = int(pp.indicator);
      g_status.sleepNextMin = app_logic::pillNextMinute(pp.next_update_at, pe, utcOffsetAt(pe));
      composeFrame();
      now = millis();
      if (g_scheduled) {  // the badge is in this frame: the last refresh before sleep is always the GC16 full
        const uint32_t t0 = millis();
        if (present::finishScheduled(true))
          wakelog::refreshed(millis() - t0);
      } else {
        timedShow(present::Mode::Partial, now);  // no flash at sleep entry; the partial debt is cleared by the next full
      }
    }
    present::prepareSleep();  // saves the record of the frame with the indicator, closes the read state
  }
  if (!low) sleepwake::saveTab(g_app.shownTab());  // task 086: the tab under any popup; Settings was left above
  if (g_real) state_cache::save(g_app.store(), g_drv ? g_drv->lastOkEpoch() : 0);
  net::powerDown();
  boot::endBurstRead();
  boot::setupDone();
  sleepwake::enter(plan, session, session_glue::wallClock(), batteryMv(), pillOnPanel);
  // Not asleep (should not happen): carry on as an awake device without the indicator.
  boot::log("sleep_failed", 1);
  g_status.sleepKind = 0;
  present::setSleepScreenOnPanel(false);
}

// Manual sleep (task 081): rocker hold 2.5 s or the Settings "Sleep now" row. Same path as the idle
// sleep (indicator, wake sources, refresh alarm from the settings in effect); also while staying awake on USB.
void manualSleep() {
  if (g_scheduled) return;  // a scheduled refresh has no input
  const bool local = !(g_real && g_drv);  // demo / config error: no refresh alarm
  const int64_t epoch = session_glue::wallClock();
  boot::log("manual_sleep", 1);
  goToSleep(app_logic::planForSleep(g_settings, power::Session::Interactive, epoch, utcOffsetAt(epoch), local),
            power::Session::Interactive, local);
  g_localIdle.on_input(millis() / 1000);  // sleeping failed: carry on
}

bool batteryTooLowForWifi() {
  const int32_t mv = batteryMv();
  batterylimit::sample(mv);
  return power::below_low_battery(mv, batteryLimitMv());
}

// Battery below the limit at wake: no session, no network. A plain notice, no alarm, power off.
void lowBatterySleep() {
  boot::log("low_battery_mv", (long)batteryMv());
  session_glue::loadClockFromRtc();
  const int64_t epoch = session_glue::wallClock();
  present::showNotice("Battery low", "Please charge the device");
  goToSleep(power::plan_sleep(g_settings, power::Session::LowBattery, epoch, 0), power::Session::LowBattery);
}

namespace loopphase {

void localIdleSleep(uint32_t now, const input::Event &e, bool popupOpen) {
  if (app_logic::localIdleSleepWanted(g_real && g_drv) && !ota::busy()) {  // demo / config error: no session machine
    if (!g_localIdleOn) { g_localIdle = power::IdleTimer(g_settings.idle_timeout_s); g_localIdle.start(now / 1000); g_localIdleOn = true; }
    if (e.kind != input::Event::None || settings_flow::active() || popupOpen) g_localIdle.on_input(now / 1000);
    if (g_localIdle.should_sleep(now / 1000)) {
      const int64_t epoch = session_glue::wallClock();
      goToSleep(app_logic::localIdlePlan(g_settings, epoch, utcOffsetAt(epoch)), power::Session::Interactive, true);
      g_localIdle.on_input(millis() / 1000);  // sleeping failed: try again later
    }
  }
}

}  // namespace loopphase
