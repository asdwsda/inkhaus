// Task 071: history graph widgets (`display: graph`): which graphs a session fetches, the per-widget state
// machine, taps/long press doing nothing, and the demo/cache entry points.
#include <unity.h>

#include "app_state.h"
#include "graph_widgets.h"

using namespace uistate;

void setUp() {}
void tearDown() {}

namespace {
const int64_t kNow = 1761696000;

const char *kYaml = R"(version: 1
tabs:
  - title: A
    widgets:
      - entity: sensor.a1
        display: graph
      - entity: sensor.plain
      - entity: binary_sensor.a2
        display: graph
        size: 2x1
  - title: B
    widgets:
      - entity: sensor.plain2
  - title: C
    widgets:
      - entity: sensor.c1
        display: graph
        history: { type: bar, hours: 168 }
)";

AppState makeApp(settings::Orientation o = settings::Orientation::Portrait) {
  auto r = config::parseConfig(kYaml);
  TEST_ASSERT_TRUE(r.ok());
  AppState a;
  a.init(r.config, o, false);
  return a;
}

history::Series series(float v, size_t cols = 424) {
  history::Series s;
  s.kind = history::Kind::Numeric;
  s.start = kNow - 86400;
  s.end = kNow;
  s.columns.resize(cols);
  for (auto &c : s.columns) { c.has = true; c.min = v - 1; c.max = v + 1; c.last = v; }
  s.points = cols;
  return s;
}

// Runs every wanted job to the given outcome; returns the entities fetched in order.
std::vector<std::string> drain(GraphWidgets &g, HistOutcome o = HistOutcome::Ok, float v = 5) {
  std::vector<std::string> out;
  HistoryJob job;
  while (g.takeJob(kNow, 0, job)) {
    out.push_back(job.entity);
    TEST_ASSERT_TRUE(job.ticket >= kGraphTicketBase);
    g.onFetchDone(job.ticket, o, series(v, size_t(job.columns)), kNow);
  }
  return out;
}
}  // namespace

void test_entries_built_for_graph_widgets_only() {
  AppState a = makeApp();
  const GraphWidgets &g = a.graphs();
  TEST_ASSERT_EQUAL(3, (int)g.entries().size());  // a1, a2 (tab A) and c1 (tab C)
  TEST_ASSERT_NOT_NULL(g.find(0, 0));
  TEST_ASSERT_NULL(g.find(0, 1));  // plain sensor
  TEST_ASSERT_NOT_NULL(g.find(0, 2));
  TEST_ASSERT_NULL(g.find(1, 0));
  TEST_ASSERT_NOT_NULL(g.find(2, 0));
  TEST_ASSERT_TRUE(g.tabHasGraphs(0) && !g.tabHasGraphs(1) && g.tabHasGraphs(2));
}

void test_columns_follow_widget_size() {
  AppState p = makeApp();
  TEST_ASSERT_EQUAL(424, p.graphs().find(0, 0)->spec.columns);  // 2x2 portrait: 508 - 84
  TEST_ASSERT_EQUAL(263, p.graphs().find(0, 2)->spec.columns);  // 2x1 compact spark: 508 - 17 - 228
  TEST_ASSERT_EQUAL(p.graphs().find(0, 2)->spec.params.columns, p.graphs().find(0, 2)->spec.columns);
  AppState l = makeApp(settings::Orientation::Landscape);
  TEST_ASSERT_EQUAL(280, l.graphs().find(0, 0)->spec.columns);  // 2x2 landscape: 364 - 84
}

void test_session_requests_only_visible_tab() {
  AppState a = makeApp();
  GraphWidgets &g = a.graphs();
  TEST_ASSERT_FALSE(g.busy());
  TEST_ASSERT_EQUAL(2, g.requestSession(0, false));
  TEST_ASSERT_TRUE(g.busy());
  const auto fetched = drain(g);
  TEST_ASSERT_EQUAL(2, (int)fetched.size());
  TEST_ASSERT_EQUAL_STRING("sensor.a1", fetched[0].c_str());
  TEST_ASSERT_EQUAL_STRING("binary_sensor.a2", fetched[1].c_str());
  TEST_ASSERT_FALSE(g.busy());
  TEST_ASSERT_TRUE(g.find(2, 0)->state == GraphState::Loading);  // tab C was not fetched
}

void test_tab_without_graphs_requests_nothing() {
  AppState a = makeApp();
  GraphWidgets &g = a.graphs();
  TEST_ASSERT_EQUAL(0, g.requestSession(1, false));
  TEST_ASSERT_FALSE(g.busy());
  HistoryJob job;
  TEST_ASSERT_FALSE(g.takeJob(kNow, 0, job));
}

void test_tab_switch_requests_new_tab_and_drops_old() {
  AppState a = makeApp();
  GraphWidgets &g = a.graphs();
  g.requestSession(0, false);
  HistoryJob job;
  TEST_ASSERT_TRUE(g.takeJob(kNow, 0, job));  // sensor.a1 in flight
  g.requestTab(2);                            // the user switched before the second one started
  // the queued binary_sensor.a2 request is gone; the running fetch still finishes
  g.onFetchDone(job.ticket, HistOutcome::Ok, series(1), kNow);
  const auto fetched = drain(g);
  TEST_ASSERT_EQUAL(1, (int)fetched.size());
  TEST_ASSERT_EQUAL_STRING("sensor.c1", fetched[0].c_str());
  TEST_ASSERT_TRUE(g.find(0, 0)->state == GraphState::Ready);
  TEST_ASSERT_TRUE(g.find(0, 2)->state == GraphState::Loading);  // never fetched
}

void test_all_tabs_option() {
  AppState a = makeApp();
  GraphWidgets &g = a.graphs();
  TEST_ASSERT_EQUAL(3, g.requestSession(0, true));
  TEST_ASSERT_EQUAL(3, (int)drain(g).size());
  // the shipped default is the visible tab only
  TEST_ASSERT_FALSE(kGraphFetchAllTabs);
}

void test_one_fetch_at_a_time() {
  AppState a = makeApp();
  GraphWidgets &g = a.graphs();
  g.requestSession(0, false);
  HistoryJob j1, j2;
  TEST_ASSERT_TRUE(g.takeJob(kNow, 0, j1));
  TEST_ASSERT_FALSE(g.takeJob(kNow, 0, j2));
  TEST_ASSERT_FALSE(g.onFetchDone(j1.ticket + 99, HistOutcome::Ok, series(1), kNow));  // unknown ticket
  TEST_ASSERT_TRUE(g.onFetchDone(j1.ticket, HistOutcome::Ok, series(1), kNow));
  TEST_ASSERT_TRUE(g.takeJob(kNow, 0, j2));
  TEST_ASSERT_TRUE(j2.ticket != j1.ticket);
}

void test_result_and_unchanged_data() {
  AppState a = makeApp();
  GraphWidgets &g = a.graphs();
  g.requestSession(0, false);
  drain(g, HistOutcome::Ok, 5);
  TEST_ASSERT_TRUE(g.takeChanged());
  TEST_ASSERT_FALSE(g.takeChanged());
  const GraphEntry *e = g.find(0, 0);
  TEST_ASSERT_TRUE(e->state == GraphState::Ready && e->hasData && !e->stale);
  // same data again: nothing to redraw
  g.requestSession(0, false);
  drain(g, HistOutcome::Ok, 5);
  TEST_ASSERT_FALSE(g.takeChanged());
  // changed data
  g.requestSession(0, false);
  drain(g, HistOutcome::Ok, 9);
  TEST_ASSERT_TRUE(g.takeChanged());
  TEST_ASSERT_EQUAL_FLOAT(9, g.find(0, 0)->series.columns[0].last);
}

void test_failures() {
  AppState a = makeApp();
  GraphWidgets &g = a.graphs();
  g.requestSession(0, false);
  drain(g, HistOutcome::Unreachable);
  TEST_ASSERT_TRUE(g.find(0, 0)->state == GraphState::Error);
  TEST_ASSERT_TRUE(g.find(0, 0)->reason == GraphReason::Unreachable);
  // with data, a failure only marks it stale
  g.requestSession(0, false);
  drain(g, HistOutcome::Ok, 5);
  g.requestSession(0, false);
  drain(g, HistOutcome::Unreachable);
  TEST_ASSERT_TRUE(g.find(0, 0)->state == GraphState::Ready);
  TEST_ASSERT_TRUE(g.find(0, 0)->stale && g.find(0, 0)->hasData);
  g.takeChanged();
  // a good fetch clears the marker and is a visible change even with the same data
  g.requestSession(0, false);
  drain(g, HistOutcome::Ok, 5);
  TEST_ASSERT_FALSE(g.find(0, 0)->stale);
  TEST_ASSERT_TRUE(g.takeChanged());
  // too many points
  g.requestSession(0, false);
  drain(g, HistOutcome::TooMany);
  TEST_ASSERT_TRUE(g.find(0, 0)->reason == GraphReason::TooMany && !g.find(0, 0)->hasData);
}

void test_unsupported_kind_is_no_graph() {
  AppState a = makeApp();
  GraphWidgets &g = a.graphs();
  g.requestSession(0, false);
  HistoryJob job;
  TEST_ASSERT_TRUE(g.takeJob(kNow, 0, job));
  history::Series s = series(1);
  s.kind = history::Kind::Unsupported;
  g.onFetchDone(job.ticket, HistOutcome::Ok, std::move(s), kNow);
  TEST_ASSERT_TRUE(g.find(0, 0)->reason == GraphReason::NoGraph);
}

void test_no_clock_cannot_plan() {
  AppState a = makeApp();
  GraphWidgets &g = a.graphs();
  g.requestSession(0, false);
  HistoryJob job;
  TEST_ASSERT_FALSE(g.takeJob(0, 0, job));
  TEST_ASSERT_FALSE(g.busy());
  TEST_ASSERT_TRUE(g.find(0, 0)->reason == GraphReason::NotFetched);
  TEST_ASSERT_TRUE(g.takeChanged());
}

void test_fetch_failed_marks_requested_tab() {
  AppState a = makeApp();
  GraphWidgets &g = a.graphs();
  TEST_ASSERT_TRUE(g.fetchFailed(0, false));
  TEST_ASSERT_TRUE(g.find(0, 0)->reason == GraphReason::Unreachable);
  TEST_ASSERT_TRUE(g.find(2, 0)->state == GraphState::Loading);  // other tab untouched
  TEST_ASSERT_FALSE(g.busy());
}

void test_cache_and_demo_data() {
  AppState a = makeApp();
  GraphWidgets &g = a.graphs();
  TEST_ASSERT_TRUE(g.applyCache(0, 0, series(3), kNow - 100));
  TEST_ASSERT_TRUE(g.find(0, 0)->state == GraphState::Ready);
  TEST_ASSERT_TRUE(g.takeChanged());
  TEST_ASSERT_FALSE(g.applyCache(0, 0, series(4), kNow));  // already has data
  g.requestSession(0, false);
  drain(g, HistOutcome::Ok, 3);  // the cache was current: no redraw for it
  g.takeChanged();
  g.requestSession(0, false);
  drain(g, HistOutcome::Ok, 3);
  TEST_ASSERT_FALSE(g.takeChanged());
  TEST_ASSERT_TRUE(g.setData(2, 0, series(7), kNow));
  TEST_ASSERT_TRUE(g.find(2, 0)->state == GraphState::Ready);
  TEST_ASSERT_FALSE(g.setData(1, 0, series(7), kNow));
}

void test_tap_and_long_press_do_nothing() {
  AppState a = makeApp();
  const ui::Rect r = a.layout().widgets[0].rect;
  const ui::Point p{r.x + r.w / 2, r.y + r.h / 2};
  TEST_ASSERT_FALSE(a.historyWidgetAt(p));
  TEST_ASSERT_FALSE(a.dimmableLightAt(p));
  TEST_ASSERT_FALSE(a.longPress(p, 1000));
  TEST_ASSERT_FALSE(a.touch(p, 1000));
  TEST_ASSERT_FALSE(a.historyOpen());
  TEST_ASSERT_FALSE(a.pendingAny());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_entries_built_for_graph_widgets_only);
  RUN_TEST(test_columns_follow_widget_size);
  RUN_TEST(test_session_requests_only_visible_tab);
  RUN_TEST(test_tab_without_graphs_requests_nothing);
  RUN_TEST(test_tab_switch_requests_new_tab_and_drops_old);
  RUN_TEST(test_all_tabs_option);
  RUN_TEST(test_one_fetch_at_a_time);
  RUN_TEST(test_result_and_unchanged_data);
  RUN_TEST(test_failures);
  RUN_TEST(test_unsupported_kind_is_no_graph);
  RUN_TEST(test_no_clock_cannot_plan);
  RUN_TEST(test_fetch_failed_marks_requested_tab);
  RUN_TEST(test_cache_and_demo_data);
  RUN_TEST(test_tap_and_long_press_do_nothing);
  return UNITY_END();
}
