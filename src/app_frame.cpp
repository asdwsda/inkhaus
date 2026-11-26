// Status bar phase and frame composition / presentation. Split from main.cpp (task 124e).
#include "app_internal.h"
#include "render/widgets.h"  // task 127: staleEligible

// Status bar phase (STYLE.md 5.3): never blank. Values restored from the cache (g_restoredEpoch, task 037)
// count as the last good update until a fetch result replaces them.
bool g_keepStatus = false;  // task 058: the first frame after a deep-sleep wake shows the last known status

// Task 120 (UNVERIFIED on device): wake-time ready indicator. Ready = Wi-Fi up and the session not Offline/AuthError
// (there is no separate HA-reachable signal; the net lane sends queued commands as soon as Wi-Fi is up).
static app_logic::ReadyTracker g_ready;
// Task 127 (UNVERIFIED on device): the greyed "not fresh yet" look from the wake until the first fetch result or the
// session giving up. g_staleShown = what the last composed frame showed (detects the end for the one end refresh).
static app_logic::WakeUpdating g_wakeUpd;
static bool g_staleShown = false;

void updateStatus() {
  g_status.batteryPct = batteryPercent();
  using P = app_logic::Phase;
  P phase = P::Demo;
  int64_t lastOk = 0;
  bool failed = false;
  if (g_real) {
    phase = P::ConnectingWifi;  // before the session driver exists (first frame)
    if (g_keepStatus && g_drv && g_drv->status() != session::Status::Connecting) g_keepStatus = false;  // resolved
    if (g_keepStatus && app_logic::plausibleEpoch(g_restoredEpoch)) {
      // No transient "connecting" after a deep-sleep wake: the cached time is the last known state until
      // the session resolves (Online, Offline or AuthError).
      phase = P::Online;
      lastOk = g_restoredEpoch;
    } else if (g_drv) {
      lastOk = g_drv->lastOkEpoch() ? g_drv->lastOkEpoch() : g_restoredEpoch;
      failed = g_drv->lastFetchFailed();
      switch (g_drv->status()) {
        case session::Status::Connecting: phase = g_drv->wifiUp() ? P::Fetching : P::ConnectingWifi; break;
        case session::Status::Online: phase = g_drv->fetching() ? P::Fetching : P::Online; break;
        case session::Status::Offline: phase = P::Offline; break;
        case session::Status::AuthError: phase = P::AuthError; break;
      }
    } else {
      lastOk = g_restoredEpoch;
    }
  } else if (!g_demoMode) {
    return;
  }
  bool queued = false;
  g_status.stale = false;
  if (g_real && !g_demoMode) {
    app_logic::ReadyInputs in;
    in.interactive = !g_scheduled;
    in.wifiUp = g_drv && g_drv->wifiUp();
    in.offline = phase == P::Offline || phase == P::AuthError;
    in.fetchDone = g_drv && g_drv->sessionDone();
    in.queuedTaps = g_drv ? int(g_drv->machine().queued()) : 0;
    const bool wasStale = g_wakeUpd.stale();
    if (g_wakeUpd.stale())
      for (const uistate::Pending &p : g_app.pendingList()) g_wakeUpd.exclude(p.tab, p.widget);
    g_status.stale = g_wakeUpd.step(in, g_drv && g_drv->lastFetchFailed());
    g_status.staleSkip.clear();
    for (const ui::Placement &pl : g_app.layout().widgets)
      if (g_wakeUpd.excluded(g_app.shownTab(), pl.index)) g_status.staleSkip.push_back(pl.index);
    if (wasStale != g_status.stale)
      wakelog::logf("APP: wake_stale=%d end=%s look_refreshes=%d\n", g_status.stale ? 1 : 0,
                    g_wakeUpd.ended() == app_logic::StaleEnd::Fresh ? "fresh" : g_wakeUpd.ended() == app_logic::StaleEnd::Timeout ? "timeout" : "-",
                    g_wakeUpd.lookRefreshes());
    const app_logic::ReadyView v = g_ready.step(millis(), in);
    if (v.shown == app_logic::ReadyShown::NotReady) phase = P::NotReady;
    else if (v.shown == app_logic::ReadyShown::Ready) phase = P::Ready;
    queued = v.queued;
  }
  using L = render::Link;
  switch (phase) {
    case P::NotReady: g_status.link = L::NotReady; break;
    case P::Ready: g_status.link = L::Ready; break;
    case P::ConnectingWifi: g_status.link = L::ConnectingWifi; break;
    case P::Fetching: g_status.link = L::Fetching; break;
    case P::Online: g_status.link = L::Online; break;
    case P::Offline: g_status.link = L::Offline; break;
    case P::AuthError: g_status.link = L::AuthError; break;
    case P::Demo: g_status.link = L::Demo; break;
  }
  const int64_t nowEpoch = session_glue::wallClock();
  const app_logic::Label l = app_logic::statusLabel(phase, lastOk, nowEpoch, utcOffsetAt(nowEpoch),
                                                    settings::effectiveRefreshMin(g_settings), failed, landscape(), queued);
  g_status.label1 = l.line1;
  g_status.label2 = l.line2;
  g_status.outdated = l.stale;
  g_status.queued = queued;
}

// Keep awake indicator (STYLE.md 5.6): shown while the device will not idle-sleep; never together with the
// asleep pill. On USB uses power::usbPresent() (task 085), the same signal as the idle gate.
static void updateAwakeStatus() {
  g_status.awakeKind = 0;
  if (g_status.sleepKind || g_scheduled || !g_real) return;
  if (power::stay_awake(g_settings, power::usbPresent()))
    g_status.awakeKind = settings::effectiveKeepAwake(g_settings) == settings::KeepAwake::Always ? 2 : 1;
}

// Task 120: the status bar indicator regions (spec: portrait one rect, landscape status + pill slot).
static std::vector<screen_track::Rect> indicatorRegions() {
  if (landscape()) return {{16, 4, 144, 56}, {706, 8, 108, 48}};
  return {{0, 0, 412, 54}};
}

void composeFrame() {
  updateStatus();
  updateAwakeStatus();
  render::renderScreen(present::canvas(), g_app, g_status);
}

void drawSettingsFrame(present::Mode mode, uint32_t now) {
  render::drawSettings(present::canvas(), settings_flow::screen(), g_info);
  timedShow(mode, now);
}

// Diagnostics: after each fetch result, how many shown-tab widget members have no state.
void logFetchApplied() {
  static uint32_t seen = 0;
  if (!g_drv || g_drv->fetchSeq() == seen) return;
  seen = g_drv->fetchSeq();
  int missing = 0;
  const auto &tabs = g_app.config().tabs;
  const int tab = g_app.shownTab();
  if (tab >= 0 && size_t(tab) < tabs.size()) {
    for (size_t i = 0; i < tabs[size_t(tab)].widgets.size(); ++i)
      for (const auto *m : g_app.widgetData(int(i)).members)
      {
        if (!m) { ++missing; continue; }
        if (m->available) continue;
        char st[33];
        snprintf(st, sizeof st, "%s", m->state.c_str());
        if (m->current_position)
          Serial.printf("APP: unavailable id=%s state=%s pos=%d\n", m->id.c_str(), st, *m->current_position);
        else
          Serial.printf("APP: unavailable id=%s state=%s pos=-\n", m->id.c_str(), st);
      }
  }
  wakelog::logf("APP: fetch_applied store=%u changed=%u missing_members=%d\n",
                (unsigned)g_app.store().size(), (unsigned)g_drv->lastChanged(), missing);
}

// Task 127: rects of the widgets drawn greyed in the shown frame. They hold real greys: a refresh touching one pushes
// them (the present::setGreyRegions path of album art) instead of the thresholded black/white.
static void addStaleRegions(std::vector<screen_track::Rect> &out) {
  if (!g_status.stale) return;
  for (const ui::Placement &pl : g_app.layout().widgets)
    if (!pl.graph && !g_wakeUpd.excluded(g_app.shownTab(), pl.index) && render::staleEligible(g_app.widgetData(pl.index))) out.push_back({pl.rect.x, pl.rect.y, pl.rect.w, pl.rect.h});
}

// Task 127: before present::bootFrame (the wake frame): the greyed widgets push real greys, and the diff is refreshed as
// ONE union rect instead of one quality refresh per changed tile group.
void prepareBootFrame() {
  g_staleShown = g_status.stale;
  if (!g_status.stale) return;
  std::vector<screen_track::Rect> greys;
  addStaleRegions(greys);
  present::setGreyRegions(greys);
  present::unionNextShow();
}

void showFrame(uint32_t now) {
  if (settings_flow::active()) {
    present::setGreyRegions({});  // the settings screen has no album art
    drawSettingsFrame(present::Mode::Partial, now);
    return;
  }
  composeFrame();
  bool full = g_app.takeFull();
  const bool staleEnded = g_staleShown && !g_status.stale;
  g_staleShown = g_status.stale;
  if (staleEnded && !full) {
    // The end of the greyed look: everything is redrawn in ONE refresh; more than 6 changed values make it a GC16 full.
    if (g_wakeUpd.ended() == app_logic::StaleEnd::Fresh && app_logic::staleEndIsFull(g_drv ? int(g_drv->lastChanged()) : 0)) full = true;
    else present::unionNextShow("stale_end");
  }
  present::setLogTab(g_app.shownTab());
  sleepwake::saveTab(g_app.shownTab());  // task 091: a restart before the next sleep keeps the tab (writes only when it changed)
  {
    std::vector<screen_track::Rect> graphs;
    for (const ui::Placement &pl : g_app.layout().widgets)
      if (pl.graph) graphs.push_back({pl.rect.x, pl.rect.y, pl.rect.w, pl.rect.h});
    present::setMergeRegions(graphs);
    std::vector<screen_track::Rect> greys;  // task 107: ready album art (the 1 px edge included)
    for (const ui::Placement &pl : g_app.layout().widgets) {
      if (!pl.art) continue;
      const uistate::WidgetData wd = g_app.widgetData(pl.index);
      if (wd.artState != art::ArtState::Ready || !wd.artPx) continue;
      const uistate::MediaWidgetLayout wl = uistate::mediaWidgetLayout(pl.rect.w, true);
      greys.push_back({pl.rect.x + wl.edge.x, pl.rect.y + wl.edge.y, wl.edge.w, wl.edge.h});
    }
    addStaleRegions(greys);
    present::setGreyRegions(greys);
    present::setIndicatorRegions(indicatorRegions());
    std::vector<screen_track::Rect> taps;  // task 126 D2: widgets with an unconfirmed tap
    for (const uistate::Pending &p : g_app.pendingList()) {
      if (p.tab != g_app.shownTab()) continue;
      for (const ui::Placement &pl : g_app.layout().widgets)
        if (pl.index == p.widget) taps.push_back({pl.rect.x, pl.rect.y, pl.rect.w, pl.rect.h});
    }
    present::setTapRegions(taps);
  }
  present::setModal(g_app.brightnessOpen() || g_app.historyPopup().visible() || g_app.notifOpen() || g_app.mediaOpen() || g_app.alarmOpen() || g_app.confirmOpen());
  {  // task 123: a keypad key press refreshes only the code box
    ui::Rect kr;
    if (!full && g_app.takeKeypadHint(kr)) present::limitNextShow({kr.x, kr.y, kr.w, kr.h});
  }
  timedShow(full ? present::Mode::Full : present::Mode::Partial, now);
}
