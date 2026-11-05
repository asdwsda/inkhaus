// History popup, graph widgets and notifications glue (per-loop pumps). Split from main.cpp (task 124e).
#include "app_internal.h"

// ---- history popup (task 053, docs/ui/history.md) ----
// The popup model lives in AppState; this glue does the cache lookup, hands the fetch to the net task and
// feeds the result back. PSRAM map + SD files per entity (lib/history cache_store).
static history_cache_sd::Backend g_histSd;
static history::CacheStore g_histCache(&g_histSd);
std::shared_ptr<volatile bool> g_histCancel;  // set when the popup closes during a fetch
static uistate::HistoryJob g_histJob;
bool g_histWaitNet = false;  // a job is planned but Wi-Fi is not up yet
bool g_histChanged = false;  // a result arrived: redraw

static uint32_t histHash() { return history::paramsHash(g_app.history().spec().params); }

static uistate::HistOutcome histOutcome(HaError e, size_t bytes, bool noStatistics) {
  if (noStatistics) return uistate::HistOutcome::NoStatistics;
  if (app_logic::histTooMany(e, bytes)) return uistate::HistOutcome::TooMany;  // the 512 KB / 15 s limits
  switch (e) {
    case HaError::None: return uistate::HistOutcome::Ok;
    case HaError::Cancelled: return uistate::HistOutcome::Cancelled;
    case HaError::Parse: return uistate::HistOutcome::Unavailable;
    default: return uistate::HistOutcome::Unreachable;
  }
}

static void graphsDone(app_logic::NetEvent &&ev);

void onHistoryDone(app_logic::NetEvent &&ev) {
  if (ev.ticket >= uistate::kGraphTicketBase) {  // a graph widget's fetch (task 071)
    graphsDone(std::move(ev));
    return;
  }
  uistate::HistoryPopup &h = g_app.historyPopup();
  const int64_t epoch = session_glue::wallClock();
  const uistate::HistOutcome o = histOutcome(ev.error, ev.bytes, ev.noStatistics);
  const std::string id = h.entity();
  if (h.onFetchDone(ev.ticket, o, std::move(ev.series), epoch)) {
    g_histChanged = true;
    if (o == uistate::HistOutcome::Cancelled)  // task 116: dropped by yielding; plan the fetch again if there is no data
      g_histWaitNet = h.takeFetch(epoch, g_histJob, utcOffsetAt(epoch));
    if (o == uistate::HistOutcome::Ok && h.state() == uistate::HistState::Ready)
      g_histCache.put(id, histHash(), h.series(), epoch);  // UNVERIFIED: the SD write (~tens of ms) is fine here
  }
}

// Just after the popup opened: cache lookup (shown at once when found), then plan the fetch.
void historyOpened() {
  uistate::HistoryPopup &h = g_app.historyPopup();
  if (g_demoMode) {  // task 068: synthetic series, no cache, no network; the clock may be unset, anchor to a fixed day then
    const int64_t now = session_glue::wallClock() ? session_glue::wallClock() : 1793361600;
    h.applyCache(history::Freshness::Missing, history::Series(), 0);
    uistate::HistoryJob job;
    g_histWaitNet = false;
    if (!h.takeFetch(now, job, utcOffsetAt(now))) return;
    history::DemoParams d;
    d.entity = job.entity;
    d.binary = h.spec().binary;
    d.bar = job.bar;
    d.start = job.start; d.end = job.end;
    d.columns = job.columns;
    d.bucketSeconds = job.bucketSeconds; d.bucketCount = job.bucketCount;
    h.onFetchDone(job.ticket, uistate::HistOutcome::Ok, history::demoSeries(d), now);
    g_histChanged = true;
    return;
  }
  history::Series s;
  int64_t at = 0;
  const history::Freshness f =
      g_histCache.lookup(h.entity(), histHash(), session_glue::wallClock(), h.spec().hours, s, at);
  h.applyCache(f, s, at);
  g_histWaitNet = h.takeFetch(session_glue::wallClock(), g_histJob, utcOffsetAt(session_glue::wallClock()));
}

// Every loop: starts a planned fetch once Wi-Fi is up, fails it when there is no way to get it, and aborts a
// running one when the popup was closed. The popup being open keeps the session awake (see loop()).
void historyPump() {
  if (g_app.historyPopup().takeCancel()) {
    if (g_histCancel) *g_histCancel = true;
    g_histWaitNet = false;
  }
  if (!g_histWaitNet) return;
  if (!g_real || !g_drv || g_drv->status() == session::Status::Offline ||
      g_drv->status() == session::Status::AuthError) {
    g_histWaitNet = false;
    if (g_app.historyPopup().fetchUnavailable()) g_histChanged = true;
    return;
  }
  if (!g_drv->wifiUp()) return;
  g_histWaitNet = false;
  g_histCancel = std::make_shared<volatile bool>(false);
  app_logic::NetCommand c;
  c.type = app_logic::NetCommand::Type::History;
  c.ticket = g_histJob.ticket;
  c.histEntity = g_histJob.entity;
  c.histStart = g_histJob.start;
  c.histEnd = g_histJob.end;
  c.histColumns = g_histJob.columns;
  c.histStats = g_histJob.statistics;
  c.histBar = g_histJob.bar;
  c.histPeriod = g_histJob.period;
  c.histStatistic = g_histJob.statistic;
  c.histBucketSeconds = g_histJob.bucketSeconds;
  c.histBucketCount = g_histJob.bucketCount;
  c.histCancel = g_histCancel;
  g_drv->postHistory(c);
}

// Closes the popup and aborts its fetch (before sleep, session restart).
void historyShutdown() {
  g_app.closeHistory();
  historyPump();
}

// ---- history graph widgets (task 071, docs/ui/history-widget.md) ----
// The model (lib/ui_state graph_widgets) knows what to fetch; this glue loads cached/demo data, posts the jobs one
// at a time through the net task and feeds the results back. Graphs are fetched after the state fetch of a session
// (hook of the session driver) for the visible tab (uistate::kGraphFetchAllTabs: every tab) and when a tab becomes
// visible; the scheduled refresh's sleep waits for them (sleep hold of the driver).
uint32_t g_graphBatchMs = 0;  // when the first request of the running batch was made (0 = none)
static uint32_t g_graphJobMs = 0;    // when the running job was posted
static int g_graphBatchN = 0;

// Cached (device) or synthetic (demo) data for the graphs of `tab` that have none yet.
static void graphsLoad(int tab) {
  uistate::GraphWidgets &g = g_app.graphs();
  std::vector<std::pair<int, int>> todo;
  for (const uistate::GraphEntry &e : g.entries())
    if (e.tab == tab && !e.hasData) todo.push_back({e.tab, e.widget});
  const uint32_t t0 = millis();
  for (const auto &tw : todo) {
    const uistate::GraphEntry *e = g.find(tw.first, tw.second);
    if (!e) continue;
    const int64_t wall = session_glue::wallClock();
    if (g_demoMode) {  // synthetic series, no cache, no network; the clock may be unset, anchor to a fixed day then
      const int64_t now = wall ? wall : 1793361600;
      uistate::HistoryJob job;
      uistate::planHistoryJob(e->spec, now, utcOffsetAt(now), job);
      history::DemoParams d;
      d.entity = job.entity;
      d.binary = e->spec.binary;
      d.bar = job.bar;
      d.start = job.start; d.end = job.end;
      d.columns = job.columns;
      d.bucketSeconds = job.bucketSeconds; d.bucketCount = job.bucketCount;
      g.setData(tw.first, tw.second, history::demoSeries(d), now);
      continue;
    }
    history::Series s;
    int64_t at = 0;
    const history::Freshness f = g_histCache.lookup(e->spec.entity, g.cacheHash(*e), wall, e->spec.hours, s, at);
    if (f != history::Freshness::Missing) g.applyCache(tw.first, tw.second, s, at);
  }
  if (!todo.empty() && !g_demoMode) wakelog::logf("APP: graph_cache n=%u ms=%u\n", (unsigned)todo.size(), (unsigned)(millis() - t0));
}

// After g_app.init() and the restore of the cached states.
void graphsInit() {
  g_app.setForecastStore(&forecast_glue::store());  // task 104
  g_app.setCalendarStore(&calendar_glue::store());  // task 105
  if (g_demoMode) {
    const int64_t w = session_glue::wallClock() ? session_glue::wallClock() : 1793361600;
    forecast_glue::fillDemo(g_app.config(), forecast::dateOf(w, utcOffsetAt(w)));
    calendar_glue::fillDemo(g_app.config(), w, utcOffsetAt(w));  // task 105
  }
  g_app.rebuildGraphs();
  graphsLoad(g_app.shownTab());
  g_app.graphs().takeChanged();  // the first frame is composed anyway
}

static void graphsRequest(int tab) {
  if (g_app.graphs().requestSession(tab, uistate::kGraphFetchAllTabs) > 0 && !g_graphBatchMs) {
    g_graphBatchMs = millis();
    g_graphBatchN = 0;
  }
}

// The session driver's hook: a full state fetch ended.
static void reportOnFetch(bool ok);

// ---- sleep screen (task 101): persistent notifications over one short WebSocket call ----
// Asked once per session after the first good state fetch, only when Settings > While asleep is Sleep screen and
// config.yaml has `sleep_screen: notifications: true`. The sleep hold waits for the answer (bounded by the driver).
bool g_notifAsked = false, g_notifPending = false, g_notifValid = false, g_notifWanted = false;
int g_notifCount = 0;
bool g_notifChanged = false;  // task 103: the widget / popup changed: redraw
std::string g_notifTitle;

static void notificationsOnFetch(bool ok) {
  if (!ok || g_demoMode || !g_drv || g_notifAsked) return;
  const bool sleepWants = g_settings.sleep_screen && g_cfg.cfg.sleepScreen.notifications;
  if (!sleepWants && !uistate::configHasNotificationsWidget(g_app.config())) return;  // task 103: or a notifications widget
  g_notifAsked = true;
  g_notifPending = true;  // the sleep hold covers the wait
  g_notifWanted = true;   // posted by notificationsPump once the graph fetches are done
}

// The WS call closes the keep-alive HTTPS socket, so it runs after the session's graph/history fetches (no extra
// TLS handshake for them). Every loop.
void notificationsPump() {
  if (!g_notifWanted || !g_drv) return;
  if (graphsBusy()) return;
  const session::Status st = g_drv->status();
  if (st == session::Status::Offline || st == session::Status::AuthError) {
    g_notifWanted = g_notifPending = false;
    g_notifChanged |= g_app.notifFetchFailed();  // task 103: the widget shows n/a (or its cached list, stale)
    return;
  }
  if (st != session::Status::Online || !g_drv->wifiUp()) return;
  g_notifWanted = false;
  app_logic::NetCommand c;
  c.type = app_logic::NetCommand::Type::Notifications;
  g_drv->postNotifications(c);
}


// Task 103: one dismiss call per id over a short WS connection (the popup marks the row pending meanwhile).
void notifDismissSink(const std::string &id) {
  if (!g_drv || g_drv->status() != session::Status::Online || !g_drv->wifiUp()) {
    g_app.notifDismissDone(id, false);  // offline: not queued (UNVERIFIED: the spec asks for a queue like other commands)
    g_notifChanged = true;
    return;
  }
  app_logic::NetCommand c;
  c.type = app_logic::NetCommand::Type::NotifDismiss;
  c.ids.push_back(id);
  g_drv->postNotifications(c);
}

void notificationsDone(app_logic::NetEvent &&ev) {
  if (ev.type == app_logic::NetEvent::Type::NotifDismissDone) {
    wakelog::logf("APP: notif_dismiss err=%d ms=%u\n", (int)ev.error, (unsigned)ev.ms);
    g_app.notifDismissDone(ev.dismissId, ev.error == HaError::None);
    g_notifChanged = true;
    return;
  }
  if (ev.error == HaError::None && ev.notif.ok) g_notifChanged |= g_app.setNotifications(std::move(ev.notifItems));
  else g_notifChanged |= g_app.notifFetchFailed();
  g_notifPending = false;
  g_notifValid = ev.error == HaError::None && ev.notif.ok;
  g_notifCount = g_notifValid ? ev.notif.count : 0;
  g_notifTitle = g_notifValid ? ev.notif.newest : std::string();
  wakelog::logf("APP: notifications err=%d count=%d ms=%u bytes=%u\n", (int)ev.error, g_notifCount, (unsigned)ev.ms,
                (unsigned)ev.bytes);
}

void graphsOnFetch(bool ok) {
  reportOnFetch(ok);  // task 100: before the graph jobs, so the sleep hold covers it
  if (!g_demoMode && g_app.graphs().any()) {
    if (ok) graphsRequest(g_app.shownTab());
    else g_app.graphs().fetchFailed(g_app.shownTab(), uistate::kGraphFetchAllTabs);
  }
  notificationsOnFetch(ok);  // after the graph request: the pump waits for graphsBusy()
}

bool graphsBusy() { return g_real && g_app.graphs().busy(); }

// Task 100: the device report (battery, temperature, humidity, last wake) after the first good state fetch of a wake.
static void reportOnFetch(bool ok) {
  if (g_demoMode || !g_drv) return;
  app_logic::NetCommand c;
  if (report_glue::onFetch(ok, batteryPercent(), batteryMv(), M5.Power.isCharging() == m5::Power_Class::is_charging,
                           session_glue::wallClock(), c))
    g_drv->postReport(c);
}

static void graphsDone(app_logic::NetEvent &&ev) {
  uistate::GraphWidgets &g = g_app.graphs();
  const int64_t epoch = session_glue::wallClock();
  const uistate::HistOutcome o = histOutcome(ev.error, ev.bytes, ev.noStatistics);
  std::string entity;
  uint32_t hash = 0;
  int tab = -1, widget = -1;
  for (const uistate::GraphEntry &e : g.entries())
    if (e.fetching && e.ticket == ev.ticket) {
      entity = e.spec.entity;
      hash = g.cacheHash(e);
      tab = e.tab;
      widget = e.widget;
    }
  const uint32_t jobMs = millis() - g_graphJobMs;
  const bool changed = g.onFetchDone(ev.ticket, o, std::move(ev.series), epoch);
  ++g_graphBatchN;
  wakelog::logf("APP: graph_fetch ms=%u net_ms=%u parse_ms=%u bytes=%u points=%u err=%d changed=%d id=%s\n",
                (unsigned)jobMs, (unsigned)ev.fetchMs, (unsigned)ev.parseMs, (unsigned)ev.bytes, (unsigned)ev.points,
                (int)ev.error, changed ? 1 : 0, entity.c_str());
  if (changed && o == uistate::HistOutcome::Ok) {
    if (const uistate::GraphEntry *e = g.find(tab, widget)) g_histCache.put(entity, hash, e->series, epoch);
  }
  if (!g.busy() && g_graphBatchMs) {  // the measurement for the "all tabs or visible only" decision (task 071)
    wakelog::logf("APP: graph_batch n=%d ms=%u all_tabs=%d\n", g_graphBatchN, (unsigned)(millis() - g_graphBatchMs),
                  uistate::kGraphFetchAllTabs ? 1 : 0);
    g_graphBatchMs = 0;
  }
}

// Every loop: a tab became visible, then the next job once Wi-Fi is up.
void graphsPump() {
  uistate::GraphWidgets &g = g_app.graphs();
  if (!g.any()) return;
  static int seenTab = -1;
  const int tab = g_app.shownTab();
  if (tab != seenTab) {
    seenTab = tab;
    graphsLoad(tab);
    if (g_real && g_drv && g_drv->sessionDone() && g.tabHasGraphs(tab)) {
      if (g_drv->status() == session::Status::Online) graphsRequest(tab);
      else g.fetchFailed(tab, false);
    }
  }
  if (g_demoMode || !g_real || !g_drv || !g.busy()) return;
  const session::Status st = g_drv->status();
  if (st == session::Status::Offline || st == session::Status::AuthError) {
    g.fetchFailed(tab, uistate::kGraphFetchAllTabs);
    g_graphBatchMs = 0;
    return;
  }
  if (st != session::Status::Online || !g_drv->wifiUp()) return;
  uistate::HistoryJob job;
  const int64_t wall = session_glue::wallClock();
  if (!g.takeJob(wall, utcOffsetAt(wall), job)) return;
  app_logic::NetCommand c;
  c.type = app_logic::NetCommand::Type::History;
  c.ticket = job.ticket;
  c.histEntity = job.entity;
  c.histStart = job.start;
  c.histEnd = job.end;
  c.histColumns = job.columns;
  c.histStats = job.statistics;
  c.histBar = job.bar;
  c.histPeriod = job.period;
  c.histStatistic = job.statistic;
  c.histBucketSeconds = job.bucketSeconds;
  c.histBucketCount = job.bucketCount;
  c.histCancel = std::make_shared<volatile bool>(false);
  g_graphJobMs = millis();
  g_drv->postHistory(c);
}
