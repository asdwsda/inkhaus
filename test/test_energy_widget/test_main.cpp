// Task 109: energy widget: config, cache, sizes, view model, layout, today's value plan, AppState wiring.
#include <unity.h>

#include <string>
#include <vector>

#include "app_state.h"
#include "config_cache.h"
#include "config_model.h"
#include "energy_view.h"
#include "history_popup.h"
#include "stats_plan.h"
#include "ui_layout.h"
#include "ui_rocker.h"

using namespace uistate;
using config::WidgetType;

void setUp() {}
void tearDown() {}

static const char *kUsage = "sensor.shellypro3em_3ce90e703834_total_active_energy";

static std::string yamlOf(const std::string &widget) {
  return "version: 1\ntabs:\n  - title: Energy\n    widgets:\n" + widget;
}

static config::ParseResult parse(const std::string &widget) { return config::parseConfig(yamlOf(widget)); }

static config::Config bothConfig() {
  config::ParseResult r = parse(
      "      - type: energy\n        usage: sensor.energy_used_today\n        solar: sensor.energy_solar_today\n");
  TEST_ASSERT_TRUE(r.ok());
  return r.config;
}

// ---- config ----

void test_config_guide_snippet_parses() {
  config::Config c = bothConfig();
  const config::Widget &w = c.tabs[0].widgets[0];
  TEST_ASSERT_TRUE(w.type == WidgetType::Energy);
  TEST_ASSERT_EQUAL_STRING("sensor.energy_used_today", w.usage.c_str());
  TEST_ASSERT_EQUAL_STRING("sensor.energy_solar_today", w.solar.c_str());
  TEST_ASSERT_EQUAL_STRING("sensor.energy_used_today", w.entity.c_str());
  TEST_ASSERT_EQUAL(2, (int)w.entities.size());
  TEST_ASSERT_EQUAL(2, (int)c.entityIds.size());
}

void test_config_single_entity_and_real_sensor() {
  config::ParseResult r = parse(std::string("      - type: energy\n        usage: ") + kUsage + "\n        name: Home\n");
  TEST_ASSERT_TRUE(r.ok());
  const config::Widget &w = r.config.tabs[0].widgets[0];
  TEST_ASSERT_EQUAL_STRING("", w.solar.c_str());
  TEST_ASSERT_EQUAL(1, (int)w.entities.size());
  config::ParseResult s = parse("      - type: energy\n        solar: sensor.pv_today\n");
  TEST_ASSERT_TRUE(s.ok());
  TEST_ASSERT_EQUAL_STRING("sensor.pv_today", s.config.tabs[0].widgets[0].entity.c_str());
}

void test_config_errors() {
  TEST_ASSERT_FALSE(parse("      - type: energy\n").ok());                                         // neither
  TEST_ASSERT_FALSE(parse("      - type: energy\n        usage: light.x\n").ok());                 // not a sensor
  TEST_ASSERT_FALSE(parse("      - type: energy\n        usage: nodot\n").ok());
  TEST_ASSERT_FALSE(parse("      - type: energy\n        entity: sensor.x\n        usage: sensor.y\n").ok());
  TEST_ASSERT_FALSE(parse("      - entity: sensor.x\n        type: energy\n").ok());               // override: no usage
  config::ParseResult r = parse("      - type: energy\n        usage: sensor.x\n        size: 3x1\n");
  TEST_ASSERT_TRUE(r.ok());  // parses; the layout clamps the size
}

void test_config_cache_roundtrip() {
  config::Config c = bothConfig();
  config::Config back;
  TEST_ASSERT_TRUE(config::deserializeConfig((const uint8_t *)config::serializeConfig(c).data(),
                                             config::serializeConfig(c).size(), back));
  const config::Widget &w = back.tabs[0].widgets[0];
  TEST_ASSERT_TRUE(w.type == WidgetType::Energy);
  TEST_ASSERT_EQUAL_STRING("sensor.energy_used_today", w.usage.c_str());
  TEST_ASSERT_EQUAL_STRING("sensor.energy_solar_today", w.solar.c_str());
}

// ---- layout / sizes ----

void test_size_is_2x1_only_and_not_focusable() {
  config::Config c = bothConfig();
  for (auto o : {settings::Orientation::Portrait, settings::Orientation::Landscape}) {
    ui::Flow f = ui::computeFlow(o, c.tabs[0].widgets, ui::columnsOf(c.tabs[0], o));
    TEST_ASSERT_EQUAL(1, (int)f.placements.size());
    TEST_ASSERT_EQUAL(2, f.placements[0].cellW);
    TEST_ASSERT_EQUAL(1, f.placements[0].cellH);
  }
  TEST_ASSERT_FALSE(ui::isFocusable(WidgetType::Energy, false));
  TEST_ASSERT_FALSE(ui::isFocusable(WidgetType::Energy, true));
}

void test_layout_numbers() {
  EnergyLayout l = energyLayout(364, 2);
  TEST_ASSERT_EQUAL(176, l.bw);  // landscape
  TEST_ASSERT_EQUAL(32, l.row[0].cy);
  TEST_ASSERT_EQUAL(72, l.row[1].cy);
  TEST_ASSERT_TRUE((ui::Rect{76, 22, 176, 20}) == l.row[0].track);
  TEST_ASSERT_TRUE((ui::Rect{76, 62, 176, 20}) == l.row[1].track);
  TEST_ASSERT_EQUAL(364 - 12, l.row[0].valueRight);
  TEST_ASSERT_EQUAL(12, l.row[0].labelX);
  TEST_ASSERT_EQUAL(364 - 36, l.badgeX);
  TEST_ASSERT_EQUAL(320, energyLayout(508, 2).bw);  // portrait
  TEST_ASSERT_EQUAL(52, energyLayout(508, 1).row[0].cy);
}

// ---- view ----

static EntityState sensor(const char *id, const char *unit, bool avail = true) {
  EntityState e;
  e.id = id;
  e.state = avail ? "1" : "unavailable";
  e.available = avail;
  e.unit_of_measurement = unit;
  return e;
}

static EnergyInput in(const EntityState *st, bool hasValue, double v) {
  EnergyInput i;
  i.configured = true;
  i.state = st;
  i.hasValue = hasValue;
  i.value = v;
  return i;
}

void test_text_and_units() {
  TEST_ASSERT_EQUAL_STRING("7.4 kWh", energyText(7.4).c_str());
  TEST_ASSERT_EQUAL_STRING("11.2 kWh", energyText(11.2).c_str());
  TEST_ASSERT_EQUAL_STRING("112 kWh", energyText(112.4).c_str());
  TEST_ASSERT_EQUAL_STRING("100 kWh", energyText(100.0).c_str());
  TEST_ASSERT_EQUAL_STRING("-12.3 kWh", energyText(-12.3).c_str());
  TEST_ASSERT_FLOAT_WITHIN(0.0001, 7.4, toKwh(7400, "Wh"));
  TEST_ASSERT_FLOAT_WITHIN(0.0001, 7.4, toKwh(7.4, "kWh"));
  TEST_ASSERT_FLOAT_WITHIN(0.0001, 2000, toKwh(2, "MWh"));
}

void test_view_shared_scale() {
  EntityState u = sensor("sensor.u", "kWh"), s = sensor("sensor.s", "kWh");
  EnergyView v = energyView(in(&u, true, 7.4), in(&s, true, 11.2), 176);
  TEST_ASSERT_TRUE(v.look == Look::Normal);
  TEST_ASSERT_EQUAL(2, v.rows);
  TEST_ASSERT_EQUAL_STRING("7.4 kWh", v.row[0].text.c_str());
  TEST_ASSERT_EQUAL_STRING("11.2 kWh", v.row[1].text.c_str());
  TEST_ASSERT_EQUAL(176, v.row[1].fillW);  // the larger one fills the track
  TEST_ASSERT_EQUAL(116, v.row[0].fillW);  // 176 * 7.4 / 11.2 = 116.3
}

void test_view_wh_and_min_fill() {
  EntityState u = sensor("sensor.u", "Wh"), s = sensor("sensor.s", "kWh");
  EnergyView v = energyView(in(&u, true, 7400), in(&s, true, 100.0), 176);
  TEST_ASSERT_EQUAL_STRING("7.4 kWh", v.row[0].text.c_str());
  TEST_ASSERT_EQUAL_STRING("100 kWh", v.row[1].text.c_str());
  TEST_ASSERT_EQUAL(13, v.row[0].fillW);  // 176 * 7.4 / 100
  EnergyView t = energyView(in(&u, true, 1), in(&s, true, 100.0), 176);  // 0.001 kWh: at least 2 px
  TEST_ASSERT_EQUAL(2, t.row[0].fillW);
  EnergyView z = energyView(in(&u, true, 0), in(&s, true, 0), 176);  // all zero: empty tracks
  TEST_ASSERT_EQUAL(0, z.row[0].fillW);
  TEST_ASSERT_EQUAL_STRING("0.0 kWh", z.row[0].text.c_str());
}

void test_view_states() {
  EntityState u = sensor("sensor.u", "kWh"), s = sensor("sensor.s", "kWh", false), un = sensor("sensor.u", "kWh", false);
  // one unavailable: only its own row blanks
  EnergyView v = energyView(in(&u, true, 5), in(&s, false, 0), 176);
  TEST_ASSERT_TRUE(v.look == Look::Normal);
  TEST_ASSERT_TRUE(v.row[1].state == EnergyRowState::NoValue);
  TEST_ASSERT_EQUAL_STRING("-", v.row[1].text.c_str());
  TEST_ASSERT_EQUAL(0, v.row[1].fillW);
  // both unavailable
  TEST_ASSERT_TRUE(energyView(in(&un, false, 0), in(&s, false, 0), 176).look == Look::Unavailable);
  // nothing cached, no value: loading, "-" and empty tracks
  EnergyView l = energyView(in(nullptr, false, 0), in(nullptr, false, 0), 176);
  TEST_ASSERT_TRUE(l.look == Look::Loading);
  TEST_ASSERT_TRUE(l.row[0].state == EnergyRowState::Loading);
  TEST_ASSERT_EQUAL_STRING("-", l.row[0].text.c_str());
  // available, statistics not there (yet / no statistics): "-", not loading
  EnergyView n = energyView(in(&u, false, 0), in(&u, false, 0), 176);
  TEST_ASSERT_TRUE(n.look == Look::Normal);
  TEST_ASSERT_TRUE(n.row[0].state == EnergyRowState::NoValue);
  // a cached value without a state still shows
  EnergyView c = energyView(in(nullptr, true, 3), in(nullptr, false, 0), 176);
  TEST_ASSERT_EQUAL_STRING("3.0 kWh", c.row[0].text.c_str());
}

void test_view_single_row() {
  EntityState s = sensor("sensor.s", "kWh");
  EnergyInput none;
  EnergyView v = energyView(none, in(&s, true, 4), 176);
  TEST_ASSERT_EQUAL(1, v.rows);
  TEST_ASSERT_TRUE(v.row[0].state == EnergyRowState::Hidden);
  TEST_ASSERT_EQUAL(176, v.row[1].fillW);
}

// ---- today's value from statistics ----

void test_today_spec_plans_one_day_bucket_since_local_midnight() {
  HistorySpec s = makeEnergyTodaySpec(kUsage);
  const int32_t off = 3600;  // CET
  const int64_t now = 1793714700;  // 2026-11-03T14:05:00Z = 15:05 local
  HistoryJob job;
  planHistoryJob(s, now, off, job);
  TEST_ASSERT_TRUE(job.statistics);
  TEST_ASSERT_TRUE(job.bar);
  TEST_ASSERT_TRUE(job.statistic == StatsType::Change);
  TEST_ASSERT_TRUE(job.period == StatsPeriod::Day);
  TEST_ASSERT_EQUAL(1, job.bucketCount);
  TEST_ASSERT_EQUAL_INT64(1793660400, job.start);  // 2026-11-03T00:00+01:00
  TEST_ASSERT_EQUAL_INT64(now, job.end);
  TEST_ASSERT_EQUAL_STRING(kUsage, job.entity.c_str());
}

void test_today_value_reads_last_bucket() {
  GraphEntry e;
  double v = 0;
  TEST_ASSERT_FALSE(energyTodayValue(nullptr, v));
  TEST_ASSERT_FALSE(energyTodayValue(&e, v));  // no data
  e.hasData = true;
  TEST_ASSERT_FALSE(energyTodayValue(&e, v));  // no buckets
  history::Bucket b;
  b.has = false;
  e.series.buckets.push_back(b);
  TEST_ASSERT_FALSE(energyTodayValue(&e, v));  // bucket without data
  e.series.buckets[0].has = true;
  e.series.buckets[0].last = 7.43f;
  TEST_ASSERT_TRUE(energyTodayValue(&e, v));
  TEST_ASSERT_FLOAT_WITHIN(0.001, 7.43, v);
}

// ---- AppState ----

static AppState makeApp(settings::Orientation o = settings::Orientation::Landscape) {
  AppState a;
  a.init(bothConfig(), o, false);
  return a;
}

static history::Series daySeries(float v) {
  history::Series s;
  s.kind = history::Kind::Numeric;
  history::Bucket b;
  b.has = true;
  b.min = b.max = b.mean = b.last = v;
  s.buckets.push_back(b);
  return s;
}

void test_graph_entries_for_both_sensors() {
  AppState a = makeApp();
  const auto &es = a.graphs().entries();
  TEST_ASSERT_EQUAL(2, (int)es.size());
  TEST_ASSERT_EQUAL(0, es[0].widget);
  TEST_ASSERT_EQUAL(kEnergySolarKey, es[1].widget);
  TEST_ASSERT_EQUAL_STRING("sensor.energy_used_today", es[0].spec.entity.c_str());
  TEST_ASSERT_EQUAL_STRING("sensor.energy_solar_today", es[1].spec.entity.c_str());
  TEST_ASSERT_TRUE(a.graphs().tabHasGraphs(0));
  TEST_ASSERT_TRUE(es[0].spec.params.entity != es[1].spec.params.entity);
}

void test_widget_data_shows_statistics_value() {
  AppState a = makeApp();
  EntityState u = sensor("sensor.energy_used_today", "kWh"), s = sensor("sensor.energy_solar_today", "kWh");
  a.store().update({u, s});
  WidgetData d = a.widgetData(0);
  TEST_ASSERT_TRUE(d.energy.row[0].state == EnergyRowState::NoValue);  // states known, statistics not yet
  a.graphs().setData(0, 0, daySeries(7.4f), 1000);
  a.graphs().setData(0, kEnergySolarKey, daySeries(11.2f), 1000);
  d = a.widgetData(0);
  TEST_ASSERT_EQUAL_STRING("7.4 kWh", d.energy.row[0].text.c_str());
  TEST_ASSERT_EQUAL_STRING("11.2 kWh", d.energy.row[1].text.c_str());
  TEST_ASSERT_EQUAL(energyLayout(364, 2).bw, d.energy.row[1].fillW);  // landscape 2x1 is 364 wide
}

void test_widget_data_loading_before_states() {
  AppState a = makeApp();
  WidgetData d = a.widgetData(0);
  TEST_ASSERT_TRUE(d.energy.look == Look::Loading);
}

void test_tap_and_long_press_open_usage_bar_history() {
  AppState a = makeApp();
  const ui::Rect r = a.layout().widgets[0].rect;
  const ui::Point p{r.x + r.w / 2, r.y + r.h / 2};
  TEST_ASSERT_TRUE(a.touch(p, 100));  // task 118: a tap opens the history too
  TEST_ASSERT_TRUE(a.historyOpen());
  a.closeHistory();
  TEST_ASSERT_TRUE(a.historyWidgetAt(p));
  TEST_ASSERT_TRUE(a.longPress(p, 1000));
  TEST_ASSERT_TRUE(a.historyOpen());
  const HistorySpec &s = a.history().spec();
  TEST_ASSERT_EQUAL_STRING("sensor.energy_used_today", s.entity.c_str());  // usage only
  TEST_ASSERT_TRUE(s.chart == HistChart::Bar);
  TEST_ASSERT_TRUE(s.source == history::Source::Statistics);
  TEST_ASSERT_TRUE(s.period == StatsPeriod::Hour);
  TEST_ASSERT_TRUE(s.statistic == StatsType::Change);
  TEST_ASSERT_EQUAL(24, s.hours);
}

void test_history_false_disables_popup() {
  config::ParseResult r = parse(
      "      - type: energy\n        usage: sensor.u\n        history: false\n");
  TEST_ASSERT_TRUE(r.ok());
  AppState a;
  a.init(r.config, settings::Orientation::Landscape, false);
  const ui::Rect rc = a.layout().widgets[0].rect;
  TEST_ASSERT_FALSE(a.longPress({rc.x + 5, rc.y + 5}, 1000));
}

void test_rocker_never_focuses_it() {
  AppState a = makeApp();
  a.rocker(ui::PhysicalKey::Down, 100);
  TEST_ASSERT_EQUAL(-1, a.focusedWidgetIndex());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_config_guide_snippet_parses);
  RUN_TEST(test_config_single_entity_and_real_sensor);
  RUN_TEST(test_config_errors);
  RUN_TEST(test_config_cache_roundtrip);
  RUN_TEST(test_size_is_2x1_only_and_not_focusable);
  RUN_TEST(test_layout_numbers);
  RUN_TEST(test_text_and_units);
  RUN_TEST(test_view_shared_scale);
  RUN_TEST(test_view_wh_and_min_fill);
  RUN_TEST(test_view_states);
  RUN_TEST(test_view_single_row);
  RUN_TEST(test_today_spec_plans_one_day_bucket_since_local_midnight);
  RUN_TEST(test_today_value_reads_last_bucket);
  RUN_TEST(test_graph_entries_for_both_sensors);
  RUN_TEST(test_widget_data_shows_statistics_value);
  RUN_TEST(test_widget_data_loading_before_states);
  RUN_TEST(test_tap_and_long_press_open_usage_bar_history);
  RUN_TEST(test_history_false_disables_popup);
  RUN_TEST(test_rocker_never_focuses_it);
  return UNITY_END();
}
