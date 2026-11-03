#pragma once
// Weather forecast (task 104, docs/ui/widgets-new.md section 2). Current HA has no `forecast` attribute: the daily
// forecast comes from the service weather.get_forecasts {type: daily} with a response, REST
// POST /api/services/weather/get_forecasts?return_response. Pure logic, no Arduino: request, response parser,
// day selection, the cache of the last answers and the "ask again?" rule.
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "config_model.h"

namespace forecast {

constexpr int kDays = 3;               // columns shown
constexpr size_t kMaxParsedDays = 10;  // entries kept from an answer
constexpr int64_t kMaxAgeS = 30 * 60;  // an interactive tab switch asks again after this

struct Ymd {
  int y = 0, m = 0, d = 0;
  bool valid() const { return y >= 2000 && m >= 1 && m <= 12 && d >= 1 && d <= 31; }
  bool operator==(const Ymd &o) const { return y == o.y && m == o.m && d == o.d; }
  bool operator<(const Ymd &o) const { return y != o.y ? y < o.y : m != o.m ? m < o.m : d < o.d; }
};

struct Day {
  Ymd date;
  std::string condition;       // HA condition ("rainy"), may be empty
  std::optional<double> high;  // `temperature`
  std::optional<double> low;   // `templow`
};

struct Data {
  std::vector<Day> days;  // as delivered, ascending
  int64_t fetchedAt = 0;  // epoch seconds, 0 = unknown
};

// Path and JSON body of the service call. `entity` must be a valid weather.* id (else false).
const char *requestPath();
bool buildRequestBody(const std::string &entity, std::string &body);

// Parses the REST answer {"changed_states":[],"service_response":{"weather.x":{"forecast":[...]}}}. The date is
// the local date written in `datetime` ("2026-11-04T00:00:00+01:00" or "2026-11-04"; the offset is not applied).
// false = not such an answer or the entity is missing; true with 0 days = an empty forecast.
bool parseResponse(const std::string &json, const std::string &entity, std::vector<Day> &out);

// 0 = Sunday .. 6 = Saturday.
int weekday(const Ymd &d);
const char *weekdayName(int wd);  // "Sun".."Sat"
Ymd nextDay(const Ymd &d);
// Local date of an epoch with a UTC offset in seconds (epoch <= 0: invalid Ymd).
Ymd dateOf(int64_t epoch, int32_t utcOffsetS);

// The first kDays entries whose date is after `today` (today and past entries are skipped).
std::vector<Day> selectDays(const std::vector<Day> &days, const Ymd &today);

// Cache of the last answers per entity (RAM), plus which entities were asked in this session.
class Store {
 public:
  void set(const std::string &entity, std::vector<Day> days, int64_t now);
  const Data *get(const std::string &entity) const;
  void newSession() { asked_.clear(); }
  // Local date used to pick the days (set by the glue from the wall clock; invalid = clock unset).
  void setToday(const Ymd &t) { today_ = t; }
  const Ymd &today() const { return today_; }
  void markAsked(const std::string &entity, int64_t now);
  void unmarkAsked(const std::string &entity);  // the request was dropped (task 116 yield): ask again later
  // Not asked yet in this session: due on a scheduled wake (once per wake), else when there is no data or it is
  // older than kMaxAgeS. Asked already: never again on a scheduled wake; interactive sessions (the screen can
  // stay awake for hours) ask again kMaxAgeS after the last attempt, failed or not. Clock unset (now <= 0): once.
  bool due(const std::string &entity, int64_t now, bool scheduledWake) const;

 private:
  std::map<std::string, Data> data_;
  Ymd today_;
  std::map<std::string, int64_t> asked_;  // entity -> time of the last attempt this session
};

// Forecast entities (de-duplicated, in order) of the weather widgets with `forecast: true` on tab `tab`.
std::vector<std::string> entitiesOnTab(const config::Config &cfg, int tab);

}  // namespace forecast
