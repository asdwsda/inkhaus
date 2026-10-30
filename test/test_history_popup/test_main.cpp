// Task 053: history popup state machine, AppState routing and the cache freshness rules.
#include <unity.h>

#include <map>
#include <string>
#include <vector>

#include "app_state.h"
#include "cache_store.h"
#include "config_model.h"
#include "history_popup.h"
#include "net_msgs.h"

using namespace uistate;
using history::Freshness;

void setUp() {}
void tearDown() {}

namespace {
const int64_t kNow = 1761696000;

history::Series makeSeries(float v, history::Kind k = history::Kind::Numeric) {
  history::Series s;
  s.kind = k;
  s.start = kNow - 86400;
  s.end = kNow;
  s.columns.resize(10);
  for (auto &c : s.columns) { c.has = true; c.min = v - 1; c.max = v + 1; c.last = v; }
  s.points = 10;
  return s;
}

HistorySpec lineSpec() {
  config::Widget w;
  w.type = config::WidgetType::Sensor;
  w.entity = "sensor.t";
  return makeHistorySpec(w, false);
}

const char *kYaml = R"(version: 1
tabs:
  - title: A
    widgets:
      - entity: sensor.temp
      - entity: binary_sensor.door
      - entity: light.lamp
      - entity: sensor.off
        history: false
      - entity: sensor.attr
        attribute: temperature
      - entity: sensor.long
        history: 72
      - entity: sensor.bars
        history:
          type: bar
)";

AppState makeApp(settings::Orientation o = settings::Orientation::Portrait) {
  auto r = config::parseConfig(kYaml);
  TEST_ASSERT_TRUE(r.ok());
  AppState a;
  a.init(r.config, o, false);
  return a;
}

ui::Point centre(const AppState &a, int idx) {
  for (auto &pl : a.layout().widgets)
    if (pl.index == idx) return {pl.rect.x + pl.rect.w / 2, pl.rect.y + pl.rect.h / 2};
  return {-1, -1};
}
}  // namespace

// ---- freshness rules ----

void test_fresh_window() {
  TEST_ASSERT_EQUAL(600, history::freshSeconds(1));
  TEST_ASSERT_EQUAL(600, history::freshSeconds(12));
  TEST_ASSERT_EQUAL(864, history::freshSeconds(24));
  TEST_ASSERT_EQUAL(25920, history::freshSeconds(720));
  TEST_ASSERT_EQUAL(Freshness::Fresh, history::classify(kNow - 800, kNow, 24));
  TEST_ASSERT_EQUAL(Freshness::Stale, history::classify(kNow - 900, kNow, 24));
  TEST_ASSERT_EQUAL(Freshness::Stale, history::classify(kNow - 10, 0, 24));        // no clock
  TEST_ASSERT_EQUAL(Freshness::Stale, history::classify(kNow + 100, kNow, 24));    // from the future
}

struct MemBackend : history::CacheBackend {
  std::map<std::string, std::vector<uint8_t>> files;
  int loads = 0, stores = 0;
  bool load(const std::string &e, std::vector<uint8_t> &out) override {
    ++loads;
    auto it = files.find(e);
    if (it == files.end()) return false;
    out = it->second;
    return true;
  }
  void store(const std::string &e, const std::vector<uint8_t> &b) override { ++stores; files[e] = b; }
};

void test_store_roundtrip_params_and_backend() {
  MemBackend be;
  history::CacheStore c(&be);
  history::Series out;
  int64_t at = 0;
  TEST_ASSERT_EQUAL(Freshness::Missing, c.lookup("sensor.t", 7, kNow, 24, out, at));
  c.put("sensor.t", 7, makeSeries(20), kNow - 100);
  TEST_ASSERT_EQUAL(1, be.stores);
  TEST_ASSERT_EQUAL(Freshness::Fresh, c.lookup("sensor.t", 7, kNow, 24, out, at));
  TEST_ASSERT_EQUAL(kNow - 100, at);
  TEST_ASSERT_EQUAL(10, (int)out.columns.size());
  TEST_ASSERT_EQUAL(Freshness::Stale, c.lookup("sensor.t", 7, kNow + 5000, 24, out, at));
  // Another parameter hash (config edited): a miss.
  TEST_ASSERT_EQUAL(Freshness::Missing, c.lookup("sensor.t", 8, kNow, 24, out, at));
  // A new instance (reboot) reads the SD copy.
  history::CacheStore c2(&be);
  TEST_ASSERT_EQUAL(Freshness::Fresh, c2.lookup("sensor.t", 7, kNow, 24, out, at));
  TEST_ASSERT_EQUAL(1, c2.memoryEntries());
  // Corrupt file: a miss.
  be.files["sensor.x"] = {1, 2, 3};
  TEST_ASSERT_EQUAL(Freshness::Missing, c2.lookup("sensor.x", 7, kNow, 24, out, at));
}

void test_store_evicts_oldest() {
  history::CacheStore c;
  for (int i = 0; i < 10; ++i) c.put("e" + std::to_string(i), 1, makeSeries(float(i)), kNow);
  TEST_ASSERT_EQUAL((int)history::kMaxCachedEntities, (int)c.memoryEntries());
  history::Series out;
  int64_t at;
  TEST_ASSERT_EQUAL(Freshness::Missing, c.lookup("e0", 1, kNow, 24, out, at));
  TEST_ASSERT_EQUAL(Freshness::Fresh, c.lookup("e9", 1, kNow, 24, out, at));
}

// ---- popup state machine ----

void test_open_without_cache_loads_then_ready() {
  HistoryPopup p;
  p.open(lineSpec(), 0, 1, 1000);
  TEST_ASSERT_EQUAL(HistState::Loading, p.state());
  p.applyCache(Freshness::Missing, history::Series(), 0);
  TEST_ASSERT_FALSE(p.visible());  // Loading hidden for the first 300 ms
  HistoryJob j;
  TEST_ASSERT_TRUE(p.takeFetch(kNow, j));
  TEST_ASSERT_FALSE(p.takeFetch(kNow, j));  // once
  TEST_ASSERT_EQUAL(kNow, j.end);
  TEST_ASSERT_EQUAL(kNow - 86400, j.start);
  TEST_ASSERT_EQUAL(392, j.columns);
  TEST_ASSERT_EQUAL(HistNote::Fetching, p.note());
  TEST_ASSERT_FALSE(p.tick(1299));
  TEST_ASSERT_TRUE(p.tick(1300));
  TEST_ASSERT_TRUE(p.visible());
  TEST_ASSERT_FALSE(p.tick(1400));  // no repeat
  TEST_ASSERT_TRUE(p.onFetchDone(j.ticket, HistOutcome::Ok, makeSeries(20), kNow + 3));
  TEST_ASSERT_EQUAL(HistState::Ready, p.state());
  TEST_ASSERT_EQUAL(HistNote::Updated, p.note());
  TEST_ASSERT_EQUAL(kNow + 3, p.dataEpoch());
  TEST_ASSERT_TRUE(p.dataChanged());
}

void test_fast_result_skips_loading_frame() {
  HistoryPopup p;
  p.open(lineSpec(), 0, 0, 0);
  HistoryJob j;
  p.takeFetch(kNow, j);
  p.tick(100);
  TEST_ASSERT_FALSE(p.visible());
  p.onFetchDone(j.ticket, HistOutcome::Ok, makeSeries(5), kNow);
  TEST_ASSERT_TRUE(p.visible());  // first visible frame is the graph
}

void test_fresh_cache_no_fetch() {
  HistoryPopup p;
  p.open(lineSpec(), 0, 0, 0);
  p.applyCache(Freshness::Fresh, makeSeries(20), kNow - 60);
  TEST_ASSERT_EQUAL(HistState::Ready, p.state());
  TEST_ASSERT_TRUE(p.visible());
  HistoryJob j;
  TEST_ASSERT_FALSE(p.takeFetch(kNow, j));
  TEST_ASSERT_FALSE(p.fetching());
}

void test_stale_cache_refreshes_changed_and_unchanged() {
  HistoryPopup p;
  p.open(lineSpec(), 0, 0, 0);
  p.applyCache(Freshness::Stale, makeSeries(20), kNow - 7200);
  TEST_ASSERT_EQUAL(HistState::Stale, p.state());
  TEST_ASSERT_TRUE(p.visible());
  HistoryJob j;
  TEST_ASSERT_TRUE(p.takeFetch(kNow, j));
  TEST_ASSERT_EQUAL(HistNote::Updating, p.note());
  TEST_ASSERT_TRUE(p.onFetchDone(j.ticket, HistOutcome::Ok, makeSeries(20), kNow));
  TEST_ASSERT_EQUAL(HistState::Ready, p.state());
  TEST_ASSERT_FALSE(p.dataChanged());  // same columns: only the note changes
  TEST_ASSERT_EQUAL(kNow, p.dataEpoch());
  // Second round with other data.
  HistoryPopup q;
  q.open(lineSpec(), 0, 0, 0);
  q.applyCache(Freshness::Stale, makeSeries(20), kNow - 7200);
  q.takeFetch(kNow, j);
  q.onFetchDone(j.ticket, HistOutcome::Ok, makeSeries(21), kNow);
  TEST_ASSERT_TRUE(q.dataChanged());
  TEST_ASSERT_EQUAL_FLOAT(21, q.series().columns[0].last);
}

void test_stale_cache_offline_keeps_graph() {
  HistoryPopup p;
  p.open(lineSpec(), 0, 0, 0);
  p.applyCache(Freshness::Stale, makeSeries(20), kNow - 7200);
  HistoryJob j;
  p.takeFetch(kNow, j);
  TEST_ASSERT_TRUE(p.onFetchDone(j.ticket, HistOutcome::Unreachable, history::Series(), kNow));
  TEST_ASSERT_EQUAL(HistState::Stale, p.state());
  TEST_ASSERT_TRUE(p.offline());
  TEST_ASSERT_EQUAL(HistNote::Cached, p.note());
  TEST_ASSERT_TRUE(p.hasData());
}

void test_errors() {
  HistoryJob j;
  {  // unreachable, no cache
    HistoryPopup p;
    p.open(lineSpec(), 0, 0, 0);
    p.takeFetch(kNow, j);
    p.onFetchDone(j.ticket, HistOutcome::Unreachable, history::Series(), kNow);
    TEST_ASSERT_EQUAL(HistState::Error, p.state());
    TEST_ASSERT_EQUAL(HistReason::Unreachable, p.reason());
  }
  {  // too many points, also with a stale cache: no partial graph
    HistoryPopup p;
    p.open(lineSpec(), 0, 0, 0);
    p.applyCache(Freshness::Stale, makeSeries(1), kNow - 9000);
    p.takeFetch(kNow, j);
    p.onFetchDone(j.ticket, HistOutcome::TooMany, history::Series(), kNow);
    TEST_ASSERT_EQUAL(HistState::Error, p.state());
    TEST_ASSERT_EQUAL(HistReason::TooMany, p.reason());
    TEST_ASSERT_EQUAL(HistNote::TooMany, p.note());
  }
  {  // text states
    HistoryPopup p;
    p.open(lineSpec(), 0, 0, 0);
    p.takeFetch(kNow, j);
    p.onFetchDone(j.ticket, HistOutcome::Ok, makeSeries(0, history::Kind::Unsupported), kNow);
    TEST_ASSERT_EQUAL(HistReason::NoGraph, p.reason());
  }
  {  // no clock: nothing fetched
    HistoryPopup p;
    p.open(lineSpec(), 0, 0, 0);
    TEST_ASSERT_FALSE(p.takeFetch(0, j));
    TEST_ASSERT_EQUAL(HistState::Error, p.state());
    TEST_ASSERT_EQUAL(HistReason::NotFetched, p.reason());
  }
  {  // empty series is Ready (the renderer says "No data")
    HistoryPopup p;
    p.open(lineSpec(), 0, 0, 0);
    p.takeFetch(kNow, j);
    history::Series e;
    e.kind = history::Kind::Numeric;
    p.onFetchDone(j.ticket, HistOutcome::Ok, std::move(e), kNow);
    TEST_ASSERT_EQUAL(HistState::Ready, p.state());
    TEST_ASSERT_TRUE(p.series().empty());
  }
}

void test_close_cancels_and_late_result_is_ignored() {
  HistoryPopup p;
  p.open(lineSpec(), 0, 0, 0);
  HistoryJob j;
  p.takeFetch(kNow, j);
  TEST_ASSERT_TRUE(p.close());
  TEST_ASSERT_FALSE(p.isOpen());
  TEST_ASSERT_TRUE(p.takeCancel());
  TEST_ASSERT_FALSE(p.takeCancel());
  TEST_ASSERT_FALSE(p.onFetchDone(j.ticket, HistOutcome::Ok, makeSeries(1), kNow));
  TEST_ASSERT_FALSE(p.close());
  // Reopen: the old job's result must not land in the new popup.
  p.open(lineSpec(), 0, 0, 100);
  HistoryJob j2;
  p.takeFetch(kNow, j2);
  TEST_ASSERT_TRUE(j2.ticket != j.ticket);
  TEST_ASSERT_FALSE(p.onFetchDone(j.ticket, HistOutcome::Ok, makeSeries(1), kNow));
  TEST_ASSERT_TRUE(p.fetching());
  // Closing a popup without a running fetch needs no cancel.
  HistoryPopup q;
  q.open(lineSpec(), 0, 0, 0);
  q.applyCache(Freshness::Fresh, makeSeries(1), kNow);
  q.close();
  TEST_ASSERT_FALSE(q.takeCancel());
}

void test_spec_chart_choice_and_columns() {
  auto r = config::parseConfig(kYaml);
  const auto &ws = r.config.tabs[0].widgets;
  HistorySpec s = makeHistorySpec(ws[0], false);
  TEST_ASSERT_EQUAL(HistChart::Line, s.chart);
  TEST_ASSERT_EQUAL(392, s.columns);
  TEST_ASSERT_EQUAL(764, makeHistorySpec(ws[0], true).columns);
  s = makeHistorySpec(ws[1], false);
  TEST_ASSERT_EQUAL(HistChart::Step, s.chart);
  TEST_ASSERT_EQUAL(452, s.columns);
  TEST_ASSERT_EQUAL(824, makeHistorySpec(ws[1], true).columns);
  TEST_ASSERT_EQUAL(72, makeHistorySpec(ws[5], false).hours);
  TEST_ASSERT_TRUE(makeHistorySpec(ws[6], false).source == history::Source::Statistics);
  // The params hash follows the config.
  TEST_ASSERT_TRUE(history::paramsHash(makeHistorySpec(ws[0], false).params) !=
                   history::paramsHash(makeHistorySpec(ws[0], true).params));
  HistoryPopup p;
  p.open(makeHistorySpec(ws[6], false), 0, 6, 0);
  TEST_ASSERT_EQUAL(HistState::Loading, p.state());
  HistoryJob j;
  TEST_ASSERT_TRUE(p.takeFetch(kNow, j));
  TEST_ASSERT_TRUE(j.statistics);
  TEST_ASSERT_TRUE(j.bar);
}

void test_source_choice_and_jobs() {
  auto r = config::parseConfig(kYaml);
  const auto &ws = r.config.tabs[0].widgets;
  // 24 h sensor: history. 72 h: statistics, hour line. Binary: history.
  HistorySpec a = makeHistorySpec(ws[0], false);
  TEST_ASSERT_TRUE(a.source == history::Source::History);
  HistorySpec l = makeHistorySpec(ws[5], false);
  TEST_ASSERT_TRUE(l.source == history::Source::Statistics);
  TEST_ASSERT_EQUAL(int(StatsPeriod::Hour), int(l.period));
  TEST_ASSERT_TRUE(makeHistorySpec(ws[1], false).source == history::Source::History);
  HistoryPopup p;
  p.open(l, 0, 5, 0);
  HistoryJob j;
  TEST_ASSERT_TRUE(p.takeFetch(kNow, j));
  TEST_ASSERT_TRUE(j.statistics);
  TEST_ASSERT_FALSE(j.bar);
  TEST_ASSERT_EQUAL(kNow - 72 * 3600, j.start);
  TEST_ASSERT_EQUAL(l.columns, j.columns);
  // History job: no statistics flags.
  HistoryPopup q;
  q.open(a, 0, 0, 0);
  TEST_ASSERT_TRUE(q.takeFetch(kNow, j));
  TEST_ASSERT_FALSE(j.statistics);
  // Bar: aligned window of 24 hourly buckets holding now.
  HistoryPopup b;
  b.open(makeHistorySpec(ws[6], false), 0, 6, 0);
  TEST_ASSERT_TRUE(b.takeFetch(kNow, j));
  TEST_ASSERT_EQUAL(int(StatsPeriod::Hour), int(j.period));
  TEST_ASSERT_EQUAL(3600, j.bucketSeconds);
  TEST_ASSERT_EQUAL(24, j.bucketCount);
  TEST_ASSERT_EQUAL(0, j.start % 3600);
  TEST_ASSERT_TRUE(j.start <= kNow - 23 * 3600 && j.start > kNow - 24 * 3600 - 3600);
}

void test_no_statistics_outcome() {
  auto r = config::parseConfig(kYaml);
  HistoryPopup p;
  p.open(makeHistorySpec(r.config.tabs[0].widgets[6], false), 0, 6, 0);
  HistoryJob j;
  p.takeFetch(kNow, j);
  TEST_ASSERT_TRUE(p.onFetchDone(j.ticket, HistOutcome::NoStatistics, history::Series(), kNow));
  TEST_ASSERT_EQUAL(HistState::Error, p.state());
  TEST_ASSERT_EQUAL(HistReason::NoStatistics, p.reason());
}

void test_bar_window() {
  const int64_t now = 1761696000 + 5 * 3600 + 123;  // 05:02:03 on a UTC midnight
  history::BarWindow w = history::barWindow(now, 24, 3600);
  TEST_ASSERT_EQUAL(24, w.count);
  TEST_ASSERT_EQUAL(1761696000 + 5 * 3600 - 23 * 3600, w.start);
  w = history::barWindow(now, 720, 86400);
  TEST_ASSERT_EQUAL(30, w.count);
  TEST_ASSERT_EQUAL(0, w.start % 86400);
  w = history::barWindow(now, 720, 3600);  // clamped to kMaxBuckets
  TEST_ASSERT_EQUAL(history::kMaxBuckets, w.count);
  w = history::barWindow(now, 1, 86400);
  TEST_ASSERT_EQUAL(1, w.count);
  TEST_ASSERT_EQUAL(now - now % 86400, w.start);
}

void test_bar_window_local_days() {
  const int64_t now = 1761696000 + 5 * 3600 + 123;  // 05:02 UTC
  // CET (+1 h): local 06:02, local midnight is 23:00 UTC the day before.
  history::BarWindow w = history::barWindow(now, 720, 86400, 3600);
  TEST_ASSERT_EQUAL(30, w.count);
  TEST_ASSERT_EQUAL(1761696000 - 3600 - 29 * 86400, w.start);
  TEST_ASSERT_EQUAL(0, (w.start + 3600) % 86400);
  // UTC-5 at 02:00 UTC is still the previous local day.
  w = history::barWindow(1761696000 + 2 * 3600, 48, 86400, -5 * 3600);
  TEST_ASSERT_EQUAL(2, w.count);
  TEST_ASSERT_EQUAL(1761696000 + 5 * 3600 - 2 * 86400, w.start);  // local midnight of Oct 27
}

void test_layout_hit() {
  HistoryLayout l = historyLayout(settings::Orientation::Portrait);
  TEST_ASSERT_EQUAL(HistoryHit::Close, historyHitTest(l, {470, 250}));
  TEST_ASSERT_EQUAL(HistoryHit::Panel, historyHitTest(l, {100, 500}));
  TEST_ASSERT_EQUAL(HistoryHit::Outside, historyHitTest(l, {100, 100}));
  l = historyLayout(settings::Orientation::Landscape);
  TEST_ASSERT_EQUAL(HistoryHit::Close, historyHitTest(l, {870, 130}));
  TEST_ASSERT_EQUAL(HistoryHit::Outside, historyHitTest(l, {10, 10}));
}

// ---- AppState routing ----

void test_app_long_press_opens_for_sensor_and_binary_only() {
  AppState a = makeApp();
  TEST_ASSERT_TRUE(a.longPress(centre(a, 0), 0));  // sensor
  TEST_ASSERT_TRUE(a.historyOpen());
  TEST_ASSERT_TRUE(a.modalOpen());
  TEST_ASSERT_EQUAL_STRING("sensor.temp", a.history().entity().c_str());
  TEST_ASSERT_TRUE(a.closeHistory());
  TEST_ASSERT_TRUE(a.longPress(centre(a, 1), 0));  // binary sensor
  TEST_ASSERT_EQUAL(HistChart::Step, a.history().spec().chart);
  a.closeHistory();
  TEST_ASSERT_FALSE(a.longPress(centre(a, 2), 0));  // light, not dimmable: nothing
  TEST_ASSERT_FALSE(a.longPress(centre(a, 3), 0));  // history: false
  TEST_ASSERT_FALSE(a.longPress(centre(a, 4), 0));  // attribute widget
  TEST_ASSERT_FALSE(a.historyOpen());
}

void test_app_lights_keep_brightness_popup() {
  AppState a = makeApp();
  EntityState l;
  l.id = "light.lamp";
  l.state = "on";
  l.available = true;
  l.brightness = 128;
  l.color_modes = "brightness";
  a.store().update({l});
  TEST_ASSERT_TRUE(a.dimmableLightAt(centre(a, 2)));
  TEST_ASSERT_TRUE(a.longPress(centre(a, 2), 0));
  TEST_ASSERT_TRUE(a.brightnessOpen());
  TEST_ASSERT_FALSE(a.historyOpen());
}

void test_app_modal_input() {
  AppState a = makeApp();
  a.longPress(centre(a, 0), 0);
  // UP / DOWN do nothing, a tap inside the panel does nothing.
  TEST_ASSERT_FALSE(a.rocker(ui::PhysicalKey::Up, 5));
  TEST_ASSERT_FALSE(a.rocker(ui::PhysicalKey::Down, 5));
  TEST_ASSERT_FALSE(a.touch({100, 500}, 6));
  TEST_ASSERT_TRUE(a.historyOpen());
  // A second long press while open does not open anything else.
  TEST_ASSERT_FALSE(a.longPress(centre(a, 1), 7));
  TEST_ASSERT_EQUAL_STRING("sensor.temp", a.history().entity().c_str());
  // Close button.
  TEST_ASSERT_TRUE(a.touch({470, 250}, 8));
  TEST_ASSERT_FALSE(a.historyOpen());
  // Tap outside closes without acting on the tab behind (a tab bar tap must not switch tabs).
  a.longPress(centre(a, 0), 9);
  const int tab = a.shownTab();
  TEST_ASSERT_TRUE(a.touch({100, 100}, 10));
  TEST_ASSERT_FALSE(a.historyOpen());
  TEST_ASSERT_EQUAL(tab, a.shownTab());
  // Rocker push and long push close.
  a.longPress(centre(a, 0), 11);
  TEST_ASSERT_TRUE(a.rocker(ui::PhysicalKey::Push, 12));
  TEST_ASSERT_FALSE(a.historyOpen());
  a.longPress(centre(a, 0), 13);
  TEST_ASSERT_TRUE(a.rocker(ui::PhysicalKey::LongPush, 14));
  TEST_ASSERT_FALSE(a.historyOpen());
}

void test_app_tick_reveals_loading() {
  AppState a = makeApp();
  a.longPress(centre(a, 0), 1000);
  TEST_ASSERT_FALSE(a.tick(1100));
  TEST_ASSERT_TRUE(a.tick(1300));
  TEST_ASSERT_TRUE(a.history().visible());
  TEST_ASSERT_FALSE(a.tick(1400));
}

void test_app_rocker_open_needs_focus() {
  AppState a = makeApp();
  TEST_ASSERT_FALSE(a.openHistoryFocused(0));  // no focus ring
}

void test_one_modal_at_a_time() {  // F2
  AppState a = makeApp();
  EntityState l;
  l.id = "light.lamp"; l.state = "on"; l.available = true; l.brightness = 128; l.color_modes = "brightness";
  a.store().update({l});
  a.longPress(centre(a, 0), 0);
  TEST_ASSERT_TRUE(a.historyOpen());
  TEST_ASSERT_FALSE(a.dimmableLightAt(centre(a, 2)));
  TEST_ASSERT_FALSE(a.longPress(centre(a, 2), 1));
  TEST_ASSERT_FALSE(a.brightnessOpen());
  TEST_ASSERT_TRUE(a.historyOpen());
  TEST_ASSERT_EQUAL_STRING("sensor.temp", a.history().entity().c_str());
}

void test_timeout_without_bytes_keeps_cache() {  // F3
  TEST_ASSERT_FALSE(app_logic::histTooMany(HaError::Timeout, 0));
  TEST_ASSERT_TRUE(app_logic::histTooMany(HaError::Timeout, 1000));
  TEST_ASSERT_TRUE(app_logic::histTooMany(HaError::TooLarge, 0));
  TEST_ASSERT_FALSE(app_logic::histTooMany(HaError::Network, 500));
  HistoryPopup p;  // such a failure arrives as Unreachable: the cached graph stays
  p.open(lineSpec(), 0, 0, 0);
  p.applyCache(Freshness::Stale, makeSeries(20), kNow - 7200);
  HistoryJob j;
  p.takeFetch(kNow, j);
  p.onFetchDone(j.ticket, HistOutcome::Unreachable, history::Series(), kNow);
  TEST_ASSERT_EQUAL(HistState::Stale, p.state());
  TEST_ASSERT_TRUE(p.hasData());
}

void test_same_columns_tolerance() {  // F4
  HistoryPopup p;
  p.open(lineSpec(), 0, 0, 0);
  p.applyCache(Freshness::Stale, makeSeries(20), kNow - 7200);
  HistoryJob j;
  p.takeFetch(kNow, j);
  const float step = 2.0f / 65534.0f;  // the cache quantisation step for a range of 2
  history::Series s = makeSeries(20);
  for (auto &c : s.columns) { c.min += step * 0.5f; c.max -= step * 0.5f; c.last += step * 0.5f; }
  TEST_ASSERT_TRUE(p.onFetchDone(j.ticket, HistOutcome::Ok, std::move(s), kNow));
  TEST_ASSERT_FALSE(p.dataChanged());
  // A real change is still a change.
  HistoryPopup q;
  q.open(lineSpec(), 0, 0, 0);
  q.applyCache(Freshness::Stale, makeSeries(20), kNow - 7200);
  q.takeFetch(kNow, j);
  TEST_ASSERT_TRUE(q.onFetchDone(j.ticket, HistOutcome::Ok, makeSeries(20.5f), kNow));
  TEST_ASSERT_TRUE(q.dataChanged());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fresh_window);
  RUN_TEST(test_store_roundtrip_params_and_backend);
  RUN_TEST(test_store_evicts_oldest);
  RUN_TEST(test_open_without_cache_loads_then_ready);
  RUN_TEST(test_fast_result_skips_loading_frame);
  RUN_TEST(test_fresh_cache_no_fetch);
  RUN_TEST(test_stale_cache_refreshes_changed_and_unchanged);
  RUN_TEST(test_stale_cache_offline_keeps_graph);
  RUN_TEST(test_errors);
  RUN_TEST(test_close_cancels_and_late_result_is_ignored);
  RUN_TEST(test_spec_chart_choice_and_columns);
  RUN_TEST(test_layout_hit);
  RUN_TEST(test_app_long_press_opens_for_sensor_and_binary_only);
  RUN_TEST(test_app_lights_keep_brightness_popup);
  RUN_TEST(test_app_modal_input);
  RUN_TEST(test_app_tick_reveals_loading);
  RUN_TEST(test_app_rocker_open_needs_focus);
  RUN_TEST(test_one_modal_at_a_time);
  RUN_TEST(test_timeout_without_bytes_keeps_cache);
  RUN_TEST(test_same_columns_tolerance);
  RUN_TEST(test_source_choice_and_jobs);
  RUN_TEST(test_no_statistics_outcome);
  RUN_TEST(test_bar_window);
  RUN_TEST(test_bar_window_local_days);
  return UNITY_END();
}
