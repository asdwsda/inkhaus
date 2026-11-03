#include "forecast.h"

#include <ArduinoJson.h>

#include <algorithm>

namespace forecast {

const char *requestPath() { return "/api/services/weather/get_forecasts?return_response"; }

static bool validWeatherId(const std::string &id) {
  if (id.size() < 9 || id.compare(0, 8, "weather.") != 0) return false;
  for (size_t i = 8; i < id.size(); ++i) {
    const char c = id[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  }
  return true;
}

bool buildRequestBody(const std::string &entity, std::string &body) {
  if (!validWeatherId(entity)) return false;
  JsonDocument doc;
  doc["entity_id"] = entity;
  doc["type"] = "daily";
  body.clear();
  serializeJson(doc, body);
  return true;
}

static bool parseDate(const char *s, Ymd &out) {
  if (!s) return false;
  int v[3] = {0, 0, 0};
  const char *p = s;
  for (int i = 0; i < 3; ++i) {
    int digits = 0, n = 0;
    while (*p >= '0' && *p <= '9' && digits < 5) {
      n = n * 10 + (*p - '0');
      ++p;
      ++digits;
    }
    if (digits == 0 || (i == 0 && digits != 4) || (i > 0 && digits != 2)) return false;
    v[i] = n;
    if (i < 2) {
      if (*p != '-') return false;
      ++p;
    }
  }
  if (*p != 0 && *p != 'T' && *p != ' ') return false;
  out = {v[0], v[1], v[2]};
  return out.valid();
}

bool parseResponse(const std::string &json, const std::string &entity, std::vector<Day> &out) {
  out.clear();
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  JsonVariantConst arr = doc["service_response"][entity]["forecast"];
  if (!arr.is<JsonArrayConst>()) return false;
  for (JsonVariantConst e : arr.as<JsonArrayConst>()) {
    if (out.size() >= kMaxParsedDays) break;
    Day d;
    if (!parseDate(e["datetime"] | static_cast<const char *>(nullptr), d.date)) continue;
    if (const char *c = e["condition"] | static_cast<const char *>(nullptr)) d.condition = c;
    if (e["temperature"].is<double>()) d.high = e["temperature"].as<double>();
    if (e["templow"].is<double>()) d.low = e["templow"].as<double>();
    out.push_back(std::move(d));
  }
  std::stable_sort(out.begin(), out.end(), [](const Day &a, const Day &b) { return a.date < b.date; });
  return true;
}

int weekday(const Ymd &d) {  // Sakamoto
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  int y = d.y;
  if (d.m < 3) --y;
  return (y + y / 4 - y / 100 + y / 400 + t[(d.m - 1) % 12] + d.d) % 7;
}

const char *weekdayName(int wd) {
  static const char *n[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  return n[((wd % 7) + 7) % 7];
}

static int daysInMonth(int y, int m) {
  static const int n[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
  return n[(m - 1) % 12];
}

Ymd nextDay(const Ymd &d) {
  Ymd r = d;
  if (++r.d > daysInMonth(r.y, r.m)) {
    r.d = 1;
    if (++r.m > 12) {
      r.m = 1;
      ++r.y;
    }
  }
  return r;
}

Ymd dateOf(int64_t epoch, int32_t utcOffsetS) {
  if (epoch <= 0) return {};
  int64_t days = (epoch + utcOffsetS) / 86400;
  if ((epoch + utcOffsetS) < 0) --days;
  days += 719468;  // civil from days (Howard Hinnant)
  const int64_t era = days / 146097;
  const int64_t doe = days - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t y = yoe + era * 400;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  const int d = int(doy - (153 * mp + 2) / 5 + 1);
  const int m = int(mp < 10 ? mp + 3 : mp - 9);
  if (m <= 2) ++y;
  return {int(y), m, d};
}

std::vector<Day> selectDays(const std::vector<Day> &days, const Ymd &today) {
  std::vector<Day> out;
  for (const Day &d : days) {
    if (!(today < d.date)) continue;
    out.push_back(d);
    if (out.size() >= size_t(kDays)) break;
  }
  return out;
}

void Store::set(const std::string &entity, std::vector<Day> days, int64_t now) {
  Data &d = data_[entity];
  d.days = std::move(days);
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
    if (w.type == config::WidgetType::Weather && w.forecast && !w.entity.empty() &&
        std::find(out.begin(), out.end(), w.entity) == out.end())
      out.push_back(w.entity);
  return out;
}

}  // namespace forecast
