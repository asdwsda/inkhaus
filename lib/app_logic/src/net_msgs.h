#pragma once
// Messages between the UI loop and the network task.
#include <string>
#include <vector>

#include "entity_state.h"
#include "ha_client.h"
#include "service_call.h"

namespace app_logic {

struct NetCommand {
  enum class Type { Fetch, Call };
  Type type = Type::Fetch;
  std::vector<std::string> ids;  // Fetch
  int ticket = 0;                // Call
  ServiceCall call;              // Call
};

struct NetEvent {
  enum class Type { FetchDone, ServiceDone };
  Type type = Type::FetchDone;
  HaError error = HaError::None;
  int http_status = 0;
  int ticket = 0;
  bool external = false;  // answered by the external URL
  std::vector<EntityState> states;
};

}  // namespace app_logic
