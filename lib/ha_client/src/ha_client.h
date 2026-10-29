#pragma once
#include <string>
#include <vector>

#include "entity_state.h"
#include "http_transport.h"
#include "service_call.h"
#include "template_builder.h"

enum class HaError {
  None,
  Auth,     // HTTP 401, never triggers fallback
  Network,  // no base URL reachable
  Http,     // other non-2xx status
  Parse,    // bad response body or bad request input
  TooLarge, // response body over the size cap, never triggers fallback
};

struct HaResult {
  HaError error = HaError::None;
  int http_status = 0;
  std::string message;
  std::string base_url;  // base URL that answered
  uint32_t ms = 0;       // duration of the last request
  bool reused = false;   // it went over a kept-alive connection
  bool ok() const { return error == HaError::None; }
};

struct FetchResult {
  HaResult result;
  std::vector<EntityState> states;
};

class HaClient {
 public:
  // external_url may be empty. Trailing slashes are stripped.
  HaClient(HttpTransport& transport, std::string internal_url, std::string external_url,
           std::string token);

  // Base URL that worked last (also set via set_remembered, e.g. from RTC memory).
  const std::string& remembered_base() const { return remembered_; }
  void set_remembered(const std::string& base_url);

  // Widget-requested attributes per entity, fetched on top of the fixed ones (task 030).
  void set_extra_attributes(ExtraAttrs extra) { extra_ = std::move(extra); }

  FetchResult fetch_states(const std::vector<std::string>& ids);
  // wanted/states: if both are given, the changed states HA returns for entities in `wanted` are
  // parsed into `states` (empty if the body is missing or unparsable; the call still succeeded).
  HaResult call_service(const ServiceCall& call, const std::vector<std::string>* wanted = nullptr,
                        std::vector<EntityState>* states = nullptr);

 private:
  HaResult post(const std::string& path, const std::string& body, std::string& response);

  HttpTransport& transport_;
  std::string internal_;
  std::string external_;
  std::string token_;
  std::string remembered_;
  ExtraAttrs extra_;
};
