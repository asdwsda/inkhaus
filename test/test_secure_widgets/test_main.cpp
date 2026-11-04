// Task 108: alarm panel, lock and garage widgets: config, views, confirm dialog, keypad popup, AppState wiring.
#include <unity.h>

#include "app_state.h"
#include "config_cache.h"

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

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_config_types_and_confirm);
  RUN_TEST(test_config_errors);
  RUN_TEST(test_config_cache_roundtrip);
  return UNITY_END();
}
