#include "cal_events.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cstdio>

namespace calendar {

const char *requestPath() { return "/api/services/calendar/get_events?return_response"; }

static bool validCalendarId(const std::string &id) {
  if (id.size() < 10 || id.compare(0, 9, "calendar.") != 0) return false;
  for (size_t i = 9; i < id.size(); ++i) {
    const char c = id[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  }
  return true;
}

bool buildRequestBody(const std::string &entity, std::string &body) {
  if (!validCalendarId(entity)) return false;
  JsonDocument doc;
  doc["entity_id"] = entity;
  doc["duration"]["days"] = kWindowDays;  // UNVERIFIED: HA takes a duration as a mapping (cv.time_period)
  body.clear();
  serializeJson(doc, body);
  return true;
}

static int64_t daysFromCivil(int y, int m, int d) {  // Howard Hinnant
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int64_t yoe = y - era * 400;
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

static bool num(const char *&p, int digits, int &out) {
  int n = 0;
  for (int i = 0; i < digits; ++i) {
    if (*p < '0' || *p > '9') return false;
    n = n * 10 + (*p - '0');
    ++p;
  }
  out = n;
  return true;
}

bool parseLocal(const char *s, int64_t &minutes, bool &hasTime) {
  if (!s) return false;
  const char *p = s;
  int y = 0, mo = 0, d = 0, h = 0, mi = 0;
  if (!num(p, 4, y) || *p++ != '-' || !num(p, 2, mo) || *p++ != '-' || !num(p, 2, d)) return false;
  if (y < 2000 || mo < 1 || mo > 12 || d < 1 || d > 31) return false;
  hasTime = false;
  if (*p == 'T' || *p == ' ') {
    ++p;
    if (!num(p, 2, h) || *p++ != ':' || !num(p, 2, mi)) return false;
    if (h > 23 || mi > 59) return false;
    hasTime = true;
  } else if (*p != 0) {
    return false;
  }
  minutes = daysFromCivil(y, mo, d) * 1440 + h * 60 + mi;
  return true;
}

int64_t localMinutes(int64_t epoch, int32_t utcOffsetS) {
  if (epoch <= 0) return 0;
  return (epoch + utcOffsetS) / 60;
}

bool parseResponse(const std::string &json, const std::string &entity, std::vector<Event> &out) {
  out.clear();
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  JsonVariantConst arr = doc["service_response"][entity]["events"];
  if (!arr.is<JsonArrayConst>()) return false;
  for (JsonVariantConst e : arr.as<JsonArrayConst>()) {
    Event ev;
    bool hasTime = false, endHasTime = false;
    if (!parseLocal(e["start"] | static_cast<const char *>(nullptr), ev.start, hasTime)) continue;
    ev.allDay = !hasTime;
    if (!parseLocal(e["end"] | static_cast<const char *>(nullptr), ev.end, endHasTime) || ev.end < ev.start)
      ev.end = ev.start + (ev.allDay ? 1440 : 60);
    if (const char *t = e["summary"] | static_cast<const char *>(nullptr)) ev.title = t;
    out.push_back(std::move(ev));
  }
  std::stable_sort(out.begin(), out.end(), [](const Event &a, const Event &b) { return a.start < b.start; });
  if (out.size() > kMaxEvents) out.resize(kMaxEvents);
  return true;
}

void Store::set(const std::string &entity, std::vector<Event> events, int64_t now) {
  Data &d = data_[entity];
  d.events = std::move(events);
  d.fetchedAt = now;
}

const Data *Store::get(const std::string &entity) const {
  auto it = data_.find(entity);
  return it == data_.end() ? nullptr : &it->second;
}

void Store::markAsked(const std::string &entity, int64_t now) { asked_[entity] = now; }
void Store::unmarkAsked(const std::string &entity) { asked_.erase(entity); }

bool Store::due(const std::string &entity, int64_t now, bool scheduledWake) const {
  auto a = asked_.find(entity);
  if (a != asked_.end()) {
    if (scheduledWake || now <= 0 || a->second <= 0) return false;
    return now - a->second > kMaxAgeS || now < a->second;
  }
  if (scheduledWake) return true;
  const Data *d = get(entity);
  if (!d || d->fetchedAt <= 0 || now <= 0) return true;
  return now - d->fetchedAt > kMaxAgeS || now < d->fetchedAt;
}

std::vector<std::string> entitiesOnTab(const config::Config &cfg, int tab) {
  std::vector<std::string> out;
  if (tab < 0 || size_t(tab) >= cfg.tabs.size()) return out;
  for (const config::Widget &w : cfg.tabs[size_t(tab)].widgets)
    if (w.type == config::WidgetType::Calendar && !w.entity.empty() &&
        std::find(out.begin(), out.end(), w.entity) == out.end())
      out.push_back(w.entity);
  return out;
}

// ---- view ----

namespace {

int64_t floorDiv(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0)) ? 1 : 0); }

const char *weekdayOf(int64_t day) {
  static const char *n[] = {"Thu", "Fri", "Sat", "Sun", "Mon", "Tue", "Wed"};  // day 0 = 1970-01-01 = Thursday
  return n[((day % 7) + 7) % 7];
}

std::string hhmm(int64_t localMin) {
  const int64_t m = ((localMin % 1440) + 1440) % 1440;
  char b[8];
  snprintf(b, sizeof b, "%02d:%02d", int(m / 60), int(m % 60));
  return b;
}

std::string timeText(const Event &e, int64_t nowMin) {
  const int64_t startDay = floorDiv(e.start, 1440);
  if (nowMin <= 0) return e.allDay ? weekdayOf(startDay) : std::string(weekdayOf(startDay)) + " " + hhmm(e.start);
  const int64_t today = floorDiv(nowMin, 1440);
  if (e.allDay) return startDay <= today ? "all day" : weekdayOf(startDay);
  if (e.start <= nowMin) return "now";
  if (startDay == today) return hhmm(e.start);
  return std::string(weekdayOf(startDay)) + " " + hhmm(e.start);
}

// The entity's own next event (attributes message, start_time, end_time, all_day), used until get_events answered.
std::vector<Event> attrEvents(const EntityState &st) {
  std::vector<Event> v;
  const std::string *msg = st.attr("message");
  const std::string *start = st.attr("start_time");
  if (!msg || msg->empty() || !start) return v;
  Event e;
  bool hasTime = false, endTime = false;
  if (!parseLocal(start->c_str(), e.start, hasTime)) return v;
  const std::string *ad = st.attr("all_day");
  e.allDay = !hasTime || (ad && *ad == "true");
  const std::string *end = st.attr("end_time");
  if (!end || !parseLocal(end->c_str(), e.end, endTime) || e.end < e.start) e.end = e.start + (e.allDay ? 1440 : 60);
  e.title = *msg;
  v.push_back(std::move(e));
  return v;
}

}  // namespace

CalendarView calendarView(const EntityState *st, const Data *data, int64_t nowMin) {
  CalendarView v;
  if (!st) {
    v.look = uistate::Look::Loading;
    v.rows[0].time = v.rows[1].time = "-";
    return v;
  }
  if (!st->available || st->state == "unavailable") {
    v.look = uistate::Look::Unavailable;
    return v;
  }
  v.look = uistate::Look::Normal;
  std::vector<Event> evs = data ? data->events : attrEvents(*st);
  for (const Event &e : evs) {
    if (nowMin > 0 && e.end <= nowMin) continue;  // already over
    if (v.count >= kRows) break;
    v.rows[v.count].time = timeText(e, nowMin);
    v.rows[v.count].title = e.title;
    ++v.count;
  }
  v.empty = v.count == 0;
  return v;
}

Layout layoutFor(int W) {
  Layout l;
  l.titleMaxW = W - 130;
  l.badgeX = W - 36;
  return l;
}

}  // namespace calendar
