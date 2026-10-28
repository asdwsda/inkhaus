#include <unity.h>

#include <map>
#include <string>
#include <vector>

#include "ha_worker.h"
#include "mailbox.h"
#include "session_driver.h"
#include "tap_intent.h"
#include "wall_time.h"

using namespace app_logic;

void setUp() {}
void tearDown() {}

// ---- fakes ----

// A tiny Home Assistant: /api/template answers from `states`; light turn_on/off changes them.
struct FakeHa : HttpTransport {
  std::map<std::string, std::string> states;
  int status = 200;       // forced status for every request (0 = no response)
  int templateCalls = 0, serviceCalls = 0;
  std::vector<std::string> urls;
  std::vector<std::string> bodies;
  bool callsChangeState = true;

  HttpResponse request(const std::string &, const std::string &url, const HttpHeaders &,
                       const std::string &body) override {
    urls.push_back(url);
    bodies.push_back(body);
    HttpResponse r;
    r.status = status;
    if (status == 0) { r.error = "timeout"; return r; }
    if (status != 200) return r;
    if (url.find("/api/template") != std::string::npos) {
      ++templateCalls;
      std::string j = "{";
      for (auto &kv : states) {
        if (j.size() > 1) j += ",";
        j += "\"" + kv.first + "\": {\"s\": \"" + kv.second + "\", \"lc\": 1700000000, \"a\": {}}";
      }
      r.body = j + "}";
    } else {
      ++serviceCalls;
      if (callsChangeState) {
        const bool on = url.find("turn_on") != std::string::npos;
        for (auto &kv : states)
          if (body.find(kv.first) != std::string::npos) kv.second = on ? "on" : "off";
      }
      r.body = "[]";
    }
    return r;
  }
};

struct FakeWifi : IWifi {
  IWifi::State result = IWifi::State::Connected;
  int polls = 0, afterPolls = 2, begins = 0, downs = 0;
  void begin() override { ++begins; polls = 0; }
  State poll() override { return ++polls >= afterPolls ? result : State::Connecting; }
  void powerDown() override { ++downs; }
};

struct Rig {
  FakeHa ha;
  FakeWifi wifi;
  HaWorker worker;
  SyncNetPort port;
  StateStore store;
  std::vector<std::string> log;
  int64_t epoch = 1767225600 + 3600 * 12;  // 2026-01-01 12:00 UTC
  SessionDriver drv;
  int64_t now = 1000;

  Rig(std::vector<std::string> ids = {"light.a", "switch.b"})
      : worker(ha, "http://ha.local:8123", "", "SECRET-TOKEN"), port(worker),
        drv(ids, store, wifi, port, [this] { return epoch; }, [this](const std::string &l) { log.push_back(l); }) {
    ha.states = {{"light.a", "off"}, {"switch.b", "on"}};
  }
  void start() { drv.start(power::WakeReason::ColdBoot, 4000, settings::Settings{}, 0, now); }
  // Ticks every 100 ms until pred or the limit (ms).
  template <class F> bool until(F pred, int64_t limit = 20000) {
    for (int64_t t = 0; t < limit; t += 100) {
      if (pred()) return true;
      now += 100;
      drv.tick(now);
    }
    return pred();
  }
  bool logHas(const std::string &s) const {
    for (auto &l : log) if (l.find(s) != std::string::npos) return true;
    return false;
  }
};

// ---- driver ----

void test_connect_fetch_render() {
  Rig r;
  r.start();
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.sessionDone(); }));
  TEST_ASSERT_EQUAL(1, r.wifi.begins);
  TEST_ASSERT_EQUAL(1, r.ha.templateCalls);
  TEST_ASSERT_EQUAL(2, r.store.size());
  TEST_ASSERT_EQUAL_STRING("off", r.store.get("light.a")->state.c_str());
  TEST_ASSERT_TRUE(r.drv.takeRender());
  TEST_ASSERT_FALSE(r.drv.takeRender());  // one-shot
  TEST_ASSERT_EQUAL((int)session::Status::Online, (int)r.drv.status());
  TEST_ASSERT_EQUAL(r.epoch, r.drv.lastOkEpoch());
  TEST_ASSERT_FALSE(r.drv.lastFetchFailed());
  TEST_ASSERT_FALSE(r.drv.lastLinkExternal());
}

void test_tap_call_confirm() {
  Rig r;
  r.start();
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.sessionDone(); }));
  r.drv.takeRender();
  EntityState req;
  req.id = "light.a";
  req.state = "on";
  auto ev = intentFromRequest(config::WidgetType::Light, 0, {req});
  TEST_ASSERT_TRUE(ev.has_value());
  r.drv.userAction(*ev, r.now);
  TEST_ASSERT_TRUE(r.until([&] { return r.ha.serviceCalls == 1; }));
  TEST_ASSERT_TRUE(r.logHas("service=light.turn_on ids=light.a"));
  // pending mark first, confirmed after the confirmation fetch shows "on"
  TEST_ASSERT_TRUE(r.until([&] { return r.store.get("light.a")->state == "on"; }));
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.marks().empty(); }));
  TEST_ASSERT_TRUE(r.drv.takeRender());
  TEST_ASSERT_EQUAL(2, r.ha.templateCalls);  // initial + one confirmation
  TEST_ASSERT_EQUAL((int)session::Status::Online, (int)r.drv.status());
}

void test_tap_before_wifi_is_sent_before_initial_fetch() {
  Rig r;
  r.start();
  EntityState req;
  req.id = "light.a";
  req.state = "on";
  r.drv.userAction(*intentFromRequest(config::WidgetType::Light, 0, {req}), r.now);
  TEST_ASSERT_EQUAL(0, r.ha.serviceCalls);
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.sessionDone(); }));
  TEST_ASSERT_EQUAL(1, r.ha.serviceCalls);
  TEST_ASSERT_TRUE(r.ha.urls[0].find("/api/services/light/turn_on") != std::string::npos);
  TEST_ASSERT_EQUAL_STRING("on", r.store.get("light.a")->state.c_str());
}

void test_wifi_failed_is_offline() {
  Rig r;
  r.wifi.result = IWifi::State::Failed;
  r.start();
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.status() == session::Status::Offline; }));
  TEST_ASSERT_EQUAL(0, r.ha.templateCalls);
  TEST_ASSERT_EQUAL(0, (int)r.store.size());
  TEST_ASSERT_TRUE(r.drv.takeRender());
}

void test_ha_unreachable_is_offline() {
  Rig r;
  r.ha.status = 0;
  r.start();
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.status() == session::Status::Offline; }));
  TEST_ASSERT_TRUE(r.drv.lastFetchFailed());
  TEST_ASSERT_EQUAL(0, (int)r.drv.lastOkEpoch());
  TEST_ASSERT_TRUE(r.drv.sessionDone());
}

void test_auth_error() {
  Rig r;
  r.ha.status = 401;
  r.start();
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.status() == session::Status::AuthError; }));
  TEST_ASSERT_TRUE(r.drv.lastFetchFailed());
}

void test_failed_call_marks_failed() {
  Rig r;
  r.start();
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.sessionDone(); }));
  r.ha.status = 0;
  EntityState req;
  req.id = "light.a";
  req.state = "on";
  r.drv.userAction(*intentFromRequest(config::WidgetType::Light, 0, {req}), r.now);
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.status() == session::Status::Offline; }));
  TEST_ASSERT_EQUAL((int)session::Mark::Failed, (int)r.drv.marks().at("light.a"));
}

void test_manual_refresh_and_idle_sleep() {
  Rig r;
  r.start();
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.sessionDone(); }));
  r.ha.states["light.a"] = "on";  // changed behind our back
  r.drv.refresh(r.now);
  TEST_ASSERT_TRUE(r.until([&] { return r.ha.templateCalls == 2 && !r.drv.fetching(); }));
  TEST_ASSERT_EQUAL_STRING("on", r.store.get("light.a")->state.c_str());
  // nothing else happens until the idle timeout (20 s default), then Sleep is requested
  SleepRequest sl;
  TEST_ASSERT_FALSE(r.drv.takeSleep(sl));
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.sleeping(); }, 130000));
  TEST_ASSERT_TRUE(r.drv.takeSleep(sl));
  TEST_ASSERT_FALSE(r.drv.takeSleep(sl));
  TEST_ASSERT_EQUAL(1, r.wifi.downs);
}

void test_log_has_no_secrets() {
  Rig r;
  r.start();
  r.ha.status = 401;
  r.until([&] { return r.drv.status() == session::Status::AuthError; });
  for (auto &l : r.log) {
    TEST_ASSERT_TRUE(l.rfind("APP: ", 0) == 0);
    TEST_ASSERT_TRUE(l.find("SECRET") == std::string::npos);
    TEST_ASSERT_TRUE(l.find("ha.local") == std::string::npos);
  }
}

void test_external_link_flag() {
  FakeHa ha;
  struct Both : HttpTransport {
    FakeHa &ha;
    explicit Both(FakeHa &h) : ha(h) {}
    HttpResponse request(const std::string &m, const std::string &u, const HttpHeaders &h, const std::string &b) override {
      if (u.find("ha.local") != std::string::npos) { HttpResponse r; r.error = "timeout"; return r; }
      return ha.request(m, u, h, b);
    }
  } t(ha);
  ha.states = {{"light.a", "on"}};
  HaWorker w(t, "http://ha.local:8123", "https://x.example.org", "T");
  NetCommand c;
  c.ids = {"light.a"};
  NetEvent e = w.run(c);
  TEST_ASSERT_EQUAL((int)HaError::None, (int)e.error);
  TEST_ASSERT_TRUE(e.external);
}

// ---- tap intent ----

void test_intent_toggles_and_actions() {
  EntityState a, b;
  a.id = "switch.x"; a.state = "off";
  b.id = "switch.y"; b.state = "off";
  auto e = intentFromRequest(config::WidgetType::Switch, 0, {a, b});
  TEST_ASSERT_TRUE(e.has_value());
  TEST_ASSERT_EQUAL((int)Action::SwitchTurnOff, (int)e->action);
  TEST_ASSERT_EQUAL(2, e->ids.size());
  TEST_ASSERT_EQUAL_STRING("off", e->expected_state.c_str());
  EntityState c; c.id = "cover.k";
  TEST_ASSERT_EQUAL((int)Action::CoverOpen, (int)intentFromRequest(config::WidgetType::Cover, 1, {c})->action);
  TEST_ASSERT_EQUAL((int)Action::CoverStop, (int)intentFromRequest(config::WidgetType::Cover, 2, {c})->action);
  TEST_ASSERT_EQUAL((int)Action::CoverClose, (int)intentFromRequest(config::WidgetType::Cover, 3, {c})->action);
  TEST_ASSERT_FALSE(intentFromRequest(config::WidgetType::Cover, 0, {c}).has_value());
  TEST_ASSERT_FALSE(intentFromRequest(config::WidgetType::Sensor, 0, {c}).has_value());
  TEST_ASSERT_FALSE(intentFromRequest(config::WidgetType::Light, 0, {}).has_value());
  EntityState v; v.id = "vacuum.r";
  TEST_ASSERT_EQUAL((int)Action::VacuumReturnToBase, (int)intentFromRequest(config::WidgetType::Vacuum, 3, {v})->action);
}

void test_intent_climate_custom() {
  EntityState c;
  c.id = "climate.l";
  c.temperature = 21.5;
  c.min_temp = 7; c.max_temp = 30; c.target_temp_step = 0.5;
  auto e = intentFromRequest(config::WidgetType::Climate, 2, {c});
  TEST_ASSERT_TRUE(e.has_value());
  TEST_ASSERT_TRUE(e->custom);
  TEST_ASSERT_EQUAL_STRING("climate", e->custom_call.domain.c_str());
  TEST_ASSERT_EQUAL_STRING("set_temperature", e->custom_call.service.c_str());
  TEST_ASSERT_TRUE(e->custom_call.body.find("21.5") != std::string::npos);
  c.temperature.reset();
  TEST_ASSERT_FALSE(intentFromRequest(config::WidgetType::Climate, 2, {c}).has_value());
}

// ---- wall time and label ----

void test_civil_roundtrip() {
  Civil c = epochToCivil(1767225600 + 3600 * 13 + 125);
  TEST_ASSERT_EQUAL(2026, c.year);
  TEST_ASSERT_EQUAL(1, c.month);
  TEST_ASSERT_EQUAL(1, c.day);
  TEST_ASSERT_EQUAL(13, c.hour);
  TEST_ASSERT_EQUAL(2, c.minute);
  TEST_ASSERT_EQUAL(1767225600 + 3600 * 13 + 125, civilToEpoch(c));
  Civil leap = epochToCivil(civilToEpoch({2028, 2, 29, 23, 59, 59}), 0);
  TEST_ASSERT_EQUAL(29, leap.day);
  TEST_ASSERT_EQUAL(2, leap.month);
  Civil off = epochToCivil(1767225600 + 3600 * 23, 7200);  // crosses midnight
  TEST_ASSERT_EQUAL(2, off.day);
  TEST_ASSERT_EQUAL(1, off.hour);
}

void test_label() {
  const int64_t now = 1767225600 + 14 * 3600 + 5 * 60;
  Label l = refreshLabel(now - 60, now, 0, 60, false, false);
  TEST_ASSERT_EQUAL_STRING("updated 14:04", l.line1.c_str());
  TEST_ASSERT_FALSE(l.stale);
  l = refreshLabel(now - 60, now, 0, 60, false, true);
  TEST_ASSERT_EQUAL_STRING("updated", l.line1.c_str());
  TEST_ASSERT_EQUAL_STRING("14:04", l.line2.c_str());
  l = refreshLabel(now - 86400, now, 0, 60, false, false);
  TEST_ASSERT_EQUAL_STRING("updated 31.12. 14:05", l.line1.c_str());
  TEST_ASSERT_TRUE(l.stale);
  l = refreshLabel(now - 3 * 3600, now, 0, 60, false, false);  // older than 2 x 60 min
  TEST_ASSERT_TRUE(l.stale);
  l = refreshLabel(now - 3 * 3600, now, 0, 0, false, false);   // off: 3 h limit, not exceeded strictly
  TEST_ASSERT_FALSE(l.stale);
  l = refreshLabel(now - 10, now, 0, 60, true, false);  // last fetch failed
  TEST_ASSERT_TRUE(l.stale);
  l = refreshLabel(0, now, 0, 60, false, false);
  TEST_ASSERT_EQUAL_STRING("not updated yet", l.line1.c_str());
  l = refreshLabel(0, 0, 0, 60, false, true);
  TEST_ASSERT_EQUAL_STRING("never", l.line2.c_str());
}

void test_mailbox() {
  Mailbox<int> m;
  int v = 0;
  TEST_ASSERT_FALSE(m.pop(v));
  m.push(1);
  m.push(2);
  TEST_ASSERT_EQUAL(2, m.size());
  TEST_ASSERT_TRUE(m.pop(v));
  TEST_ASSERT_EQUAL(1, v);
  TEST_ASSERT_TRUE(m.pop(v));
  TEST_ASSERT_EQUAL(2, v);
  TEST_ASSERT_FALSE(m.pop(v));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_connect_fetch_render);
  RUN_TEST(test_tap_call_confirm);
  RUN_TEST(test_tap_before_wifi_is_sent_before_initial_fetch);
  RUN_TEST(test_wifi_failed_is_offline);
  RUN_TEST(test_ha_unreachable_is_offline);
  RUN_TEST(test_auth_error);
  RUN_TEST(test_failed_call_marks_failed);
  RUN_TEST(test_manual_refresh_and_idle_sleep);
  RUN_TEST(test_log_has_no_secrets);
  RUN_TEST(test_external_link_flag);
  RUN_TEST(test_intent_toggles_and_actions);
  RUN_TEST(test_intent_climate_custom);
  RUN_TEST(test_civil_roundtrip);
  RUN_TEST(test_label);
  RUN_TEST(test_mailbox);
  return UNITY_END();
}
