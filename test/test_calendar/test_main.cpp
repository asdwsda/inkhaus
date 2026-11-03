// Task 105: calendar widget (config, request/parse, view texts, layout, fetch rule, worker, demo data).
#include <deque>
#include <unity.h>

#include "cal_events.h"
#include "config_cache.h"
#include "config_model.h"
#include "ha_worker.h"
#include "ui_layout.h"
#include "ui_rocker.h"

using namespace calendar;
using config::WidgetType;
using settings::Orientation;
using uistate::Look;

void setUp() {}
void tearDown() {}

// 2026-11-03 is a Tuesday.
static int64_t at(const char *s) {
  int64_t m = 0;
  bool t = false;
  TEST_ASSERT_TRUE(parseLocal(s, m, t));
  return m;
}

static const char *kAnswer =
    "{\"changed_states\":[],\"service_response\":{\"calendar.family\":{\"events\":["
    "{\"start\":\"2026-11-05\",\"end\":\"2026-11-06\",\"summary\":\"Holiday\"},"
    "{\"start\":\"2026-11-04T09:00:00+01:00\",\"end\":\"2026-11-04T09:30:00+01:00\",\"summary\":\"Team stand-up\"},"
    "{\"start\":\"2026-11-03T14:30:00+01:00\",\"end\":\"2026-11-03T15:30:00+01:00\",\"summary\":\"Dentist\"},"
    "{\"start\":\"bad\",\"summary\":\"skipped\"}]}}}";

// ---- config ----
static config::ParseResult parseWith(const char *entity, const char *extra = "") {
  std::string y = "version: \"1\"\ntabs:\n  - title: A\n    widgets:\n      - entity: ";
  y += entity;
  y += "\n";
  y += extra;
  return config::parseConfig(y);
}

void test_config_calendar_type() {
  auto r = parseWith("calendar.family", "        name: Family\n");
  TEST_ASSERT_TRUE(r.ok());
  TEST_ASSERT_EQUAL_INT(int(WidgetType::Calendar), int(r.config.tabs[0].widgets[0].type));
  TEST_ASSERT_EQUAL_STRING("Family", r.config.tabs[0].widgets[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING("calendar", config::widgetTypeName(WidgetType::Calendar));
  auto s = parseWith("sensor.x", "        type: calendar\n");
  TEST_ASSERT_TRUE(s.ok());
  TEST_ASSERT_EQUAL_INT(int(WidgetType::Calendar), int(s.config.tabs[0].widgets[0].type));
}

void test_config_cache_keeps_calendar() {
  auto r = parseWith("calendar.family");
  config::Config out;
  TEST_ASSERT_TRUE(config::deserializeConfig(config::serializeConfig(r.config), out));
  TEST_ASSERT_EQUAL_INT(int(WidgetType::Calendar), int(out.tabs[0].widgets[0].type));
}

void test_size_2x1_default_3x1_max() {
  config::Widget w;
  w.type = WidgetType::Calendar;
  auto s = ui::resolveSize(WidgetType::Calendar, w, ui::gridFor(Orientation::Landscape, 5));
  TEST_ASSERT_EQUAL_INT(2, s.w);
  TEST_ASSERT_EQUAL_INT(364, ui::cellRect(ui::gridFor(Orientation::Landscape, 5), 0, 0, s.w, 1).w);
  s = ui::resolveSize(WidgetType::Calendar, w, ui::gridFor(Orientation::Portrait, 2));
  TEST_ASSERT_EQUAL_INT(508, ui::cellRect(ui::gridFor(Orientation::Portrait, 2), 0, 0, s.w, 1).w);
  w.size = config::WidgetSize{1, 1};  // no 1x1
  s = ui::resolveSize(WidgetType::Calendar, w, ui::gridFor(Orientation::Landscape, 5));
  TEST_ASSERT_EQUAL_INT(2, s.w);
  w.size = config::WidgetSize{3, 1};  // 3x1 allowed
  s = ui::resolveSize(WidgetType::Calendar, w, ui::gridFor(Orientation::Landscape, 5));
  TEST_ASSERT_EQUAL_INT(3, s.w);
  w.size = config::WidgetSize{5, 1};
  s = ui::resolveSize(WidgetType::Calendar, w, ui::gridFor(Orientation::Landscape, 5));
  TEST_ASSERT_EQUAL_INT(3, s.w);
}

void test_not_focusable() {
  TEST_ASSERT_FALSE(ui::isFocusable(WidgetType::Calendar, false));
}

// ---- request / parse ----
void test_request() {
  std::string body;
  TEST_ASSERT_TRUE(buildRequestBody("calendar.family", body));
  TEST_ASSERT_EQUAL_STRING("{\"entity_id\":\"calendar.family\",\"duration\":{\"days\":7}}", body.c_str());
  TEST_ASSERT_EQUAL_STRING("/api/services/calendar/get_events?return_response", requestPath());
  TEST_ASSERT_FALSE(buildRequestBody("weather.home", body));
  TEST_ASSERT_FALSE(buildRequestBody("calendar.", body));
  TEST_ASSERT_FALSE(buildRequestBody("calendar.a b", body));
}

void test_parse_response_sorted() {
  std::vector<Event> ev;
  TEST_ASSERT_TRUE(parseResponse(kAnswer, "calendar.family", ev));
  TEST_ASSERT_EQUAL_UINT(3, ev.size());  // the unreadable entry is skipped
  TEST_ASSERT_EQUAL_STRING("Dentist", ev[0].title.c_str());
  TEST_ASSERT_EQUAL_STRING("Team stand-up", ev[1].title.c_str());
  TEST_ASSERT_EQUAL_STRING("Holiday", ev[2].title.c_str());
  TEST_ASSERT_FALSE(ev[0].allDay);
  TEST_ASSERT_TRUE(ev[2].allDay);
  TEST_ASSERT_EQUAL_INT64(at("2026-11-03T14:30"), ev[0].start);
  TEST_ASSERT_EQUAL_INT64(at("2026-11-03T15:30"), ev[0].end);
  TEST_ASSERT_EQUAL_INT64(at("2026-11-06"), ev[2].end);
}

void test_parse_response_empty_and_bad() {
  std::vector<Event> ev;
  TEST_ASSERT_TRUE(parseResponse("{\"service_response\":{\"calendar.x\":{\"events\":[]}}}", "calendar.x", ev));
  TEST_ASSERT_EQUAL_UINT(0, ev.size());
  TEST_ASSERT_FALSE(parseResponse("{\"service_response\":{\"calendar.y\":{\"events\":[]}}}", "calendar.x", ev));
  TEST_ASSERT_FALSE(parseResponse("garbage", "calendar.x", ev));
}

void test_parse_caps_events_and_default_end() {
  std::string j = "{\"service_response\":{\"calendar.x\":{\"events\":[";
  for (int i = 1; i <= 9; ++i) {
    if (i > 1) j += ",";
    j += "{\"start\":\"2026-11-0" + std::to_string(i) + "T10:00:00\",\"summary\":\"E" + std::to_string(i) + "\"}";
  }
  j += "]}}}";
  std::vector<Event> ev;
  TEST_ASSERT_TRUE(parseResponse(j, "calendar.x", ev));
  TEST_ASSERT_EQUAL_UINT(kMaxEvents, ev.size());
  TEST_ASSERT_EQUAL_INT64(ev[0].start + 60, ev[0].end);  // no end: one hour
}

void test_parse_local() {
  int64_t m;
  bool t;
  TEST_ASSERT_TRUE(parseLocal("2026-11-04 09:05:00", m, t));
  TEST_ASSERT_TRUE(t);
  TEST_ASSERT_TRUE(parseLocal("2026-11-04", m, t));
  TEST_ASSERT_FALSE(t);
  TEST_ASSERT_FALSE(parseLocal("2026-13-04", m, t));
  TEST_ASSERT_FALSE(parseLocal("2026-11-04T25:00", m, t));
  TEST_ASSERT_FALSE(parseLocal("2026-11-04x", m, t));
  TEST_ASSERT_FALSE(parseLocal(nullptr, m, t));
  TEST_ASSERT_EQUAL_INT64(at("2026-11-04") - at("2026-11-03"), 1440);
  // epoch 1793361600 = 2026-10-30T12:00:00Z; +1 h offset = 13:00 local
  TEST_ASSERT_EQUAL_INT64(at("2026-10-30T13:00"), localMinutes(1793361600, 3600));
  TEST_ASSERT_EQUAL_INT64(0, localMinutes(0, 3600));
}

// ---- view ----
static EntityState cal(const char *state = "off") {
  EntityState s;
  s.id = "calendar.family";
  s.state = state;
  s.available = std::string(state) != "unavailable";
  return s;
}

static Data data(std::initializer_list<Event> e) {
  Data d;
  d.events = e;
  return d;
}

static Event timed(const char *title, const char *s, const char *e) {
  Event v;
  v.title = title;
  v.start = at(s);
  v.end = at(e);
  return v;
}

static Event allDay(const char *title, const char *day, const char *endDay) {
  Event v = timed(title, day, endDay);
  v.allDay = true;
  return v;
}

void test_view_time_texts() {
  const int64_t now = at("2026-11-03T10:00");
  EntityState st = cal();
  Data d = data({timed("Dentist", "2026-11-03T14:30", "2026-11-03T15:30"),
                 timed("Team stand-up", "2026-11-04T09:00", "2026-11-04T09:30"),
                 timed("Third", "2026-11-04T11:00", "2026-11-04T12:00")});
  CalendarView v = calendarView(&st, &d, now);
  TEST_ASSERT_TRUE(v.look == Look::Normal);
  TEST_ASSERT_FALSE(v.empty);
  TEST_ASSERT_EQUAL_INT(2, v.count);  // only the first two
  TEST_ASSERT_EQUAL_STRING("14:30", v.rows[0].time.c_str());
  TEST_ASSERT_EQUAL_STRING("Dentist", v.rows[0].title.c_str());
  TEST_ASSERT_EQUAL_STRING("Wed 09:00", v.rows[1].time.c_str());  // 2026-11-04 is a Wednesday
  TEST_ASSERT_EQUAL_STRING("Team stand-up", v.rows[1].title.c_str());
}

void test_view_running_and_ended() {
  const int64_t now = at("2026-11-03T14:45");
  EntityState st = cal();
  Data d = data({timed("Over", "2026-11-03T08:00", "2026-11-03T09:00"),
                 timed("Dentist", "2026-11-03T14:30", "2026-11-03T15:30"),
                 timed("Later", "2026-11-03T18:00", "2026-11-03T19:00")});
  CalendarView v = calendarView(&st, &d, now);
  TEST_ASSERT_EQUAL_INT(2, v.count);
  TEST_ASSERT_EQUAL_STRING("now", v.rows[0].time.c_str());
  TEST_ASSERT_EQUAL_STRING("Dentist", v.rows[0].title.c_str());
  TEST_ASSERT_EQUAL_STRING("18:00", v.rows[1].time.c_str());
  // an event that ends exactly now is over
  v = calendarView(&st, &d, at("2026-11-03T15:30"));
  TEST_ASSERT_EQUAL_STRING("Later", v.rows[0].title.c_str());
}

void test_view_all_day() {
  const int64_t now = at("2026-11-03T10:00");
  EntityState st = cal();
  Data d = data({allDay("Today", "2026-11-03", "2026-11-04"), allDay("Later", "2026-11-05", "2026-11-06")});
  CalendarView v = calendarView(&st, &d, now);
  TEST_ASSERT_EQUAL_STRING("all day", v.rows[0].time.c_str());
  TEST_ASSERT_EQUAL_STRING("Thu", v.rows[1].time.c_str());
  // an all-day event that started yesterday and runs over today counts as today
  d = data({allDay("Trip", "2026-11-02", "2026-11-05")});
  v = calendarView(&st, &d, now);
  TEST_ASSERT_EQUAL_STRING("all day", v.rows[0].time.c_str());
  // yesterday's all-day event is over
  d = data({allDay("Old", "2026-11-02", "2026-11-03")});
  TEST_ASSERT_TRUE(calendarView(&st, &d, now).empty);
}

void test_view_clock_unset() {
  EntityState st = cal();
  Data d = data({timed("Dentist", "2026-11-03T14:30", "2026-11-03T15:30"), allDay("Holiday", "2026-11-05", "2026-11-06")});
  CalendarView v = calendarView(&st, &d, 0);  // no filtering, no now/today words
  TEST_ASSERT_EQUAL_STRING("Tue 14:30", v.rows[0].time.c_str());
  TEST_ASSERT_EQUAL_STRING("Thu", v.rows[1].time.c_str());
}

void test_view_empty() {
  EntityState st = cal();
  Data d;
  CalendarView v = calendarView(&st, &d, at("2026-11-03T10:00"));
  TEST_ASSERT_TRUE(v.look == Look::Normal);
  TEST_ASSERT_TRUE(v.empty);
  TEST_ASSERT_EQUAL_INT(0, v.count);
  TEST_ASSERT_EQUAL_STRING("No events", kEmptyTitle);
  TEST_ASSERT_EQUAL_STRING("next 7 days", kEmptySub);
}

void test_view_loading_and_unavailable() {
  CalendarView v = calendarView(nullptr, nullptr, at("2026-11-03T10:00"));
  TEST_ASSERT_TRUE(v.look == Look::Loading);
  TEST_ASSERT_EQUAL_STRING("-", v.rows[0].time.c_str());
  TEST_ASSERT_EQUAL_STRING("-", v.rows[1].time.c_str());
  TEST_ASSERT_EQUAL_STRING("", v.rows[0].title.c_str());
  EntityState st = cal("unavailable");
  Data d = data({timed("X", "2026-11-03T14:30", "2026-11-03T15:30")});
  TEST_ASSERT_TRUE(calendarView(&st, &d, at("2026-11-03T10:00")).look == Look::Unavailable);
}

void test_view_attribute_fallback() {
  const int64_t now = at("2026-11-03T10:00");
  EntityState st = cal("on");
  st.attrs.emplace_back("message", "Dentist");
  st.attrs.emplace_back("start_time", "2026-11-03 14:30:00");
  st.attrs.emplace_back("end_time", "2026-11-03 15:30:00");
  st.attrs.emplace_back("all_day", "false");
  CalendarView v = calendarView(&st, nullptr, now);  // get_events not answered yet: the entity's next event
  TEST_ASSERT_EQUAL_INT(1, v.count);
  TEST_ASSERT_EQUAL_STRING("14:30", v.rows[0].time.c_str());
  TEST_ASSERT_EQUAL_STRING("Dentist", v.rows[0].title.c_str());
  EntityState none = cal("off");  // no message attribute: nothing planned
  TEST_ASSERT_TRUE(calendarView(&none, nullptr, now).empty);
  // an answer, even an empty one, wins over the attributes
  Data d;
  TEST_ASSERT_TRUE(calendarView(&st, &d, now).empty);
}

// ---- layout ----
void test_layout_numbers() {
  for (int W : {364, 508, 552}) {
    const Layout l = layoutFor(W);
    TEST_ASSERT_EQUAL_INT(12, l.timeX);
    TEST_ASSERT_EQUAL_INT(118, l.titleX);
    TEST_ASSERT_EQUAL_INT(36, l.cy[0]);
    TEST_ASSERT_EQUAL_INT(69, l.cy[1]);
    TEST_ASSERT_EQUAL_INT(W - 130, l.titleMaxW);
    TEST_ASSERT_EQUAL_INT(52, l.unavailCy);
    TEST_ASSERT_EQUAL_INT(W - 36, l.badgeX);
    TEST_ASSERT_EQUAL_INT(12, l.badgeY);
  }
  TEST_ASSERT_EQUAL_INT(234, layoutFor(364).titleMaxW);  // spec: landscape title width 234
  TEST_ASSERT_EQUAL_INT(378, layoutFor(508).titleMaxW);  // spec: portrait 378
}

// ---- store ----
void test_store_due_rules() {
  Store s;
  TEST_ASSERT_TRUE(s.due("calendar.a", 1000, true));   // scheduled wake: ask
  TEST_ASSERT_TRUE(s.due("calendar.a", 1000, false));  // no data
  s.set("calendar.a", {}, 1000);
  TEST_ASSERT_FALSE(s.due("calendar.a", 1000 + 600, false));
  TEST_ASSERT_TRUE(s.due("calendar.a", 1000 + kMaxAgeS + 1, false));
  s.markAsked("calendar.a", 5000);
  TEST_ASSERT_FALSE(s.due("calendar.a", 5001, true));  // once per wake
  TEST_ASSERT_FALSE(s.due("calendar.a", 5000 + 60, false));
  TEST_ASSERT_TRUE(s.due("calendar.a", 5000 + kMaxAgeS + 1, false));
  s.newSession();
  TEST_ASSERT_TRUE(s.due("calendar.a", 5001, true));
  TEST_ASSERT_NULL(s.get("calendar.zzz"));
  TEST_ASSERT_NOT_NULL(s.get("calendar.a"));
}

void test_entities_on_shown_tab_only() {
  auto r = config::parseConfig(
      "version: \"1\"\ntabs:\n  - title: A\n    widgets:\n      - entity: calendar.a\n      - entity: weather.x\n"
      "      - entity: calendar.a\n  - title: B\n    widgets:\n      - entity: calendar.b\n");
  TEST_ASSERT_TRUE(r.ok());
  auto a = entitiesOnTab(r.config, 0);
  TEST_ASSERT_EQUAL_UINT(1, a.size());
  TEST_ASSERT_EQUAL_STRING("calendar.a", a[0].c_str());
  TEST_ASSERT_EQUAL_STRING("calendar.b", entitiesOnTab(r.config, 1)[0].c_str());
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

void test_worker_calendar_ok() {
  FakeTransport t;
  t.queue = {ok(kAnswer)};
  app_logic::HaWorker w(t, "http://ha.local:8123", "", "TOKEN");
  app_logic::NetCommand c;
  c.type = app_logic::NetCommand::Type::Calendar;
  c.calendarEntity = "calendar.family";
  app_logic::NetEvent e = w.run(c);
  TEST_ASSERT_TRUE(e.type == app_logic::NetEvent::Type::CalendarDone);
  TEST_ASSERT_TRUE(e.error == HaError::None);
  TEST_ASSERT_EQUAL_UINT(3, e.calendarEvents.size());
  TEST_ASSERT_EQUAL_STRING("calendar.family", e.calendarEntity.c_str());
  TEST_ASSERT_EQUAL_UINT(1, t.calls.size());
  TEST_ASSERT_EQUAL_STRING("POST", t.calls[0].method.c_str());
  TEST_ASSERT_EQUAL_STRING("http://ha.local:8123/api/services/calendar/get_events?return_response",
                           t.calls[0].url.c_str());
  TEST_ASSERT_EQUAL_STRING("{\"entity_id\":\"calendar.family\",\"duration\":{\"days\":7}}", t.calls[0].body.c_str());
}

void test_worker_calendar_failures() {
  FakeTransport t;
  app_logic::HaWorker w(t, "http://ha.local:8123", "", "TOKEN");
  app_logic::NetCommand c;
  c.type = app_logic::NetCommand::Type::Calendar;
  c.calendarEntity = "calendar.family";
  t.queue = {ok("garbage")};
  TEST_ASSERT_TRUE(w.run(c).error == HaError::Parse);
  HttpResponse bad;
  bad.status = 500;
  t.queue = {bad};
  app_logic::NetEvent e = w.run(c);
  TEST_ASSERT_TRUE(e.error == HaError::Http);
  TEST_ASSERT_EQUAL_UINT(0, e.calendarEvents.size());
  c.calendarEntity = "light.x";  // not a calendar: no request at all
  const size_t before = t.calls.size();
  TEST_ASSERT_TRUE(w.run(c).error == HaError::Parse);
  TEST_ASSERT_EQUAL_UINT(before, t.calls.size());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_config_calendar_type);
  RUN_TEST(test_config_cache_keeps_calendar);
  RUN_TEST(test_size_2x1_default_3x1_max);
  RUN_TEST(test_not_focusable);
  RUN_TEST(test_request);
  RUN_TEST(test_parse_response_sorted);
  RUN_TEST(test_parse_response_empty_and_bad);
  RUN_TEST(test_parse_caps_events_and_default_end);
  RUN_TEST(test_parse_local);
  RUN_TEST(test_view_time_texts);
  RUN_TEST(test_view_running_and_ended);
  RUN_TEST(test_view_all_day);
  RUN_TEST(test_view_clock_unset);
  RUN_TEST(test_view_empty);
  RUN_TEST(test_view_loading_and_unavailable);
  RUN_TEST(test_view_attribute_fallback);
  RUN_TEST(test_layout_numbers);
  RUN_TEST(test_store_due_rules);
  RUN_TEST(test_entities_on_shown_tab_only);
  RUN_TEST(test_worker_calendar_ok);
  RUN_TEST(test_worker_calendar_failures);
  return UNITY_END();
}
