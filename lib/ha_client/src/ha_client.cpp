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

HaResult HaClient::call_service(const ServiceCall& call) {
  std::string resp;
  return post("/api/services/" + call.domain + "/" + call.service, call.body, resp);
}
