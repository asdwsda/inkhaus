#pragma once
// Messages between the UI loop and the network task.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "cal_events.h"  // task 105
#include "collector.h"
#include "entity_state.h"
#include "forecast.h"
#include "ha_client.h"
#include "notif_list.h"
#include "notifications.h"
#include "service_call.h"
#include "stats_types.h"

namespace app_logic {

struct NetCommand {
  enum class Type { Fetch, Call, Close, History, Report, Notifications, Forecast, NotifDismiss, Calendar };  // Close: drop the kept-alive connection (before sleep)
  Type type = Type::Fetch;
  std::vector<std::string> ids;  // Fetch: entities to fetch; Call: entities whose returned state is parsed; NotifDismiss: [notification_id]
  int ticket = 0;                // Call
  ServiceCall call;              // Call
  // History (task 053): stream the entity's history of [histStart, histEnd) into `columns` columns.
  // The UI sets *histCancel = true to abort the transfer (popup closed).
  std::string histEntity;
  int64_t histStart = 0, histEnd = 0;
  int histColumns = 0;
  std::shared_ptr<volatile bool> histCancel;
  // Statistics source (task 055): WebSocket recorder/statistics_during_period. Bars: histBucketCount
  // buckets of histBucketSeconds from histStart; lines: histColumns columns.
  bool histStats = false, histBar = false;
  StatsPeriod histPeriod = StatsPeriod::Hour;
  StatsType histStatistic = StatsType::Mean;
  int64_t histBucketSeconds = 0;
  int histBucketCount = 0;
  // Report (task 100): POST /api/states/<entity> for each (entity, JSON body) pair, in order.
  std::vector<std::pair<std::string, std::string>> reports;
  // Forecast (task 104): weather.get_forecasts for this weather entity (ids unused).
  std::string forecastEntity;
  // Calendar (task 105): calendar.get_events for this calendar entity.
  std::string calendarEntity;
};

struct NetEvent {
  enum class Type { FetchDone, ServiceDone, Closed, HistoryDone, ReportDone, NotificationsDone, ForecastDone, NotifDismissDone, CalendarDone };
  Type type = Type::FetchDone;
  HaError error = HaError::None;
  int http_status = 0;
  int ticket = 0;
  bool external = false;  // answered by the external URL
  uint32_t ms = 0;        // duration of the HTTP request
  bool reused = false;    // over a kept-alive connection
  std::string httpDate;   // FetchDone: Date header of the answer ("" = none), task 097
  std::vector<EntityState> states;  // FetchDone: fetched; ServiceDone: states returned by the call
  // HistoryDone (ticket echoes NetCommand::ticket): the downsampled series (error None only), the scan
  // result and the timings of the log line `HIST: fetch_ms parse_ms bytes points`.
  history::Series series;
  history::ScanResult scan = history::ScanResult::Ok;
  uint32_t fetchMs = 0, parseMs = 0;
  size_t bytes = 0, points = 0;
  bool stats = false;          // the statistics source ran (log field src=stats)
  bool noStatistics = false;   // statistics: valid answer, but the entity has none (error is None)
  uint32_t heapMin = 0;        // filled by the net task for the log line
  // ReportDone: one flag per posted entity (1 = HA accepted it), in command order. After a failure the rest is
  // skipped (flag 0, never sent): a failed report is not retried within the session. ms = total time.
  std::vector<uint8_t> reportOk;
  uint32_t reportSent = 0, reportSkipped = 0;
  // NotificationsDone (task 101): error None + notif.ok = the count and newest title are valid.
  NotificationSummary notif;
  // ForecastDone (task 104, ticket echoes NetCommand::ticket): error None = forecastDays are valid (may be empty).
  std::string forecastEntity;
  std::vector<forecast::Day> forecastDays;
  std::vector<NotifItem> notifItems;  // NotificationsDone (task 103): the list, newest first
  std::string dismissId;              // NotifDismissDone (task 103): error None = dismissed
  std::string calendarEntity;         // CalendarDone (task 105): error None = calendarEvents are valid (may be empty)
  std::vector<calendar::Event> calendarEvents;
};

// History fetch failure that means "too much data": over the size cap, or the 15 s deadline hit while data was
// flowing. A timeout with 0 bytes received is a dead link, not a big answer (task 053 F3).
inline bool histTooMany(HaError e, size_t bytesReceived) {
  return e == HaError::TooLarge || (e == HaError::Timeout && bytesReceived > 0);
}

}  // namespace app_logic
