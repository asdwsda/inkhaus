#include <unity.h>

#include <algorithm>
#include <vector>

#include "session.h"

using namespace session;
using C = Command::Type;
using power::WakeReason;

namespace {

// Fake driver: owns the clock, feeds events, and keeps every command emitted so far.
struct Driver {
  SessionMachine m;
  int64_t now = 1000;
  std::vector<Command> all;
  settings::Settings st;

  std::vector<Command> send(const Event& e) {
    auto cmds = m.handle(e, now);
    all.insert(all.end(), cmds.begin(), cmds.end());
    return cmds;
  }
  std::vector<Command> advance(int64_t ms) {
    now += ms;
    return send(Event::tick());
  }
  std::vector<Command> start(WakeReason r = WakeReason::Button) {
    return send(Event::start(r, 4000, st, 1767225600 + 1800, 0));
  }
};

int count(const std::vector<Command>& v, C t) {
  return (int)std::count_if(v.begin(), v.end(), [&](const Command& c) { return c.type == t; });
}
const Command* find(const std::vector<Command>& v, C t) {
  for (const Command& c : v)
    if (c.type == t) return &c;
  return nullptr;
}
bool has_mark(const std::vector<Command>& v, Mark m, const std::string& id) {
  for (const Command& c : v)
    if (c.type == C::MarkWidgets && c.mark == m &&
        std::find(c.ids.begin(), c.ids.end(), id) != c.ids.end())
      return true;
  return false;
}
EntityState es(const std::string& id, const std::string& state) {
  EntityState s;
  s.id = id;
  s.state = state;
  s.available = true;
  return s;
}

// Brings a driver to "interactive, online, first fetch done".
void online(Driver& d) {
  auto c = d.start();
  TEST_ASSERT_EQUAL(1, count(c, C::ConnectWifi));
  c = d.send(Event::wifi(true));
  TEST_ASSERT_EQUAL(1, count(c, C::FetchStates));
  d.send(Event::fetch_done(HaError::None, {"light.a"}, {es("light.a", "off")}));
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_happy_path_interactive() {
  Driver d;
  auto c = d.start();
  TEST_ASSERT_EQUAL(C::ShowStatus, c[0].type);
  TEST_ASSERT_EQUAL((int)Status::Connecting, (int)c[0].status);
  TEST_ASSERT_EQUAL(C::ConnectWifi, c[1].type);

  d.now += 800;
  c = d.send(Event::wifi(true));
  TEST_ASSERT_EQUAL(1, count(c, C::FetchStates));
  TEST_ASSERT_TRUE(find(c, C::FetchStates)->ids.empty());

  d.now += 300;
  c = d.send(Event::fetch_done(HaError::None, {"light.a"}, {es("light.a", "off")}));
  TEST_ASSERT_EQUAL((int)Status::Online, (int)find(c, C::ShowStatus)->status);
  TEST_ASSERT_EQUAL(1, count(c, C::Render));

  // tap: pending + render, call goes out at once
  d.now += 2000;
  c = d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Pending, "light.a"));
  TEST_ASSERT_EQUAL(1, count(c, C::Render));
  const Command* call = find(c, C::CallService);
  TEST_ASSERT_NOT_NULL(call);
  TEST_ASSERT_EQUAL((int)Action::LightToggle, (int)call->action);
  int ticket = call->ticket;

  d.now += 200;
  c = d.send(Event::service_done(ticket, HaError::None));
  TEST_ASSERT_EQUAL(0, count(c, C::FetchStates));  // 500 ms delay first
  c = d.advance(499);
  TEST_ASSERT_EQUAL(0, count(c, C::FetchStates));
  c = d.advance(1);
  TEST_ASSERT_EQUAL(1, count(c, C::FetchStates));
  TEST_ASSERT_EQUAL_STRING("light.a", find(c, C::FetchStates)->ids[0].c_str());

  c = d.send(Event::fetch_done(HaError::None, {"light.a"}, {es("light.a", "on")}));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Confirmed, "light.a"));
  TEST_ASSERT_EQUAL(1, count(c, C::Render));
  TEST_ASSERT_EQUAL(0, count(c, C::Sleep));

  // idle: 20 s after the tap (default idle timeout), not before
  c = d.advance(19000);
  TEST_ASSERT_EQUAL(0, count(c, C::Sleep));
  c = d.advance(1500);
  TEST_ASSERT_EQUAL(1, count(c, C::Sleep));
  const Command* sl = find(c, C::Sleep);
  TEST_ASSERT_EQUAL((int)power::Session::Interactive, (int)sl->session);
  TEST_ASSERT_TRUE(sl->plan.rtc_alarm_at.has_value());
  TEST_ASSERT_TRUE(d.m.sleeping());
  TEST_ASSERT_EQUAL(0, (int)d.advance(1000).size());
}

void test_input_postpones_sleep() {
  Driver d;
  online(d);
  d.advance(15000);
  d.send(Event::input());
  auto c = d.advance(15000);
  TEST_ASSERT_EQUAL(0, count(c, C::Sleep));
  c = d.advance(6000);
  TEST_ASSERT_EQUAL(1, count(c, C::Sleep));
}

void test_taps_before_wifi_are_queued() {
  Driver d;
  d.start();
  auto c = d.send(Event::user_action({"cover.x"}, Action::CoverOpen, "open"));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Pending, "cover.x"));
  TEST_ASSERT_EQUAL(0, count(c, C::CallService));
  c = d.send(Event::user_action({"light.a"}, Action::LightTurnOn, "on"));
  TEST_ASSERT_EQUAL(2, (int)d.m.queued());
  c = d.send(Event::wifi(true));
  TEST_ASSERT_EQUAL(0, count(c, C::FetchStates));  // actions first, fetch later
  const Command* call = find(c, C::CallService);
  TEST_ASSERT_NOT_NULL(call);
  TEST_ASSERT_EQUAL((int)Action::CoverOpen, (int)call->action);  // order of first tap
  c = d.send(Event::service_done(call->ticket, HaError::None));
  call = find(c, C::CallService);
  TEST_ASSERT_NOT_NULL(call);
  TEST_ASSERT_EQUAL((int)Action::LightTurnOn, (int)call->action);
  c = d.send(Event::service_done(call->ticket, HaError::None));
  TEST_ASSERT_EQUAL(0, count(c, C::CallService));
  TEST_ASSERT_EQUAL(1, count(c, C::FetchStates));  // initial full fetch after the actions
  TEST_ASSERT_TRUE(find(c, C::FetchStates)->ids.empty());
}

void test_fetch_all_after_actions_confirms_matching_states() {
  Driver d;
  d.start();
  d.send(Event::user_action({"light.a"}, Action::LightTurnOn, "on"));
  auto c = d.send(Event::wifi(true));
  c = d.send(Event::service_done(find(c, C::CallService)->ticket, HaError::None));
  c = d.send(Event::fetch_done(HaError::None, {"light.a"}, {es("light.a", "on")}));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Confirmed, "light.a"));
  d.advance(1000);
  for (const Command& x : d.all)
    if (x.type == C::FetchStates && !x.ids.empty()) TEST_FAIL_MESSAGE("extra confirm fetch");
}

void test_fetch_all_after_actions_mismatch_still_confirms_later() {
  Driver d;
  d.start();
  d.send(Event::user_action({"light.a"}, Action::LightTurnOn, "on"));
  auto c = d.send(Event::wifi(true));
  c = d.send(Event::service_done(find(c, C::CallService)->ticket, HaError::None));
  c = d.send(Event::fetch_done(HaError::None, {}, {es("light.a", "off")}));
  TEST_ASSERT_FALSE(has_mark(c, Mark::Confirmed, "light.a"));
  c = d.advance(600);
  const Command* f = find(c, C::FetchStates);
  TEST_ASSERT_NOT_NULL(f);
  TEST_ASSERT_EQUAL(1, (int)f->ids.size());
  c = d.send(Event::fetch_done(HaError::None, {"light.a"}, {es("light.a", "on")}));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Confirmed, "light.a"));
}

void test_explicit_on_then_off_before_wifi_sends_nothing() {
  Driver d;
  d.start();
  d.send(Event::user_action({"light.a"}, Action::LightTurnOn, "on"));
  auto c = d.send(Event::user_action({"light.a"}, Action::LightTurnOff, "off"));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Confirmed, "light.a"));  // pending cleared
  TEST_ASSERT_EQUAL(0, (int)d.m.queued());
  c = d.send(Event::wifi(true));
  TEST_ASSERT_EQUAL(1, count(c, C::FetchStates));
  d.send(Event::fetch_done(HaError::None));
  for (const Command& x : d.all) TEST_ASSERT_NOT_EQUAL(C::CallService, x.type);
}

void test_explicit_on_off_on_sends_turn_on() {
  Driver d;
  d.start();
  d.send(Event::user_action({"switch.a"}, Action::SwitchTurnOn, "on"));
  d.send(Event::user_action({"switch.a"}, Action::SwitchTurnOff, "off"));
  d.send(Event::user_action({"switch.a"}, Action::SwitchTurnOn, "on"));
  TEST_ASSERT_EQUAL(1, (int)d.m.queued());
  auto c = d.send(Event::wifi(true));
  TEST_ASSERT_EQUAL((int)Action::SwitchTurnOn, (int)find(c, C::CallService)->action);
}

void test_explicit_repeat_same_action_is_one_call() {
  Driver d;
  d.start();
  d.send(Event::user_action({"light.a"}, Action::LightTurnOn, "on"));
  d.send(Event::user_action({"light.a"}, Action::LightTurnOn, "on"));
  TEST_ASSERT_EQUAL(1, (int)d.m.queued());
}

void test_toggle_twice_sends_nothing() {
  Driver d;
  d.start();
  d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
  auto c = d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Confirmed, "light.a"));  // pending cleared
  TEST_ASSERT_EQUAL(0, (int)d.m.queued());
  d.send(Event::wifi(true));
  c = d.send(Event::fetch_done(HaError::None));
  TEST_ASSERT_EQUAL(0, count(c, C::CallService));
  for (const Command& x : d.all) TEST_ASSERT_NOT_EQUAL(C::CallService, x.type);
}

void test_last_action_wins_keeps_order_and_groups() {
  Driver d;
  d.start();
  d.send(Event::user_action({"cover.a"}, Action::CoverOpen, "open"));
  d.send(Event::user_action({"cover.b"}, Action::CoverOpen, "open"));
  d.send(Event::user_action({"cover.a"}, Action::CoverClose, "closed"));  // replaces, keeps slot
  auto c = d.send(Event::wifi(true));
  const Command* call = find(c, C::CallService);
  TEST_ASSERT_NOT_NULL(call);
  TEST_ASSERT_EQUAL((int)Action::CoverClose, (int)call->action);
  TEST_ASSERT_EQUAL(1, (int)call->ids.size());
  TEST_ASSERT_EQUAL_STRING("cover.a", call->ids[0].c_str());
}

void test_adjacent_same_actions_form_one_call() {
  Driver d;
  d.start();
  d.send(Event::user_action({"cover.a", "cover.b"}, Action::CoverOpen, "open"));
  auto c = d.send(Event::wifi(true));
  const Command* call = find(c, C::CallService);
  TEST_ASSERT_EQUAL(2, (int)call->ids.size());
}

void test_custom_call_is_sent() {
  Driver d;
  online(d);
  ServiceCall sc{"climate", "set_temperature", "{}"};
  auto c = d.send(Event::user_custom({"climate.x"}, sc, ""));
  const Command* call = find(c, C::CallService);
  TEST_ASSERT_NOT_NULL(call);
  TEST_ASSERT_TRUE(call->custom);
  TEST_ASSERT_EQUAL_STRING("set_temperature", call->custom_call.service.c_str());
}

void test_confirmation_retry_then_ok() {
  Driver d;
  online(d);
  auto c = d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
  d.send(Event::service_done(find(c, C::CallService)->ticket, HaError::None));
  c = d.advance(500);
  TEST_ASSERT_EQUAL(1, count(c, C::FetchStates));
  c = d.send(Event::fetch_done(HaError::None, {}, {es("light.a", "off")}));
  TEST_ASSERT_EQUAL(0, count(c, C::MarkWidgets));
  c = d.advance(999);
  TEST_ASSERT_EQUAL(0, count(c, C::FetchStates));
  c = d.advance(1);
  TEST_ASSERT_EQUAL(1, count(c, C::FetchStates));
  c = d.send(Event::fetch_done(HaError::None, {"light.a"}, {es("light.a", "on")}));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Confirmed, "light.a"));
}

void test_confirmation_fails_after_two_retries() {
  Driver d;
  online(d);
  auto c = d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
  d.send(Event::service_done(find(c, C::CallService)->ticket, HaError::None));
  int fetches = 0;
  bool failed = false;
  for (int i = 0; i < 10 && !failed; ++i) {
    c = d.advance(1000);
    if (count(c, C::FetchStates)) {
      ++fetches;
      c = d.send(Event::fetch_done(HaError::None, {}, {es("light.a", "off")}));
      failed = has_mark(c, Mark::Failed, "light.a");
      if (failed) TEST_ASSERT_EQUAL(1, count(c, C::Render));
    }
  }
  TEST_ASSERT_TRUE(failed);
  TEST_ASSERT_EQUAL(3, fetches);  // first + two retries
}

void test_service_call_http_error_marks_failed() {
  Driver d;
  online(d);
  auto c = d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
  c = d.send(Event::service_done(find(c, C::CallService)->ticket, HaError::Http));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Failed, "light.a"));
  TEST_ASSERT_EQUAL((int)Status::Online, (int)d.m.status());
}

void test_wifi_failure_offline_and_sleeps() {
  Driver d;
  d.start();
  d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
  auto c = d.send(Event::wifi(false));
  TEST_ASSERT_EQUAL((int)Status::Offline, (int)find(c, C::ShowStatus)->status);
  TEST_ASSERT_TRUE(has_mark(c, Mark::Failed, "light.a"));
  TEST_ASSERT_EQUAL(1, count(c, C::Render));
  TEST_ASSERT_EQUAL(0, count(c, C::FetchStates));
  c = d.advance(21000);
  TEST_ASSERT_EQUAL(1, count(c, C::Sleep));
}

void test_fetch_network_error_goes_offline() {
  Driver d;
  d.start();
  d.send(Event::wifi(true));
  auto c = d.send(Event::fetch_done(HaError::Network));
  TEST_ASSERT_EQUAL((int)Status::Offline, (int)find(c, C::ShowStatus)->status);
  TEST_ASSERT_EQUAL(0, count(c, C::FetchStates));  // no retry here (ha_client did it)
}

void test_auth_error_no_retry() {
  Driver d;
  d.start();
  d.send(Event::wifi(true));
  auto c = d.send(Event::fetch_done(HaError::Auth));
  TEST_ASSERT_EQUAL((int)Status::AuthError, (int)find(c, C::ShowStatus)->status);
  TEST_ASSERT_EQUAL(0, count(c, C::FetchStates));
  c = d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Failed, "light.a"));
  TEST_ASSERT_EQUAL(0, count(c, C::CallService));
  c = d.advance(21000);
  TEST_ASSERT_EQUAL(1, count(c, C::Sleep));
}

void test_auth_error_on_service_call() {
  Driver d;
  online(d);
  auto c = d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
  c = d.send(Event::service_done(find(c, C::CallService)->ticket, HaError::Auth));
  TEST_ASSERT_EQUAL((int)Status::AuthError, (int)find(c, C::ShowStatus)->status);
  TEST_ASSERT_TRUE(has_mark(c, Mark::Failed, "light.a"));
}

void test_scheduled_no_change_no_render() {
  Driver d;
  auto c = d.start(WakeReason::RtcAlarm);
  TEST_ASSERT_EQUAL(1, count(c, C::ConnectWifi));
  d.send(Event::wifi(true));
  c = d.send(Event::fetch_done(HaError::None));
  TEST_ASSERT_EQUAL(0, count(c, C::Render));
  TEST_ASSERT_EQUAL(1, count(c, C::Sleep));
  const Command* sl = find(c, C::Sleep);
  TEST_ASSERT_EQUAL((int)power::Session::ScheduledRefresh, (int)sl->session);
  TEST_ASSERT_TRUE(sl->plan.rtc_alarm_at.has_value());
}

void test_scheduled_change_renders_before_sleep() {
  Driver d;
  d.start(WakeReason::RtcAlarm);
  d.send(Event::wifi(true));
  auto c = d.send(Event::fetch_done(HaError::None, {"light.a"}, {es("light.a", "on")}));
  TEST_ASSERT_EQUAL(1, count(c, C::Render));
  TEST_ASSERT_EQUAL(C::Render, c[c.size() - 2].type);
  TEST_ASSERT_EQUAL(C::Sleep, c.back().type);
}

void test_scheduled_ignores_input_and_sleeps_when_offline() {
  Driver d;
  d.start(WakeReason::RtcAlarm);
  auto c = d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
  TEST_ASSERT_EQUAL(0, (int)c.size());
  c = d.send(Event::wifi(false));
  TEST_ASSERT_EQUAL(0, count(c, C::Render));
  TEST_ASSERT_EQUAL(1, count(c, C::Sleep));
}

void test_low_battery_sleeps_at_once() {
  Driver d;
  auto c = d.send(Event::start(WakeReason::Button, 3200, d.st));
  TEST_ASSERT_EQUAL(1, count(c, C::Sleep));
  TEST_ASSERT_EQUAL(0, count(c, C::ConnectWifi));
  TEST_ASSERT_EQUAL((int)power::Session::LowBattery, (int)find(c, C::Sleep)->session);
}

void test_hard_cap_with_hanging_request() {
  Driver d;
  d.start();
  d.send(Event::wifi(true));  // fetch in flight, never answered
  d.send(Event::input());
  auto c = d.advance(60000);
  TEST_ASSERT_EQUAL(0, count(c, C::Sleep));
  c = d.advance(59000);
  TEST_ASSERT_EQUAL(0, count(c, C::Sleep));
  c = d.advance(2000);  // 121 s since the last input
  TEST_ASSERT_EQUAL(1, count(c, C::Sleep));
}

void test_hard_cap_scheduled_hang() {
  Driver d;
  d.start(WakeReason::RtcAlarm);  // Wi-Fi connect never answers
  auto c = d.advance(119000);
  TEST_ASSERT_EQUAL(0, count(c, C::Sleep));
  c = d.advance(2000);
  TEST_ASSERT_EQUAL(1, count(c, C::Sleep));
}

void test_busy_blocks_idle_sleep_until_done() {
  Driver d;
  online(d);
  auto c = d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
  d.send(Event::service_done(find(c, C::CallService)->ticket, HaError::None));
  d.advance(500);  // confirm fetch in flight, unanswered
  c = d.advance(30000);  // past idle timeout, below hard cap
  TEST_ASSERT_EQUAL(0, count(c, C::Sleep));
}


// ---- tester additions (task 018) -------------------------------------------------------

namespace {
int outstanding_delta(const std::vector<Command>& v) {
  return count(v, C::ConnectWifi) + count(v, C::FetchStates) + count(v, C::CallService);
}
}  // namespace

void test_stray_events_are_ignored() {
  Driver d;
  d.start();
  // fetch result / service result / wifi(ok) with nothing matching in flight
  auto c = d.send(Event::fetch_done(HaError::None, {"light.a"}, {es("light.a", "on")}));
  TEST_ASSERT_EQUAL(0, count(c, C::Render));
  TEST_ASSERT_EQUAL(0, outstanding_delta(c));
  c = d.send(Event::service_done(7, HaError::None));
  TEST_ASSERT_EQUAL(0, outstanding_delta(c));
  d.send(Event::wifi(true));  // fetch now in flight
  c = d.send(Event::wifi(true));  // duplicate
  TEST_ASSERT_EQUAL(0, outstanding_delta(c));
  d.send(Event::fetch_done(HaError::None, {"light.a"}, {es("light.a", "off")}));
  c = d.send(Event::fetch_done(HaError::None, {"light.a"}));  // duplicate
  TEST_ASSERT_EQUAL(0, outstanding_delta(c));
}

void test_duplicate_and_wrong_ticket_service_done() {
  Driver d;
  online(d);
  auto c = d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
  int t = find(c, C::CallService)->ticket;
  c = d.send(Event::service_done(t + 99, HaError::None));  // wrong ticket: ignored
  TEST_ASSERT_EQUAL(0, outstanding_delta(c));
  d.send(Event::service_done(t, HaError::None));
  c = d.send(Event::service_done(t, HaError::Network));  // duplicate: must not go offline
  TEST_ASSERT_TRUE(d.m.status() == Status::Online);
  TEST_ASSERT_EQUAL(0, count(c, C::ShowStatus));
}

void test_wifi_ok_after_offline_does_nothing() {
  Driver d;
  d.start();
  d.send(Event::wifi(false));
  TEST_ASSERT_TRUE(d.m.status() == Status::Offline);
  auto c = d.send(Event::wifi(true));
  TEST_ASSERT_EQUAL(0, outstanding_delta(c));
  c = d.advance(100);
  TEST_ASSERT_EQUAL(0, outstanding_delta(c));
  TEST_ASSERT_TRUE(d.m.status() == Status::Offline);
}

void test_nothing_emitted_after_sleep() {
  Driver d;
  d.start(WakeReason::RtcAlarm);
  d.send(Event::wifi(true));
  d.send(Event::fetch_done(HaError::None));
  TEST_ASSERT_TRUE(d.m.sleeping());
  size_t n = d.all.size();
  d.send(Event::user_action({"light.a"}, Action::LightToggle));
  d.send(Event::input());
  d.send(Event::wifi(true));
  d.send(Event::fetch_done(HaError::None, {"x"}));
  d.send(Event::service_done(1, HaError::None));
  d.send(Event::start(WakeReason::Button, 4000, d.st));
  d.advance(500000);
  TEST_ASSERT_EQUAL((int)n, (int)d.all.size());

  Driver e;  // interactive: sleep via idle
  online(e);
  e.advance(25000);
  TEST_ASSERT_TRUE(e.m.sleeping());
  n = e.all.size();
  e.send(Event::user_action({"light.a"}, Action::LightToggle));
  e.advance(1000);
  TEST_ASSERT_EQUAL((int)n, (int)e.all.size());
  TEST_ASSERT_EQUAL(1, count(e.all, C::Sleep));
}

void test_events_before_start_are_ignored() {
  Driver d;
  auto c = d.send(Event::user_action({"light.a"}, Action::LightToggle));
  TEST_ASSERT_EQUAL(0, (int)c.size());
  c = d.advance(1000);
  TEST_ASSERT_EQUAL(0, (int)c.size());
  c = d.send(Event::wifi(true));
  TEST_ASSERT_EQUAL(0, (int)c.size());
  c = d.start();
  TEST_ASSERT_EQUAL(1, count(c, C::ConnectWifi));
  TEST_ASSERT_EQUAL(0, (int)d.m.queued());
}

void test_second_start_is_ignored() {
  Driver d;
  d.start();
  auto c = d.start();
  TEST_ASSERT_EQUAL(0, (int)c.size());
}

void test_clock_going_backwards_does_not_crash_or_sleep_early() {
  Driver d;
  online(d);
  d.advance(10000);
  d.now -= 50000;  // jumps back before the session start
  auto c = d.advance(0);
  TEST_ASSERT_EQUAL(0, count(c, C::Sleep));
  c = d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
  TEST_ASSERT_EQUAL(1, count(c, C::CallService));
  // eventually still sleeps once time moves forward past the cap
  d.send(Event::service_done(find(c, C::CallService)->ticket, HaError::Network));
  c = d.advance(200000);
  TEST_ASSERT_TRUE(d.m.sleeping());
}

void test_huge_clock_jump_sleeps_with_sane_plan() {
  Driver d;
  d.start();
  d.send(Event::wifi(true));
  auto c = d.advance(int64_t(10) * 365 * 24 * 3600 * 1000);
  const Command* s = find(c, C::Sleep);
  TEST_ASSERT_NOT_NULL(s);
  if (s->plan.rtc_alarm_at) TEST_ASSERT_TRUE(*s->plan.rtc_alarm_at > 0);
}

void test_tap_storm_before_wifi() {
  Driver d;
  d.start();
  std::vector<std::string> ids;
  for (int i = 0; i < 20; i++) ids.push_back("light.e" + std::to_string(i));
  for (int round = 0; round < 5; round++)
    for (const auto& id : ids) d.send(Event::user_action({id}, Action::LightToggle, "on"));
  // 5 toggles per entity: odd -> one left per entity
  TEST_ASSERT_EQUAL(20, (int)d.m.queued());
  TEST_ASSERT_EQUAL(0, count(d.all, C::CallService));
  d.send(Event::wifi(true));
  d.send(Event::fetch_done(HaError::None));
  std::vector<std::string> sent;
  // collect from the log
  int calls = 0;
  for (const Command& c : d.all)
    if (c.type == C::CallService) {
      calls++;
      for (auto& id : c.ids) sent.push_back(id);
    }
  TEST_ASSERT_EQUAL(1, calls);  // identical actions grouped into one call
  TEST_ASSERT_EQUAL(20, (int)sent.size());
  for (int i = 0; i < 20; i++) TEST_ASSERT_EQUAL_STRING(ids[i].c_str(), sent[i].c_str());
}

void test_tap_storm_hundred_taps_even_cancels_all() {
  Driver d;
  d.start();
  for (int i = 0; i < 100; i++)
    d.send(Event::user_action({"light.e" + std::to_string(i % 20)}, Action::LightToggle));
  // each entity tapped 5 times -> odd -> 20 queued
  TEST_ASSERT_EQUAL(20, (int)d.m.queued());
  for (int i = 0; i < 100; i++)
    d.send(Event::user_action({"light.e" + std::to_string(i % 20)}, Action::LightToggle));
  TEST_ASSERT_EQUAL(0, (int)d.m.queued());  // 10 toggles each: all cancelled
}

void test_hard_cap_in_every_state() {
  // Wi-Fi connecting (interactive)
  {
    Driver d; d.start();
    TEST_ASSERT_EQUAL(1, count(d.advance(121000), C::Sleep));
  }
  // initial fetch hanging
  {
    Driver d; d.start(); d.send(Event::wifi(true));
    TEST_ASSERT_EQUAL(1, count(d.advance(121000), C::Sleep));
  }
  // service call hanging
  {
    Driver d; online(d);
    d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
    TEST_ASSERT_EQUAL(1, count(d.advance(121000), C::Sleep));
  }
  // confirmation fetch hanging
  {
    Driver d; online(d);
    auto c = d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
    d.send(Event::service_done(find(c, C::CallService)->ticket, HaError::None));
    d.advance(500);
    TEST_ASSERT_EQUAL(1, count(d.advance(121000), C::Sleep));
  }
  // taps queued, Wi-Fi never up
  {
    Driver d; d.start();
    d.send(Event::user_action({"light.a"}, Action::LightToggle));
    TEST_ASSERT_EQUAL(1, count(d.advance(121000), C::Sleep));
  }
  // scheduled: fetch hanging
  {
    Driver d; d.start(WakeReason::RtcAlarm); d.send(Event::wifi(true));
    TEST_ASSERT_EQUAL(1, count(d.advance(121000), C::Sleep));
  }
  // dead (auth) with a stuck queue cleared: sleeps by idle or cap
  {
    Driver d; online(d);
    d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
    d.send(Event::service_done(1, HaError::Auth));
    TEST_ASSERT_EQUAL(1, count(d.advance(121000), C::Sleep));
  }
}

void test_hard_cap_with_continuous_input_is_not_hit() {
  Driver d;
  online(d);
  for (int i = 0; i < 20; i++) {
    d.advance(10000);
    d.send(Event::input());
  }
  TEST_ASSERT_FALSE(d.m.sleeping());
}

void test_auth_error_on_fetch_no_retry_and_marks_queue_failed() {
  Driver d;
  d.start();
  d.send(Event::wifi(true));  // full fetch in flight
  d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));  // queued behind it
  auto c = d.send(Event::fetch_done(HaError::Auth));
  TEST_ASSERT_TRUE(d.m.status() == Status::AuthError);
  TEST_ASSERT_EQUAL(0, outstanding_delta(c));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Failed, "light.a"));
  c = d.advance(5000);
  TEST_ASSERT_EQUAL(0, outstanding_delta(c));
}

void test_user_action_after_offline_marks_failed_sends_nothing() {
  Driver d;
  d.start();
  d.send(Event::wifi(false));
  auto c = d.send(Event::user_action({"light.a"}, Action::LightToggle));
  TEST_ASSERT_EQUAL(0, outstanding_delta(c));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Failed, "light.a"));
  TEST_ASSERT_EQUAL(0, (int)d.m.queued());
}

void test_action_during_confirm_supersedes_it() {
  Driver d;
  online(d);
  auto c = d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
  d.send(Event::service_done(find(c, C::CallService)->ticket, HaError::None));
  c = d.send(Event::user_action({"light.a"}, Action::LightToggle, "off"));
  // new call goes out; the old confirmation must not report failure for it
  TEST_ASSERT_EQUAL(1, count(c, C::CallService));
  int t = find(c, C::CallService)->ticket;
  d.send(Event::service_done(t, HaError::None));
  c = d.advance(500);
  TEST_ASSERT_EQUAL(1, count(c, C::FetchStates));
  c = d.send(Event::fetch_done(HaError::None, {"light.a"}, {es("light.a", "off")}));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Confirmed, "light.a"));
  TEST_ASSERT_FALSE(has_mark(d.all, Mark::Failed, "light.a"));
}


// ---- review additions for task 019 ----

// Tap on, the call is in flight, tap off: the stale "on" confirmation must not fail the entity.
void test_t019_opposite_tap_while_first_call_in_flight() {
  Driver d;
  online(d);
  auto c = d.send(Event::user_action({"light.a"}, Action::LightTurnOn, "on"));
  int t1 = find(c, C::CallService)->ticket;
  c = d.send(Event::user_action({"light.a"}, Action::LightTurnOff, "off"));
  TEST_ASSERT_EQUAL(0, count(c, C::CallService));  // one request at a time
  c = d.send(Event::service_done(t1, HaError::None));
  TEST_ASSERT_EQUAL(1, count(c, C::CallService));
  int t2 = find(c, C::CallService)->ticket;
  d.send(Event::service_done(t2, HaError::None));
  d.advance(500);
  d.send(Event::fetch_done(HaError::None, {"light.a"}, {es("light.a", "off")}));
  for (int i = 0; i < 6; i++) {
    d.advance(1000);
    d.send(Event::fetch_done(HaError::None, {}, {es("light.a", "off")}));
  }
  TEST_ASSERT_TRUE(has_mark(d.all, Mark::Confirmed, "light.a"));
  TEST_ASSERT_FALSE(has_mark(d.all, Mark::Failed, "light.a"));
}

// Mixed toggle and explicit actions on one entity: last wins, nothing crashes, no cancel.
void test_t019_toggle_then_explicit_last_wins() {
  Driver d;
  d.start();
  d.send(Event::user_action({"light.a"}, Action::LightToggle, "on"));
  d.send(Event::user_action({"light.a"}, Action::LightTurnOff, "off"));
  auto c = d.send(Event::wifi(true));
  const Command* call = find(c, C::CallService);
  TEST_ASSERT_NOT_NULL(call);
  TEST_ASSERT_EQUAL((int)Action::LightTurnOff, (int)call->action);
}

// Three taps before Wi-Fi: on, off, on -> a single TurnOn.
void test_t019_on_off_on_sends_one_turn_on() {
  Driver d;
  d.start();
  d.send(Event::user_action({"light.a"}, Action::LightTurnOn, "on"));
  d.send(Event::user_action({"light.a"}, Action::LightTurnOff, "off"));
  d.send(Event::user_action({"light.a"}, Action::LightTurnOn, "on"));
  auto c = d.send(Event::wifi(true));
  TEST_ASSERT_EQUAL(1, count(c, C::CallService));
  TEST_ASSERT_EQUAL((int)Action::LightTurnOn, (int)find(c, C::CallService)->action);
  TEST_ASSERT_EQUAL(0, count(c, C::FetchStates));  // fetch follows after the call
}

// on then off before Wi-Fi: nothing sent, but the initial fetch still happens.
void test_t019_on_off_before_wifi_only_fetches() {
  Driver d;
  d.start();
  d.send(Event::user_action({"light.a"}, Action::SwitchTurnOn, "on"));
  d.send(Event::user_action({"light.a"}, Action::SwitchTurnOff, "off"));
  auto c = d.send(Event::wifi(true));
  TEST_ASSERT_EQUAL(0, count(c, C::CallService));
  TEST_ASSERT_EQUAL(1, count(c, C::FetchStates));
}

// Different action classes on one id (light on vs switch off) do not cancel.
void test_t019_cross_domain_does_not_cancel() {
  Driver d;
  d.start();
  d.send(Event::user_action({"x.a"}, Action::LightTurnOn, "on"));
  d.send(Event::user_action({"x.a"}, Action::SwitchTurnOff, "off"));
  auto c = d.send(Event::wifi(true));
  TEST_ASSERT_EQUAL(1, count(c, C::CallService));
}

// Automation intent: on/off cancel too.
void test_t019_automation_cancel() {
  Driver d;
  d.start();
  d.send(Event::user_action({"automation.a"}, Action::AutomationTurnOff, "off"));
  d.send(Event::user_action({"automation.a"}, Action::AutomationTurnOn, "on"));
  auto c = d.send(Event::wifi(true));
  TEST_ASSERT_EQUAL(0, count(c, C::CallService));
}

// Group: members cancel individually; a member added later stays.
void test_t019_group_partial_cancel() {
  Driver d;
  d.start();
  d.send(Event::user_action({"light.a", "light.b"}, Action::LightTurnOn, "on"));
  d.send(Event::user_action({"light.a"}, Action::LightTurnOff, "off"));
  auto c = d.send(Event::wifi(true));
  TEST_ASSERT_EQUAL(1, count(c, C::CallService));
  const Command* call = find(c, C::CallService);
  TEST_ASSERT_EQUAL(1, (int)call->ids.size());
  TEST_ASSERT_EQUAL_STRING("light.b", call->ids[0].c_str());
}

// ServiceDone before the first FetchDone: confirmation by the initial fetch, no failure.
void test_t019_service_done_before_first_fetch_confirmed_by_fetch() {
  Driver d;
  d.start();
  d.send(Event::user_action({"light.a"}, Action::LightTurnOn, "on"));
  auto c = d.send(Event::wifi(true));
  int t = find(c, C::CallService)->ticket;
  c = d.send(Event::service_done(t, HaError::None));
  TEST_ASSERT_EQUAL(1, count(c, C::FetchStates));  // initial fetch (all)
  TEST_ASSERT_TRUE(find(c, C::FetchStates)->ids.empty());
  c = d.send(Event::fetch_done(HaError::None, {"light.a"}, {es("light.a", "on")}));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Confirmed, "light.a"));
  d.advance(5000);
  TEST_ASSERT_FALSE(has_mark(d.all, Mark::Failed, "light.a"));
}

// Initial fetch shows the old state (HA not yet updated): not confirmed, retried by id.
void test_t019_initial_fetch_stale_state_retries() {
  Driver d;
  d.start();
  d.send(Event::user_action({"light.a"}, Action::LightTurnOn, "on"));
  auto c = d.send(Event::wifi(true));
  d.send(Event::service_done(find(c, C::CallService)->ticket, HaError::None));
  c = d.send(Event::fetch_done(HaError::None, {"light.a"}, {es("light.a", "off")}));
  TEST_ASSERT_FALSE(has_mark(c, Mark::Confirmed, "light.a"));
  c = d.advance(600);
  TEST_ASSERT_EQUAL(1, count(c, C::FetchStates));
  TEST_ASSERT_EQUAL(1, (int)find(c, C::FetchStates)->ids.size());
  c = d.send(Event::fetch_done(HaError::None, {"light.a"}, {es("light.a", "on")}));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Confirmed, "light.a"));
}

// Tap arrives while the initial fetch is in flight: sent right after it, before any confirm.
void test_t019_tap_during_initial_fetch() {
  Driver d;
  d.start();
  auto c = d.send(Event::wifi(true));
  TEST_ASSERT_EQUAL(1, count(c, C::FetchStates));
  c = d.send(Event::user_action({"light.a"}, Action::LightTurnOn, "on"));
  TEST_ASSERT_EQUAL(0, count(c, C::CallService));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Pending, "light.a"));
  c = d.send(Event::fetch_done(HaError::None, {"light.a"}, {es("light.a", "on")}));
  // initial fetch already shows "on", but the call is still sent (idempotent)
  TEST_ASSERT_EQUAL(1, count(c, C::CallService));
}

// Scheduled session never sends actions; unaffected by the reorder.
void test_t019_service_done_wrong_state_first_fetch_error() {
  Driver d;
  d.start();
  d.send(Event::user_action({"light.a"}, Action::LightTurnOn, "on"));
  auto c = d.send(Event::wifi(true));
  d.send(Event::service_done(find(c, C::CallService)->ticket, HaError::None));
  c = d.send(Event::fetch_done(HaError::Network, {}, {}));
  TEST_ASSERT_TRUE(has_mark(c, Mark::Failed, "light.a"));
  TEST_ASSERT_EQUAL(1, count(c, C::ShowStatus));
}

// Randomised, well-behaved host: answers only the request that is really outstanding, at
// random delays, mixed with taps/input/ticks. Invariants: at most one request outstanding,
// nothing after Sleep, Sleep is last and unique, tickets strictly increase.
void test_fuzz_invariants() {
  uint32_t seed = 12345;
  auto rnd = [&](uint32_t n) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % n; };
  for (int run = 0; run < 500; run++) {
    Driver d;
    int out_kind = 0, out_ticket = 0, last_ticket = 0;  // 1 wifi, 2 fetch, 3 call
    bool slept = false;
    auto check = [&](const std::vector<Command>& c) {
      if (slept) TEST_ASSERT_EQUAL(0, (int)c.size());
      TEST_ASSERT_TRUE(count(c, C::Sleep) <= 1);
      for (size_t i = 0; i < c.size(); i++) {
        if (c[i].type == C::Sleep) {
          TEST_ASSERT_EQUAL((int)c.size() - 1, (int)i);
          slept = true;
        }
        if (c[i].type == C::ConnectWifi || c[i].type == C::FetchStates ||
            c[i].type == C::CallService) {
          TEST_ASSERT_EQUAL_MESSAGE(0, out_kind, "second request while one in flight");
          out_kind = c[i].type == C::ConnectWifi ? 1 : c[i].type == C::FetchStates ? 2 : 3;
          if (out_kind == 3) {
            TEST_ASSERT_TRUE(c[i].ticket > last_ticket);
            last_ticket = out_ticket = c[i].ticket;
          }
        }
      }
    };
    bool sched = rnd(3) == 0;
    check(d.start(sched ? WakeReason::RtcAlarm : WakeReason::Button));
    for (int step = 0; step < 120; step++) {
      std::vector<Command> c;
      uint32_t r = rnd(10);
      if (r < 3 && out_kind) {
        int k = out_kind;
        out_kind = 0;
        if (k == 1) c = d.send(Event::wifi(rnd(5) != 0));
        else if (k == 2)
          c = d.send(Event::fetch_done(rnd(8) == 0 ? HaError::Network : HaError::None,
                                       {"light.0"}, {es("light.0", rnd(2) ? "on" : "off")}));
        else c = d.send(Event::service_done(out_ticket, rnd(8) == 0 ? HaError::Auth : HaError::None));
        // a terminal error leaves no request outstanding; the machine must agree
      } else if (r < 5) {
        c = d.send(Event::user_action({"light." + std::to_string(rnd(4))},
                                      rnd(2) ? Action::LightToggle : Action::CoverOpen,
                                      rnd(2) ? "on" : ""));
      } else if (r == 5) {
        c = d.send(Event::input());
      } else if (r == 6 && rnd(10) == 0) {
        c = d.advance(130000);
      } else {
        c = d.advance(rnd(1500));
      }
      check(c);
    }
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_happy_path_interactive);
  RUN_TEST(test_input_postpones_sleep);
  RUN_TEST(test_taps_before_wifi_are_queued);
  RUN_TEST(test_toggle_twice_sends_nothing);
  RUN_TEST(test_fetch_all_after_actions_confirms_matching_states);
  RUN_TEST(test_fetch_all_after_actions_mismatch_still_confirms_later);
  RUN_TEST(test_explicit_on_then_off_before_wifi_sends_nothing);
  RUN_TEST(test_explicit_on_off_on_sends_turn_on);
  RUN_TEST(test_explicit_repeat_same_action_is_one_call);
  RUN_TEST(test_last_action_wins_keeps_order_and_groups);
  RUN_TEST(test_adjacent_same_actions_form_one_call);
  RUN_TEST(test_custom_call_is_sent);
  RUN_TEST(test_confirmation_retry_then_ok);
  RUN_TEST(test_confirmation_fails_after_two_retries);
  RUN_TEST(test_service_call_http_error_marks_failed);
  RUN_TEST(test_wifi_failure_offline_and_sleeps);
  RUN_TEST(test_fetch_network_error_goes_offline);
  RUN_TEST(test_auth_error_no_retry);
  RUN_TEST(test_auth_error_on_service_call);
  RUN_TEST(test_scheduled_no_change_no_render);
  RUN_TEST(test_scheduled_change_renders_before_sleep);
  RUN_TEST(test_scheduled_ignores_input_and_sleeps_when_offline);
  RUN_TEST(test_low_battery_sleeps_at_once);
  RUN_TEST(test_hard_cap_with_hanging_request);
  RUN_TEST(test_hard_cap_scheduled_hang);
  RUN_TEST(test_busy_blocks_idle_sleep_until_done);
  RUN_TEST(test_stray_events_are_ignored);
  RUN_TEST(test_duplicate_and_wrong_ticket_service_done);
  RUN_TEST(test_wifi_ok_after_offline_does_nothing);
  RUN_TEST(test_nothing_emitted_after_sleep);
  RUN_TEST(test_events_before_start_are_ignored);
  RUN_TEST(test_second_start_is_ignored);
  RUN_TEST(test_clock_going_backwards_does_not_crash_or_sleep_early);
  RUN_TEST(test_huge_clock_jump_sleeps_with_sane_plan);
  RUN_TEST(test_tap_storm_before_wifi);
  RUN_TEST(test_tap_storm_hundred_taps_even_cancels_all);
  RUN_TEST(test_hard_cap_in_every_state);
  RUN_TEST(test_hard_cap_with_continuous_input_is_not_hit);
  RUN_TEST(test_auth_error_on_fetch_no_retry_and_marks_queue_failed);
  RUN_TEST(test_user_action_after_offline_marks_failed_sends_nothing);
  RUN_TEST(test_action_during_confirm_supersedes_it);
  RUN_TEST(test_fuzz_invariants);
  RUN_TEST(test_t019_opposite_tap_while_first_call_in_flight);
  RUN_TEST(test_t019_toggle_then_explicit_last_wins);
  RUN_TEST(test_t019_on_off_on_sends_one_turn_on);
  RUN_TEST(test_t019_on_off_before_wifi_only_fetches);
  RUN_TEST(test_t019_cross_domain_does_not_cancel);
  RUN_TEST(test_t019_automation_cancel);
  RUN_TEST(test_t019_group_partial_cancel);
  RUN_TEST(test_t019_service_done_before_first_fetch_confirmed_by_fetch);
  RUN_TEST(test_t019_initial_fetch_stale_state_retries);
  RUN_TEST(test_t019_tap_during_initial_fetch);
  RUN_TEST(test_t019_service_done_wrong_state_first_fetch_error);
  return UNITY_END();
}
