#pragma once
// Calendar widget (task 105, docs/ui/widgets-new.md section 3). Pure logic, no Arduino: the calendar.get_events
// request and answer parser, the cache of the last answers, the "ask again?" rule, the view model and the layout.
// Times are local wall-clock minutes since 1970-01-01 00:00 ("local minutes"): the date/time written in HA's answer
// is taken as it is (the UTC offset in it is not applied), like the forecast's dates.
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "config_model.h"
#include "entity_state.h"
#include "widget_view.h"

namespace calendar {

constexpr int kWindowDays = 7;         // request window
constexpr size_t kMaxEvents = 6;       // kept from an answer
constexpr int kRows = 2;               // events shown
constexpr int64_t kMaxAgeS = 30 * 60;  // an interactive tab switch asks again after this

struct Event {
  std::string title;
  int64_t start = 0;  // local minutes
  int64_t end = 0;    // local minutes, exclusive (all-day: 00:00 of the day after)
  bool allDay = false;
};

struct Data {
  std::vector<Event> events;  // ascending by start
  int64_t fetchedAt = 0;      // epoch seconds, 0 = unknown
};

// Service call (REST, with a response). `entity` must be a valid calendar.* id (else false).
const char *requestPath();
bool buildRequestBody(const std::string &entity, std::string &body);

// {"service_response":{"calendar.x":{"events":[{"start":..,"end":..,"summary":..}]}}}; start/end are
// "2026-11-04T09:00:00+01:00", "2026-11-04 09:00:00" or "2026-11-04" (all day). Entries without a readable start are
// skipped; sorted by start, at most kMaxEvents. false = not such an answer; true with 0 events = nothing planned.
bool parseResponse(const std::string &json, const std::string &entity, std::vector<Event> &out);

// Local minutes of a time written like above; false if unreadable. hasTime=false for a date only.
bool parseLocal(const char *s, int64_t &minutes, bool &hasTime);
// Local minutes of an epoch with a UTC offset in seconds (epoch <= 0: 0 = unknown).
int64_t localMinutes(int64_t epoch, int32_t utcOffsetS);

// Cache of the last answers per entity (RAM) and the asked-in-this-session rule (same as the forecast's).
class Store {
 public:
  void set(const std::string &entity, std::vector<Event> events, int64_t now);
  const Data *get(const std::string &entity) const;
  void newSession() { asked_.clear(); }
  // Local minutes of "now" (0 = clock unset), kept by the glue from the wall clock.
  void setNow(int64_t localMin) { nowMin_ = localMin; }
  int64_t nowMin() const { return nowMin_; }
  void markAsked(const std::string &entity, int64_t now);
  bool due(const std::string &entity, int64_t now, bool scheduledWake) const;

 private:
  std::map<std::string, Data> data_;
  std::map<std::string, int64_t> asked_;
  int64_t nowMin_ = 0;
};

// Calendar entities (de-duplicated, in order) of the calendar widgets on tab `tab`.
std::vector<std::string> entitiesOnTab(const config::Config &cfg, int tab);

// ---- view ----
struct RowView {
  std::string time;   // "14:30", "now", "Tue 09:00", "all day", "Tue", "-"
  std::string title;  // raw (the renderer fits it)
};

struct CalendarView {
  uistate::Look look = uistate::Look::Loading;
  bool empty = false;  // Normal, nothing to show: "No events" / "next 7 days"
  int count = 0;       // rows filled (0..kRows)
  RowView rows[kRows];
};

constexpr const char *kEmptyTitle = "No events";
constexpr const char *kEmptySub = "next 7 days";

// st: the calendar entity (nullptr = no cached value). data: its get_events answer (nullptr = none yet: the entity's own
// next event is used). nowMin: local minutes of now (0 = clock unset: no filtering, no "now"/"today" words).
CalendarView calendarView(const EntityState *st, const Data *data, int64_t nowMin);

// ---- layout (widget-relative, W = widget width) ----
struct Layout {
  int timeX = 12, titleX = 118;
  int cy[kRows] = {36, 69};      // centre lines of both rows (time F18, title F24)
  int titleMaxW = 0;             // W - 130
  int unavailX = 12, unavailCy = 52;
  int badgeX = 0, badgeY = 12;   // W - 36
};
Layout layoutFor(int W);

}  // namespace calendar
