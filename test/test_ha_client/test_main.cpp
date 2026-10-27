#include <ArduinoJson.h>
#include <unity.h>

#include <deque>

#include "body_limit.h"
#include "ha_client.h"
#include "service_call.h"
#include "state_parser.h"
#include "template_builder.h"

#include <cmath>
#define ASSERT_DBL(exp, act) TEST_ASSERT_TRUE_MESSAGE(std::fabs((double)(exp) - (double)(act)) < 1e-9, #act)

void setUp() {}
void tearDown() {}

// ---- template builder ----

void test_template_golden() {
  std::string t, err;
  TEST_ASSERT_TRUE(build_state_template({"cover.shutter", "foo.bar"}, t, err));
  const char* expected =
      "{\"cover.shutter\": {% set s = states.cover['shutter'] %}{% if s %}{{ {'s': s.state, "
      "'lc': as_timestamp(s.last_changed)|int, 'a': {'friendly_name': "
      "s.attributes.get('friendly_name'), 'current_position': "
      "s.attributes.get('current_position')}}|tojson }}{% else %}{\"s\": null}{% endif %}, "
      "\"foo.bar\": {% set s = states.foo['bar'] %}{% if s %}{{ {'s': s.state, "
      "'lc': as_timestamp(s.last_changed)|int, 'a': {'friendly_name': "
      "s.attributes.get('friendly_name')}}|tojson }}{% else %}{\"s\": null}{% endif %}}";
  TEST_ASSERT_EQUAL_STRING(expected, t.c_str());
}

void test_template_climate_attrs() {
  std::string t, err;
  TEST_ASSERT_TRUE(build_state_template({"climate.x"}, t, err));
  for (const char* a : {"current_temperature", "'temperature'", "current_humidity", "hvac_mode",
                        "hvac_action", "min_temp", "max_temp", "target_temp_step"})
    TEST_ASSERT_TRUE_MESSAGE(t.find(a) != std::string::npos, a);
}

void test_template_request_body_is_json() {
  std::string body, err;
  TEST_ASSERT_TRUE(build_template_request_body({"light.a"}, body, err));
  JsonDocument d;
  TEST_ASSERT_FALSE(deserializeJson(d, body));
  std::string t;
  build_state_template({"light.a"}, t, err);
  TEST_ASSERT_EQUAL_STRING(t.c_str(), d["template"].as<const char*>());
}

void test_template_rejects_bad_ids() {
  std::string t, err;
  TEST_ASSERT_FALSE(build_state_template({}, t, err));
  TEST_ASSERT_FALSE(build_state_template({"light.a %}{{ evil"}, t, err));
  TEST_ASSERT_FALSE(build_state_template({"nodot"}, t, err));
  TEST_ASSERT_FALSE(build_state_template({"Light.A"}, t, err));
}

// ---- parser ----

static const char* kFixture =
    "{\"cover.c\": {\"s\": \"open\", \"lc\": 1700000000, \"a\": {\"friendly_name\": \"Shutter\", "
    "\"current_position\": 40}},"
    "\"light.l\": {\"s\": \"on\", \"lc\": 1700000100, \"a\": {\"friendly_name\": \"Lamp\", "
    "\"brightness\": 128}},"
    "\"climate.t\": {\"s\": \"heat\", \"lc\": 5, \"a\": {\"current_temperature\": 20.5, "
    "\"temperature\": 22, \"current_humidity\": 45, \"hvac_mode\": \"heat\", \"hvac_action\": "
    "\"heating\", \"min_temp\": 7, \"max_temp\": 30, \"target_temp_step\": 0.5}},"
    "\"sensor.s\": {\"s\": \"3.2\", \"lc\": 6, \"a\": {\"unit_of_measurement\": \"C\", "
    "\"device_class\": \"temperature\"}},"
    "\"weather.w\": {\"s\": \"sunny\", \"lc\": 7, \"a\": {\"temperature\": 11.5, \"humidity\": 80, "
    "\"temperature_unit\": \"C\"}},"
    "\"vacuum.v\": {\"s\": \"docked\", \"lc\": 8, \"a\": {\"battery_level\": 90, \"status\": "
    "\"Docked\"}},"
    "\"switch.off\": {\"s\": \"unavailable\", \"lc\": 9, \"a\": {}},"
    "\"sensor.unk\": {\"s\": \"unknown\", \"lc\": 9, \"a\": {}},"
    "\"light.gone\": {\"s\": null}}";

void test_parser_all_domains() {
  std::vector<std::string> ids = {"cover.c",   "light.l",    "climate.t",  "sensor.s",
                                  "weather.w", "vacuum.v",   "switch.off", "sensor.unk",
                                  "light.gone", "light.absent"};
  ParseResult r = parse_state_response(kFixture, ids);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL(ids.size(), r.states.size());
  const auto& c = r.states[0];
  TEST_ASSERT_TRUE(c.available);
  TEST_ASSERT_EQUAL_STRING("open", c.state.c_str());
  TEST_ASSERT_EQUAL(1700000000, c.last_changed);
  TEST_ASSERT_EQUAL_STRING("Shutter", c.friendly_name.c_str());
  TEST_ASSERT_EQUAL(40, *c.current_position);
  TEST_ASSERT_EQUAL(128, *r.states[1].brightness);
  const auto& t = r.states[2];
  ASSERT_DBL(20.5, *t.current_temperature);
  ASSERT_DBL(22, *t.temperature);
  ASSERT_DBL(45, *t.current_humidity);
  TEST_ASSERT_EQUAL_STRING("heating", t.hvac_action.c_str());
  TEST_ASSERT_EQUAL_STRING("heat", t.hvac_mode.c_str());
  ASSERT_DBL(7, *t.min_temp);
  ASSERT_DBL(30, *t.max_temp);
  ASSERT_DBL(0.5, *t.target_temp_step);
  TEST_ASSERT_EQUAL_STRING("C", r.states[3].unit_of_measurement.c_str());
  TEST_ASSERT_EQUAL_STRING("temperature", r.states[3].device_class.c_str());
  ASSERT_DBL(11.5, *r.states[4].temperature);
  ASSERT_DBL(80, *r.states[4].humidity);
  TEST_ASSERT_EQUAL_STRING("C", r.states[4].temperature_unit.c_str());
  TEST_ASSERT_EQUAL(90, *r.states[5].battery_level);
  TEST_ASSERT_EQUAL_STRING("Docked", r.states[5].status.c_str());
  TEST_ASSERT_FALSE(r.states[6].available);  // unavailable
  TEST_ASSERT_FALSE(r.states[7].available);  // unknown
  TEST_ASSERT_FALSE(r.states[8].available);  // {"s": null}
  TEST_ASSERT_FALSE(r.states[9].available);  // not in response
  TEST_ASSERT_EQUAL_STRING("light.absent", r.states[9].id.c_str());
  TEST_ASSERT_FALSE(r.states[0].current_temperature.has_value());
}

void test_parser_bad_json() {
  TEST_ASSERT_FALSE(parse_state_response("{not json", {"a.b"}).ok);
  TEST_ASSERT_FALSE(parse_state_response("", {"a.b"}).ok);
  TEST_ASSERT_FALSE(parse_state_response("[1,2]", {"a.b"}).ok);
  TEST_ASSERT_FALSE(parse_state_response("Error rendering template", {"a.b"}).ok);
}

// ---- service calls ----

static void check_call(Action a, const char* domain, const char* service) {
  ServiceCall c;
  TEST_ASSERT_TRUE(build_service_call(a, {"x.one", "x.two"}, c));
  TEST_ASSERT_EQUAL_STRING(domain, c.domain.c_str());
  TEST_ASSERT_EQUAL_STRING(service, c.service.c_str());
  TEST_ASSERT_EQUAL_STRING("{\"entity_id\":[\"x.one\",\"x.two\"]}", c.body.c_str());
}

void test_actions() {
  check_call(Action::CoverOpen, "cover", "open_cover");
  check_call(Action::CoverClose, "cover", "close_cover");
  check_call(Action::CoverStop, "cover", "stop_cover");
  check_call(Action::LightToggle, "light", "toggle");
  check_call(Action::SwitchToggle, "switch", "toggle");
  check_call(Action::AutomationTurnOn, "automation", "turn_on");
  check_call(Action::AutomationTurnOff, "automation", "turn_off");
  check_call(Action::VacuumStart, "vacuum", "start");
  check_call(Action::VacuumPause, "vacuum", "pause");
  check_call(Action::VacuumReturnToBase, "vacuum", "return_to_base");
}

void test_action_single_id_is_list_and_empty_rejected() {
  ServiceCall c;
  TEST_ASSERT_TRUE(build_service_call(Action::CoverOpen, {"cover.a"}, c));
  TEST_ASSERT_EQUAL_STRING("{\"entity_id\":[\"cover.a\"]}", c.body.c_str());
  TEST_ASSERT_FALSE(build_service_call(Action::CoverOpen, {}, c));
}

void test_climate_clamp_and_round() {
  ASSERT_DBL(30, normalize_target_temperature(35, 7, 30, 0.5));
  ASSERT_DBL(7, normalize_target_temperature(2, 7, 30, 0.5));
  ASSERT_DBL(21.5, normalize_target_temperature(21.6, 7, 30, 0.5));
  ASSERT_DBL(21.5, normalize_target_temperature(21.3, 7, 30, 0.5));
  ASSERT_DBL(21.0, normalize_target_temperature(21.2, 7, 30, 0.5));
  ASSERT_DBL(22, normalize_target_temperature(21.6, 7, 30, 1));
  ASSERT_DBL(21.6, normalize_target_temperature(21.6, 7, 30, 0));
  ASSERT_DBL(30, normalize_target_temperature(29.9, 7, 30, 1));
}

void test_climate_call_body() {
  ServiceCall c;
  TEST_ASSERT_TRUE(build_climate_set_temperature({"climate.a", "climate.b"}, 99, 7, 30, 0.5, c));
  TEST_ASSERT_EQUAL_STRING("climate", c.domain.c_str());
  TEST_ASSERT_EQUAL_STRING("set_temperature", c.service.c_str());
  JsonDocument d;
  TEST_ASSERT_FALSE(deserializeJson(d, c.body));
  TEST_ASSERT_EQUAL(2, d["entity_id"].size());
  TEST_ASSERT_EQUAL_STRING("climate.b", d["entity_id"][1].as<const char*>());
  ASSERT_DBL(30, d["temperature"].as<double>());
}

// ---- client with fake transport ----

struct FakeTransport : HttpTransport {
  struct Req { std::string method, url, body; HttpHeaders headers; };
  std::vector<Req> calls;
  std::deque<HttpResponse> queue;
  HttpResponse request(const std::string& m, const std::string& u, const HttpHeaders& h,
                       const std::string& b) override {
    calls.push_back({m, u, b, h});
    HttpResponse r = queue.front();
    queue.pop_front();
    return r;
  }
};

static HttpResponse ok(const char* body = "{}") { HttpResponse r; r.status = 200; r.body = body; return r; }
static HttpResponse timeout() { HttpResponse r; r.error = "timeout"; return r; }
static HttpResponse status(int s) { HttpResponse r; r.status = s; return r; }

static const char* kInt = "http://ha.local:8123";
static const char* kExt = "https://x.ui.nabu.casa";

void test_client_internal_ok_no_external() {
  FakeTransport t;
  t.queue = {ok("{\"light.a\": {\"s\": \"on\", \"lc\": 1, \"a\": {}}}")};
  HaClient c(t, std::string(kInt) + "/", kExt, "TOKEN");
  FetchResult f = c.fetch_states({"light.a"});
  TEST_ASSERT_TRUE(f.result.ok());
  TEST_ASSERT_EQUAL(1, t.calls.size());
  TEST_ASSERT_EQUAL_STRING("http://ha.local:8123/api/template", t.calls[0].url.c_str());
  TEST_ASSERT_EQUAL_STRING("POST", t.calls[0].method.c_str());
  TEST_ASSERT_EQUAL_STRING("Authorization", t.calls[0].headers[0].first.c_str());
  TEST_ASSERT_EQUAL_STRING("Bearer TOKEN", t.calls[0].headers[0].second.c_str());
  TEST_ASSERT_EQUAL_STRING("application/json", t.calls[0].headers[1].second.c_str());
  TEST_ASSERT_EQUAL_STRING(kInt, c.remembered_base().c_str());
  TEST_ASSERT_EQUAL(1, f.states.size());
  TEST_ASSERT_EQUAL_STRING("on", f.states[0].state.c_str());
}

void test_client_fallback_and_remember() {
  FakeTransport t;
  t.queue = {timeout(), ok(), ok()};
  HaClient c(t, kInt, kExt, "T");
  ServiceCall call;
  build_service_call(Action::LightToggle, {"light.a"}, call);
  HaResult r = c.call_service(call);
  TEST_ASSERT_TRUE(r.ok());
  TEST_ASSERT_EQUAL(2, t.calls.size());
  TEST_ASSERT_EQUAL_STRING("https://x.ui.nabu.casa/api/services/light/toggle", t.calls[1].url.c_str());
  TEST_ASSERT_EQUAL_STRING(kExt, c.remembered_base().c_str());
  // next call tries the remembered external first
  TEST_ASSERT_TRUE(c.call_service(call).ok());
  TEST_ASSERT_EQUAL(3, t.calls.size());
  TEST_ASSERT_EQUAL_STRING("https://x.ui.nabu.casa/api/services/light/toggle", t.calls[2].url.c_str());
}

void test_client_set_remembered_used_first() {
  FakeTransport t;
  t.queue = {ok()};
  HaClient c(t, kInt, kExt, "T");
  c.set_remembered(kExt);
  ServiceCall call;
  build_service_call(Action::SwitchToggle, {"switch.a"}, call);
  TEST_ASSERT_TRUE(c.call_service(call).ok());
  TEST_ASSERT_EQUAL_STRING("https://x.ui.nabu.casa/api/services/switch/toggle", t.calls[0].url.c_str());
}

void test_client_401_no_fallback() {
  FakeTransport t;
  t.queue = {status(401)};
  HaClient c(t, kInt, kExt, "T");
  FetchResult f = c.fetch_states({"light.a"});
  TEST_ASSERT_EQUAL(HaError::Auth, f.result.error);
  TEST_ASSERT_EQUAL(1, t.calls.size());
  TEST_ASSERT_EQUAL_STRING("", c.remembered_base().c_str());
}

void test_client_all_fail_network_error() {
  FakeTransport t;
  t.queue = {timeout(), timeout()};
  HaClient c(t, kInt, kExt, "T");
  FetchResult f = c.fetch_states({"light.a"});
  TEST_ASSERT_EQUAL(HaError::Network, f.result.error);
  TEST_ASSERT_EQUAL(2, t.calls.size());
}

void test_client_no_external_and_http_error_and_bad_body() {
  FakeTransport t;
  t.queue = {timeout(), status(500), ok("garbage")};
  HaClient c(t, kInt, "", "T");
  TEST_ASSERT_EQUAL(HaError::Network, c.fetch_states({"light.a"}).result.error);
  TEST_ASSERT_EQUAL(1, t.calls.size());
  TEST_ASSERT_EQUAL(HaError::Http, c.fetch_states({"light.a"}).result.error);
  TEST_ASSERT_EQUAL(HaError::Parse, c.fetch_states({"light.a"}).result.error);
}

// ---- tester additions: hostile responses and fallback edge cases ----

void test_parser_wrong_types_treated_as_absent() {
  const char* j =
      "{\"cover.a\": {\"s\": \"open\", \"lc\": \"x\", \"a\": {\"current_position\": \"abc\", "
      "\"friendly_name\": 5, \"brightness\": [1], \"hvac_mode\": {}, \"min_temp\": \"7\"}},"
      "\"light.b\": {\"s\": 5, \"a\": []},"
      "\"light.c\": {\"s\": \"on\", \"a\": []},"
      "\"light.d\": [1,2],"
      "\"light.e\": \"str\","
      "\"light.f\": {\"s\": \"on\", \"a\": null, \"lc\": null}}";
  std::vector<std::string> ids = {"cover.a", "light.b", "light.c", "light.d", "light.e", "light.f"};
  ParseResult r = parse_state_response(j, ids);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL(6, r.states.size());
  TEST_ASSERT_TRUE(r.states[0].available);
  TEST_ASSERT_EQUAL(0, r.states[0].last_changed);
  TEST_ASSERT_FALSE(r.states[0].current_position.has_value());
  TEST_ASSERT_EQUAL_STRING("", r.states[0].friendly_name.c_str());
  TEST_ASSERT_FALSE(r.states[0].brightness.has_value());
  TEST_ASSERT_EQUAL_STRING("", r.states[0].hvac_mode.c_str());
  TEST_ASSERT_FALSE(r.states[0].min_temp.has_value());
  TEST_ASSERT_FALSE(r.states[1].available);  // "s": 5
  TEST_ASSERT_TRUE(r.states[2].available);   // "a": []
  TEST_ASSERT_FALSE(r.states[3].available);
  TEST_ASSERT_FALSE(r.states[4].available);
  TEST_ASSERT_TRUE(r.states[5].available);
}

void test_parser_huge_numbers_no_ub() {
  const char* j =
      "{\"cover.a\": {\"s\": \"open\", \"lc\": 1e300, \"a\": {\"current_position\": 1e300, "
      "\"brightness\": -1e300, \"battery_level\": 99999999999999999999, "
      "\"temperature\": 1e999}},"
      "\"cover.b\": {\"s\": \"open\", \"lc\": -1e300, \"a\": {\"current_position\": 3.7}}}";
  ParseResult r = parse_state_response(j, {"cover.a", "cover.b"});
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL(2, r.states.size());
  // Out-of-range values must not be stored as garbage (they must be absent or clamped).
  if (r.states[0].current_position) {
    TEST_ASSERT_TRUE(*r.states[0].current_position >= 0 && *r.states[0].current_position <= 100);
  }
}

void test_parser_nested_garbage_and_duplicates() {
  std::string deep(5000, '[');
  TEST_ASSERT_FALSE(parse_state_response(deep, {"a.b"}).ok);
  std::string deepobj;
  for (int i = 0; i < 200; i++) deepobj += "{\"a\":";
  deepobj += "1";
  for (int i = 0; i < 200; i++) deepobj += "}";
  parse_state_response(deepobj, {"a.b"});  // must not crash
  ParseResult r = parse_state_response(
      "{\"a.b\": {\"s\": \"on\"}, \"a.b\": {\"s\": \"off\"}, \"a.c\": {\"s\": \"x\", \"s\": \"y\"}}",
      {"a.b", "a.c", "a.b"});
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL(3, r.states.size());
  {
    std::string nul = "{\"a.b\":{\"s\":\"xy\"}}";
    nul[nul.find("xy") + 1] = '\0';
    parse_state_response(nul, {"a.b"});
  }
}

void test_template_unicode_and_quote_friendly_name_parses() {
  ParseResult r = parse_state_response(
      "{\"cover.k\": {\"s\":\"open\",\"lc\":1,\"a\":{\"friendly_name\":\"Rol\\\"lo \\\\ K\\u00fcche "
      "\\u2603\"}}}",
      {"cover.k"});
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_STRING("Rol\"lo \\ K\xC3\xBC" "che \xE2\x98\x83", r.states[0].friendly_name.c_str());
}

// Entity object ids may start with a digit in HA (sensor.1st_floor). The
// template pastes them as states.sensor.1st_floor which is invalid Jinja.
void test_template_digit_leading_object_id_is_safe() {
  std::string t, err;
  bool built = build_state_template({"sensor.1st_floor"}, t, err);
  if (built) TEST_ASSERT_TRUE_MESSAGE(t.find("states.sensor.1st_floor") == std::string::npos,
                                      "invalid Jinja: attribute access with digit-leading name");
}

void test_client_too_large_no_fallback() {
  FakeTransport t;
  HttpResponse big; big.too_large = true; big.error = "response too large";
  t.queue = {big};
  HaClient c(t, kInt, kExt, "TOKEN");
  FetchResult f = c.fetch_states({"light.a"});
  TEST_ASSERT_EQUAL(HaError::TooLarge, f.result.error);
  TEST_ASSERT_EQUAL(1, t.calls.size());
  TEST_ASSERT_EQUAL_STRING("", c.remembered_base().c_str());
}

void test_body_sink() {
  const unsigned char d[4] = {'a', 'b', 'c', 'd'};
  BodySink s;
  s.limit = 6;
  TEST_ASSERT_EQUAL(4, s.accept(d, 4, 0));
  TEST_ASSERT_EQUAL(0, s.accept(d, 4, 0));  // would exceed
  TEST_ASSERT_TRUE(s.too_large);
  TEST_ASSERT_EQUAL(0, s.accept(d, 1, 0));  // sticky
  TEST_ASSERT_EQUAL_STRING("abcd", s.data.c_str());
  BodySink t;
  TEST_ASSERT_EQUAL(4, t.accept(d, 4, 15000));
  TEST_ASSERT_EQUAL(0, t.accept(d, 4, 15001));
  TEST_ASSERT_TRUE(t.deadline_exceeded);
  TEST_ASSERT_FALSE(t.too_large);
  BodySink e;  // empty body is fine
  TEST_ASSERT_TRUE(e.data.empty());
}

void test_body_limit_helpers() {
  TEST_ASSERT_FALSE(content_length_too_large(-1));
  TEST_ASSERT_FALSE(content_length_too_large(0));
  TEST_ASSERT_FALSE(content_length_too_large(65536));
  TEST_ASSERT_TRUE(content_length_too_large(65537));
  TEST_ASSERT_TRUE(body_fits(0, 65536));
  TEST_ASSERT_FALSE(body_fits(0, 65537));
  TEST_ASSERT_TRUE(body_fits(65000, 536));
  TEST_ASSERT_FALSE(body_fits(65000, 537));
  TEST_ASSERT_FALSE(body_fits(70000, 0));
}

void test_client_fallback_edge_cases() {
  {  // external == internal (trailing slash differs): tried only once
    FakeTransport t;
    t.queue = {timeout(), timeout()};
    HaClient c(t, "http://a:8123", "http://a:8123/", "T");
    TEST_ASSERT_EQUAL(HaError::Network, c.fetch_states({"light.a"}).result.error);
    TEST_ASSERT_EQUAL(1, t.calls.size());
  }
  {  // trailing slashes / multiple slashes stripped, no double slash in url
    FakeTransport t;
    t.queue = {ok()};
    HaClient c(t, "http://a:8123//", "", "T");
    ServiceCall call;
    build_service_call(Action::LightToggle, {"light.a"}, call);
    c.call_service(call);
    TEST_ASSERT_EQUAL_STRING("http://a:8123/api/services/light/toggle", t.calls[0].url.c_str());
  }
  {  // remembered external later fails -> falls back to internal, remembers it
    FakeTransport t;
    t.queue = {timeout(), ok()};
    HaClient c(t, kInt, kExt, "T");
    c.set_remembered(kExt);
    ServiceCall call;
    build_service_call(Action::LightToggle, {"light.a"}, call);
    TEST_ASSERT_TRUE(c.call_service(call).ok());
    TEST_ASSERT_EQUAL(2, t.calls.size());
    TEST_ASSERT_TRUE(t.calls[0].url.rfind(kExt, 0) == 0);
    TEST_ASSERT_TRUE(t.calls[1].url.rfind(kInt, 0) == 0);
    TEST_ASSERT_EQUAL_STRING(kInt, c.remembered_base().c_str());
  }
  {  // remembered value not matching either configured URL is ignored
    FakeTransport t;
    t.queue = {ok()};
    HaClient c(t, kInt, "", "T");
    c.set_remembered("http://evil:1");
    TEST_ASSERT_EQUAL_STRING("", c.remembered_base().c_str());
  }
  {  // empty internal, external only
    FakeTransport t;
    t.queue = {ok()};
    HaClient c(t, "", kExt, "T");
    ServiceCall call;
    build_service_call(Action::LightToggle, {"light.a"}, call);
    TEST_ASSERT_TRUE(c.call_service(call).ok());
    TEST_ASSERT_TRUE(t.calls[0].url.rfind(kExt, 0) == 0);
  }
  {  // both empty: error, no transport call
    FakeTransport t;
    HaClient c(t, "", "", "T");
    TEST_ASSERT_EQUAL(HaError::Network, c.fetch_states({"light.a"}).result.error);
    TEST_ASSERT_EQUAL(0, t.calls.size());
  }
  {  // 401 from remembered external does not fall back to internal
    FakeTransport t;
    t.queue = {status(401)};
    HaClient c(t, kInt, kExt, "T");
    c.set_remembered(kExt);
    TEST_ASSERT_EQUAL(HaError::Auth, c.fetch_states({"light.a"}).result.error);
    TEST_ASSERT_EQUAL(1, t.calls.size());
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_template_golden);
  RUN_TEST(test_template_climate_attrs);
  RUN_TEST(test_template_request_body_is_json);
  RUN_TEST(test_template_rejects_bad_ids);
  RUN_TEST(test_parser_all_domains);
  RUN_TEST(test_parser_bad_json);
  RUN_TEST(test_actions);
  RUN_TEST(test_action_single_id_is_list_and_empty_rejected);
  RUN_TEST(test_climate_clamp_and_round);
  RUN_TEST(test_climate_call_body);
  RUN_TEST(test_client_internal_ok_no_external);
  RUN_TEST(test_client_fallback_and_remember);
  RUN_TEST(test_client_set_remembered_used_first);
  RUN_TEST(test_client_401_no_fallback);
  RUN_TEST(test_client_all_fail_network_error);
  RUN_TEST(test_client_no_external_and_http_error_and_bad_body);
  RUN_TEST(test_parser_wrong_types_treated_as_absent);
  RUN_TEST(test_parser_huge_numbers_no_ub);
  RUN_TEST(test_parser_nested_garbage_and_duplicates);
  RUN_TEST(test_template_unicode_and_quote_friendly_name_parses);
  RUN_TEST(test_template_digit_leading_object_id_is_safe);
  RUN_TEST(test_client_fallback_edge_cases);
  RUN_TEST(test_body_limit_helpers);
  RUN_TEST(test_client_too_large_no_fallback);
  RUN_TEST(test_body_sink);
  return UNITY_END();
}
