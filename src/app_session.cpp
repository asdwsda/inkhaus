// Session start and the UI action sinks. Split from main.cpp (task 124e).
#include "app_internal.h"

// ---- session ----

power::WakeReason g_sessionWake = power::WakeReason::Button;  // the wake reason the session was planned with
bool g_netRestartDue = false;  // task 047: a network change waits for a pending user action
uint32_t g_netRestartSince = 0;

// Names of the secrets.yaml Wi-Fi profiles, list order (the forced profile is stored by name).
std::vector<std::string> profileNames() {
  std::vector<std::string> v;
  for (const auto &p : g_cfg.cfg.connection.wifiProfiles) v.push_back(p.name);
  return v;
}

// restart: the same session again (network change): keep its wake reason, so the sleep plan is unchanged.
void startSession(uint32_t now, bool restart) {
  g_net.reset();
  session_glue::ntpSessionStart();
  g_drv.reset();  // before the Wi-Fi adapter it points to
  net::configure(g_cfg.cfg.connection.wifiProfiles,
                 settings::wifiProfileIndex(g_settings.wifi_profile, profileNames()), (int)g_settings.ha_link);  // task 047
  g_wifi.reset(new session_glue::ArduinoWifi(g_cfg.cfg.connection.wifi));
  g_drv.reset(new app_logic::SessionDriver(g_cfg.cfg.entityIds, g_app.store(), *g_wifi, g_net,
                                           session_glue::wallClock, appLog));
  g_sessionOver = false;
  g_drv->setHistoryHandler(onHistoryDone);
  g_drv->setFetchHook(graphsOnFetch);
  g_drv->setHttpDateHook(session_glue::httpDateObserved);  // task 097
  g_drv->setReportHandler([](app_logic::NetEvent &&ev) { report_glue::onDone(ev, session_glue::wallClock()); });
  g_drv->setNotificationsHandler(notificationsDone);
  g_drv->setForecastHandler([](app_logic::NetEvent &&ev) { forecast_glue::onDone(std::move(ev), session_glue::wallClock()); });  // task 104
  forecast_glue::newSession();
  g_drv->setCalendarHandler([](app_logic::NetEvent &&ev) { calendar_glue::onDone(std::move(ev), session_glue::wallClock()); });  // task 105
  calendar_glue::newSession();
  g_drv->setArtHandler([](app_logic::NetEvent &&ev) { art_glue::onDone(std::move(ev), g_app); });  // task 107
  art_glue::newSession(g_app);
  g_drv->setSleepHold([] { return graphsBusy() || ota::busy() || report_glue::busy() || g_notifPending || forecast_glue::pending() || calendar_glue::pending() || art_glue::pending(); });  // task 087: no sleep during a check / download
  g_app.setNotifDismissSink(notifDismissSink);  // task 103
  report_glue::abort();
  g_notifAsked = g_notifPending = g_notifWanted = false;  // the old net session's answer is dropped; the new one asks again
  g_app.graphs().abortFetch();  // the old net session is gone: a graph result will not arrive
  g_graphBatchMs = 0;
  if (g_app.history().fetching() || g_histWaitNet) {  // the old net session is gone: its result will not arrive
    g_histWaitNet = false;
    if (g_histCancel) *g_histCancel = true;
    if (g_app.historyPopup().fetchUnavailable()) g_histChanged = true;
  }
  wakelog::sessionStarted(now);
  if (!restart && g_firstSession) boot::log("t_wifi_start_ms", (long)boot::elapsedMs());  // task 058
  logMem("session_start");
  // The offset is taken now (local time at planning time); the machine plans the wake-up from it.
  if (!restart) g_sessionWake = g_firstSession ? g_wake : power::WakeReason::Button;
  g_drv->start(g_sessionWake, batteryMv(), g_settings, utcOffsetAt(session_glue::wallClock()), now);
  g_firstSession = false;
}

// Action sink: a tap started an action in the UI state; real mode turns it into a session action.
uint32_t g_tapNow = 0;
void onAppAction(int tab, int widget, int sub, const std::vector<EntityState> &requested) {
  if (!g_real || !g_drv) return;
  const auto &tabs = g_app.config().tabs;
  if (tab < 0 || size_t(tab) >= tabs.size() || widget < 0 || size_t(widget) >= tabs[size_t(tab)].widgets.size())
    return;
  const config::Widget &w = tabs[size_t(tab)].widgets[size_t(widget)];
  auto ev = app_logic::intentFromRequest(w.type, sub, requested);
  if (ev) g_drv->userAction(*ev, g_tapNow);
}

// Brightness popup send (task 045): light.turn_on with brightness_pct, or light.turn_off for 0.
void onBrightnessSend(const std::string &id, int pct) {
  if (!g_real || !g_drv) return;
  auto ev = app_logic::brightnessIntent(id, pct);
  if (ev) g_drv->userAction(*ev, millis());
}

// Media popup send (task 106): media_player.volume_set / volume_mute.
void onMediaSend(const std::string &id, uistate::MediaSend what, int value) {
  if (!g_real || !g_drv) return;
  auto ev = what == uistate::MediaSend::Volume ? app_logic::mediaVolumeIntent(id, value) : app_logic::mediaMuteIntent(id, value != 0);
  if (ev) g_drv->userAction(*ev, millis());
}

// Alarm keypad send (task 108): alarm_control_panel.alarm_* with the code. The code is never logged.
void onAlarmSend(const std::string &id, AlarmCmd cmd, const std::string &code) {
  if (!g_real || !g_drv) return;
  auto ev = app_logic::alarmIntent(id, cmd, code);
  if (ev) g_drv->userAction(*ev, millis());
}

// A tap on a dimmable light waits for the release or the long press (task 045).
uistate::DeferredTap g_defer;

// Driver marks (pending/failed by entity id) -> widget visuals. True if something visible changed.
bool syncMarks() {
  if (!g_real || !g_drv) return false;
  std::map<std::string, uistate::Mark> m;
  for (const auto &kv : g_drv->marks())
    m[kv.first] = kv.second == session::Mark::Failed ? uistate::Mark::Failed : uistate::Mark::Pending;
  return g_app.setMarks(m);
}
