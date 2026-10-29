#pragma once
// Messages between the UI loop and the network task.
#include <string>
#include <vector>

#include "entity_state.h"
#include "ha_client.h"
#include "service_call.h"

namespace app_logic {

struct NetCommand {
  enum class Type { Fetch, Call, Close };  // Close: drop the kept-alive connection (before sleep)
  Type type = Type::Fetch;
  std::vector<std::string> ids;  // Fetch: entities to fetch; Call: entities whose returned state is parsed
  int ticket = 0;                // Call
  ServiceCall call;              // Call
};

struct NetEvent {
  enum class Type { FetchDone, ServiceDone, Closed };
  Type type = Type::FetchDone;
  HaError error = HaError::None;
  int http_status = 0;
  int ticket = 0;
  bool external = false;  // answered by the external URL
  uint32_t ms = 0;        // duration of the HTTP request
  bool reused = false;    // over a kept-alive connection
  std::vector<EntityState> states;  // FetchDone: fetched; ServiceDone: states returned by the call
};

}  // namespace app_logic
