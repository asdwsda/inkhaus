#include "brightness_layout.h"
#include "brightness_popup.h"
#include "deferred_tap.h"
#include <cstring>
#include <fstream>
#include <sstream>
#include <unity.h>

#include "app_state.h"
#include "config_model.h"
#include "demo_states.h"
#include "fake_actions.h"
#include "ui_layout.h"
#include "widget_view.h"

using namespace uistate;
using config::WidgetType;

void test_brightness_popup_steps_and_bar() {
  uistate::BrightnessPopup p;
  p.open("light.a", 0, 2, 37, 0);
  TEST_ASSERT_EQUAL(37, p.value());
  TEST_ASSERT_TRUE(p.step(+1, 10));
  TEST_ASSERT_EQUAL(40, p.value());
  TEST_ASSERT_TRUE(p.step(-1, 20));
  TEST_ASSERT_EQUAL(30, p.value());
  p.setPct(95, 30);
  p.step(+1, 40);
  TEST_ASSERT_EQUAL(100, p.value());
  TEST_ASSERT_FALSE(p.step(+1, 50));
  TEST_ASSERT_TRUE(p.preset(1, 60));
  TEST_ASSERT_EQUAL(50, p.value());
  TEST_ASSERT_TRUE(p.tapBar(0.734, 70));
  TEST_ASSERT_EQUAL(73, p.value());
  p.tapBar(1.7, 80);
  TEST_ASSERT_EQUAL(100, p.value());
  TEST_ASSERT_TRUE(p.off(90));
  TEST_ASSERT_EQUAL(0, p.value());
  TEST_ASSERT_FALSE(p.step(-1, 100));
  TEST_ASSERT_EQUAL(0, uistate::brightnessToPct(0));
  TEST_ASSERT_EQUAL(60, uistate::brightnessToPct(153));
  TEST_ASSERT_EQUAL(100, uistate::brightnessToPct(255));
}

void test_brightness_popup_debounce_sends_last_value() {
  uistate::BrightnessPopup p;
  std::vector<int> sent;
  p.setSink([&](const std::string &id, int pct) {
    TEST_ASSERT_EQUAL_STRING("light.a", id.c_str());
    sent.push_back(pct);
  });
  p.open("light.a", 0, 0, 50, 1000);
  p.step(+1, 1000);
  p.step(+1, 1200);
  p.step(+1, 1300);
  p.tick(1600);  // 300 ms after the last input
  TEST_ASSERT_EQUAL(0, (int)sent.size());
  TEST_ASSERT_EQUAL(80, p.optimisticPct());
  p.tick(1700);
  TEST_ASSERT_EQUAL(1, (int)sent.size());
  TEST_ASSERT_EQUAL(80, sent[0]);
  p.tick(2000);
  TEST_ASSERT_EQUAL(1, (int)sent.size());  // nothing new, nothing sent
  p.step(-1, 2100);
  p.done(2150);  // Done flushes at once and closes
  TEST_ASSERT_EQUAL(2, (int)sent.size());
  TEST_ASSERT_EQUAL(70, sent[1]);
  TEST_ASSERT_FALSE(p.isOpen());
  TEST_ASSERT_EQUAL(70, p.optimisticPct());  // unconfirmed until the driver says so
  p.clearOptimistic();
  TEST_ASSERT_EQUAL(-1, p.optimisticPct());
}

void test_brightness_popup_idle_close_and_off() {
  uistate::BrightnessPopup p;
  std::vector<int> sent;
  p.setSink([&](const std::string &, int pct) { sent.push_back(pct); });
  p.open("light.a", 1, 1, 30, 0);
  p.off(5000);
  TEST_ASSERT_FALSE(p.tick(14000));
  TEST_ASSERT_EQUAL(1, (int)sent.size());
  TEST_ASSERT_EQUAL(0, sent[0]);
  TEST_ASSERT_TRUE(p.isOpen());
  p.preset(0, 14500);  // input restarts the 10 s timer
  TEST_ASSERT_FALSE(p.tick(24000));
  TEST_ASSERT_TRUE(p.tick(24500));
  TEST_ASSERT_FALSE(p.isOpen());
  TEST_ASSERT_EQUAL(2, (int)sent.size());
  TEST_ASSERT_EQUAL(25, sent[1]);
}

void setUp() {}
void tearDown() {}

static EntityState E(const char *id, const char *state, bool avail = true) {
  EntityState e;
  e.id = id;
  e.state = state;
  e.available = avail;
  return e;
}

static config::Config exampleConfig() {
  std::ifstream f("config.example.yaml");
  std::stringstream ss;
  ss << f.rdbuf();
  auto r = config::parseConfig(ss.str());
  TEST_ASSERT_TRUE(r.ok());
  return r.config;
}

// ---- views ----

void test_fmt_number() {
  TEST_ASSERT_EQUAL_STRING("21.5", fmtNumber(21.5, 1).c_str());
  TEST_ASSERT_EQUAL_STRING("22", fmtNumber(22.0, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("0.0", fmtNumber(-0.04, 1).c_str());
  TEST_ASSERT_EQUAL_STRING("-12.5", fmtNumber(-12.5, 1).c_str());
}

void test_cover_single() {
  EntityState c = E("cover.a", "opening");
  c.current_position = 40;
  CoverView v = coverView({&c}, false);
  TEST_ASSERT_EQUAL_STRING("Opening 40%", v.state.c_str());
  TEST_ASSERT_TRUE(v.upFilled);
  TEST_ASSERT_FALSE(v.downFilled);
  c.state = "open";
  c.current_position = 100;
  TEST_ASSERT_EQUAL_STRING("Open", coverView({&c}, false).state.c_str());
  c.current_position = 60;
  TEST_ASSERT_EQUAL_STRING("Open 60%", coverView({&c}, false).state.c_str());
  c.state = "closing";
  v = coverView({&c}, false);
  TEST_ASSERT_TRUE(v.downFilled);
  TEST_ASSERT_EQUAL_STRING("Closing 60%", v.state.c_str());
  c.state = "closed";
  TEST_ASSERT_EQUAL_STRING("Closed", coverView({&c}, false).state.c_str());
}

void test_cover_unavailable_and_loading() {
  EntityState c = E("cover.a", "unavailable", false);
  TEST_ASSERT_EQUAL(Look::Unavailable, coverView({&c}, false).look);
  TEST_ASSERT_EQUAL_STRING("n/a", coverView({&c}, false).state.c_str());
  CoverView l = coverView({nullptr}, false);
  TEST_ASSERT_EQUAL(Look::Loading, l.look);
  TEST_ASSERT_EQUAL_STRING("-", l.state.c_str());
}

void test_cover_group() {
  EntityState a = E("cover.a", "closed"), b = E("cover.b", "closed");
  a.current_position = 0;
  b.current_position = 0;
  TEST_ASSERT_EQUAL_STRING("Closed", coverView({&a, &b}, true).state.c_str());
  b.state = "opening";
  CoverView v = coverView({&a, &b}, true);
  TEST_ASSERT_EQUAL_STRING("Opening", v.state.c_str());
  TEST_ASSERT_TRUE(v.upFilled);
  a.state = b.state = "open";
  a.current_position = b.current_position = 30;
  TEST_ASSERT_EQUAL_STRING("Open 30%", coverView({&a, &b}, true).state.c_str());
  b.current_position = 50;
  TEST_ASSERT_EQUAL_STRING("Mixed", coverView({&a, &b}, true).state.c_str());
  b.available = false;
  CoverView p = coverView({&a, &b}, true);
  TEST_ASSERT_TRUE(p.alertBadge);
  TEST_ASSERT_EQUAL(Look::Normal, p.look);
}

void test_toggle_text() {
  EntityState l = E("light.a", "on");
  l.brightness = 204;
  TEST_ASSERT_EQUAL_STRING("On 80%", toggleView(WidgetType::Light, &l, false).state.c_str());
  TEST_ASSERT_EQUAL_STRING("On", toggleView(WidgetType::Light, &l, true).state.c_str());
  TEST_ASSERT_TRUE(toggleView(WidgetType::Light, &l, false).on);
  l.state = "off";
  ToggleView off = toggleView(WidgetType::Light, &l, false);
  TEST_ASSERT_FALSE(off.on);
  TEST_ASSERT_EQUAL_STRING("Off", off.state.c_str());
  EntityState sw = E("switch.a", "on");
  TEST_ASSERT_EQUAL_STRING("On", toggleView(WidgetType::Switch, &sw, false).state.c_str());
  EntityState un = E("light.b", "unavailable", false);
  TEST_ASSERT_EQUAL_STRING("n/a", toggleView(WidgetType::Light, &un, false).state.c_str());
  TEST_ASSERT_EQUAL_STRING("-", toggleView(WidgetType::Light, nullptr, false).state.c_str());
  // Window sensor: Open/Closed (on = open); other binary sensors stay On/Off.
  EntityState win = E("binary_sensor.w", "on");
  TEST_ASSERT_EQUAL_STRING("On", toggleView(WidgetType::BinarySensor, &win, false).state.c_str());
  win.device_class = "window";
  TEST_ASSERT_EQUAL_STRING("Open", toggleView(WidgetType::BinarySensor, &win, false).state.c_str());
  TEST_ASSERT_TRUE(toggleView(WidgetType::BinarySensor, &win, false).on);
  win.state = "off";
  TEST_ASSERT_EQUAL_STRING("Closed", toggleView(WidgetType::BinarySensor, &win, false).state.c_str());
}

void test_climate_view() {
  EntityState c = E("climate.a", "heat");
  c.hvac_mode = "heat";
  c.hvac_action = "heating";
  c.current_temperature = 21.5;
  c.temperature = 22.0;
  c.current_humidity = 45;
  c.target_temp_step = 0.5;
  ClimateView v = climateView(&c);
  TEST_ASSERT_EQUAL_STRING("21.5\xC2\xB0", v.temp.c_str());
  TEST_ASSERT_EQUAL_STRING("22.0\xC2\xB0", v.target.c_str());
  TEST_ASSERT_EQUAL_STRING("45%", v.hum.c_str());
  TEST_ASSERT_EQUAL(ActionIcon::Flame, v.act);
  c.target_temp_step = 1;
  TEST_ASSERT_EQUAL_STRING("22\xC2\xB0", climateView(&c).target.c_str());
  c.hvac_action = "off";  // unknown action: falls back to the mode icon
  c.hvac_mode = "cool";
  TEST_ASSERT_EQUAL(ActionIcon::Snow, climateView(&c).act);
  c.hvac_mode = "off";
  c.state = "off";
  v = climateView(&c);
  TEST_ASSERT_EQUAL_STRING("Off", v.target.c_str());
  TEST_ASSERT_EQUAL(ActionIcon::Power, v.act);
  c.current_humidity.reset();
  TEST_ASSERT_TRUE(climateView(&c).hum.empty());
}

void test_sensor_numeric_parsing() {
  EntityState s = E("sensor.t", "8.4");
  const char *good[] = {"8.4", "-3", "+2.5", ".5", "5.", "1e3", "-1.5E-2", "0"};
  const char *bad[] = {"nan", "inf", "-inf", "NaN", "0x1A", "", " 5", "5 ", "1e", "e5", ".", "-", "1e999", "5,5", "1.2.3", "infinity"};
  for (auto g : good) { s.state = g; TEST_ASSERT_TRUE_MESSAGE(sensorView(&s).numeric, g); }
  for (auto b : bad) { s.state = b; TEST_ASSERT_FALSE_MESSAGE(sensorView(&s).numeric, b); }
}

void test_fake_climate_step_zero_or_negative() {
  EntityState c = E("climate.a", "heat");
  c.temperature = 21.0;
  for (double st : {0.0, -1.0}) {
    c.target_temp_step = st;
    ActionResult r = fakeAction(WidgetType::Climate, 2, {&c});
    TEST_ASSERT_TRUE(r.changed);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 21.5, *r.states[0].temperature);
  }
}

void test_humanise_and_text_state() {
  TEST_ASSERT_EQUAL_STRING("Above horizon", humanise("above_horizon").c_str());
  TEST_ASSERT_EQUAL_STRING("21.5", humanise("21.5").c_str());
  TEST_ASSERT_EQUAL_STRING("-", humanise("").c_str());
  TEST_ASSERT_EQUAL_STRING("Heat cool", humanise("HEAT_COOL").c_str());
  TEST_ASSERT_EQUAL_STRING("-", humanise("Unknown").c_str());
  TEST_ASSERT_EQUAL_STRING("n/a", humanise("UNAVAILABLE").c_str());
  EntityState s = E("sun.sun", "above_horizon");
  SensorView sv = sensorView(&s);
  TEST_ASSERT_TRUE(sv.textState);
  TEST_ASSERT_EQUAL_STRING("Above horizon", sv.value.c_str());
  s.state = "8.4";
  sv = sensorView(&s);
  TEST_ASSERT_FALSE(sv.textState);
  TEST_ASSERT_EQUAL_STRING("8.4", sv.value.c_str());
}

void test_sensor_weather_vacuum() {
  EntityState s = E("sensor.t", "8.4");
  s.unit_of_measurement = "\xC2\xB0" "C";
  SensorView sv = sensorView(&s);
  TEST_ASSERT_TRUE(sv.numeric);
  TEST_ASSERT_EQUAL_STRING("\xC2\xB0" "C", sv.unit.c_str());
  s.unit_of_measurement = "kWh";
  TEST_ASSERT_EQUAL_STRING(" kWh", sensorView(&s).unit.c_str());
  s.state = "home";
  TEST_ASSERT_FALSE(sensorView(&s).numeric);
  TEST_ASSERT_TRUE(sensorView(&s).unit.empty());

  EntityState w = E("weather.h", "cloudy");
  w.temperature = 12.4;
  w.humidity = 76;
  WeatherView wv = weatherView(&w);
  TEST_ASSERT_EQUAL_STRING("12", wv.temp.c_str());
  TEST_ASSERT_EQUAL_STRING("Cloudy", wv.cond.c_str());
  TEST_ASSERT_EQUAL(WeatherIcon::Cloudy, wv.icon);
  TEST_ASSERT_EQUAL_STRING("76%", wv.hum.c_str());
  w.state = "exceptional";
  TEST_ASSERT_EQUAL(WeatherIcon::Unknown, weatherView(&w).icon);

  EntityState v = E("vacuum.r", "cleaning");
  v.battery_level = 12;
  VacuumView vv = vacuumView(&v);
  TEST_ASSERT_TRUE(vv.startFilled);
  TEST_ASSERT_TRUE(vv.lowBattery);
  v.state = "error";
  v.status = "Stuck";
  vv = vacuumView(&v);
  TEST_ASSERT_TRUE(vv.error);
  TEST_ASSERT_FALSE(vv.showBattery);
  TEST_ASSERT_EQUAL_STRING("Stuck", vv.status.c_str());
}

// ---- fake actions ----

void test_fake_toggle_and_group() {
  EntityState a = E("light.a", "off"), b = E("light.b", "off");
  ActionResult r = fakeAction(WidgetType::Light, 0, {&a, &b});
  TEST_ASSERT_TRUE(r.changed);
  TEST_ASSERT_EQUAL(2, r.states.size());
  TEST_ASSERT_EQUAL_STRING("on", r.states[0].state.c_str());
  TEST_ASSERT_EQUAL_STRING("on", r.states[1].state.c_str());
  TEST_ASSERT_TRUE(r.states[0].brightness.has_value());
  // mixed group: any on -> everything goes off
  EntityState c = E("light.c", "off"), d = E("light.d", "on");
  ActionResult m = fakeAction(WidgetType::Light, 0, {&c, &d});
  TEST_ASSERT_EQUAL_STRING("off", m.states[0].state.c_str());
  TEST_ASSERT_EQUAL_STRING("off", m.states[1].state.c_str());
  EntityState un = E("light.c", "unavailable", false);
  TEST_ASSERT_FALSE(fakeAction(WidgetType::Light, 0, {&un}).changed);
}

void test_toggle_intent_from_shown_state() {
  EntityState off = E("light.a", "off"), on = E("light.b", "on");
  EntityState un = E("light.c", "unavailable", false);
  TEST_ASSERT_TRUE(toggleIntent(WidgetType::Light, {&off}) == ToggleIntent::TurnOn);
  TEST_ASSERT_TRUE(toggleIntent(WidgetType::Light, {&on}) == ToggleIntent::TurnOff);
  TEST_ASSERT_TRUE(toggleIntent(WidgetType::Switch, {&off, &on}) == ToggleIntent::TurnOff);
  TEST_ASSERT_TRUE(toggleIntent(WidgetType::Automation, {&on, &off}) == ToggleIntent::TurnOff);
  TEST_ASSERT_TRUE(toggleIntent(WidgetType::Light, {&un, &on}) == ToggleIntent::TurnOff);
  TEST_ASSERT_TRUE(toggleIntent(WidgetType::Light, {&un}) == ToggleIntent::None);
  TEST_ASSERT_TRUE(toggleIntent(WidgetType::Light, {nullptr}) == ToggleIntent::None);
  TEST_ASSERT_TRUE(toggleIntent(WidgetType::Cover, {&on}) == ToggleIntent::None);
}

void test_fake_cover() {
  EntityState c = E("cover.a", "closed");
  c.current_position = 0;
  TEST_ASSERT_EQUAL_STRING("opening", fakeAction(WidgetType::Cover, 1, {&c}).states[0].state.c_str());
  TEST_ASSERT_EQUAL_STRING("closing", fakeAction(WidgetType::Cover, 3, {&c}).states[0].state.c_str());
  TEST_ASSERT_EQUAL_STRING("closed", fakeAction(WidgetType::Cover, 2, {&c}).states[0].state.c_str());
  c.state = "opening";
  c.current_position = 40;
  TEST_ASSERT_EQUAL_STRING("open", fakeAction(WidgetType::Cover, 2, {&c}).states[0].state.c_str());
  TEST_ASSERT_FALSE(fakeAction(WidgetType::Cover, 0, {&c}).changed);
}

// Task 040: the user's shutter (state unknown, supported_features 11, no position).
void test_unknown_cover_no_position() {
  EntityState c = E("cover.livingroom_shutter", "unknown");
  c.supported_features = 11;
  CoverView v = coverView({&c}, false);
  TEST_ASSERT_TRUE(v.look == Look::Normal);
  TEST_ASSERT_EQUAL_STRING("-", v.state.c_str());
  TEST_ASSERT_EQUAL_STRING("-", coverView({&c, &c}, true).state.c_str());
  ActionResult open = fakeAction(WidgetType::Cover, 1, {&c});
  TEST_ASSERT_TRUE(open.changed);
  TEST_ASSERT_EQUAL_STRING("Opening", coverView({&open.states[0]}, false).state.c_str());
  TEST_ASSERT_EQUAL_STRING("Closing", coverView({&fakeAction(WidgetType::Cover, 3, {&c}).states[0]}, false).state.c_str());
  ActionResult stop = fakeAction(WidgetType::Cover, 2, {&c});
  TEST_ASSERT_EQUAL_STRING("Stopped", coverView({&stop.states[0]}, false).state.c_str());
  EntityState un = E("cover.u", "unavailable", false);
  TEST_ASSERT_TRUE(coverView({&un}, false).look == Look::Unavailable);
  TEST_ASSERT_FALSE(fakeAction(WidgetType::Cover, 1, {&un}).changed);
}

void test_fake_climate_step_and_limits() {
  EntityState c = E("climate.a", "heat");
  c.temperature = 22.0;
  c.target_temp_step = 0.5;
  c.min_temp = 20;
  c.max_temp = 22.5;
  ActionResult r = fakeAction(WidgetType::Climate, 2, {&c});
  TEST_ASSERT_TRUE(r.changed);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 22.5, *r.states[0].temperature);
  c.temperature = 22.5;
  TEST_ASSERT_FALSE(fakeAction(WidgetType::Climate, 2, {&c}).changed);
  c.temperature = 20.0;
  TEST_ASSERT_FALSE(fakeAction(WidgetType::Climate, 1, {&c}).changed);
}

void test_fake_vacuum() {
  EntityState v = E("vacuum.r", "docked");
  TEST_ASSERT_EQUAL_STRING("cleaning", fakeAction(WidgetType::Vacuum, 1, {&v}).states[0].state.c_str());
  TEST_ASSERT_EQUAL_STRING("paused", fakeAction(WidgetType::Vacuum, 2, {&v}).states[0].state.c_str());
  TEST_ASSERT_EQUAL_STRING("returning", fakeAction(WidgetType::Vacuum, 3, {&v}).states[0].state.c_str());
}

void test_demo_states_cover_every_entity() {
  config::Config cfg = exampleConfig();
  auto st = demoStates(cfg);
  TEST_ASSERT_EQUAL(cfg.entityIds.size(), st.size());
  for (const auto &s : st) TEST_ASSERT_TRUE(s.available);
}

void test_demo_unavailable_suffix() {
  config::Config cfg;
  cfg.entityIds = {"light.garage_unavailable", "light.ok"};
  auto st = demoStates(cfg);
  TEST_ASSERT_FALSE(st[0].available);
  TEST_ASSERT_TRUE(st[1].available);
  TEST_ASSERT_EQUAL_STRING("Ok", prettyName("light.ok").c_str());
}

static config::Config demoConfig() {
  std::ifstream f("demo.yaml");
  std::stringstream ss;
  ss << f.rdbuf();
  auto r = config::parseConfig(ss.str());
  TEST_ASSERT_TRUE(r.errors.empty());
  TEST_ASSERT_FALSE(r.config.hasConnection);
  return r.config;
}

void test_demo_yaml_showcase() {
  config::Config cfg = demoConfig();
  TEST_ASSERT_TRUE(cfg.tabs.size() > 5);  // tab overflow
  auto st = demoStates(cfg);
  TEST_ASSERT_EQUAL(cfg.entityIds.size(), st.size());
  int unavailable = 0;
  for (const auto &s : st) {
    if (!s.available) { ++unavailable; continue; }
    TEST_ASSERT_TRUE(s.state != "home" || s.id == "person.alice");  // no generic fallback state
  }
  TEST_ASSERT_EQUAL_INT(1, unavailable);
  for (auto o : {settings::Orientation::Portrait, settings::Orientation::Landscape}) {
    bool pager = false;
    for (size_t t = 0; t < cfg.tabs.size(); ++t) {
      ui::Flow f = ui::computeFlow(o, cfg.tabs[t].widgets);
      TEST_ASSERT_EQUAL_INT(0, (int)f.clampReports.size());
      pager |= f.pagerVisible;
    }
    TEST_ASSERT_TRUE(pager);
  }
}

void test_demo_fails_entity_ends_failed() {
  config::Config cfg = demoConfig();
  AppState a;
  a.init(cfg, settings::Orientation::Portrait, true);
  int idx = -1;
  for (size_t i = 0; i < cfg.tabs[0].widgets.size(); ++i)
    if (cfg.tabs[0].widgets[i].entity == "switch.coffee_machine_fails") idx = int(i);
  TEST_ASSERT_TRUE(idx >= 0);
  ui::Rect r;
  for (auto &pl : a.layout().widgets) if (pl.index == idx) r = pl.rect;
  TEST_ASSERT_TRUE(a.touch({r.x + r.w / 2, r.y + r.h / 2}, 0));
  TEST_ASSERT_TRUE(a.widgetData(idx).optimistic);
  TEST_ASSERT_TRUE(a.tick(kPendingMs * 10));
  TEST_ASSERT_FALSE(a.widgetData(idx).optimistic);
  TEST_ASSERT_TRUE(a.widgetData(idx).failed);
}

// ---- app state ----

static settings::Orientation P = settings::Orientation::Portrait;

static int findWidget(const AppState &a, WidgetType t) {
  for (const auto &pl : a.layout().widgets) if (pl.type == t) return pl.index;
  return -1;
}

void test_app_toggle_pending_then_confirmed() {
  AppState a;
  a.init(exampleConfig(), P, true);
  // Ceiling light: the first light widget; it is on in the demo.
  int idx = findWidget(a, WidgetType::Light);
  TEST_ASSERT_TRUE(idx >= 0);
  ui::Rect r;
  for (auto &pl : a.layout().widgets) if (pl.index == idx) r = pl.rect;
  const std::string id = a.config().tabs[0].widgets[size_t(idx)].entity;
  TEST_ASSERT_EQUAL_STRING("on", a.store().get(id)->state.c_str());
  TEST_ASSERT_TRUE(a.touch({r.x + 10, r.y + 10}, 1000));
  WidgetData d = a.widgetData(idx);
  TEST_ASSERT_TRUE(d.optimistic);
  TEST_ASSERT_FALSE(d.pending);  // no hourglass any more
  TEST_ASSERT_EQUAL_STRING("off", d.members[0]->state.c_str());  // requested state shown at once
  TEST_ASSERT_EQUAL_STRING("on", a.store().get(id)->state.c_str());
  TEST_ASSERT_FALSE(a.tick(1000 + kPendingMs - 1));
  TEST_ASSERT_TRUE(a.tick(1000 + kPendingMs));
  TEST_ASSERT_FALSE(a.widgetData(idx).optimistic);
  TEST_ASSERT_EQUAL_STRING("off", a.store().get(id)->state.c_str());
}

void test_app_cover_button_pending_keeps_state() {
  AppState a;
  a.init(exampleConfig(), P, true);
  int idx = findWidget(a, WidgetType::Cover);
  ui::Rect r;
  for (auto &pl : a.layout().widgets) if (pl.index == idx) r = pl.rect;
  auto subs = ui::subRegions(WidgetType::Cover, config::WidgetSize{2, 1}, r);
  const ui::Rect stop = subs[1].rect;
  TEST_ASSERT_TRUE(a.touch({stop.x + 5, stop.y + 5}, 0));
  WidgetData d = a.widgetData(idx);
  TEST_ASSERT_TRUE(d.optimistic);
  TEST_ASSERT_EQUAL(2, d.pendingSub);
  TEST_ASSERT_EQUAL_STRING("open", d.members[0]->state.c_str());  // requested result (stop) shown at once
  TEST_ASSERT_EQUAL_STRING("opening", a.store().get(a.config().tabs[0].widgets[size_t(idx)].entity)->state.c_str());
  // the body of a multi-button widget does nothing
  TEST_ASSERT_FALSE(a.touch({r.x + 5, r.y + 5}, 10));
  TEST_ASSERT_TRUE(a.tick(kPendingMs));
  TEST_ASSERT_EQUAL_STRING("open", a.store().get(a.config().tabs[0].widgets[size_t(idx)].entity)->state.c_str());
}

void test_app_climate_taps_accumulate() {
  AppState a;
  a.init(exampleConfig(), P, true);
  int idx = findWidget(a, WidgetType::Climate);
  ui::Rect r;
  for (auto &pl : a.layout().widgets) if (pl.index == idx) r = pl.rect;
  auto subs = ui::subRegions(WidgetType::Climate, config::WidgetSize{2, 1}, r);
  const ui::Rect plus = subs[1].rect;
  TEST_ASSERT_TRUE(a.touch({plus.x + 5, plus.y + 5}, 0));
  TEST_ASSERT_TRUE(a.touch({plus.x + 5, plus.y + 5}, 100));
  WidgetData d = a.widgetData(idx);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 23.0, *d.members[0]->temperature);  // 22.0 + 2 x 0.5
  TEST_ASSERT_FALSE(a.tick(100 + kPendingMs - 1));
  TEST_ASSERT_TRUE(a.tick(100 + kPendingMs));
  TEST_ASSERT_FLOAT_WITHIN(0.001, 23.0, *a.store().get(a.config().tabs[0].widgets[size_t(idx)].entity)->temperature);
}

void test_app_tab_touch_is_full() {
  AppState a;
  a.init(exampleConfig(), P, true);
  const ui::Rect s1 = a.layout().tabSlots[1].rect;
  TEST_ASSERT_TRUE(a.touch({s1.x + 5, s1.y + 5}, 0));
  TEST_ASSERT_EQUAL(1, a.shownTab());
  TEST_ASSERT_TRUE(a.takeFull());
  TEST_ASSERT_FALSE(a.takeFull());
}

void test_app_rocker_tab_debounce() {
  AppState a;
  a.init(exampleConfig(), P, true);
  TEST_ASSERT_TRUE(a.rocker(ui::PhysicalKey::Down, 1000));  // NEXT: tab 1, content follows later
  TEST_ASSERT_EQUAL(1, a.focus().tab);
  TEST_ASSERT_EQUAL(0, a.shownTab());
  TEST_ASSERT_TRUE(a.focus().ringVisible);
  TEST_ASSERT_FALSE(a.tick(1399));
  TEST_ASSERT_EQUAL(0, a.shownTab());
  TEST_ASSERT_TRUE(a.tick(1400));
  TEST_ASSERT_EQUAL(1, a.shownTab());
  TEST_ASSERT_TRUE(a.takeFull());
}

void test_app_rocker_toggle_action() {
  AppState a;
  a.init(exampleConfig(), P, true);
  // PUSH in TAB mode enters WIDGET mode on the first focusable widget (the cover), NEXT moves on.
  TEST_ASSERT_TRUE(a.rocker(ui::PhysicalKey::Push, 0));
  TEST_ASSERT_EQUAL(ui::FocusMode::Widget, a.focus().mode);
  const int cover = a.focusedWidgetIndex();
  TEST_ASSERT_EQUAL(0, cover);
  TEST_ASSERT_TRUE(a.rocker(ui::PhysicalKey::Push, 10));  // multi-button: enter INNER
  TEST_ASSERT_EQUAL(ui::FocusMode::Inner, a.focus().mode);
  TEST_ASSERT_TRUE(a.rocker(ui::PhysicalKey::Push, 20));  // entry = stop
  TEST_ASSERT_TRUE(a.widgetData(0).optimistic);
  TEST_ASSERT_EQUAL(2, a.widgetData(0).pendingSub);
}

void test_app_error_screen() {
  AppState a;
  config::ConfigError e("tabs[0].widgets[1].entity", "unknown entity", 7);
  a.initError({e}, P);
  TEST_ASSERT_EQUAL(Screen::ConfigError, a.screen());
  TEST_ASSERT_FALSE(a.touch({10, 10}, 0));
  TEST_ASSERT_EQUAL(7, a.errors()[0].line);
}


// ---- tester additions (task 017) ----

void test_t_tick_survives_millis_wraparound() {
  AppState a;
  a.init(exampleConfig(), P, true);
  int idx = findWidget(a, WidgetType::Light);
  ui::Rect r;
  for (auto &pl : a.layout().widgets) if (pl.index == idx) r = pl.rect;
  const uint32_t t0 = 0xFFFFFF00u;  // commit time wraps past 2^32
  TEST_ASSERT_TRUE(a.touch({r.x + 10, r.y + 10}, t0));
  TEST_ASSERT_FALSE(a.tick(t0 + 100));
  TEST_ASSERT_TRUE(a.widgetData(idx).optimistic);
  TEST_ASSERT_TRUE(a.tick(t0 + kPendingMs));  // wrapped value
  TEST_ASSERT_FALSE(a.widgetData(idx).optimistic);
}

void test_t_tab_touch_cancels_rocker_debounce() {
  AppState a;
  a.init(exampleConfig(), P, true);
  TEST_ASSERT_TRUE(a.rocker(ui::PhysicalKey::Down, 1000));  // focus tab 1, content not yet
  const ui::Rect s0 = a.layout().tabSlots[0].rect;
  a.touch({s0.x + 5, s0.y + 5}, 1100);  // tap tab 0 meanwhile
  TEST_ASSERT_EQUAL(0, a.shownTab());
  TEST_ASSERT_EQUAL(0, a.focus().tab);
  a.takeFull();
  a.tick(5000);  // the stale debounce must not switch to tab 1
  TEST_ASSERT_EQUAL(0, a.shownTab());
  TEST_ASSERT_FALSE(a.takeFull());
}

void test_t_retap_same_tab_is_not_full() {
  AppState a;
  a.init(exampleConfig(), P, true);
  const ui::Rect s0 = a.layout().tabSlots[0].rect;
  a.touch({s0.x + 5, s0.y + 5}, 0);
  TEST_ASSERT_FALSE(a.takeFull());
}

void test_t_toggle_repeat_during_pending_ignored() {
  AppState a;
  a.init(exampleConfig(), P, true);
  int idx = findWidget(a, WidgetType::Light);
  ui::Rect r;
  for (auto &pl : a.layout().widgets) if (pl.index == idx) r = pl.rect;
  TEST_ASSERT_TRUE(a.touch({r.x + 10, r.y + 10}, 0));
  TEST_ASSERT_FALSE(a.touch({r.x + 10, r.y + 10}, 50));
  TEST_ASSERT_EQUAL(1, (int)a.pendingList().size());
  a.tick(kPendingMs);
  TEST_ASSERT_TRUE(a.pendingList().empty());
  // after the commit a new tap toggles back
  TEST_ASSERT_TRUE(a.touch({r.x + 10, r.y + 10}, 1000));
}

void test_t_touch_outside_everything_changes_nothing() {
  AppState a;
  a.init(exampleConfig(), P, true);
  TEST_ASSERT_FALSE(a.touch({-5, -5}, 0));
  TEST_ASSERT_FALSE(a.touch({100000, 100000}, 0));
  TEST_ASSERT_TRUE(a.pendingList().empty());
  TEST_ASSERT_FALSE(a.takeFull());
}

void test_t_error_screen_ignores_rocker_and_tick() {
  AppState a;
  a.initError({config::ConfigError("a.b", "bad", 3)}, P);
  TEST_ASSERT_FALSE(a.rocker(ui::PhysicalKey::Push, 0));
  TEST_ASSERT_FALSE(a.rocker(ui::PhysicalKey::LongPush, 0));
  TEST_ASSERT_FALSE(a.tick(100000));
  TEST_ASSERT_EQUAL(Screen::ConfigError, a.screen());
}

void test_t_cover_group_partly_unavailable_shows_badge() {
  EntityState a1 = E("cover.a", "open"), b = E("cover.b", "open", false);
  a1.current_position = 100;
  CoverView v = coverView({&a1, &b}, true);
  TEST_ASSERT_TRUE(v.alertBadge);
  TEST_ASSERT_EQUAL_STRING("Open", v.state.c_str());
  // a group with a member not loaded yet counts as incomplete too
  v = coverView({&a1, nullptr}, true);
  TEST_ASSERT_TRUE(v.alertBadge);
}

void test_t_fake_action_on_unavailable_or_missing_does_nothing() {
  EntityState u = E("light.a", "on", false);
  TEST_ASSERT_FALSE(fakeAction(WidgetType::Light, 0, {&u}).changed);
  TEST_ASSERT_FALSE(fakeAction(WidgetType::Light, 0, {nullptr}).changed);
  TEST_ASSERT_FALSE(fakeAction(WidgetType::Climate, 1, {}).changed);
  TEST_ASSERT_FALSE(fakeAction(WidgetType::Vacuum, 1, {nullptr}).changed);
  EntityState c = E("climate.a", "heat");  // no target temperature known
  TEST_ASSERT_FALSE(fakeAction(WidgetType::Climate, 2, {&c}).changed);
}

void test_t_light_group_turns_all_to_opposite_of_first() {
  EntityState a1 = E("light.a", "on"), b = E("light.b", "off");
  ActionResult r = fakeAction(WidgetType::Light, 0, {&a1, &b});
  TEST_ASSERT_EQUAL(2, (int)r.states.size());
  TEST_ASSERT_EQUAL_STRING("off", r.states[0].state.c_str());
  TEST_ASSERT_EQUAL_STRING("off", r.states[1].state.c_str());
}

void test_t_window_sensor_texts() {
  EntityState w = E("binary_sensor.w", "on");
  w.device_class = "window";
  TEST_ASSERT_EQUAL_STRING("Open", toggleView(WidgetType::BinarySensor, &w, false).state.c_str());
  w.state = "off";
  TEST_ASSERT_EQUAL_STRING("Closed", toggleView(WidgetType::BinarySensor, &w, false).state.c_str());
  w.device_class = "";
  TEST_ASSERT_EQUAL_STRING("Off", toggleView(WidgetType::BinarySensor, &w, false).state.c_str());
}

// ---- task 028: state wording (widgets.md section 11) ----

static void test_binary_sensor_table() {
  struct Row { const char *dc, *onT, *offT; StateIcon on, off; };
  using I = StateIcon;
  const Row rows[] = {
      {"door", "Open", "Closed", I::DoorOn, I::DoorOff},
      {"garage_door", "Open", "Closed", I::GarageOn, I::GarageOff},
      {"window", "Open", "Closed", I::WindowOn, I::WindowOff},
      {"opening", "Open", "Closed", I::WindowOn, I::WindowOff},
      {"motion", "Motion", "Clear", I::MotionOn, I::MotionOff},
      {"occupancy", "Present", "Empty", I::PersonOn, I::PersonOff},
      {"presence", "Home", "Away", I::PersonOn, I::PersonOff},
      {"moisture", "Wet", "Dry", I::PuddleOn, I::PuddleOff},
      {"smoke", "Smoke", "Clear", I::SmokeOn, I::SmokeOff},
      {"gas", "Gas", "Clear", I::GasOn, I::GasOff},
      {"problem", "Problem", "OK", I::ProblemOn, I::ProblemOff},
      {"battery", "Low", "OK", I::BatteryLow, I::Battery},
      {"connectivity", "Online", "Offline", I::Wifi, I::WifiOff},
      {"lock", "Open", "Locked", I::LockOpen, I::LockClosed},
      {"plug", "Plugged", "Out", I::Plug, I::PlugOff},
      {"power", "Power", "None", I::Bolt, I::BoltOff},
      {"", "On", "Off", I::DotOn, I::DotOff},
      {"vibration", "On", "Off", I::DotOn, I::DotOff},
  };
  for (const Row &r : rows) {
    EntityState e = E("binary_sensor.x", "on");
    e.device_class = r.dc;
    ToggleView on = toggleView(WidgetType::BinarySensor, &e, false);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(r.onT, on.state.c_str(), r.dc);
    TEST_ASSERT_TRUE_MESSAGE(on.icon == r.on, r.dc);
    TEST_ASSERT_TRUE(on.on);
    e.state = "off";
    ToggleView off = toggleView(WidgetType::BinarySensor, &e, false);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(r.offT, off.state.c_str(), r.dc);
    TEST_ASSERT_TRUE_MESSAGE(off.icon == r.off, r.dc);
    TEST_ASSERT_TRUE(off.look == Look::Normal);
    TEST_ASSERT_TRUE(strlen(r.onT) <= 7 && strlen(r.offT) <= 7);
  }
  EntityState u = E("binary_sensor.x", "unavailable", false);
  u.device_class = "door";
  ToggleView uv = toggleView(WidgetType::BinarySensor, &u, false);
  TEST_ASSERT_EQUAL_STRING("n/a", uv.state.c_str());
  TEST_ASSERT_TRUE(uv.look == Look::Unavailable);
  EntityState k = E("binary_sensor.x", "unknown");
  uv = toggleView(WidgetType::BinarySensor, &k, false);
  TEST_ASSERT_EQUAL_STRING("-", uv.state.c_str());
  TEST_ASSERT_TRUE(uv.look == Look::Normal);
}

static void test_vacuum_wording() {
  struct Row { const char *st, *text; bool start, pause, dock; };
  const Row rows[] = {{"cleaning", "Cleaning", true, false, false}, {"docked", "Docked", false, false, true},
                      {"returning", "Returning", false, false, true}, {"paused", "Paused", false, true, false},
                      {"idle", "Idle", false, false, false}};
  for (const Row &r : rows) {
    EntityState e = E("vacuum.r", r.st);
    e.battery_level = 80;
    VacuumView v = vacuumView(&e);
    TEST_ASSERT_EQUAL_STRING(r.text, v.status.c_str());
    TEST_ASSERT_EQUAL(r.start, v.startFilled);
    TEST_ASSERT_EQUAL(r.pause, v.pauseFilled);
    TEST_ASSERT_EQUAL(r.dock, v.dockFilled);
    TEST_ASSERT_TRUE(v.showBattery);
    TEST_ASSERT_FALSE(v.lowBattery);
  }
  EntityState e = E("vacuum.r", "error");
  VacuumView v = vacuumView(&e);
  TEST_ASSERT_EQUAL_STRING("Error", v.status.c_str());
  TEST_ASSERT_TRUE(v.error);
  TEST_ASSERT_FALSE(v.showBattery);
  e.status = "Brush stuck";
  TEST_ASSERT_EQUAL_STRING("Brush stuck", vacuumView(&e).status.c_str());
  EntityState u = E("vacuum.r", "unavailable", false);
  TEST_ASSERT_EQUAL_STRING("n/a", vacuumView(&u).status.c_str());
  EntityState k = E("vacuum.r", "unknown");
  TEST_ASSERT_EQUAL_STRING("-", vacuumView(&k).status.c_str());
  TEST_ASSERT_TRUE(vacuumView(&k).look == Look::Normal);
  EntityState o = E("vacuum.r", "spot_cleaning");
  TEST_ASSERT_EQUAL_STRING("Spot cleaning", vacuumView(&o).status.c_str());
  // Low battery: <= 20 % and not docked; the percent stays.
  EntityState l = E("vacuum.r", "cleaning");
  l.battery_level = 20;
  TEST_ASSERT_TRUE(vacuumView(&l).lowBattery);
  l.battery_level = 21;
  TEST_ASSERT_FALSE(vacuumView(&l).lowBattery);
  l.battery_level = 20;
  l.state = "docked";
  TEST_ASSERT_FALSE(vacuumView(&l).lowBattery);
  l.state = "returning";
  TEST_ASSERT_TRUE(vacuumView(&l).lowBattery);
  l.battery_level.reset();
  TEST_ASSERT_FALSE(vacuumView(&l).showBattery);
}

static void test_cover_wording() {
  struct Row { const char *st; std::optional<int> pos; const char *text; };
  const Row rows[] = {
      {"closed", std::nullopt, "Closed"}, {"closed", 0, "Closed"}, {"closed", 30, "Closed"},
      {"open", std::nullopt, "Open"}, {"open", 100, "Open"}, {"open", 40, "Open 40%"},
      {"opening", std::nullopt, "Opening"}, {"opening", 55, "Opening 55%"},
      {"closing", std::nullopt, "Closing"}, {"closing", 100, "Closing 100%"},
      {"unavailable", std::nullopt, "n/a"}, {"unknown", std::nullopt, "-"},
      {"weird_state", std::nullopt, "Weird state"},
  };
  for (const Row &r : rows) {
    EntityState e = E("cover.c", r.st, std::string(r.st) != "unavailable");
    e.current_position = r.pos;
    TEST_ASSERT_EQUAL_STRING_MESSAGE(r.text, coverView({&e}, false).state.c_str(), r.st);
  }
  EntityState o = E("cover.c", "opening");
  TEST_ASSERT_TRUE(coverView({&o}, false).upFilled);
  EntityState c = E("cover.c", "closing");
  TEST_ASSERT_TRUE(coverView({&c}, false).downFilled);
  TEST_ASSERT_EQUAL_STRING("-", coverView({nullptr}, false).state.c_str());
}

static void test_generic_fallback() {
  EntityState s = E("sensor.x", "partly_cloudy");
  TEST_ASSERT_EQUAL_STRING("Partly cloudy", sensorView(&s).value.c_str());
  s.state = "HEAT_COOL";
  TEST_ASSERT_EQUAL_STRING("Heat cool", sensorView(&s).value.c_str());
  s.state = "unknown";
  TEST_ASSERT_EQUAL_STRING("-", sensorView(&s).value.c_str());
  TEST_ASSERT_TRUE(sensorView(&s).look == Look::Normal);
  s.state = "";
  TEST_ASSERT_EQUAL_STRING("-", sensorView(&s).value.c_str());
}

static void test_mixed_group_aggregate() {
  EntityState on, off, un;
  on.id = "light.a"; on.state = "on"; on.available = true;
  off.id = "light.b"; off.state = "off"; off.available = true;
  un.id = "light.c"; un.state = "unavailable"; un.available = false;
  // off first, on second: shown On, intent TurnOff
  auto v = toggleView(WidgetType::Light, toggleSummary({&off, &on}), false);
  TEST_ASSERT_TRUE(v.on);
  TEST_ASSERT_EQUAL_STRING("On", v.state.c_str());
  TEST_ASSERT_TRUE(toggleIntent(WidgetType::Light, {&off, &on}) == ToggleIntent::TurnOff);
  TEST_ASSERT_TRUE(toggleIntent(WidgetType::Light, {&on, &off}) == ToggleIntent::TurnOff);
  // all off: Off, TurnOn
  TEST_ASSERT_FALSE(toggleView(WidgetType::Light, toggleSummary({&off, &off}), false).on);
  TEST_ASSERT_TRUE(toggleIntent(WidgetType::Light, {&off, &off}) == ToggleIntent::TurnOn);
  // unavailable first does not hide an on member
  TEST_ASSERT_TRUE(toggleView(WidgetType::Light, toggleSummary({&un, &off, &on}), false).on);
  // only unavailable: n/a
  TEST_ASSERT_EQUAL_STRING("n/a", toggleView(WidgetType::Light, toggleSummary({&un}), false).state.c_str());
  TEST_ASSERT_NULL(toggleSummary({}));
  TEST_ASSERT_NULL(toggleSummary({nullptr}));
}

// ---- marks and action sink (task 023 round 2) ----

struct Sunk { int tab, widget, sub; std::vector<EntityState> st; };

static ui::Rect widgetRect(const AppState &a, int idx) {
  ui::Rect r;
  for (auto &pl : a.layout().widgets) if (pl.index == idx) r = pl.rect;
  return r;
}

void test_marks_pending_until_confirmed() {
  AppState a;
  a.init(exampleConfig(), P, false);
  a.setMarksDriven(true);
  int idx = findWidget(a, WidgetType::Light);
  const std::string id = a.config().tabs[0].widgets[size_t(idx)].entity;
  a.store().update({EntityState{id, "on", true}});
  const ui::Rect r = widgetRect(a, idx);
  TEST_ASSERT_TRUE(a.touch({r.x + 10, r.y + 10}, 0));
  TEST_ASSERT_FALSE(a.tick(kPendingMs * 10));  // the demo timer does not apply
  TEST_ASSERT_TRUE(a.widgetData(idx).optimistic);
  TEST_ASSERT_EQUAL_STRING("on", a.store().get(id)->state.c_str());  // never committed locally
  // before the driver reports anything the entry stays; Pending keeps it
  a.setMarks({});
  TEST_ASSERT_TRUE(a.widgetData(idx).optimistic);
  a.setMarks({{id, Mark::Pending}});
  TEST_ASSERT_TRUE(a.widgetData(idx).optimistic);
  TEST_ASSERT_TRUE(a.setMarks({}));  // confirmed
  TEST_ASSERT_FALSE(a.widgetData(idx).optimistic);
  TEST_ASSERT_FALSE(a.widgetData(idx).failed);
}

void test_optimistic_cover_direction_and_vacuum() {
  AppState a;
  a.init(exampleConfig(), P, false);
  a.setMarksDriven(true);
  int idx = findWidget(a, WidgetType::Cover);
  const std::string id = a.config().tabs[0].widgets[size_t(idx)].entity;
  EntityState cs{id, "closed", true};
  cs.current_position = 0;
  a.store().update({cs});
  const ui::Rect r = widgetRect(a, idx);
  auto subs = ui::subRegions(WidgetType::Cover, config::WidgetSize{2, 1}, r);
  const ui::Rect up = subs[0].rect;
  TEST_ASSERT_TRUE(a.touch({up.x + 5, up.y + 5}, 0));
  WidgetData d = a.widgetData(idx);
  TEST_ASSERT_TRUE(d.optimistic);
  TEST_ASSERT_FALSE(d.pending);
  TEST_ASSERT_EQUAL_STRING("opening", d.members[0]->state.c_str());
  TEST_ASSERT_TRUE(coverView(d.members, false).upFilled);  // moving direction shown at once
  TEST_ASSERT_EQUAL_STRING("closed", a.store().get(id)->state.c_str());
  // Failed mark: back to the last real state, with the failed badge, no optimistic view
  TEST_ASSERT_TRUE(a.setMarks({{id, Mark::Failed}}));
  d = a.widgetData(idx);
  TEST_ASSERT_FALSE(d.optimistic);
  TEST_ASSERT_TRUE(d.failed);
  TEST_ASSERT_EQUAL_STRING("closed", d.members[0]->state.c_str());
  TEST_ASSERT_FALSE(coverView(d.members, false).upFilled);
}

void test_marks_failed_reverts_and_shows_failed() {
  AppState a;
  a.init(exampleConfig(), P, false);
  a.setMarksDriven(true);
  int idx = findWidget(a, WidgetType::Light);
  const std::string id = a.config().tabs[0].widgets[size_t(idx)].entity;
  a.store().update({EntityState{id, "on", true}});
  const ui::Rect r = widgetRect(a, idx);
  a.touch({r.x + 10, r.y + 10}, 0);
  a.setMarks({{id, Mark::Pending}});
  TEST_ASSERT_TRUE(a.setMarks({{id, Mark::Failed}}));
  WidgetData d = a.widgetData(idx);
  TEST_ASSERT_FALSE(d.optimistic);
  TEST_ASSERT_TRUE(d.failed);
  TEST_ASSERT_EQUAL_STRING("on", d.members[0]->state.c_str());  // last real state
  TEST_ASSERT_TRUE(a.setMarks({}));
  TEST_ASSERT_FALSE(a.widgetData(idx).failed);
}

void test_marks_unmarked_pending_times_out() {
  AppState a;
  a.init(exampleConfig(), P, false);
  a.setMarksDriven(true, 5000);
  int idx = findWidget(a, WidgetType::Light);
  const std::string id = a.config().tabs[0].widgets[size_t(idx)].entity;
  a.store().update({EntityState{id, "on", true}});
  const ui::Rect r = widgetRect(a, idx);
  a.touch({r.x + 10, r.y + 10}, 0);
  TEST_ASSERT_FALSE(a.tick(4999));
  TEST_ASSERT_TRUE(a.tick(5000));
  TEST_ASSERT_FALSE(a.widgetData(idx).optimistic);
  TEST_ASSERT_EQUAL_STRING("on", a.store().get(id)->state.c_str());
}

void test_sink_fast_double_taps() {
  AppState a;
  a.init(exampleConfig(), P, false);
  a.setMarksDriven(true);
  std::vector<Sunk> got;
  a.setActionSink([&](int t, int w, int s, const std::vector<EntityState> &st) { got.push_back({t, w, s, st}); });
  // light: the second tap while pending is ignored
  int li = findWidget(a, WidgetType::Light);
  const std::string lid = a.config().tabs[0].widgets[size_t(li)].entity;
  a.store().update({EntityState{lid, "on", true}});
  const ui::Rect lr = widgetRect(a, li);
  a.touch({lr.x + 10, lr.y + 10}, 0);
  a.touch({lr.x + 10, lr.y + 10}, 30);
  TEST_ASSERT_EQUAL(1, (int)got.size());
  TEST_ASSERT_EQUAL(li, got[0].widget);
  TEST_ASSERT_EQUAL_STRING("off", got[0].st[0].state.c_str());
  // cover: different buttons are two actions, the same button twice only one
  int ci = findWidget(a, WidgetType::Cover);
  const std::string cid = a.config().tabs[0].widgets[size_t(ci)].entity;
  a.store().update({EntityState{cid, "open", true}});
  auto subs = ui::subRegions(WidgetType::Cover, config::WidgetSize{2, 1}, widgetRect(a, ci));
  a.touch({subs[1].rect.x + 5, subs[1].rect.y + 5}, 100);
  a.touch({subs[1].rect.x + 5, subs[1].rect.y + 5}, 110);
  a.touch({subs[2].rect.x + 5, subs[2].rect.y + 5}, 120);
  TEST_ASSERT_EQUAL(3, (int)got.size());
  TEST_ASSERT_EQUAL(2, got[1].sub);
  TEST_ASSERT_EQUAL(3, got[2].sub);
  // climate: taps accumulate, each action carries the accumulated target
  int ti = findWidget(a, WidgetType::Climate);
  const std::string tid = a.config().tabs[0].widgets[size_t(ti)].entity;
  EntityState cs{tid, "heat", true};
  cs.temperature = 22.0;
  a.store().update({cs});
  auto csub = ui::subRegions(WidgetType::Climate, config::WidgetSize{2, 1}, widgetRect(a, ti));
  a.touch({csub[1].rect.x + 5, csub[1].rect.y + 5}, 200);
  a.touch({csub[1].rect.x + 5, csub[1].rect.y + 5}, 210);
  TEST_ASSERT_EQUAL(5, (int)got.size());
  TEST_ASSERT_FLOAT_WITHIN(0.001, 22.5, *got[3].st[0].temperature);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 23.0, *got[4].st[0].temperature);
  // a tap that changes nothing sends nothing
  a.touch({-5, -5}, 300);
  TEST_ASSERT_EQUAL(5, (int)got.size());
}

void test_sink_also_in_demo_mode() {
  AppState a;
  a.init(exampleConfig(), P, true);
  int n = 0;
  a.setActionSink([&](int, int, int, const std::vector<EntityState> &) { ++n; });
  int idx = findWidget(a, WidgetType::Light);
  const ui::Rect r = widgetRect(a, idx);
  a.touch({r.x + 10, r.y + 10}, 0);
  TEST_ASSERT_EQUAL(1, n);
  a.tick(kPendingMs);  // demo timer still commits
  TEST_ASSERT_TRUE(a.pendingList().empty());
}


static config::Widget sensorOpts(const char* attr) {
  config::Widget w;
  w.attribute = attr;
  return w;
}

void test_sensor_default_one_decimal() {
  EntityState s = E("sensor.d", "45.37");
  TEST_ASSERT_EQUAL_STRING("45.4", sensorView(&s).value.c_str());
  s.state = "45.3712345678901";
  TEST_ASSERT_EQUAL_STRING("45.4", sensorView(&s).value.c_str());
  s.state = "45";
  TEST_ASSERT_EQUAL_STRING("45", sensorView(&s).value.c_str());
  config::Widget w;
  s.state = "45.37";
  TEST_ASSERT_EQUAL_STRING("45.4", sensorView(&s, &w).value.c_str());
  w.decimals = 0;
  TEST_ASSERT_EQUAL_STRING("45", sensorView(&s, &w).value.c_str());
  w.decimals = 2;
  TEST_ASSERT_EQUAL_STRING("45.37", sensorView(&s, &w).value.c_str());
  s.state = "45";
  TEST_ASSERT_EQUAL_STRING("45.00", sensorView(&s, &w).value.c_str());
}

void test_sensor_attribute_main_value() {
  EntityState s = E("sensor.c", "21.5");
  s.unit_of_measurement = "kWh";
  s.attrs = {{"humidity", "45.678"}, {"mode", "auto_dry"}, {"humidity_unit", "%"}};
  config::Widget w = sensorOpts("humidity");
  SensorView v = sensorView(&s, &w);
  TEST_ASSERT_TRUE(v.numeric);
  TEST_ASSERT_EQUAL_STRING("45.7", v.value.c_str());  // default: 1 decimal
  TEST_ASSERT_EQUAL_STRING("%", v.unit.c_str());  // attribute's own unit, not the state's kWh
  w.decimals = 1;
  TEST_ASSERT_EQUAL_STRING("45.7", sensorView(&s, &w).value.c_str());
  w.decimals = 0;
  TEST_ASSERT_EQUAL_STRING("46", sensorView(&s, &w).value.c_str());
  w.unit = "pct";
  TEST_ASSERT_EQUAL_STRING(" pct", sensorView(&s, &w).unit.c_str());
  // no unit known: none (state's unit is not used for attributes)
  s.attrs = {{"humidity", "45"}};
  config::Widget w2 = sensorOpts("humidity");
  TEST_ASSERT_TRUE(sensorView(&s, &w2).unit.empty());
  // text attribute
  s.attrs = {{"mode", "auto_dry"}};
  config::Widget w3 = sensorOpts("mode");
  v = sensorView(&s, &w3);
  TEST_ASSERT_FALSE(v.numeric);
  TEST_ASSERT_TRUE(v.textState);
  TEST_ASSERT_EQUAL_STRING("Auto dry", v.value.c_str());
  // missing attribute
  config::Widget w4 = sensorOpts("nope");
  v = sensorView(&s, &w4);
  TEST_ASSERT_EQUAL_STRING("-", v.value.c_str());
  TEST_ASSERT_TRUE(v.unit.empty());
}

void test_sensor_state_with_decimals_and_unit_override() {
  EntityState s = E("sensor.t", "21.456");
  s.unit_of_measurement = "kWh";
  config::Widget w;
  w.decimals = 2;
  w.unit = "\xC2\xB0" "C";
  SensorView v = sensorView(&s, &w);
  TEST_ASSERT_EQUAL_STRING("21.46", v.value.c_str());
  TEST_ASSERT_EQUAL_STRING("\xC2\xB0" "C", v.unit.c_str());
  w.unit.reset();
  TEST_ASSERT_EQUAL_STRING(" kWh", sensorView(&s, &w).unit.c_str());
  TEST_ASSERT_TRUE(sensorView(&s, &w).details.empty());
}

void test_sensor_details() {
  EntityState s = E("sensor.c", "21.5");
  s.attrs = {{"humidity", "45.55"}, {"dew_point", "9.123"}, {"dew_point_unit", "\xC2\xB0" "C"},
             {"trend", "rising"}};
  config::Widget w;
  w.details = {"dew_point", "humidity"};
  w.decimals = 1;
  SensorView v = sensorView(&s, &w);
  TEST_ASSERT_EQUAL(2, v.details.size());
  TEST_ASSERT_EQUAL_STRING("Dew point", v.details[0].label.c_str());
  TEST_ASSERT_EQUAL_STRING("9.1", v.details[0].value.c_str());
  TEST_ASSERT_EQUAL_STRING("\xC2\xB0" "C", v.details[0].unit.c_str());
  TEST_ASSERT_EQUAL_STRING("Humidity", v.details[1].label.c_str());
  TEST_ASSERT_EQUAL_STRING("45.5", v.details[1].value.c_str());  // 45.55 -> %.1f
  w.details = {"trend", "absent"};
  v = sensorView(&s, &w);
  TEST_ASSERT_EQUAL_STRING("Rising", v.details[0].value.c_str());
  TEST_ASSERT_EQUAL_STRING("-", v.details[1].value.c_str());
  TEST_ASSERT_EQUAL_STRING("Absent", v.details[1].label.c_str());
}

// ---- brightness popup in the app state (task 045) ----

static AppState dimApp(bool marks, int *lightIdx, std::string *id) {
  AppState a;
  a.init(exampleConfig(), P, false);
  a.setMarksDriven(marks, 5000);
  *lightIdx = findWidget(a, WidgetType::Light);
  *id = a.config().tabs[0].widgets[size_t(*lightIdx)].entity;
  EntityState l = E(id->c_str(), "on");
  l.brightness = 153;  // 60 %
  a.store().update({l});
  return a;
}

void test_brightness_layout_hits_and_bar() {
  for (auto o : {settings::Orientation::Portrait, settings::Orientation::Landscape}) {
    const auto l = brightnessLayout(o);
    TEST_ASSERT_EQUAL(int(BrightnessHit::Outside), int(brightnessHitTest(l, {0, 0})));
    TEST_ASSERT_EQUAL(int(BrightnessHit::Close), int(brightnessHitTest(l, {l.close.x + 5, l.close.y + 5})));
    TEST_ASSERT_EQUAL(int(BrightnessHit::Bar), int(brightnessHitTest(l, {l.bar.x + 5, l.bar.y + 5})));
    TEST_ASSERT_EQUAL(int(BrightnessHit::Preset2), int(brightnessHitTest(l, {l.presets[2].x + 3, l.presets[2].y + 3})));
    TEST_ASSERT_EQUAL(int(BrightnessHit::Off), int(brightnessHitTest(l, {l.off.x + 3, l.off.y + 3})));
    TEST_ASSERT_TRUE(ui::rectInside(l.panel, l.dirty));
    TEST_ASSERT_EQUAL(int(BrightnessHit::Panel), int(brightnessHitTest(l, {l.panel.x + 2, l.panel.y + 2})));
    TEST_ASSERT_TRUE(brightnessBarFraction(l, l.bar.x) == 0.0);
    TEST_ASSERT_TRUE(brightnessBarFraction(l, l.bar.x + l.bar.w + 50) == 1.0);
    // every control sits inside the panel
    for (const ui::Rect &r : {l.close, l.bar, l.minus, l.plus, l.off, l.presets[0], l.presets[3]})
      TEST_ASSERT_TRUE(ui::rectInside(r, l.panel));
  }
}

void test_app_brightness_open_modal_and_send() {
  int li; std::string id;
  AppState a = dimApp(true, &li, &id);
  std::vector<std::pair<std::string, int>> sent;
  a.setBrightnessSink([&](const std::string &i, int p) { sent.push_back({i, p}); });
  const ui::Rect r = widgetRect(a, li);
  TEST_ASSERT_TRUE(a.dimmableLightAt({r.x + 10, r.y + 10}));
  TEST_ASSERT_TRUE(a.longPress({r.x + 10, r.y + 10}, 0));
  TEST_ASSERT_TRUE(a.brightnessOpen());
  TEST_ASSERT_EQUAL(60, a.brightness().value());
  const auto l = brightnessLayout(P);
  // a tap on a tab while open only closes (modal), no tab switch
  const int shown = a.shownTab();
  TEST_ASSERT_TRUE(a.touch({l.panel.x + 1, l.panel.y - 20}, 100));
  TEST_ASSERT_FALSE(a.brightnessOpen());
  TEST_ASSERT_EQUAL(shown, a.shownTab());
  // reopen, Plus then bar tap; sends the last value after the debounce
  a.longPress({r.x + 10, r.y + 10}, 1000);
  a.touch({l.plus.x + 5, l.plus.y + 5}, 1100);
  TEST_ASSERT_EQUAL(70, a.brightness().value());
  a.touch({l.bar.x + l.bar.w / 2, l.bar.y + 10}, 1200);
  TEST_ASSERT_EQUAL(50, a.brightness().value());
  TEST_ASSERT_TRUE(a.brightnessAwaiting());
  a.tick(1500);
  TEST_ASSERT_EQUAL(0, (int)sent.size());
  a.tick(1600);
  TEST_ASSERT_EQUAL(1, (int)sent.size());
  TEST_ASSERT_EQUAL(50, sent[0].second);
  // optimistic percent on the widget
  const WidgetData d = a.widgetData(li);
  TEST_ASSERT_EQUAL_STRING("On 50%", toggleView(WidgetType::Light, d.members[0], false).state.c_str());
  // confirmation through the marks: Pending, then gone
  a.setMarks({{id, Mark::Pending}});
  TEST_ASSERT_TRUE(a.brightnessAwaiting());
  a.setMarks({});
  TEST_ASSERT_FALSE(a.brightnessAwaiting());
  // rocker: up +10, push closes
  a.rocker(ui::PhysicalKey::Up, 2000);
  TEST_ASSERT_EQUAL(60, a.brightness().value());
  TEST_ASSERT_TRUE(a.rocker(ui::PhysicalKey::Push, 2100));
  TEST_ASSERT_FALSE(a.brightnessOpen());
  TEST_ASSERT_EQUAL(2, (int)sent.size());  // close flushed the unsent 60
  TEST_ASSERT_EQUAL(60, sent[1].second);
  TEST_ASSERT_EQUAL(60, a.brightness().optimisticPct());  // unconfirmed: the widget keeps showing 60
}

void test_app_brightness_failed_and_idle_close() {
  int li; std::string id;
  AppState a = dimApp(true, &li, &id);
  a.setBrightnessSink([](const std::string &, int) {});
  const ui::Rect r = widgetRect(a, li);
  a.longPress({r.x + 10, r.y + 10}, 0);
  a.rocker(ui::PhysicalKey::Down, 10);
  a.tick(500);
  a.setMarks({{id, Mark::Pending}});
  a.setMarks({{id, Mark::Failed}});
  TEST_ASSERT_TRUE(a.brightnessFailed());
  TEST_ASSERT_TRUE(a.brightnessOpen());
  TEST_ASSERT_TRUE(a.tick(10010));  // idle close
  TEST_ASSERT_FALSE(a.brightnessOpen());
  TEST_ASSERT_FALSE(a.brightnessFailed());
  TEST_ASSERT_EQUAL(-1, a.brightness().optimisticPct());  // the widget shows the real state again
}

void test_app_brightness_not_for_plain_lights_and_demo_store() {
  int li; std::string id;
  AppState a = dimApp(false, &li, &id);
  const ui::Rect r = widgetRect(a, li);
  EntityState plain = E(id.c_str(), "on");
  plain.color_modes = "onoff";
  a.store().update({plain});
  TEST_ASSERT_FALSE(a.longPress({r.x + 10, r.y + 10}, 0));
  EntityState l = E(id.c_str(), "off");
  l.brightness = 100;
  a.store().update({l});
  TEST_ASSERT_TRUE(a.longPress({r.x + 10, r.y + 10}, 0));
  TEST_ASSERT_EQUAL(0, a.brightness().value());  // off: starts at 0
  a.touch({brightnessLayout(P).presets[0].x + 3, brightnessLayout(P).presets[0].y + 3}, 10);
  a.tick(500);
  TEST_ASSERT_EQUAL_STRING("on", a.store().get(id)->state.c_str());
  TEST_ASSERT_EQUAL(64, *a.store().get(id)->brightness);  // 25 %
  TEST_ASSERT_FALSE(a.brightnessAwaiting());
}

void test_popup_idle_close_flushes_and_poke_restarts_idle() {
  BrightnessPopup p;
  std::vector<int> sent;
  p.setSink([&](const std::string &, int v) { sent.push_back(v); });
  p.open("light.a", 0, 0, 50, 0);
  p.step(+1, 100);
  p.poke(9000);  // panel tap: idle restarts
  TEST_ASSERT_FALSE(p.tick(9400));
  TEST_ASSERT_EQUAL(1, (int)sent.size());  // debounce sent 60
  p.step(+1, 9400);
  TEST_ASSERT_FALSE(p.tick(19399));
  TEST_ASSERT_TRUE(p.tick(19400));  // 10 s after the last input; unsent value is flushed (a fresh step)
  TEST_ASSERT_FALSE(p.isOpen());
  TEST_ASSERT_EQUAL(2, (int)sent.size());
  TEST_ASSERT_EQUAL(70, sent[1]);
}

void test_popup_cancel_sends_nothing() {
  BrightnessPopup p;
  int n = 0;
  p.setSink([&](const std::string &, int) { ++n; });
  p.open("light.a", 0, 0, 50, 0);
  p.step(+1, 10);
  p.cancel();
  TEST_ASSERT_FALSE(p.isOpen());
  TEST_ASSERT_EQUAL(-1, p.optimisticPct());
  p.done(20);
  p.tick(1000);
  TEST_ASSERT_EQUAL(0, n);
}

void test_app_brightness_rocker_landscape_down_is_brighter() {
  AppState a;
  a.init(exampleConfig(), settings::Orientation::Landscape, false);
  a.setMarksDriven(true, 5000);
  const int li = findWidget(a, WidgetType::Light);
  const std::string id = a.config().tabs[0].widgets[size_t(li)].entity;
  EntityState l = E(id.c_str(), "on");
  l.brightness = 153;
  a.store().update({l});
  a.setBrightnessSink([](const std::string &, int) {});
  const ui::Rect r = widgetRect(a, li);
  TEST_ASSERT_TRUE(a.longPress({r.x + 10, r.y + 10}, 0));
  a.rocker(ui::PhysicalKey::Down, 10);
  TEST_ASSERT_EQUAL(70, a.brightness().value());
  a.rocker(ui::PhysicalKey::Up, 20);
  a.rocker(ui::PhysicalKey::Up, 30);
  TEST_ASSERT_EQUAL(50, a.brightness().value());
}

void test_app_brightness_panel_tap_restarts_idle() {
  int li; std::string id;
  AppState a = dimApp(true, &li, &id);
  a.setBrightnessSink([](const std::string &, int) {});
  const ui::Rect r = widgetRect(a, li);
  a.longPress({r.x + 10, r.y + 10}, 0);
  const auto l = brightnessLayout(P);
  TEST_ASSERT_FALSE(a.touch({l.panel.x + 2, l.panel.y + 2}, 8000));
  TEST_ASSERT_FALSE(a.tick(12000));
  TEST_ASSERT_TRUE(a.brightnessOpen());
  TEST_ASSERT_TRUE(a.tick(18000));
  TEST_ASSERT_FALSE(a.brightnessOpen());
}

void test_app_brightness_unavailable_closes_without_sending() {
  int li; std::string id;
  AppState a = dimApp(true, &li, &id);
  int n = 0;
  a.setBrightnessSink([&](const std::string &, int) { ++n; });
  const ui::Rect r = widgetRect(a, li);
  a.longPress({r.x + 10, r.y + 10}, 0);
  a.rocker(ui::PhysicalKey::Up, 10);
  EntityState u = E(id.c_str(), "unavailable");
  u.available = false;
  a.store().update({u});
  TEST_ASSERT_TRUE(a.tick(20));
  TEST_ASSERT_FALSE(a.brightnessOpen());
  a.tick(1000);
  TEST_ASSERT_EQUAL(0, n);
}

void test_app_close_before_sleep_flushes() {
  int li; std::string id;
  AppState a = dimApp(true, &li, &id);
  std::vector<int> sent;
  a.setBrightnessSink([&](const std::string &, int v) { sent.push_back(v); });
  const ui::Rect r = widgetRect(a, li);
  a.longPress({r.x + 10, r.y + 10}, 0);
  a.rocker(ui::PhysicalKey::Up, 10);
  TEST_ASSERT_TRUE(a.closeBrightness(50));  // what goToSleep does
  TEST_ASSERT_EQUAL(1, (int)sent.size());
  TEST_ASSERT_EQUAL(70, sent[0]);
  TEST_ASSERT_FALSE(a.closeBrightness(60));
}

void test_deferred_tap_machine() {
  DeferredTap d;
  using R = DeferredTap::Result;
  TEST_ASSERT_EQUAL(R::None, d.release().kind);
  d.press({10, 20}, 100);
  TEST_ASSERT_TRUE(d.armed());
  R r = d.release();
  TEST_ASSERT_EQUAL(R::Tap, r.kind);
  TEST_ASSERT_EQUAL(10, r.point.x);
  TEST_ASSERT_FALSE(d.armed());
  TEST_ASSERT_EQUAL(R::None, d.release().kind);  // release after the long press / tap: nothing
  d.press({5, 6}, 200);
  r = d.longPress();
  TEST_ASSERT_EQUAL(R::LongPress, r.kind);
  TEST_ASSERT_EQUAL(6, r.point.y);
  TEST_ASSERT_EQUAL(R::None, d.release().kind);  // the release after a long press is swallowed
  d.press({1, 1}, 1000);
  TEST_ASSERT_EQUAL(R::None, d.tick(2500).kind);  // exactly 1500 ms: not yet
  TEST_ASSERT_EQUAL(R::Tap, d.tick(2501).kind);
  TEST_ASSERT_EQUAL(R::None, d.tick(9000).kind);
  d.press({1, 1}, 0);
  d.cancel();
  TEST_ASSERT_EQUAL(R::None, d.release().kind);
  TEST_ASSERT_EQUAL(R::None, d.longPress().kind);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fmt_number);
  RUN_TEST(test_cover_single);
  RUN_TEST(test_cover_unavailable_and_loading);
  RUN_TEST(test_cover_group);
  RUN_TEST(test_mixed_group_aggregate);
  RUN_TEST(test_toggle_text);
  RUN_TEST(test_climate_view);
  RUN_TEST(test_humanise_and_text_state);
  RUN_TEST(test_binary_sensor_table);
  RUN_TEST(test_vacuum_wording);
  RUN_TEST(test_cover_wording);
  RUN_TEST(test_generic_fallback);
  RUN_TEST(test_sensor_weather_vacuum);
  RUN_TEST(test_fake_toggle_and_group);
  RUN_TEST(test_toggle_intent_from_shown_state);
  RUN_TEST(test_fake_cover);
  RUN_TEST(test_unknown_cover_no_position);
  RUN_TEST(test_fake_climate_step_and_limits);
  RUN_TEST(test_fake_vacuum);
  RUN_TEST(test_demo_states_cover_every_entity);
  RUN_TEST(test_demo_unavailable_suffix);
  RUN_TEST(test_app_toggle_pending_then_confirmed);
  RUN_TEST(test_app_cover_button_pending_keeps_state);
  RUN_TEST(test_app_climate_taps_accumulate);
  RUN_TEST(test_app_tab_touch_is_full);
  RUN_TEST(test_app_rocker_tab_debounce);
  RUN_TEST(test_app_rocker_toggle_action);
  RUN_TEST(test_app_error_screen);
  RUN_TEST(test_t_tick_survives_millis_wraparound);
  RUN_TEST(test_t_tab_touch_cancels_rocker_debounce);
  RUN_TEST(test_t_retap_same_tab_is_not_full);
  RUN_TEST(test_t_toggle_repeat_during_pending_ignored);
  RUN_TEST(test_t_touch_outside_everything_changes_nothing);
  RUN_TEST(test_t_error_screen_ignores_rocker_and_tick);
  RUN_TEST(test_t_cover_group_partly_unavailable_shows_badge);
  RUN_TEST(test_t_fake_action_on_unavailable_or_missing_does_nothing);
  RUN_TEST(test_t_light_group_turns_all_to_opposite_of_first);
  RUN_TEST(test_t_window_sensor_texts);
  RUN_TEST(test_sensor_numeric_parsing);
  RUN_TEST(test_fake_climate_step_zero_or_negative);
  RUN_TEST(test_marks_pending_until_confirmed);
  RUN_TEST(test_marks_failed_reverts_and_shows_failed);
  RUN_TEST(test_marks_unmarked_pending_times_out);
  RUN_TEST(test_optimistic_cover_direction_and_vacuum);
  RUN_TEST(test_sink_fast_double_taps);
  RUN_TEST(test_sink_also_in_demo_mode);
  RUN_TEST(test_sensor_default_one_decimal);
  RUN_TEST(test_sensor_attribute_main_value);
  RUN_TEST(test_sensor_state_with_decimals_and_unit_override);
  RUN_TEST(test_sensor_details);
  RUN_TEST(test_demo_yaml_showcase);
  RUN_TEST(test_demo_fails_entity_ends_failed);
  RUN_TEST(test_brightness_popup_steps_and_bar);
  RUN_TEST(test_brightness_popup_debounce_sends_last_value);
  RUN_TEST(test_brightness_popup_idle_close_and_off);
  RUN_TEST(test_brightness_layout_hits_and_bar);
  RUN_TEST(test_app_brightness_open_modal_and_send);
  RUN_TEST(test_app_brightness_failed_and_idle_close);
  RUN_TEST(test_app_brightness_not_for_plain_lights_and_demo_store);
  RUN_TEST(test_popup_idle_close_flushes_and_poke_restarts_idle);
  RUN_TEST(test_popup_cancel_sends_nothing);
  RUN_TEST(test_app_brightness_rocker_landscape_down_is_brighter);
  RUN_TEST(test_app_brightness_panel_tap_restarts_idle);
  RUN_TEST(test_app_brightness_unavailable_closes_without_sending);
  RUN_TEST(test_app_close_before_sleep_flushes);
  RUN_TEST(test_deferred_tap_machine);
  return UNITY_END();
}
