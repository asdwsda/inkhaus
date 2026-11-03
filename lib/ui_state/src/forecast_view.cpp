#include "forecast_view.h"

namespace forecast {

namespace {

std::string hilo(const Day &d) {
  if (!d.high) return "-";
  std::string s = uistate::fmtNumber(*d.high, 0) + uistate::kDeg;
  if (d.low) s += "/" + uistate::fmtNumber(*d.low, 0) + uistate::kDeg;
  return s;
}

// Condition text -> icon through the 1x1 weather mapping, so both blocks agree.
bool conditionIcon(const std::string &cond, uistate::WeatherIcon &icon) {
  if (cond.empty() || cond == "unknown" || cond == "unavailable") return false;
  EntityState s;
  s.state = cond;
  s.available = true;
  icon = uistate::weatherView(&s).icon;
  return icon != uistate::WeatherIcon::Unknown;
}

}  // namespace

ForecastView forecastView(const EntityState *st, const Data *data, const Ymd &today) {
  ForecastView v;
  v.now = uistate::weatherView(st);
  if (v.now.look != uistate::Look::Normal) return v;  // loading / unavailable: "-" columns, no icons
  std::vector<Day> days;
  if (data && today.valid()) days = selectDays(data->days, today);
  else if (data && !today.valid()) {  // clock unset: the first entries as they come
    for (const Day &d : data->days)
      if (days.size() < size_t(kDays)) days.push_back(d);
  }
  v.forecastMissing = days.empty();
  Ymd next = today;
  for (int i = 0; i < kDays; ++i) {
    ColumnView &c = v.cols[i];
    if (today.valid()) next = nextDay(next);
    if (size_t(i) < days.size()) {
      const Day &d = days[size_t(i)];
      c.day = weekdayName(weekday(d.date));
      c.hilo = hilo(d);
      c.hasIcon = conditionIcon(d.condition, c.icon);
    } else if (today.valid()) {
      c.day = weekdayName(weekday(next));  // day names are kept without a forecast
    }
  }
  return v;
}

Layout layoutFor(int W) {
  Layout l;
  l.cw = W >= 552 ? 108 : 100;
  l.fx = W - 12 - kDays * l.cw;
  l.condMaxW = l.fx - 16 - l.condX;
  l.divX = l.fx - 8;
  l.unavailTempRight = W - 13;
  for (int i = 0; i < kDays; ++i) {
    ColumnRect &c = l.cols[i];
    c.cx = l.fx + l.cw * i + l.cw / 2;
    c.iconX = c.cx - 12;
  }
  return l;
}

}  // namespace forecast
