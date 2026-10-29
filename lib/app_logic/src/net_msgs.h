#pragma once
// Messages between the UI loop and the network task.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "collector.h"
#include "entity_state.h"
#include "ha_client.h"
#include "service_call.h"

namespace app_logic {

struct NetCommand {
  enum class Type { Fetch, Call, Close, History };  // Close: drop the kept-alive connection (before sleep)
  Type type = Type::Fetch;
  std::vector<std::string> ids;  // Fetch: entities to fetch; Call: entities whose returned state is parsed
  int ticket = 0;                // Call
  ServiceCall call;              // Call
  // History (task 053): stream the entity's history of [histStart, histEnd) into `columns` columns.
  // The UI sets *histCancel = true to abort the transfer (popup closed).
  std::string histEntity;
  int64_t histStart = 0, histEnd = 0;
  int histColumns = 0;
  std::shared_ptr<volatile bool> histCancel;
};

struct NetEvent {
  enum class Type { FetchDone, ServiceDone, Closed, HistoryDone };
  Type type = Type::FetchDone;
  HaError error = HaError::None;
  int http_status = 0;
  int ticket = 0;
  bool external = false;  // answered by the external URL
  uint32_t ms = 0;        // duration of the HTTP request
  bool reused = false;    // over a kept-alive connection
  std::vector<EntityState> states;  // FetchDone: fetched; ServiceDone: states returned by the call
  // HistoryDone (ticket echoes NetCommand::ticket): the downsampled series (error None only), the scan
  // result and the timings of the log line `HIST: fetch_ms parse_ms bytes points`.
  history::Series series;
  history::ScanResult scan = history::ScanResult::Ok;
  uint32_t fetchMs = 0, parseMs = 0;
  size_t bytes = 0, points = 0;
};

}  // namespace app_logic
