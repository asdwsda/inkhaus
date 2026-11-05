// loop() net-event, pump and frame phases. Split from main.cpp (task 124e).
#include "app_internal.h"
#include "app_loop.h"

namespace loopphase {

void netEvents(uint32_t now, bool &changed) {
  if (g_netRestartDue && g_real && !settings_flow::active() &&
      (!actionPending() || now - g_netRestartSince > kNetRestartWaitMs)) {
    g_netRestartDue = false;
    startSession(now, true);  // UNVERIFIED: net::begin restarts a running connect (disconnects first)
  }

  if (g_drv) {
    g_drv->tick(now);
    session_glue::ntpPoll(g_drv->wifiUp());
    ota::tick(now, session_glue::wallClock(), g_drv->wifiUp(), g_drv->sessionDone(), !g_scheduled, now - g_lastInputMs, g_net.userWaiting());  // task 087: due check; task 116: idle-only when interactive
    if (g_drv->takeRender()) changed = true;
    logFetchApplied();
    if (g_drv->wifiUp()) wakelog::wifiUp(now);
    {
      static bool wifiLogged = false;
      if (!wifiLogged && g_drv->wifiUp()) {
        wifiLogged = true;
        boot::log("t_wifi_up_ms", (long)boot::elapsedMs());  // task 058
      }
    }
    {
      static uint32_t seenFetch = 0;
      if (g_drv->fetchSeq() != seenFetch) {
        seenFetch = g_drv->fetchSeq();
        wakelog::fetchDone(now, !g_drv->lastFetchFailed());
      }
    }
    changed |= syncMarks();
    if (!g_stackLogged && g_drv->sessionDone()) {
      g_stackLogged = true;
      Serial.printf("APP: net_stack_free=%u\n", (unsigned)g_net.stackFreeBytes());
    }
    if (g_settingsRefreshing && g_drv->fetchSeq() != g_refreshSeq) {
      g_settingsRefreshing = false;
      const bool ok = !g_drv->lastFetchFailed();
      if (ok) snapshotInfo();
      handleSettingsResult(settings_flow::refreshDone(ok), now);
    }
    app_logic::SleepRequest sl;
    if (g_drv->takeSleep(sl)) {
      session_glue::ntpAbort();
      goToSleep(sl.plan, sl.session);  // does not return when it sleeps
      g_sessionOver = true;
      changed = true;
    }
  }
}

void pumps(bool &changed) {
  historyPump();
  if (g_histChanged) { g_histChanged = false; changed = true; }
  if (g_notifChanged) { g_notifChanged = false; changed = true; }  // task 103
  graphsPump();
  notificationsPump();
  forecast_glue::pump(g_drv.get(), g_app.config(), g_app.shownTab(), g_sessionWake == power::WakeReason::RtcAlarm, graphsBusy(),
                      session_glue::wallClock(), utcOffsetAt(session_glue::wallClock()));  // task 104
  if (forecast_glue::takeChanged()) changed = true;
  calendar_glue::pump(g_drv.get(), g_app.config(), g_app.shownTab(), g_sessionWake == power::WakeReason::RtcAlarm, graphsBusy(),
                      session_glue::wallClock(), utcOffsetAt(session_glue::wallClock()));  // task 105
  if (calendar_glue::takeChanged()) changed = true;
  art_glue::pump(g_drv.get(), g_app, graphsBusy());  // task 107
  if (art_glue::takeChanged()) changed = true;
  if (g_app.graphs().takeChanged()) changed = true;
}

void frame(uint32_t now, const input::Event &e, bool popupOpen, bool changed) {
  if (!settings_flow::active() && g_app.screen() == uistate::Screen::Tabs) {
    // A phase change repaints only the label region: the frame diff leaves everything else alone.
    const render::StatusInfo before = g_status;
    updateStatus();
    if (before.link != g_status.link || before.label1 != g_status.label1 || before.label2 != g_status.label2 ||
        before.outdated != g_status.outdated)
      changed = true;
  }

  shot::Hooks hooks;
  hooks.frame = &present::canvas();
  hooks.previous = &present::previous();
  hooks.setPendingMs = [](uint32_t ms) { g_app.setPendingMs(ms); };
  shot::poll(hooks);

  const auto late = g_defer.tick(now);  // neither release nor hold seen: treat as a tap
  if (late.kind == uistate::DeferredTap::Result::Tap) {
    g_tapNow = now;
    changed |= g_app.touch(late.point, now);
  }
  changed |= g_app.tick(now);
  if (changed) showFrame(now);
  if (e.kind != input::Event::None || popupOpen) g_lastInputMs = now;
  {  // task 114: clean after a burst of taps (before 095's idle full, which it makes unnecessary by clearing the debt)
    screen_track::BurstContext bc;
    bc.popup = g_app.brightnessOpen() || g_app.historyPopup().visible() || g_app.notifOpen() || g_app.alarmOpen() || g_app.confirmOpen();
    bc.settings = settings_flow::active();
    bc.ota = ota::busy();
    bc.tap_pending = g_defer.armed();
    bc.scheduled = g_scheduled;
    if (g_real && present::burstCleanDue(now - g_lastInputMs, bc)) present::burstClean();
  }
  if (!g_scheduled && g_real && !ota::busy() &&
      app_logic::idleFullDue(present::partialDebt(), now - g_lastInputMs,
                             settings_flow::active() || g_app.brightnessOpen() || g_app.historyPopup().visible() || g_app.notifOpen() || g_app.alarmOpen() || g_app.confirmOpen(),
                             g_defer.armed()))  // task 095: grey drift in a long awake session
    present::idleFull();
  present::idle(millis());
  delay(10);
}

}  // namespace loopphase
