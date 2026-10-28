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
  std::string errorBody;  // body of non-200 answers

  HttpResponse request(const std::string &, const std::string &url, const HttpHeaders &,
                       const std::string &body) override {
    urls.push_back(url);
    bodies.push_back(body);
    HttpResponse r;
    r.status = status;
    if (status == 0) { r.error = "timeout"; return r; }
    if (status != 200) { r.body = errorBody; return r; }
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

// Every path (online, failed call, 500 with a body, 401, offline, sleep) logs "APP: " lines only,
// without the token, the URL, response bodies or entity attributes.
void test_log_has_no_secrets_all_paths() {
  Rig r;
  r.ha.errorBody = "BODYSENTINEL token SECRET-TOKEN";
  r.start();
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.sessionDone(); }));
  EntityState req;
  req.id = "light.a";
  req.state = "on";
  r.drv.userAction(*intentFromRequest(config::WidgetType::Light, 0, {req}), r.now);
  TEST_ASSERT_TRUE(r.until([&] { return r.ha.serviceCalls == 1; }));
  r.ha.status = 500;
  req.state = "off";
  r.drv.userAction(*intentFromRequest(config::WidgetType::Light, 0, {req}), r.now);
  r.until([&] { return r.drv.status() != session::Status::Online; }, 5000);
  r.ha.status = 401;
  r.drv.refresh(r.now);
  r.until([&] { return r.drv.status() == session::Status::AuthError; }, 20000);
  r.ha.status = 0;
  r.drv.refresh(r.now);
  r.until([&] { return r.drv.sleeping(); }, 200000);
  TEST_ASSERT_TRUE(r.log.size() > 10);
  for (auto &l : r.log) {
    TEST_ASSERT_TRUE(l.rfind("APP: ", 0) == 0);
    TEST_ASSERT_TRUE(l.find("SECRET") == std::string::npos);
    TEST_ASSERT_TRUE(l.find("BODYSENTINEL") == std::string::npos);
    TEST_ASSERT_TRUE(l.find("ha.local") == std::string::npos);
    TEST_ASSERT_TRUE(l.find("8123") == std::string::npos);
    TEST_ASSERT_TRUE(l.find("Bearer") == std::string::npos);
    TEST_ASSERT_TRUE(l.find("\"") == std::string::npos);  // no JSON
  }
}

// The same explicit action twice (AppState normally drops the repeat while pending): at most two
// requests, both idempotent, and the entity ends in the requested state.
void test_double_tap_same_target_is_idempotent() {
  Rig r;
  r.start();
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.sessionDone(); }));
  EntityState req;
  req.id = "light.a";
  req.state = "on";
  auto ev = *intentFromRequest(config::WidgetType::Light, 0, {req});
  r.drv.userAction(ev, r.now);
  r.drv.userAction(ev, r.now);
  r.until([&] { return r.drv.marks().empty() && r.ha.serviceCalls >= 1; });
  r.until([&] { return false; }, 3000);
  TEST_ASSERT_TRUE(r.ha.serviceCalls >= 1 && r.ha.serviceCalls <= 2);
  TEST_ASSERT_EQUAL_STRING("on", r.store.get("light.a")->state.c_str());
}

// Tap on, then tap off before the first request went out: the end state is off, never left on.
void test_on_then_off_ends_off() {
  Rig r;
  r.start();
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.sessionDone(); }));
  EntityState req;
  req.id = "light.a";
  req.state = "on";
  r.drv.userAction(*intentFromRequest(config::WidgetType::Light, 0, {req}), r.now);
  req.state = "off";
  r.drv.userAction(*intentFromRequest(config::WidgetType::Light, 0, {req}), r.now);
  r.until([&] { return false; }, 5000);
  TEST_ASSERT_EQUAL_STRING("off", r.store.get("light.a")->state.c_str());
  TEST_ASSERT_EQUAL_STRING("off", r.ha.states["light.a"].c_str());
  TEST_ASSERT_TRUE(r.drv.marks().empty());
}

// A successful fetch while the wall clock is unset records no time (the label says "never"),
// and the next fetch with a valid clock records it.
void test_fetch_without_clock_then_with_clock() {
  Rig r;
  r.epoch = 0;
  r.start();
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.sessionDone(); }));
  TEST_ASSERT_FALSE(r.drv.lastFetchFailed());
  TEST_ASSERT_EQUAL(0, (int)r.drv.lastOkEpoch());
  r.epoch = 1767225600 + 3600 * 12;
  r.drv.refresh(r.now);
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.lastOkEpoch() == r.epoch; }));
}

// The clock becomes known after the OK fetch: the fetch time is derived from the monotonic age.
void test_last_ok_derived_when_clock_arrives() {
  Rig r;
  r.epoch = 0;
  r.start();
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.sessionDone(); }));
  TEST_ASSERT_EQUAL(0, (int)r.drv.lastOkEpoch());
  r.until([&] { return false; }, 30000);
  r.epoch = 1767225600 + 3600 * 12;
  r.drv.tick(r.now);
  const int64_t d = r.epoch - r.drv.lastOkEpoch();
  TEST_ASSERT_TRUE(d >= 29 && d <= 40);
}

// refresh() reports whether the machine started a fetch; ignored requests do not complete.
void test_refresh_reports_issue_and_seq() {
  Rig r;
  r.start();
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.sessionDone(); }));
  const uint32_t seq0 = r.drv.fetchSeq();
  TEST_ASSERT_TRUE(seq0 >= 1);
  TEST_ASSERT_TRUE(r.drv.refresh(r.now));
  TEST_ASSERT_EQUAL_UINT32(seq0, r.drv.fetchSeq());
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.fetchSeq() != seq0; }));
  // A second request while the first is in flight (or while connecting) must not claim a fetch.
  Rig q;
  q.start();
  TEST_ASSERT_FALSE(q.drv.refresh(q.now) && q.drv.refresh(q.now));
}

// Recovery: a failed session followed by a manual refresh reconnects and fetches again.
void test_offline_then_refresh_recovers() {
  Rig r;
  r.ha.status = 0;
  r.start();
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.status() == session::Status::Offline; }));
  r.ha.status = 200;
  r.drv.refresh(r.now);
  TEST_ASSERT_TRUE(r.until([&] { return r.drv.status() == session::Status::Online; }));
  TEST_ASSERT_FALSE(r.drv.lastFetchFailed());
  TEST_ASSERT_EQUAL(2, r.wifi.begins);
}

// Climate: the requested target is clamped to min/max and snapped to the step.
void test_intent_climate_clamps_and_snaps() {
  EntityState c;
  c.id = "climate.l";
  c.min_temp = 10; c.max_temp = 25; c.target_temp_step = 0.5;
  c.temperature = 35.0;
  auto e = intentFromRequest(config::WidgetType::Climate, 2, {c});
  TEST_ASSERT_TRUE(e.has_value());
  TEST_ASSERT_TRUE(e->custom_call.body.find("25") != std::string::npos);
  TEST_ASSERT_TRUE(e->custom_call.body.find("35") == std::string::npos);
  c.temperature = 21.3;
  e = intentFromRequest(config::WidgetType::Climate, 1, {c});
  TEST_ASSERT_TRUE(e->custom_call.body.find("21.5") != std::string::npos);
  c.temperature = 2.0;
  e = intentFromRequest(config::WidgetType::Climate, 1, {c});
  TEST_ASSERT_TRUE(e->custom_call.body.find("\"temperature\":10") != std::string::npos ||
                   e->custom_call.body.find("\"temperature\": 10") != std::string::npos);
}

// Stale-label rules without a usable clock.
void test_label_without_clock() {
  const int64_t ok = 1767225600 + 3600;
  Label l = refreshLabel(ok, 0, 0, 60, false, false);  // clock unknown: no age, no day change
  TEST_ASSERT_FALSE(l.stale);
  TEST_ASSERT_EQUAL_STRING("updated 01:00", l.line1.c_str());
  l = refreshLabel(ok, 0, 0, 60, true, false);  // failed fetch is stale regardless
  TEST_ASSERT_TRUE(l.stale);
  l = refreshLabel(0, 0, 0, 60, true, false);
  TEST_ASSERT_TRUE(l.stale);
  l = refreshLabel(0, ok, 0, 60, false, false);  // never fetched, no failure: not stale, not a time
  TEST_ASSERT_FALSE(l.stale);
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
  RUN_TEST(test_log_has_no_secrets_all_paths);
  RUN_TEST(test_double_tap_same_target_is_idempotent);
  RUN_TEST(test_on_then_off_ends_off);
  RUN_TEST(test_fetch_without_clock_then_with_clock);
  RUN_TEST(test_offline_then_refresh_recovers);
  RUN_TEST(test_intent_climate_clamps_and_snaps);
  RUN_TEST(test_label_without_clock);
  RUN_TEST(test_external_link_flag);
  RUN_TEST(test_intent_toggles_and_actions);
  RUN_TEST(test_intent_climate_custom);
  RUN_TEST(test_last_ok_derived_when_clock_arrives);
  RUN_TEST(test_refresh_reports_issue_and_seq);
  RUN_TEST(test_civil_roundtrip);
  RUN_TEST(test_label);
  RUN_TEST(test_mailbox);
  return UNITY_END();
}
