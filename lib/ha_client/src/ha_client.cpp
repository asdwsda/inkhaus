#include "ha_client.h"

#include <algorithm>

#include "state_parser.h"
#include "template_builder.h"

namespace {
std::string strip_slash(std::string s) {
  while (!s.empty() && s.back() == '/') s.pop_back();
  return s;
}
}  // namespace

HaClient::HaClient(HttpTransport& transport, std::string internal_url, std::string external_url,
                   std::string token)
    : transport_(transport),
      internal_(strip_slash(std::move(internal_url))),
      external_(strip_slash(std::move(external_url))),
      token_(std::move(token)) {}

void HaClient::set_remembered(const std::string& base_url) {
  std::string b = strip_slash(base_url);
  if (b == internal_ || (!external_.empty() && b == external_)) remembered_ = b;
}

HaResult HaClient::post(const std::string& path, const std::string& body, std::string& response) {
  std::vector<std::string> bases;
  for (const std::string& b : {remembered_, internal_, external_}) {
    if (b.empty() || std::find(bases.begin(), bases.end(), b) != bases.end()) continue;
    bases.push_back(b);
  }
  HttpHeaders headers = {{"Authorization", "Bearer " + token_},
                         {"Content-Type", "application/json"}};
  HaResult r;
  r.error = HaError::Network;
  r.message = "no base URL configured";
  for (const std::string& base : bases) {
    HttpResponse resp = transport_.request("POST", base + path, headers, body);
    if (resp.too_large) {  // not a connectivity problem: no fallback
      r.error = HaError::TooLarge;
      r.message = "response too large";
      return r;
    }
    if (resp.status == 0) {  // connection error or timeout: try the next base
      r.error = HaError::Network;
      r.message = resp.error;
      continue;
    }
    r.http_status = resp.status;
    r.base_url = base;
    r.ms = resp.ms;
    r.reused = resp.reused;
    if (resp.status == 401) {
      r.error = HaError::Auth;
      r.message = "unauthorized";
    } else if (resp.status < 200 || resp.status >= 300) {
      r.error = HaError::Http;
      r.message = "HTTP " + std::to_string(resp.status);
    } else {
      r.error = HaError::None;
      r.message.clear();
      remembered_ = base;
      response = std::move(resp.body);
    }
    return r;
  }
  return r;
}

FetchResult HaClient::fetch_states(const std::vector<std::string>& ids) {
  FetchResult f;
  std::string body, err;
  if (!build_template_request_body(ids, body, err, extra_)) {
    f.result.error = HaError::Parse;
    f.result.message = err;
    return f;
  }
  std::string resp;
  f.result = post("/api/template", body, resp);
  if (!f.result.ok()) return f;
  ParseResult p = parse_state_response(resp, ids);
  if (!p.ok) {
    f.result.error = HaError::Parse;
    f.result.message = p.error;
    return f;
  }
  f.states = std::move(p.states);
  return f;
}

HaResult HaClient::call_service(const ServiceCall& call, const std::vector<std::string>* wanted,
                                std::vector<EntityState>* states) {
  std::string resp;
  HaResult r = post("/api/services/" + call.domain + "/" + call.service, call.body, resp);
  if (r.error == HaError::TooLarge) {  // the call went out and answered 2xx-or-not unknown; body only too big
    r.error = HaError::None;           // to read: HA executes the call, the confirmation fetch checks it
    r.message.clear();
    return r;
  }
  if (r.ok() && wanted && states) {
    ParseResult p = parse_service_response(resp, *wanted, extra_);
    if (p.ok) *states = std::move(p.states);
  }
  return r;
}

HaResult HaClient::stream_history(const std::string& entity_id, int64_t start_epoch,
                                  int64_t end_epoch, const StreamLimits& lim, const ChunkFn& sink,
                                  size_t* bytes_out) {
  if (bytes_out) *bytes_out = 0;
  HaResult r;
  std::string path;
  if (!build_history_path(entity_id, start_epoch, end_epoch, path)) {
    r.error = HaError::Parse;
    r.message = "bad history request";
    return r;
  }
  std::vector<std::string> bases;
  for (const std::string& b : {remembered_, internal_, external_}) {
    if (b.empty() || std::find(bases.begin(), bases.end(), b) != bases.end()) continue;
    bases.push_back(b);
  }
  HttpHeaders headers = {{"Authorization", "Bearer " + token_}, {"Accept", "application/json"}};
  r.error = HaError::Network;
  r.message = "no base URL configured";
  uint32_t spent = 0;  // one deadline budget for the whole call, fallbacks included
  size_t total = 0;
  for (const std::string& base : bases) {
    StreamLimits l = lim;
    if (spent >= lim.deadline_ms) { r.error = HaError::Timeout; r.message = "deadline exceeded"; return r; }
    l.deadline_ms = lim.deadline_ms - spent;
    StreamResult s = transport_.stream("GET", base + path, headers, "", l, sink);
    total += s.bytes;
    if (bytes_out) *bytes_out = total;
    spent += s.ms;
    r.ms = spent;
    r.reused = s.reused;
    if (s.cancelled) { r.error = HaError::Cancelled; r.message = "cancelled"; return r; }
    if (s.too_large) { r.error = HaError::TooLarge; r.message = "response too large"; r.base_url = base; return r; }
    if (s.deadline || s.status == 0) {  // timeout or connect/transport failure
      if (s.deadline) { r.error = HaError::Timeout; r.message = "deadline exceeded"; }
      else { r.error = HaError::Network; r.message = s.error; }
      if (s.bytes > 0) return r;  // partial data already delivered: never fall back
      continue;
    }
    r.http_status = s.status;
    r.base_url = base;
    if (s.status == 401) { r.error = HaError::Auth; r.message = "unauthorized"; }
    else if (s.status < 200 || s.status >= 300) { r.error = HaError::Http; r.message = "HTTP " + std::to_string(s.status); }
    else { r.error = HaError::None; r.message.clear(); remembered_ = base; }  // only a complete 2xx body
    return r;
  }
  return r;
}
