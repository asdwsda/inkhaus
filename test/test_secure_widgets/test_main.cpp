// Task 108: alarm panel, lock and garage widgets: config, views, confirm dialog, keypad popup, AppState wiring.
#include <unity.h>

#include "app_state.h"
#include "config_cache.h"
#include "secure_intent.h"
#include "service_call.h"
#include "tap_intent.h"
#include <ArduinoJson.h>

using namespace uistate;
using settings::Orientation;

void setUp() {}
void tearDown() {}

namespace {

const char *kYaml = R"(version: 1
tabs:
  - title: A
    widgets:
      - entity: alarm_control_panel.home
        name: Alarm
      - entity: lock.front_door
      - entity: lock.back
        confirm: true
      - entity: cover.garage
        type: garage
      - entity: cover.garage2
        type: garage
        confirm: false
)";

config::Config parsed(const char *y) {
  auto r = config::parseConfig(y);
  std::string all;
  for (auto &e : r.errors) all += e.path + ": " + e.message + "\n";
  TEST_ASSERT_TRUE_MESSAGE(r.ok(), all.c_str());
  return r.config;
}

}  // namespace

// ---- config ----
void test_config_types_and_confirm() {
  const config::Config c = parsed(kYaml);
  const auto &w = c.tabs[0].widgets;
  TEST_ASSERT_EQUAL(int(config::WidgetType::Alarm), int(w[0].type));
  TEST_ASSERT_EQUAL(int(config::WidgetType::Lock), int(w[1].type));
  TEST_ASSERT_FALSE(w[1].confirm.has_value());
  TEST_ASSERT_TRUE(w[2].confirm.has_value() && *w[2].confirm);
  TEST_ASSERT_EQUAL(int(config::WidgetType::Garage), int(w[3].type));
  TEST_ASSERT_TRUE(w[4].confirm.has_value() && !*w[4].confirm);
  TEST_ASSERT_EQUAL_STRING("garage", config::widgetTypeName(config::WidgetType::Garage));
}

void test_config_errors() {
  auto bad = [](const char *y, const char *path) {
    auto r = config::parseConfig(y);
    bool found = false;
    for (auto &e : r.errors) found |= e.path == path;
    TEST_ASSERT_TRUE_MESSAGE(found, y);
  };
  const std::string head = "version: 1\ntabs:\n  - title: A\n    widgets:\n";
  bad((head + "      - entity: light.a\n        confirm: true\n").c_str(), "tabs[0].widgets[0].confirm");
  bad((head + "      - entity: lock.a\n        confirm: maybe\n").c_str(), "tabs[0].widgets[0].confirm");
  bad((head + "      - entity: light.a\n        type: garage\n").c_str(), "tabs[0].widgets[0].entity");
  bad((head + "      - entity: light.a\n        type: alarm_control_panel\n").c_str(), "tabs[0].widgets[0].entity");
  bad("version: 1\ngroups:\n  g: [lock.a, lock.b]\ntabs:\n  - title: A\n    widgets:\n      - group: g\n        name: G\n",
      "tabs[0].widgets[0].group");
}

void test_config_cache_roundtrip() {
  const config::Config c = parsed(kYaml);
  config::Fingerprint fp{1, 2, 3, 4}, fp2;
  config::Config out;
  TEST_ASSERT_TRUE(config::unpackCache(config::packCache(fp, c), fp2, out));
  const auto &w = out.tabs[0].widgets;
  TEST_ASSERT_EQUAL(int(config::WidgetType::Alarm), int(w[0].type));
  TEST_ASSERT_FALSE(w[1].confirm.has_value());
  TEST_ASSERT_TRUE(w[2].confirm.has_value() && *w[2].confirm);
  TEST_ASSERT_EQUAL(int(config::WidgetType::Garage), int(w[3].type));
  TEST_ASSERT_TRUE(w[4].confirm.has_value() && !*w[4].confirm);
}

// ---- service calls and intents ----
void test_service_calls() {
  ServiceCall c;
  TEST_ASSERT_TRUE(build_service_call(Action::LockLock, {"lock.a"}, c));
  TEST_ASSERT_EQUAL_STRING("lock", c.domain.c_str());
  TEST_ASSERT_EQUAL_STRING("lock", c.service.c_str());
  TEST_ASSERT_TRUE(build_service_call(Action::LockUnlock, {"lock.a"}, c));
  TEST_ASSERT_EQUAL_STRING("unlock", c.service.c_str());
  TEST_ASSERT_TRUE(build_alarm_call({"alarm_control_panel.h"}, AlarmCmd::ArmAway, "1234", c));
  TEST_ASSERT_EQUAL_STRING("alarm_control_panel", c.domain.c_str());
  TEST_ASSERT_EQUAL_STRING("alarm_arm_away", c.service.c_str());
  JsonDocument d;
  TEST_ASSERT_TRUE(!deserializeJson(d, c.body));
  TEST_ASSERT_EQUAL_STRING("1234", d["code"].as<const char *>());
  TEST_ASSERT_EQUAL_STRING("alarm_control_panel.h", d["entity_id"][0].as<const char *>());
  TEST_ASSERT_TRUE(build_alarm_call({"alarm_control_panel.h"}, AlarmCmd::Disarm, "", c));
  TEST_ASSERT_EQUAL_STRING("alarm_disarm", c.service.c_str());
  TEST_ASSERT_FALSE(c.body.find("code") != std::string::npos);  // no code key without a code
  TEST_ASSERT_TRUE(build_alarm_call({"alarm_control_panel.h"}, AlarmCmd::ArmHome, "1", c));
  TEST_ASSERT_EQUAL_STRING("alarm_arm_home", c.service.c_str());
  TEST_ASSERT_TRUE(build_alarm_call({"a.b"}, AlarmCmd::ArmNight, "1", c));
  TEST_ASSERT_EQUAL_STRING("alarm_arm_night", c.service.c_str());
  TEST_ASSERT_FALSE(build_alarm_call({}, AlarmCmd::Disarm, "", c));
}

void test_intents() {
  using T = config::WidgetType;
  EntityState lk;
  lk.id = "lock.a";
  lk.state = "locked";
  auto ev = app_logic::intentFromRequest(T::Lock, 0, {lk});
  TEST_ASSERT_TRUE(ev.has_value());
  TEST_ASSERT_EQUAL(int(Action::LockLock), int(ev->action));
  lk.state = "unlocked";
  ev = app_logic::intentFromRequest(T::Lock, 0, {lk});
  TEST_ASSERT_EQUAL(int(Action::LockUnlock), int(ev->action));
  EntityState g;
  g.id = "cover.g";
  g.state = "open";
  ev = app_logic::intentFromRequest(T::Garage, 0, {g});
  TEST_ASSERT_EQUAL(int(Action::CoverOpen), int(ev->action));
  g.state = "closed";
  ev = app_logic::intentFromRequest(T::Garage, 0, {g});
  TEST_ASSERT_EQUAL(int(Action::CoverClose), int(ev->action));
  ev = app_logic::intentFromRequest(T::Garage, 2, {g});
  TEST_ASSERT_EQUAL(int(Action::CoverStop), int(ev->action));
  auto al = app_logic::alarmIntent("alarm_control_panel.h", AlarmCmd::Disarm, "9999");
  TEST_ASSERT_TRUE(al.has_value());
  TEST_ASSERT_EQUAL_STRING("alarm_disarm", al->custom_call.service.c_str());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_config_types_and_confirm);
  RUN_TEST(test_config_errors);
  RUN_TEST(test_config_cache_roundtrip);
  RUN_TEST(test_service_calls);
  RUN_TEST(test_intents);
  return UNITY_END();
}
