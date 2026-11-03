// Task 104: weather widget with forecast (config, request/parse, day selection, view, layout, fetch rule, worker).
#include <cmath>
#include <deque>
#include <unity.h>

#include "config_cache.h"
#include "config_model.h"
#include "forecast.h"
#include "forecast_view.h"
#include "ha_worker.h"
#include "ui_layout.h"

using namespace forecast;
using config::WidgetType;
using settings::Orientation;

void setUp() {}
void tearDown() {}

static const char *kAnswer =
    "{\"changed_states\":[],\"service_response\":{\"weather.home\":{\"forecast\":["
    "{\"datetime\":\"2026-11-03T00:00:00+01:00\",\"condition\":\"cloudy\",\"temperature\":9.0,\"templow\":4.0},"
    "{\"datetime\":\"2026-11-04T00:00:00+01:00\",\"condition\":\"rainy\",\"temperature\":14.4,\"templow\":6.0},"
    "{\"datetime\":\"2026-11-05\",\"condition\":\"sunny\",\"temperature\":12.6},"
    "{\"datetime\":\"2026-11-06T00:00:00+01:00\",\"condition\":\"pouring\",\"temperature\":-2.0,\"templow\":-7.6},"
    "{\"datetime\":\"2026-11-07T00:00:00+01:00\",\"condition\":\"snowy\",\"temperature\":1.0}]}}}";

// ---- config ----
static config::ParseResult parseWith(const char *line) {
  std::string y = "version: \"1\"\ntabs:\n  - title: A\n    widgets:\n      - entity: weather.home\n";
  y += line;
  return config::parseConfig(y);
}

void test_config_forecast_true() {
  auto r = parseWith("        forecast: true\n");
  TEST_ASSERT_TRUE(r.ok());
  TEST_ASSERT_TRUE(r.config.tabs[0].widgets[0].forecast);
  TEST_ASSERT_EQUAL_INT(int(WidgetType::Weather), int(r.config.tabs[0].widgets[0].type));
}

void test_config_forecast_default_and_false() {
  TEST_ASSERT_FALSE(parseWith("").config.tabs[0].widgets[0].forecast);
  auto r = parseWith("        forecast: false\n");
  TEST_ASSERT_TRUE(r.ok());
  TEST_ASSERT_FALSE(r.config.tabs[0].widgets[0].forecast);
}

void test_config_forecast_errors() {
  auto r = parseWith("        forecast: maybe\n");
  TEST_ASSERT_FALSE(r.ok());
  TEST_ASSERT_EQUAL_STRING("tabs[0].widgets[0].forecast", r.errors[0].path.c_str());
  TEST_ASSERT_TRUE(r.errors[0].message.find("maybe") == std::string::npos);  // never echoes the value
  auto s = config::parseConfig(
      "version: \"1\"\ntabs:\n  - title: A\n    widgets:\n      - entity: sensor.t\n        forecast: true\n");
  TEST_ASSERT_FALSE(s.ok());
  auto g = config::parseConfig(
      "version: \"1\"\ngroups:\n  g:\n    - weather.a\n    - weather.b\ntabs:\n  - title: A\n    widgets:\n"
      "      - group: g\n        name: W\n        type: weather\n        forecast: true\n");
  TEST_ASSERT_FALSE(g.ok());
}

void test_config_cache_keeps_forecast() {
  auto r = parseWith("        forecast: true\n");
  config::Config out;
  TEST_ASSERT_TRUE(config::deserializeConfig(config::serializeConfig(r.config), out));
  TEST_ASSERT_TRUE(out.tabs[0].widgets[0].forecast);
  auto r2 = parseWith("");
  TEST_ASSERT_TRUE(config::deserializeConfig(config::serializeConfig(r2.config), out));
  TEST_ASSERT_FALSE(out.tabs[0].widgets[0].forecast);
}

// ---- size ----
void test_forecast_widget_needs_wide() {
  config::Widget w;
  w.type = WidgetType::Weather;
  w.forecast = true;
  // landscape 5 columns: 3 cells = 552; portrait 2 columns: 2 cells = 508; landscape 4 columns: 3 cells (>= 500).
  auto s = ui::resolveSize(WidgetType::Weather, w, ui::gridFor(Orientation::Landscape, 5));
  TEST_ASSERT_EQUAL_INT(3, s.w);
  TEST_ASSERT_EQUAL_INT(552, ui::cellRect(ui::gridFor(Orientation::Landscape, 5), 0, 0, s.w, 1).w);
  s = ui::resolveSize(WidgetType::Weather, w, ui::gridFor(Orientation::Portrait, 2));
  TEST_ASSERT_EQUAL_INT(2, s.w);
  TEST_ASSERT_EQUAL_INT(508, ui::cellRect(ui::gridFor(Orientation::Portrait, 2), 0, 0, s.w, 1).w);
  for (int cols : {4}) {
    s = ui::resolveSize(WidgetType::Weather, w, ui::gridFor(Orientation::Landscape, cols));
    TEST_ASSERT_GREATER_OR_EQUAL_INT(ui::kForecastMinW, ui::cellRect(ui::gridFor(Orientation::Landscape, cols), 0, 0, s.w, 1).w);
  }
  w.size = config::WidgetSize{1, 1};  // too small: clamped up
  s = ui::resolveSize(WidgetType::Weather, w, ui::gridFor(Orientation::Landscape, 5));
  TEST_ASSERT_EQUAL_INT(3, s.w);
  w.forecast = false;  // plain weather is unchanged (1x1 allowed)
  s = ui::resolveSize(WidgetType::Weather, w, ui::gridFor(Orientation::Landscape, 5));
  TEST_ASSERT_EQUAL_INT(1, s.w);
}

// ---- request / parse ----
void test_request() {
  std::string body;
  TEST_ASSERT_TRUE(buildRequestBody("weather.home", body));
  TEST_ASSERT_EQUAL_STRING("{\"entity_id\":\"weather.home\",\"type\":\"daily\"}", body.c_str());
  TEST_ASSERT_EQUAL_STRING("/api/services/weather/get_forecasts?return_response", requestPath());
  TEST_ASSERT_FALSE(buildRequestBody("sensor.x", body));
  TEST_ASSERT_FALSE(buildRequestBody("weather.a\"b", body));
}

void test_parse_response() {
  std::vector<Day> d;
  TEST_ASSERT_TRUE(parseResponse(kAnswer, "weather.home", d));
  TEST_ASSERT_EQUAL_UINT(5, d.size());
  TEST_ASSERT_EQUAL_INT(4, d[1].date.d);
  TEST_ASSERT_EQUAL_STRING("rainy", d[1].condition.c_str());
  TEST_ASSERT_TRUE(std::abs(*d[1].high - 14.4) < 1e-3);
  TEST_ASSERT_TRUE(std::abs(*d[1].low - 6.0) < 1e-9);
  TEST_ASSERT_FALSE(d[2].low.has_value());
  TEST_ASSERT_EQUAL_INT(5, d[2].date.d);  // plain date without a time
  TEST_ASSERT_FALSE(parseResponse(kAnswer, "weather.other", d));
  TEST_ASSERT_FALSE(parseResponse("not json", "weather.home", d));
  TEST_ASSERT_FALSE(parseResponse("{\"service_response\":{}}", "weather.home", d));
  TEST_ASSERT_TRUE(parseResponse("{\"service_response\":{\"weather.home\":{\"forecast\":[]}}}", "weather.home", d));
  TEST_ASSERT_EQUAL_UINT(0, d.size());
  // entries with a bad date are skipped
  TEST_ASSERT_TRUE(parseResponse("{\"service_response\":{\"weather.home\":{\"forecast\":[{\"datetime\":\"x\"},"
                                 "{\"datetime\":\"2026-11-09\",\"temperature\":1}]}}}",
                                 "weather.home", d));
  TEST_ASSERT_EQUAL_UINT(1, d.size());
}

void test_dates() {
  TEST_ASSERT_EQUAL_INT(2, weekday({2026, 11, 3}));  // Tuesday
  TEST_ASSERT_EQUAL_STRING("Wed", weekdayName(weekday({2026, 11, 4})));
  TEST_ASSERT_TRUE((nextDay({2026, 12, 31}) == Ymd{2027, 1, 1}));
  TEST_ASSERT_TRUE((nextDay({2028, 2, 28}) == Ymd{2028, 2, 29}));
  TEST_ASSERT_TRUE((nextDay({2026, 2, 28}) == Ymd{2026, 3, 1}));
  TEST_ASSERT_TRUE((dateOf(1793361600, 3600) == Ymd{2026, 10, 30}));  // 2026-10-30 12:00 UTC
  TEST_ASSERT_TRUE((dateOf(1793361600 + 13 * 3600, 3600) == Ymd{2026, 10, 31}));  // local midnight passed
  TEST_ASSERT_FALSE(dateOf(0, 0).valid());
}

void test_select_days_skips_today() {
  std::vector<Day> d;
  TEST_ASSERT_TRUE(parseResponse(kAnswer, "weather.home", d));
  auto s = selectDays(d, {2026, 11, 3});
  TEST_ASSERT_EQUAL_UINT(3, s.size());
  TEST_ASSERT_EQUAL_INT(4, s[0].date.d);
  TEST_ASSERT_EQUAL_INT(6, s[2].date.d);
  s = selectDays(d, {2026, 11, 5});  // only two future days left
  TEST_ASSERT_EQUAL_UINT(2, s.size());
  TEST_ASSERT_EQUAL_UINT(0, selectDays(d, {2026, 12, 1}).size());
}

// ---- view ----
static EntityState weatherState(const char *cond) {
  EntityState s;
  s.id = "weather.home";
  s.state = cond;
  s.available = true;
  s.temperature = 12.4;
  return s;
}

void test_view_normal() {
  std::vector<Day> d;
  parseResponse(kAnswer, "weather.home", d);
  Data data;
  data.days = d;
  EntityState st = weatherState("partlycloudy");
  const ForecastView v = forecastView(&st, &data, {2026, 11, 3});
  TEST_ASSERT_TRUE(v.now.look == uistate::Look::Normal);
  TEST_ASSERT_EQUAL_STRING("12", v.now.temp.c_str());
  TEST_ASSERT_FALSE(v.forecastMissing);
  TEST_ASSERT_EQUAL_STRING("Wed", v.cols[0].day.c_str());
  TEST_ASSERT_TRUE(v.cols[0].hasIcon);
  TEST_ASSERT_TRUE(v.cols[0].icon == uistate::WeatherIcon::Rainy);
  TEST_ASSERT_EQUAL_STRING("14\xC2\xB0/6\xC2\xB0", v.cols[0].hilo.c_str());
  TEST_ASSERT_EQUAL_STRING("13\xC2\xB0", v.cols[1].hilo.c_str());  // 12.6 rounds up, no low
  TEST_ASSERT_EQUAL_STRING("Fri", v.cols[2].day.c_str());
  TEST_ASSERT_TRUE(v.cols[2].icon == uistate::WeatherIcon::Pouring);
  TEST_ASSERT_EQUAL_STRING("-2\xC2\xB0/-8\xC2\xB0", v.cols[2].hilo.c_str());
}

void test_view_forecast_missing_keeps_day_names() {
  EntityState st = weatherState("sunny");
  const ForecastView v = forecastView(&st, nullptr, {2026, 11, 3});
  TEST_ASSERT_TRUE(v.forecastMissing);
  TEST_ASSERT_EQUAL_STRING("Wed", v.cols[0].day.c_str());
  TEST_ASSERT_EQUAL_STRING("Thu", v.cols[1].day.c_str());
  TEST_ASSERT_EQUAL_STRING("Fri", v.cols[2].day.c_str());
  for (const ColumnView &c : v.cols) {
    TEST_ASSERT_FALSE(c.hasIcon);
    TEST_ASSERT_EQUAL_STRING("-", c.hilo.c_str());
  }
  TEST_ASSERT_EQUAL_STRING("Sunny", v.now.cond.c_str());  // the current block stays normal
  const ForecastView nc = forecastView(&st, nullptr, Ymd{});  // clock unset: no day names either
  TEST_ASSERT_EQUAL_STRING("-", nc.cols[0].day.c_str());
}

void test_view_unavailable_and_loading() {
  EntityState st = weatherState("unavailable");
  st.available = false;
  ForecastView v = forecastView(&st, nullptr, {2026, 11, 3});
  TEST_ASSERT_TRUE(v.now.look == uistate::Look::Unavailable);
  TEST_ASSERT_EQUAL_STRING("n/a", v.now.temp.c_str());
  v = forecastView(nullptr, nullptr, {2026, 11, 3});
  TEST_ASSERT_TRUE(v.now.look == uistate::Look::Loading);
  TEST_ASSERT_EQUAL_STRING("-", v.now.temp.c_str());
  for (const ColumnView &c : v.cols) {
    TEST_ASSERT_EQUAL_STRING("-", c.day.c_str());
    TEST_ASSERT_EQUAL_STRING("-", c.hilo.c_str());
    TEST_ASSERT_FALSE(c.hasIcon);
  }
}

void test_view_unknown_condition_drops_icon() {
  Data data;
  data.days = {Day{{2026, 11, 4}, "weird", 5.0, 1.0}, Day{{2026, 11, 5}, "", 5.0, std::nullopt}};
  EntityState st = weatherState("sunny");
  const ForecastView v = forecastView(&st, &data, {2026, 11, 3});
  TEST_ASSERT_FALSE(v.cols[0].hasIcon);
  TEST_ASSERT_FALSE(v.cols[1].hasIcon);
  TEST_ASSERT_EQUAL_STRING("5\xC2\xB0/1\xC2\xB0", v.cols[0].hilo.c_str());
  TEST_ASSERT_EQUAL_STRING("-", v.cols[2].hilo.c_str());  // only two days delivered
}

// ---- layout (docs/ui/widgets-new.md section 2) ----
void test_layout_numbers() {
  Layout l = layoutFor(552);  // landscape
  TEST_ASSERT_EQUAL_INT(108, l.cw);
  TEST_ASSERT_EQUAL_INT(216, l.fx);
  TEST_ASSERT_EQUAL_INT(216 - 16 - 68, l.condMaxW);
  TEST_ASSERT_EQUAL_INT(208, l.divX);
  TEST_ASSERT_EQUAL_INT(20, l.divY);
  TEST_ASSERT_EQUAL_INT(64, l.divH);
  TEST_ASSERT_EQUAL_INT(216 + 54, l.cols[0].cx);
  TEST_ASSERT_EQUAL_INT(216 + 108 + 54, l.cols[1].cx);
  TEST_ASSERT_EQUAL_INT(l.cols[2].cx - 12, l.cols[2].iconX);
  TEST_ASSERT_EQUAL_INT(24, l.cols[0].dayCy);
  TEST_ASSERT_EQUAL_INT(40, l.cols[0].iconY);
  TEST_ASSERT_EQUAL_INT(80, l.cols[0].hiloCy);
  TEST_ASSERT_EQUAL_INT(539, l.unavailTempRight);
  l = layoutFor(508);  // portrait
  TEST_ASSERT_EQUAL_INT(100, l.cw);
  TEST_ASSERT_EQUAL_INT(196, l.fx);
  TEST_ASSERT_EQUAL_INT(196 + 50, l.cols[0].cx);
  TEST_ASSERT_EQUAL_INT(496, l.cols[2].cx + 50);  // the last column ends 12 px from the right edge
}

// ---- fetch rule ----
void test_store_due_rules() {
  Store s;
  const std::string e = "weather.home";
  TEST_ASSERT_TRUE(s.due(e, 1000000, false));  // no data
  s.set(e, {}, 1000000);
  TEST_ASSERT_FALSE(s.due(e, 1000000 + 600, false));  // fresh, interactive
  TEST_ASSERT_TRUE(s.due(e, 1000000 + 600, true));    // scheduled wake: always once
  TEST_ASSERT_TRUE(s.due(e, 1000000 + kMaxAgeS + 1, false));  // old
  s.markAsked(e, 1000000 + 600);
  TEST_ASSERT_FALSE(s.due(e, 1000000 + 700, true));   // never twice in a scheduled wake
  TEST_ASSERT_FALSE(s.due(e, 1000000 + 700, false));
  TEST_ASSERT_TRUE(s.due(e, 1000000 + 600 + kMaxAgeS + 1, false));  // interactive stays awake: asks again later
  TEST_ASSERT_FALSE(s.due(e, 1000000 + 600 + kMaxAgeS + 1, true));
  s.newSession();
  TEST_ASSERT_TRUE(s.due(e, 1000000 + 700, true));
  s.markAsked(e, 0);  // clock unset: asked once only
  TEST_ASSERT_FALSE(s.due(e, 0, false));
  s.unmarkAsked(e);  // task 116: dropped by yielding, asked again
  TEST_ASSERT_TRUE(s.due(e, 0, false));
}

void test_entities_on_shown_tab_only() {
  auto r = config::parseConfig(
      "version: \"1\"\ntabs:\n  - title: A\n    widgets:\n      - entity: weather.home\n        forecast: true\n"
      "      - entity: weather.plain\n      - entity: weather.home\n        forecast: true\n"
      "  - title: B\n    widgets:\n      - entity: weather.far\n        forecast: true\n");
  TEST_ASSERT_TRUE(r.ok());
  auto a = entitiesOnTab(r.config, 0);
  TEST_ASSERT_EQUAL_UINT(1, a.size());  // plain weather and duplicates are not asked
  TEST_ASSERT_EQUAL_STRING("weather.home", a[0].c_str());
  TEST_ASSERT_EQUAL_STRING("weather.far", entitiesOnTab(r.config, 1)[0].c_str());
  TEST_ASSERT_EQUAL_UINT(0, entitiesOnTab(r.config, 2).size());
}

// ---- worker ----
struct FakeTransport : HttpTransport {
  struct Req { std::string method, url, body; };
  std::vector<Req> calls;
  std::deque<HttpResponse> queue;
  HttpResponse request(const std::string &m, const std::string &u, const HttpHeaders &, const std::string &b) override {
    calls.push_back({m, u, b});
    HttpResponse r = queue.front();
    queue.pop_front();
    return r;
  }
};

static HttpResponse ok(const char *body) {
  HttpResponse r;
  r.status = 200;
  r.body = body;
  return r;
}

void test_worker_forecast_ok() {
  FakeTransport t;
  t.queue = {ok(kAnswer)};
  app_logic::HaWorker w(t, "http://ha.local:8123", "", "TOKEN");
  app_logic::NetCommand c;
  c.type = app_logic::NetCommand::Type::Forecast;
  c.forecastEntity = "weather.home";
  app_logic::NetEvent e = w.run(c);
  TEST_ASSERT_TRUE(e.type == app_logic::NetEvent::Type::ForecastDone);
  TEST_ASSERT_TRUE(e.error == HaError::None);
  TEST_ASSERT_EQUAL_UINT(5, e.forecastDays.size());
  TEST_ASSERT_EQUAL_STRING("weather.home", e.forecastEntity.c_str());
  TEST_ASSERT_EQUAL_UINT(1, t.calls.size());
  TEST_ASSERT_EQUAL_STRING("POST", t.calls[0].method.c_str());
  TEST_ASSERT_EQUAL_STRING("http://ha.local:8123/api/services/weather/get_forecasts?return_response",
                           t.calls[0].url.c_str());
  TEST_ASSERT_EQUAL_STRING("{\"entity_id\":\"weather.home\",\"type\":\"daily\"}", t.calls[0].body.c_str());
}

void test_worker_forecast_failures() {
  FakeTransport t;
  app_logic::HaWorker w(t, "http://ha.local:8123", "", "TOKEN");
  app_logic::NetCommand c;
  c.type = app_logic::NetCommand::Type::Forecast;
  c.forecastEntity = "weather.home";
  t.queue = {ok("garbage")};
  TEST_ASSERT_TRUE(w.run(c).error == HaError::Parse);
  HttpResponse bad;
  bad.status = 500;
  t.queue = {bad};
  app_logic::NetEvent e = w.run(c);
  TEST_ASSERT_TRUE(e.error == HaError::Http);
  TEST_ASSERT_EQUAL_UINT(0, e.forecastDays.size());
  c.forecastEntity = "light.x";  // not a weather entity: no request at all
  const size_t before = t.calls.size();
  TEST_ASSERT_TRUE(w.run(c).error == HaError::Parse);
  TEST_ASSERT_EQUAL_UINT(before, t.calls.size());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_config_forecast_true);
  RUN_TEST(test_config_forecast_default_and_false);
  RUN_TEST(test_config_forecast_errors);
  RUN_TEST(test_config_cache_keeps_forecast);
  RUN_TEST(test_forecast_widget_needs_wide);
  RUN_TEST(test_request);
  RUN_TEST(test_parse_response);
  RUN_TEST(test_dates);
  RUN_TEST(test_select_days_skips_today);
  RUN_TEST(test_view_normal);
  RUN_TEST(test_view_forecast_missing_keeps_day_names);
  RUN_TEST(test_view_unavailable_and_loading);
  RUN_TEST(test_view_unknown_condition_drops_icon);
  RUN_TEST(test_layout_numbers);
  RUN_TEST(test_store_due_rules);
  RUN_TEST(test_entities_on_shown_tab_only);
  RUN_TEST(test_worker_forecast_ok);
  RUN_TEST(test_worker_forecast_failures);
  return UNITY_END();
}
