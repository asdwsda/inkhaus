// Task 108: alarm panel, lock and garage widgets: config, views, confirm dialog, keypad popup, AppState wiring.
#include <unity.h>

#include <algorithm>
#include <fstream>
#include <sstream>

#include "app_state.h"
#include "alarm_popup.h"
#include "config_cache.h"
#include "confirm_dialog.h"
#include "secure_view.h"
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

// ---- views ----
namespace {
EntityState st(const char *id, const char *state, bool avail = true) {
  EntityState s;
  s.id = id;
  s.state = state;
  s.available = avail;
  return s;
}
void attr(EntityState &s, const char *k, const char *v) { s.attrs.emplace_back(k, v); }
}  // namespace

void test_alarm_view_table() {
  struct Row { const char *state; const char *text; SecureIcon icon; bool fill; };
  const Row rows[] = {
      {"disarmed", "Off", SecureIcon::ShieldOff, false}, {"armed_home", "Home", SecureIcon::Shield, false},
      {"armed_away", "Away", SecureIcon::Shield, false}, {"armed_night", "Night", SecureIcon::Shield, false},
      {"armed_vacation", "Vacat.", SecureIcon::Shield, false}, {"armed_custom_bypass", "Custom", SecureIcon::Shield, false},
      {"arming", "Arming", SecureIcon::ShieldOff, false}, {"pending", "Entry", SecureIcon::ShieldOff, false},
      {"triggered", "ALARM", SecureIcon::ShieldAlert, true}};
  for (const Row &r : rows) {
    const EntityState s = st("alarm_control_panel.h", r.state);
    const SecureView v = alarmView(&s);
    TEST_ASSERT_EQUAL_STRING(r.text, v.text.c_str());
    TEST_ASSERT_EQUAL(int(r.icon), int(v.icon));
    TEST_ASSERT_EQUAL(r.fill, v.fill);
    TEST_ASSERT_EQUAL(int(Look::Normal), int(v.look));
  }
  const EntityState una = st("alarm_control_panel.h", "unavailable", false);
  SecureView v = alarmView(&una);
  TEST_ASSERT_EQUAL(int(Look::Unavailable), int(v.look));
  TEST_ASSERT_EQUAL_STRING("n/a", v.text.c_str());
  TEST_ASSERT_EQUAL(int(SecureIcon::Warn), int(v.icon));
  v = alarmView(nullptr);
  TEST_ASSERT_EQUAL(int(Look::Loading), int(v.look));
  TEST_ASSERT_EQUAL_STRING("-", v.text.c_str());
  TEST_ASSERT_EQUAL(int(SecureIcon::ShieldOff), int(v.icon));
}

void test_lock_view_table() {
  struct Row { const char *state; const char *text; SecureIcon icon; bool fill; };
  const Row rows[] = {{"locked", "Locked", SecureIcon::Lock, false}, {"unlocked", "Open", SecureIcon::LockOpen, true},
                      {"open", "Open", SecureIcon::LockOpen, true}, {"locking", "Locking", SecureIcon::Lock, false},
                      {"unlocking", "Opening", SecureIcon::LockOpen, false}, {"jammed", "Jammed", SecureIcon::Alert, true}};
  for (const Row &r : rows) {
    const EntityState s = st("lock.a", r.state);
    const SecureView v = lockView(&s);
    TEST_ASSERT_EQUAL_STRING(r.text, v.text.c_str());
    TEST_ASSERT_EQUAL(int(r.icon), int(v.icon));
    TEST_ASSERT_EQUAL(r.fill, v.fill);
    TEST_ASSERT_FALSE(v.alertBadge);
  }
  EntityState coded = st("lock.a", "locked");
  attr(coded, "code_format", "^\\d{4}$");
  TEST_ASSERT_TRUE(lockView(&coded).alertBadge);
  TEST_ASSERT_EQUAL(int(SecureAction::None), int(lockAction(&coded)));
  const EntityState una = st("lock.a", "unavailable", false);
  TEST_ASSERT_EQUAL(int(Look::Unavailable), int(lockView(&una).look));
  TEST_ASSERT_EQUAL(int(Look::Loading), int(lockView(nullptr).look));
}

void test_garage_view_table() {
  struct Row { const char *state; const char *text; SecureIcon icon; bool fill; };
  const Row rows[] = {{"closed", "Closed", SecureIcon::GarageOff, false}, {"open", "Open", SecureIcon::GarageOn, true},
                      {"opening", "Opening", SecureIcon::GarageOn, false}, {"closing", "Closing", SecureIcon::GarageOff, false}};
  for (const Row &r : rows) {
    const EntityState s = st("cover.g", r.state);
    const SecureView v = garageView(&s);
    TEST_ASSERT_EQUAL_STRING(r.text, v.text.c_str());
    TEST_ASSERT_EQUAL(int(r.icon), int(v.icon));
    TEST_ASSERT_EQUAL(r.fill, v.fill);
  }
  const EntityState una = st("cover.g", "unavailable", false);
  TEST_ASSERT_EQUAL_STRING("n/a", garageView(&una).text.c_str());
  TEST_ASSERT_EQUAL_STRING("-", garageView(nullptr).text.c_str());
  const EntityState odd = st("cover.g", "stopped");
  TEST_ASSERT_EQUAL_STRING("Stopped", garageView(&odd).text.c_str());
}

void test_actions_and_confirm_defaults() {
  using T = config::WidgetType;
  const EntityState locked = st("lock.a", "locked"), unlocked = st("lock.a", "unlocked"), jam = st("lock.a", "jammed");
  TEST_ASSERT_EQUAL(int(SecureAction::Unlock), int(lockAction(&locked)));
  TEST_ASSERT_EQUAL(int(SecureAction::Lock), int(lockAction(&unlocked)));
  TEST_ASSERT_EQUAL(int(SecureAction::None), int(lockAction(&jam)));
  TEST_ASSERT_EQUAL(int(SecureAction::None), int(lockAction(nullptr)));
  const EntityState closed = st("cover.g", "closed"), open = st("cover.g", "open"), opening = st("cover.g", "opening");
  TEST_ASSERT_EQUAL(int(SecureAction::Open), int(garageAction(&closed)));
  TEST_ASSERT_EQUAL(int(SecureAction::Close), int(garageAction(&open)));
  TEST_ASSERT_EQUAL(int(SecureAction::Stop), int(garageAction(&opening)));
  config::Widget w;
  TEST_ASSERT_TRUE(needsConfirm(T::Lock, SecureAction::Unlock, w));
  TEST_ASSERT_FALSE(needsConfirm(T::Lock, SecureAction::Lock, w));
  TEST_ASSERT_TRUE(needsConfirm(T::Garage, SecureAction::Open, w));
  TEST_ASSERT_TRUE(needsConfirm(T::Garage, SecureAction::Close, w));
  TEST_ASSERT_FALSE(needsConfirm(T::Garage, SecureAction::Stop, w));
  w.confirm = true;
  TEST_ASSERT_TRUE(needsConfirm(T::Lock, SecureAction::Lock, w));
  TEST_ASSERT_FALSE(needsConfirm(T::Garage, SecureAction::Stop, w));
  w.confirm = false;
  TEST_ASSERT_FALSE(needsConfirm(T::Garage, SecureAction::Open, w));
  TEST_ASSERT_FALSE(needsConfirm(T::Lock, SecureAction::Unlock, w));
  TEST_ASSERT_EQUAL_STRING("Unlock Front door?", confirmTitle(SecureAction::Unlock, "Front door").c_str());
  TEST_ASSERT_EQUAL_STRING("Open Garage?", confirmTitle(SecureAction::Open, "Garage").c_str());
  TEST_ASSERT_EQUAL_STRING("Close Garage?", confirmTitle(SecureAction::Close, "Garage").c_str());
}

void test_alarm_panel_info() {
  EntityState s = st("alarm_control_panel.h", "armed_away");
  attr(s, "code_format", "number");
  attr(s, "code_arm_required", "false");
  s.supported_features = 1 | 2;  // home + away
  AlarmPanelInfo i = alarmPanelInfo(&s);
  TEST_ASSERT_EQUAL(int(CodeFormat::Number), int(i.codeFormat));
  TEST_ASSERT_FALSE(i.codeArmRequired);
  TEST_ASSERT_TRUE(i.canHome && i.canAway && !i.canNight);
  TEST_ASSERT_EQUAL(2, i.currentMode);
  TEST_ASSERT_EQUAL_STRING("Armed away", i.stateLine.c_str());
  s.state = "triggered";
  TEST_ASSERT_EQUAL_STRING("TRIGGERED", alarmPanelInfo(&s).stateLine.c_str());
  TEST_ASSERT_EQUAL(-1, alarmPanelInfo(&s).currentMode);
  attr(s, "x", "y");
  EntityState t = st("alarm_control_panel.h", "disarmed");
  attr(t, "code_format", "text");
  TEST_ASSERT_EQUAL(int(CodeFormat::Text), int(alarmPanelInfo(&t).codeFormat));
  TEST_ASSERT_TRUE(alarmPanelInfo(nullptr).canNight);
}

// ---- confirm dialog ----
void test_confirm_dialog_layout_and_answers() {
  const ConfirmLayout l = confirmLayout(Orientation::Landscape);
  TEST_ASSERT_TRUE(l.panel == (ui::Rect{200, 188, 560, 220}));
  TEST_ASSERT_TRUE(l.yes == (ui::Rect{228, 316, 160, 64}));
  TEST_ASSERT_TRUE(l.no == (ui::Rect{400, 316, 160, 64}));
  TEST_ASSERT_TRUE(l.skip == (ui::Rect{572, 316, 160, 64}));
  TEST_ASSERT_EQUAL(232, l.titleCy);
  TEST_ASSERT_EQUAL(266, l.subCy);
  const ConfirmLayout p = confirmLayout(Orientation::Portrait);
  TEST_ASSERT_TRUE(p.panel == (ui::Rect{30, 356, 480, 224}));
  TEST_ASSERT_TRUE(p.yes == (ui::Rect{54, 484, 136, 64}));

  ConfirmDialog d;
  d.open("Open Garage?", 3, 0, 1000);
  TEST_ASSERT_TRUE(d.isOpen());
  TEST_ASSERT_EQUAL(int(ConfirmAnswer::None), int(d.touch({480, 220}, Orientation::Landscape)));  // inside the panel, no button
  TEST_ASSERT_TRUE(d.isOpen());
  TEST_ASSERT_EQUAL(int(ConfirmAnswer::Yes), int(d.touch({300, 340}, Orientation::Landscape)));
  TEST_ASSERT_FALSE(d.isOpen());
  d.open("x", 0, 0, 0);
  TEST_ASSERT_EQUAL(int(ConfirmAnswer::No), int(d.touch({480, 340}, Orientation::Landscape)));
  d.open("x", 0, 0, 0);
  TEST_ASSERT_EQUAL(int(ConfirmAnswer::Skip), int(d.touch({650, 340}, Orientation::Landscape)));
  d.open("x", 0, 0, 0);
  TEST_ASSERT_EQUAL(int(ConfirmAnswer::Skip), int(d.touch({10, 10}, Orientation::Landscape)));  // outside = skip
}

void test_confirm_dialog_rocker_and_timeout() {
  ConfirmDialog d;
  d.open("x", 0, 0, 0);
  TEST_ASSERT_EQUAL(int(ConfirmAnswer::Yes), int(d.rocker(ui::PhysicalKey::Up)));
  TEST_ASSERT_FALSE(d.isOpen());
  d.open("x", 0, 0, 0);
  TEST_ASSERT_EQUAL(int(ConfirmAnswer::No), int(d.rocker(ui::PhysicalKey::Down)));
  d.open("x", 0, 0, 0);
  TEST_ASSERT_EQUAL(int(ConfirmAnswer::Skip), int(d.rocker(ui::PhysicalKey::Push)));  // push is never yes
  d.open("x", 0, 0, 0);
  TEST_ASSERT_EQUAL(int(ConfirmAnswer::Skip), int(d.rocker(ui::PhysicalKey::LongPush)));
  d.open("x", 0, 0, 100);
  TEST_ASSERT_FALSE(d.tick(100 + kConfirmTimeoutMs - 1));
  TEST_ASSERT_TRUE(d.tick(100 + kConfirmTimeoutMs));
  TEST_ASSERT_FALSE(d.isOpen());
  TEST_ASSERT_FALSE(d.tick(100 + 2 * kConfirmTimeoutMs));
}

// ---- keypad geometry and model ----
void test_alarm_layout_numbers() {
  const AlarmLayout l = alarmLayout(Orientation::Landscape);
  TEST_ASSERT_TRUE(l.panel == (ui::Rect{40, 94, 880, 408}));
  TEST_ASSERT_TRUE(l.close == (ui::Rect{840, 110, 64, 64}));
  TEST_ASSERT_TRUE(l.codeBox == (ui::Rect{56, 262, 300, 64}));
  TEST_ASSERT_TRUE(l.keys[0] == (ui::Rect{380, 186, 96, 64}));
  TEST_ASSERT_TRUE(l.keys[4] == (ui::Rect{488, 262, 96, 64}));  // "5"
  TEST_ASSERT_TRUE(l.keys[11] == (ui::Rect{596, 414, 96, 64}));
  TEST_ASSERT_TRUE(l.modes[0] == (ui::Rect{728, 186, 176, 64}));
  TEST_ASSERT_TRUE(l.modes[3] == (ui::Rect{728, 414, 176, 64}));
  TEST_ASSERT_EQUAL(142, l.nameCy);
  TEST_ASSERT_EQUAL(222, l.stateCy);
  TEST_ASSERT_EQUAL(358, l.msgCy);
  const AlarmLayout p = alarmLayout(Orientation::Portrait);
  TEST_ASSERT_TRUE(p.panel == (ui::Rect{16, 116, 508, 704}));
  TEST_ASSERT_TRUE(p.close == (ui::Rect{444, 132, 64, 64}));
  TEST_ASSERT_TRUE(p.codeBox == (ui::Rect{32, 268, 476, 64}));
  TEST_ASSERT_TRUE(p.keys[0] == (ui::Rect{32, 380, 150, 64}));
  TEST_ASSERT_TRUE(p.keys[2] == (ui::Rect{358, 380, 150, 64}));
  TEST_ASSERT_TRUE(p.keys[9] == (ui::Rect{32, 608, 150, 64}));
  TEST_ASSERT_TRUE(p.modes[0] == (ui::Rect{32, 688, 110, 64}));
  TEST_ASSERT_TRUE(p.modes[3] == (ui::Rect{398, 688, 110, 64}));
  TEST_ASSERT_EQUAL(784, p.hint1Cy);
  // every control lies inside its panel
  for (const AlarmLayout *L : {&l, &p}) {
    for (const ui::Rect &r : L->keys) TEST_ASSERT_TRUE(L->panel.contains(r.x, r.y) && L->panel.contains(r.x + r.w - 1, r.y + r.h - 1));
    for (const ui::Rect &r : L->modes) TEST_ASSERT_TRUE(L->panel.contains(r.x, r.y) && L->panel.contains(r.x + r.w - 1, r.y + r.h - 1));
  }
}

namespace {
AlarmPanelInfo numberPanel() {
  AlarmPanelInfo i;
  i.codeFormat = CodeFormat::Number;
  i.currentMode = 0;
  return i;
}
ui::Point at(const ui::Rect &r) { return {r.x + r.w / 2, r.y + r.h / 2}; }
}  // namespace

void test_alarm_keypad_code_entry() {
  const AlarmLayout l = alarmLayout(Orientation::Landscape);
  const AlarmPanelInfo info = numberPanel();
  AlarmPopup p;
  p.open("alarm_control_panel.h", 0, 0, false, 0);
  TEST_ASSERT_EQUAL(int(AlarmMsg::EnterCode), int(p.message(info)));
  p.touch(at(l.keys[0]), Orientation::Landscape, info, 10);  // 1
  p.touch(at(l.keys[10]), Orientation::Landscape, info, 20);  // 0
  p.touch(at(l.keys[8]), Orientation::Landscape, info, 30);  // 9
  TEST_ASSERT_EQUAL(3, p.codeLen());
  TEST_ASSERT_EQUAL(int(AlarmMsg::CodeEntered), int(p.message(info)));
  p.touch(at(l.keys[9]), Orientation::Landscape, info, 40);  // backspace
  TEST_ASSERT_EQUAL(2, p.codeLen());
  p.touch(at(l.keys[11]), Orientation::Landscape, info, 50);  // C
  TEST_ASSERT_EQUAL(0, p.codeLen());
  for (int i = 0; i < 12; ++i) p.touch(at(l.keys[1]), Orientation::Landscape, info, 60);
  TEST_ASSERT_EQUAL(kAlarmMaxCode, p.codeLen());  // max 8 digits
}

void test_alarm_send_rules() {
  const AlarmLayout l = alarmLayout(Orientation::Landscape);
  AlarmPanelInfo info = numberPanel();
  AlarmPopup p;
  p.open("alarm_control_panel.h", 0, 0, false, 0);
  // Disarm without a code: no send, message stays "Enter code"
  AlarmResult r = p.touch(at(l.modes[0]), Orientation::Landscape, info, 5);
  TEST_ASSERT_FALSE(r.send);
  // arm needs a code when code_arm_required (default)
  r = p.touch(at(l.modes[2]), Orientation::Landscape, info, 6);
  TEST_ASSERT_FALSE(r.send);
  p.touch(at(l.keys[0]), Orientation::Landscape, info, 7);
  p.touch(at(l.keys[1]), Orientation::Landscape, info, 8);
  r = p.touch(at(l.modes[2]), Orientation::Landscape, info, 9);
  TEST_ASSERT_TRUE(r.send);
  TEST_ASSERT_EQUAL(int(AlarmCmd::ArmAway), int(r.cmd));
  TEST_ASSERT_EQUAL_STRING("12", r.code.c_str());
  TEST_ASSERT_EQUAL(0, p.codeLen());  // cleared on send
  TEST_ASSERT_EQUAL(2, p.awaitingMode());
  TEST_ASSERT_EQUAL(int(AlarmMsg::Sent), int(p.message(info)));
  // a second press while awaiting does nothing
  p.touch(at(l.keys[0]), Orientation::Landscape, info, 10);
  r = p.touch(at(l.modes[1]), Orientation::Landscape, info, 11);
  TEST_ASSERT_FALSE(r.send);
  // wrong code reply
  p.sendFailed(true);
  TEST_ASSERT_EQUAL(-1, p.awaitingMode());
  TEST_ASSERT_EQUAL(0, p.codeLen());
  TEST_ASSERT_EQUAL(int(AlarmMsg::WrongCode), int(p.message(info)));
  p.touch(at(l.keys[0]), Orientation::Landscape, info, 12);  // a key press replaces the message
  TEST_ASSERT_EQUAL(int(AlarmMsg::CodeEntered), int(p.message(info)));
  // arming without a code when the panel does not require it
  info.codeArmRequired = false;
  p.touch(at(l.keys[11]), Orientation::Landscape, info, 13);
  r = p.touch(at(l.modes[1]), Orientation::Landscape, info, 14);
  TEST_ASSERT_TRUE(r.send);
  TEST_ASSERT_EQUAL(int(AlarmCmd::ArmHome), int(r.cmd));
  p.sendFailed(false);
  TEST_ASSERT_EQUAL(int(AlarmMsg::Failed), int(p.message(info)));
  // disarm always needs the code
  r = p.touch(at(l.modes[0]), Orientation::Landscape, info, 15);
  TEST_ASSERT_FALSE(r.send);
}

void test_alarm_no_code_format_and_text_format() {
  const AlarmLayout l = alarmLayout(Orientation::Landscape);
  AlarmPanelInfo none;  // no code_format
  AlarmPopup p;
  p.open("a.b", 0, 0, false, 0);
  AlarmResult r = p.touch(at(l.modes[0]), Orientation::Landscape, none, 1);
  TEST_ASSERT_TRUE(r.send);
  TEST_ASSERT_TRUE(r.code.empty());
  AlarmPanelInfo text;
  text.codeFormat = CodeFormat::Text;
  text.codeArmRequired = false;
  p.open("a.b", 0, 0, false, 0);
  TEST_ASSERT_EQUAL(int(AlarmMsg::TextUnsupported), int(p.message(text)));
  p.touch(at(l.keys[0]), Orientation::Landscape, text, 2);
  TEST_ASSERT_EQUAL(0, p.codeLen());  // keys are inert
  r = p.touch(at(l.modes[0]), Orientation::Landscape, text, 3);
  TEST_ASSERT_FALSE(r.send);  // disarm needs a code that cannot be typed
  r = p.touch(at(l.modes[2]), Orientation::Landscape, text, 4);
  TEST_ASSERT_TRUE(r.send);  // arming without a code still works
  TEST_ASSERT_TRUE(r.code.empty());
}

void test_alarm_inert_modes_and_close() {
  const AlarmLayout l = alarmLayout(Orientation::Landscape);
  AlarmPanelInfo info = numberPanel();
  info.canNight = false;
  AlarmPopup p;
  p.open("a.b", 0, 0, false, 0);
  TEST_ASSERT_FALSE(p.modeActive(3, info));
  p.touch(at(l.keys[0]), Orientation::Landscape, info, 1);
  AlarmResult r = p.touch(at(l.modes[3]), Orientation::Landscape, info, 2);
  TEST_ASSERT_FALSE(r.send);
  TEST_ASSERT_EQUAL(1, p.codeLen());
  r = p.touch(at(l.close), Orientation::Landscape, info, 3);
  TEST_ASSERT_TRUE(r.closed);
  TEST_ASSERT_FALSE(p.isOpen());
  TEST_ASSERT_EQUAL(0, p.codeLen());  // cleared on close
  p.open("a.b", 0, 0, false, 0);
  TEST_ASSERT_TRUE(p.touch({5, 5}, Orientation::Landscape, info, 4).closed);  // outside closes
  // panel tap on empty area: nothing
  p.open("a.b", 0, 0, false, 0);
  r = p.touch({60, 460}, Orientation::Landscape, info, 4);
  TEST_ASSERT_FALSE(r.changed || r.closed);
}

void test_alarm_idle_close_clears_code() {
  const AlarmLayout l = alarmLayout(Orientation::Landscape);
  const AlarmPanelInfo info = numberPanel();
  AlarmPopup p;
  p.open("a.b", 0, 0, false, 1000);
  p.touch(at(l.keys[0]), Orientation::Landscape, info, 2000);
  TEST_ASSERT_FALSE(p.tick(2000 + kAlarmIdleCloseMs - 1));
  TEST_ASSERT_TRUE(p.tick(2000 + kAlarmIdleCloseMs));
  TEST_ASSERT_EQUAL(0, p.codeLen());
}

void test_alarm_rocker_walk() {
  AlarmPanelInfo info = numberPanel();
  info.canNight = false;
  AlarmPopup p;
  p.open("a.b", 0, 0, true, 0);
  TEST_ASSERT_TRUE(p.ringVisible());
  TEST_ASSERT_EQUAL(kEntryFocus, p.focus());  // key "5"
  const std::vector<int> st = p.stops(info);
  TEST_ASSERT_EQUAL(12 + 3 + 1, int(st.size()));  // 12 keys, 3 usable modes, close
  p.rocker(ui::PhysicalKey::Down, Orientation::Portrait, info, 1);
  TEST_ASSERT_EQUAL(5, p.focus());
  // PUSH presses the focused key: key index 5 = "6"
  p.rocker(ui::PhysicalKey::Push, Orientation::Portrait, info, 2);
  TEST_ASSERT_EQUAL(1, p.codeLen());
  // walk to the end: wraps from close to key 0
  for (int i = 0; i < 40 && p.focus() != kStopClose; ++i) p.rocker(ui::PhysicalKey::Down, Orientation::Portrait, info, 3);
  TEST_ASSERT_EQUAL(kStopClose, p.focus());
  p.rocker(ui::PhysicalKey::Down, Orientation::Portrait, info, 4);
  TEST_ASSERT_EQUAL(0, p.focus());
  p.rocker(ui::PhysicalKey::Up, Orientation::Portrait, info, 5);
  TEST_ASSERT_EQUAL(kStopClose, p.focus());
  // the mode "Night" is skipped
  p.rocker(ui::PhysicalKey::Up, Orientation::Portrait, info, 6);
  TEST_ASSERT_EQUAL(kStopKeys + 2, p.focus());
  // a mode press with the code: send
  p.rocker(ui::PhysicalKey::Up, Orientation::Portrait, info, 7);  // Home
  TEST_ASSERT_EQUAL(kStopKeys + 1, p.focus());
  AlarmResult r = p.rocker(ui::PhysicalKey::Push, Orientation::Portrait, info, 8);
  TEST_ASSERT_TRUE(r.send);
  TEST_ASSERT_EQUAL(int(AlarmCmd::ArmHome), int(r.cmd));
  // push on close, long push closes anywhere
  p.sendConfirmed();
  while (p.focus() != kStopClose) p.rocker(ui::PhysicalKey::Down, Orientation::Portrait, info, 9);
  TEST_ASSERT_TRUE(p.rocker(ui::PhysicalKey::Push, Orientation::Portrait, info, 10).closed);
  p.open("a.b", 0, 0, true, 0);
  TEST_ASSERT_TRUE(p.rocker(ui::PhysicalKey::LongPush, Orientation::Portrait, info, 1).closed);
}

void test_alarm_rocker_first_event_reveals_ring() {
  AlarmPanelInfo info = numberPanel();
  AlarmPopup p;
  p.open("a.b", 0, 0, false, 0);  // by touch: no ring
  TEST_ASSERT_FALSE(p.ringVisible());
  p.rocker(ui::PhysicalKey::Push, Orientation::Portrait, info, 1);
  TEST_ASSERT_TRUE(p.ringVisible());
  TEST_ASSERT_EQUAL(0, p.codeLen());  // the first event only reveals the ring
}

// ---- AppState wiring ----
namespace {

const char *kAppYaml = R"(version: 1
tabs:
  - title: A
    widgets:
      - entity: alarm_control_panel.home
        name: Alarm
      - entity: lock.front
        name: Front door
      - entity: lock.back
        name: Back door
      - entity: cover.garage
        type: garage
        name: Garage
      - entity: cover.moving
        type: garage
        name: Gate
)";

struct Rig {
  AppState app;
  std::vector<std::string> calls;       // action sink log: "type:sub:state"
  std::vector<std::string> alarmCalls;  // "id:cmd" (the code is checked separately)
  std::string lastCode;
  Rig(bool demo = false, Orientation o = Orientation::Landscape) {
    config::Config cfg = parsed(kAppYaml);
    app.init(cfg, o, demo);
    if (!demo) {
      EntityState a = st("alarm_control_panel.home", "disarmed");
      attr(a, "code_format", "number");
      app.store().update({a, st("lock.front", "locked"), st("lock.back", "unlocked"), st("cover.garage", "closed"),
                          st("cover.moving", "opening")});
    }
    app.setActionSink([this](int, int w, int sub, const std::vector<EntityState> &req) {
      calls.push_back(std::to_string(w) + ":" + std::to_string(sub) + ":" + req[0].state);
    });
    app.setAlarmSink([this](const std::string &id, AlarmCmd c, const std::string &code) {
      alarmCalls.push_back(id + ":" + std::to_string(int(c)));
      lastCode = code;
    });
    app.setMarksDriven(!demo);
  }
  ui::Point centre(int idx) const {
    for (const ui::Placement &pl : app.layout().widgets)
      if (pl.index == idx) return {pl.rect.x + pl.rect.w / 2, pl.rect.y + pl.rect.h / 2};
    TEST_FAIL_MESSAGE("widget not placed");
    return {0, 0};
  }
};

}  // namespace

void test_layout_is_1x1() {
  Rig r;
  for (const ui::Placement &pl : r.app.layout().widgets) {
    TEST_ASSERT_EQUAL(1, pl.cellW);
    TEST_ASSERT_EQUAL(1, pl.cellH);
    TEST_ASSERT_EQUAL(176, pl.rect.w);  // landscape 5 columns (spec: W = 176)
  }
  TEST_ASSERT_EQUAL(5, int(r.app.layout().widgets.size()));
}

void test_lock_unlock_asks_first_and_yes_acts() {
  Rig r;
  r.app.touch(r.centre(1), 100);  // locked front door
  TEST_ASSERT_TRUE(r.app.confirmOpen());
  TEST_ASSERT_EQUAL_STRING("Unlock Front door?", r.app.confirm().title().c_str());
  TEST_ASSERT_TRUE(r.calls.empty());
  TEST_ASSERT_TRUE(r.app.modalOpen());
  // a tap on another widget while the dialog is open is a skip, not an action
  const ConfirmLayout l = confirmLayout(Orientation::Landscape);
  r.app.touch({l.yes.x + 10, l.yes.y + 10}, 200);
  TEST_ASSERT_FALSE(r.app.confirmOpen());
  TEST_ASSERT_EQUAL(1, int(r.calls.size()));
  TEST_ASSERT_EQUAL_STRING("1:0:unlocked", r.calls[0].c_str());
  // optimistic: the widget already shows the requested state
  const WidgetData d = r.app.widgetData(1);
  TEST_ASSERT_TRUE(d.optimistic);
  TEST_ASSERT_EQUAL_STRING("unlocked", d.members[0]->state.c_str());
}

void test_lock_lock_does_not_ask() {
  Rig r;
  r.app.touch(r.centre(2), 100);  // unlocked back door: locking is safe
  TEST_ASSERT_FALSE(r.app.confirmOpen());
  TEST_ASSERT_EQUAL(1, int(r.calls.size()));
  TEST_ASSERT_EQUAL_STRING("2:0:locked", r.calls[0].c_str());
}

void test_garage_asks_both_ways_skip_changes_nothing() {
  Rig r;
  r.app.touch(r.centre(3), 100);
  TEST_ASSERT_TRUE(r.app.confirmOpen());
  TEST_ASSERT_EQUAL_STRING("Open Garage?", r.app.confirm().title().c_str());
  const ConfirmLayout l = confirmLayout(Orientation::Landscape);
  r.app.touch({l.skip.x + 5, l.skip.y + 5}, 200);
  TEST_ASSERT_FALSE(r.app.confirmOpen());
  TEST_ASSERT_TRUE(r.calls.empty());
  TEST_ASSERT_FALSE(r.app.widgetData(3).optimistic);
  // close direction asks too
  r.app.store().update({st("cover.garage", "open")});
  r.app.touch(r.centre(3), 300);
  TEST_ASSERT_EQUAL_STRING("Close Garage?", r.app.confirm().title().c_str());
}

void test_garage_moving_stops_without_dialog() {
  Rig r;
  r.app.touch(r.centre(4), 100);
  TEST_ASSERT_FALSE(r.app.confirmOpen());
  TEST_ASSERT_EQUAL(1, int(r.calls.size()));
  TEST_ASSERT_EQUAL_STRING("4:2:opening", r.calls[0].c_str());
}

void test_confirm_times_out_and_rocker_rules() {
  Rig r;
  r.app.touch(r.centre(3), 1000);
  TEST_ASSERT_FALSE(r.app.tick(1000 + kConfirmTimeoutMs - 1));
  TEST_ASSERT_TRUE(r.app.confirmOpen());
  TEST_ASSERT_TRUE(r.app.tick(1000 + kConfirmTimeoutMs));
  TEST_ASSERT_FALSE(r.app.confirmOpen());
  TEST_ASSERT_TRUE(r.calls.empty());
  // rocker: PUSH on the focused widget opens the dialog; PUSH inside = skip; UP = yes
  r.app.rocker(ui::PhysicalKey::Push, 2000);  // Tab mode -> the first widget
  TEST_ASSERT_EQUAL(0, r.app.focusedWidgetIndex());
  while (r.app.focusedWidgetIndex() != 3) r.app.rocker(ui::PhysicalKey::Down, 2100);
  r.app.rocker(ui::PhysicalKey::Push, 2200);
  TEST_ASSERT_TRUE(r.app.confirmOpen());
  r.app.rocker(ui::PhysicalKey::Push, 2300);  // skip
  TEST_ASSERT_FALSE(r.app.confirmOpen());
  TEST_ASSERT_TRUE(r.calls.empty());
  r.app.rocker(ui::PhysicalKey::Push, 2400);
  TEST_ASSERT_TRUE(r.app.confirmOpen());
  r.app.rocker(ui::PhysicalKey::Up, 2500);  // yes
  TEST_ASSERT_FALSE(r.app.confirmOpen());
  TEST_ASSERT_EQUAL(1, int(r.calls.size()));
  TEST_ASSERT_EQUAL_STRING("3:0:open", r.calls[0].c_str());
}

// Safety: a pending rocker tab switch must not move the dialog's widget under it.
void test_confirm_holds_tab_switch_while_open() {
  config::Config cfg = parsed(
      "version: 1\ntabs:\n  - title: A\n    widgets:\n      - entity: lock.a\n  - title: B\n    widgets:\n"
      "      - entity: lock.b\n");
  AppState app;
  app.init(cfg, Orientation::Landscape, false);
  app.store().update({st("lock.a", "locked"), st("lock.b", "locked")});
  std::vector<std::string> ids;
  app.setActionSink([&](int, int, int, const std::vector<EntityState> &req) { ids.push_back(req[0].id); });
  app.setMarksDriven(true);
  app.rocker(ui::PhysicalKey::Down, 100);  // Tab mode: next tab, debounced
  TEST_ASSERT_EQUAL(0, app.shownTab());
  ui::Point c{0, 0};
  for (const ui::Placement &pl : app.layout().widgets) c = {pl.rect.x + pl.rect.w / 2, pl.rect.y + pl.rect.h / 2};
  app.touch(c, 200);
  TEST_ASSERT_TRUE(app.confirmOpen());
  app.tick(1000);  // the debounce is due, but a modal is open
  TEST_ASSERT_EQUAL(0, app.shownTab());
  TEST_ASSERT_TRUE(app.confirmOpen());
  app.rocker(ui::PhysicalKey::Up, 1100);  // yes: acts on lock.a
  TEST_ASSERT_EQUAL(1, int(ids.size()));
  TEST_ASSERT_EQUAL_STRING("lock.a", ids[0].c_str());
  app.tick(1200);  // the held switch applies after the dialog
  TEST_ASSERT_EQUAL(1, app.shownTab());
}

// Safety: Yes re-checks the action; "Close garage?" must not open a garage that is already closed.
void test_confirm_yes_refused_when_action_changed() {
  Rig r;
  r.app.store().update({st("cover.garage", "open")});
  r.app.touch(r.centre(3), 100);
  TEST_ASSERT_EQUAL_STRING("Close Garage?", r.app.confirm().title().c_str());
  r.app.store().update({st("cover.garage", "closed")});  // closed meanwhile
  const ConfirmLayout l = confirmLayout(Orientation::Landscape);
  TEST_ASSERT_TRUE(r.app.touch({l.yes.x + 10, l.yes.y + 10}, 200));  // redraw
  TEST_ASSERT_FALSE(r.app.confirmOpen());
  TEST_ASSERT_TRUE(r.calls.empty());
  TEST_ASSERT_FALSE(r.app.widgetData(3).optimistic);
}

void test_confirm_override_off_and_on() {
  config::Config cfg = parsed(
      "version: 1\ntabs:\n  - title: A\n    widgets:\n      - entity: lock.a\n        confirm: false\n"
      "      - entity: lock.b\n        confirm: true\n");
  AppState app;
  app.init(cfg, Orientation::Landscape, false);
  app.store().update({st("lock.a", "locked"), st("lock.b", "unlocked")});
  int acts = 0;
  app.setActionSink([&](int, int, int, const std::vector<EntityState> &) { ++acts; });
  app.setMarksDriven(true);
  ui::Point p0, p1;
  for (const ui::Placement &pl : app.layout().widgets) (pl.index == 0 ? p0 : p1) = {pl.rect.x + 20, pl.rect.y + 20};
  app.touch(p0, 100);
  TEST_ASSERT_FALSE(app.confirmOpen());  // confirm: false unlocks at once
  TEST_ASSERT_EQUAL(1, acts);
  app.touch(p1, 200);
  TEST_ASSERT_TRUE(app.confirmOpen());  // confirm: true asks even for locking
}

void test_unavailable_and_jammed_do_nothing() {
  Rig r;
  r.app.store().update({st("lock.front", "jammed"), st("cover.garage", "unavailable", false)});
  r.app.touch(r.centre(1), 100);
  r.app.touch(r.centre(3), 200);
  TEST_ASSERT_FALSE(r.app.confirmOpen());
  TEST_ASSERT_TRUE(r.calls.empty());
}

void test_alarm_keypad_through_appstate_real_mode() {
  Rig r;
  const AlarmLayout l = alarmLayout(Orientation::Landscape);
  r.app.touch(r.centre(0), 100);
  TEST_ASSERT_TRUE(r.app.alarmOpen());
  TEST_ASSERT_TRUE(r.app.modalOpen());
  r.app.touch(at(l.keys[0]), 110);
  r.app.touch(at(l.keys[1]), 120);
  r.app.touch(at(l.keys[2]), 130);
  r.app.touch(at(l.keys[10]), 140);
  TEST_ASSERT_EQUAL(4, r.app.alarm().codeLen());
  r.app.touch(at(l.modes[2]), 150);  // Away
  TEST_ASSERT_EQUAL(1, int(r.alarmCalls.size()));
  TEST_ASSERT_EQUAL_STRING("alarm_control_panel.home:2", r.alarmCalls[0].c_str());
  TEST_ASSERT_EQUAL_STRING("1230", r.lastCode.c_str());
  TEST_ASSERT_EQUAL(0, r.app.alarm().codeLen());
  TEST_ASSERT_TRUE(r.app.widgetData(0).pending);  // the widget shows the pending look
  // driver: Pending, then cleared with the state unchanged -> Wrong code (a code was sent)
  r.app.setMarks({{"alarm_control_panel.home", Mark::Pending}});
  r.app.setMarks({});
  TEST_ASSERT_EQUAL(int(AlarmMsg::WrongCode), int(r.app.alarm().message(r.app.alarmInfo())));
  TEST_ASSERT_EQUAL(-1, r.app.alarm().awaitingMode());
  TEST_ASSERT_FALSE(r.app.widgetData(0).pending);
}

void test_alarm_confirmed_by_state_change() {
  Rig r;
  const AlarmLayout l = alarmLayout(Orientation::Landscape);
  r.app.touch(r.centre(0), 100);
  r.app.touch(at(l.keys[0]), 110);
  r.app.touch(at(l.modes[1]), 150);  // Home
  r.app.setMarks({{"alarm_control_panel.home", Mark::Pending}});
  EntityState a = st("alarm_control_panel.home", "armed_home");
  attr(a, "code_format", "number");
  r.app.store().update({a});
  r.app.setMarks({});
  TEST_ASSERT_EQUAL(int(AlarmMsg::Sent), int(r.app.alarm().message(r.app.alarmInfo())));
  TEST_ASSERT_EQUAL(1, r.app.alarmInfo().currentMode);
}

void test_alarm_service_error_and_safety_timeout() {
  Rig r;
  const AlarmLayout l = alarmLayout(Orientation::Landscape);
  r.app.touch(r.centre(0), 100);
  r.app.touch(at(l.keys[0]), 110);
  r.app.touch(at(l.modes[0]), 150);
  r.app.setMarks({{"alarm_control_panel.home", Mark::Failed}});
  TEST_ASSERT_EQUAL(int(AlarmMsg::WrongCode), int(r.app.alarm().message(r.app.alarmInfo())));
  // no code typed in a no-code panel: "Failed"
  r.app.closeAlarm();
  r.app.setMarks({});
  EntityState a = st("alarm_control_panel.home", "disarmed");
  r.app.store().update({a});  // no code_format any more
  r.app.touch(r.centre(0), 1000);
  r.app.touch(at(l.modes[1]), 1010);
  TEST_ASSERT_EQUAL(2, int(r.alarmCalls.size()));
  TEST_ASSERT_TRUE(r.lastCode.empty());
  r.app.setMarksDriven(true, 5000);
  r.app.tick(1010 + 5000);  // the driver never answered
  TEST_ASSERT_EQUAL(int(AlarmMsg::Failed), int(r.app.alarm().message(r.app.alarmInfo())));
}

void test_alarm_closes_clear_code_and_unavailable_closes() {
  Rig r;
  const AlarmLayout l = alarmLayout(Orientation::Landscape);
  r.app.touch(r.centre(0), 100);
  r.app.touch(at(l.keys[0]), 110);
  TEST_ASSERT_EQUAL(1, r.app.alarm().codeLen());
  TEST_ASSERT_TRUE(r.app.closeAlarm());  // what goToSleep calls
  TEST_ASSERT_EQUAL(0, r.app.alarm().codeLen());
  TEST_ASSERT_FALSE(r.app.alarmOpen());
  r.app.touch(r.centre(0), 200);
  r.app.touch(at(l.keys[0]), 210);
  r.app.store().update({st("alarm_control_panel.home", "unavailable", false)});
  r.app.tick(300);
  TEST_ASSERT_FALSE(r.app.alarmOpen());
  TEST_ASSERT_EQUAL(0, r.app.alarm().codeLen());
  // unavailable / loading panels do not open
  r.app.touch(r.centre(0), 400);
  TEST_ASSERT_FALSE(r.app.alarmOpen());
}

void test_alarm_rocker_push_opens_keypad() {
  Rig r;
  r.app.rocker(ui::PhysicalKey::Push, 100);  // Tab mode -> the first widget
  TEST_ASSERT_EQUAL(0, r.app.focusedWidgetIndex());
  r.app.rocker(ui::PhysicalKey::Push, 200);
  TEST_ASSERT_TRUE(r.app.alarmOpen());
  TEST_ASSERT_TRUE(r.app.alarm().ringVisible());
  TEST_ASSERT_EQUAL(kEntryFocus, r.app.alarm().focus());
  r.app.rocker(ui::PhysicalKey::LongPush, 300);
  TEST_ASSERT_FALSE(r.app.alarmOpen());
}

// ---- demo mode ----
namespace {
config::Config demoCfg() {
  std::ifstream f("demo.yaml");
  std::stringstream ss;
  ss << f.rdbuf();
  return parsed(ss.str().c_str());
}
}  // namespace

void test_demo_security_tab_shows_all_states() {
  const config::Config cfg = demoCfg();
  AppState app;
  app.init(cfg, Orientation::Landscape, true);
  std::vector<std::string> texts;
  bool fill = false, una = false;
  for (const char *title : {"Alarm", "Doors"}) {
    int tab = -1;
    for (size_t i = 0; i < cfg.tabs.size(); ++i) if (cfg.tabs[i].title == title) tab = int(i);
    TEST_ASSERT_TRUE(tab >= 0);
    app.restoreTab(tab);
    TEST_ASSERT_EQUAL(tab, app.shownTab());
    const auto &ws = cfg.tabs[size_t(tab)].widgets;
    for (size_t i = 0; i < ws.size(); ++i) {
      const WidgetData d = app.widgetData(int(i));
      const SecureView v = secureView(ws[i].type, d.members.empty() ? nullptr : d.members[0]);
      texts.push_back(ws[i].type == config::WidgetType::Alarm ? "A:" + v.text : ws[i].type == config::WidgetType::Lock ? "L:" + v.text : "G:" + v.text);
      fill |= v.fill;
      una |= v.look == Look::Unavailable;
    }
    TEST_ASSERT_FALSE(ui::computeFlow(Orientation::Landscape, ws).pagerVisible);  // each tab fits one page
    TEST_ASSERT_FALSE(ui::computeFlow(Orientation::Portrait, ws).pagerVisible);
  }
  TEST_ASSERT_TRUE(fill && una);
  auto has = [&](const char *t) { return std::find(texts.begin(), texts.end(), t) != texts.end(); };
  for (const char *t : {"A:Off", "A:Away", "A:Night", "A:Home", "A:Arming", "A:Entry", "A:ALARM", "A:n/a", "L:Locked", "L:Open", "L:Jammed",
                        "L:Opening", "L:n/a", "G:Closed", "G:Open", "G:Opening", "G:n/a"})
    TEST_ASSERT_TRUE_MESSAGE(has(t), t);
}

void test_demo_keypad_wrong_and_right_code() {
  const config::Config cfg = demoCfg();
  int tab = 0;
  for (size_t i = 0; i < cfg.tabs.size(); ++i) if (cfg.tabs[i].title == "Alarm") tab = int(i);
  AppState app;
  app.init(cfg, Orientation::Landscape, true);
  app.restoreTab(tab);
  ui::Point p0{0, 0};
  for (const ui::Placement &pl : app.layout().widgets) if (pl.index == 0) p0 = {pl.rect.x + 20, pl.rect.y + 20};
  const AlarmLayout l = alarmLayout(Orientation::Landscape);
  app.touch(p0, 100);
  TEST_ASSERT_TRUE(app.alarmOpen());
  // arming needs no code in the demo
  app.touch(at(l.modes[1]), 110);
  TEST_ASSERT_EQUAL(1, app.alarmInfo().currentMode);
  // disarming with a wrong code
  app.touch(at(l.keys[0]), 120);
  app.touch(at(l.modes[0]), 130);
  TEST_ASSERT_EQUAL(int(AlarmMsg::WrongCode), int(app.alarm().message(app.alarmInfo())));
  TEST_ASSERT_EQUAL(1, app.alarmInfo().currentMode);
  // with 1234
  app.touch(at(l.keys[11]), 160);  // clear
  for (int k : {0, 1, 2, 3}) app.touch(at(l.keys[k]), 170);
  app.touch(at(l.modes[0]), 180);
  TEST_ASSERT_EQUAL(0, app.alarmInfo().currentMode);
  TEST_ASSERT_EQUAL(int(AlarmMsg::Sent), int(app.alarm().message(app.alarmInfo())));
}

void test_demo_lock_commits_after_pending() {
  const config::Config cfg = demoCfg();
  int tab = 0, lockIdx = 0;
  for (size_t i = 0; i < cfg.tabs.size(); ++i) if (cfg.tabs[i].title == "Doors") tab = int(i);
  const auto &ws = cfg.tabs[size_t(tab)].widgets;
  for (size_t i = 0; i < ws.size(); ++i) if (ws[i].entity == "lock.front_door") lockIdx = int(i);
  AppState app;
  app.init(cfg, Orientation::Landscape, true);
  app.restoreTab(tab);
  ui::Point p{0, 0};
  for (const ui::Placement &pl : app.layout().widgets) if (pl.index == lockIdx) p = {pl.rect.x + 20, pl.rect.y + 20};
  app.touch(p, 100);
  TEST_ASSERT_TRUE(app.confirmOpen());
  app.rocker(ui::PhysicalKey::Up, 110);
  app.tick(110 + kPendingMs + 1);
  TEST_ASSERT_EQUAL_STRING("unlocked", app.store().get("lock.front_door")->state.c_str());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_config_types_and_confirm);
  RUN_TEST(test_config_errors);
  RUN_TEST(test_config_cache_roundtrip);
  RUN_TEST(test_service_calls);
  RUN_TEST(test_intents);
  RUN_TEST(test_alarm_view_table);
  RUN_TEST(test_lock_view_table);
  RUN_TEST(test_garage_view_table);
  RUN_TEST(test_actions_and_confirm_defaults);
  RUN_TEST(test_alarm_panel_info);
  RUN_TEST(test_confirm_dialog_layout_and_answers);
  RUN_TEST(test_confirm_dialog_rocker_and_timeout);
  RUN_TEST(test_alarm_layout_numbers);
  RUN_TEST(test_alarm_keypad_code_entry);
  RUN_TEST(test_alarm_send_rules);
  RUN_TEST(test_alarm_no_code_format_and_text_format);
  RUN_TEST(test_alarm_inert_modes_and_close);
  RUN_TEST(test_alarm_idle_close_clears_code);
  RUN_TEST(test_alarm_rocker_walk);
  RUN_TEST(test_alarm_rocker_first_event_reveals_ring);
  RUN_TEST(test_layout_is_1x1);
  RUN_TEST(test_lock_unlock_asks_first_and_yes_acts);
  RUN_TEST(test_lock_lock_does_not_ask);
  RUN_TEST(test_garage_asks_both_ways_skip_changes_nothing);
  RUN_TEST(test_garage_moving_stops_without_dialog);
  RUN_TEST(test_confirm_times_out_and_rocker_rules);
  RUN_TEST(test_confirm_override_off_and_on);
  RUN_TEST(test_confirm_holds_tab_switch_while_open);
  RUN_TEST(test_confirm_yes_refused_when_action_changed);
  RUN_TEST(test_unavailable_and_jammed_do_nothing);
  RUN_TEST(test_alarm_keypad_through_appstate_real_mode);
  RUN_TEST(test_alarm_confirmed_by_state_change);
  RUN_TEST(test_alarm_service_error_and_safety_timeout);
  RUN_TEST(test_alarm_closes_clear_code_and_unavailable_closes);
  RUN_TEST(test_alarm_rocker_push_opens_keypad);
  RUN_TEST(test_demo_security_tab_shows_all_states);
  RUN_TEST(test_demo_keypad_wrong_and_right_code);
  RUN_TEST(test_demo_lock_commits_after_pending);
  return UNITY_END();
}
