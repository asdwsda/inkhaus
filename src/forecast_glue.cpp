#include "forecast_glue.h"

#include "wakelog.h"

namespace forecast_glue {

namespace {
forecast::Store g_store;
bool g_pending = false, g_changed = false;
}  // namespace

forecast::Store &store() { return g_store; }

void newSession() {
  g_store.newSession();
  g_pending = false;  // the old session's answer is dropped
}

bool pending() { return g_pending; }

bool takeChanged() {
  const bool c = g_changed;
  g_changed = false;
  return c;
}

void pump(app_logic::SessionDriver *drv, const config::Config &cfg, int shownTab, bool scheduledWake, bool graphsBusy,
          int64_t nowEpoch, int32_t utcOffsetS) {
  const forecast::Ymd today = forecast::dateOf(nowEpoch, utcOffsetS);
  if (!(today == g_store.today())) g_store.setToday(today);  // the next redraw picks it up
  if (!drv || g_pending || graphsBusy) return;
  const std::vector<std::string> ents = forecast::entitiesOnTab(cfg, shownTab);
  if (ents.empty()) return;
  if (drv->status() != session::Status::Online || !drv->wifiUp() || !drv->sessionDone()) return;
  for (const std::string &e : ents) {
    if (!g_store.due(e, nowEpoch, scheduledWake)) continue;
    g_store.markAsked(e, nowEpoch);  // failed or not: no second try within the wake
    g_pending = true;
    app_logic::NetCommand c;
    c.type = app_logic::NetCommand::Type::Forecast;
    c.forecastEntity = e;
    drv->postForecast(c);
    break;  // one per loop; the next entity follows after the answer
  }
}

void onDone(app_logic::NetEvent &&ev, int64_t nowEpoch) {
  g_pending = false;
  const bool ok = ev.error == HaError::None;
  size_t n = ev.forecastDays.size();
  if (ok) {
    g_store.set(ev.forecastEntity, std::move(ev.forecastDays), nowEpoch);
    g_changed = true;
  } else {
    n = 0;
  }
  wakelog::logf("APP: forecast err=%d days=%u ms=%u\n", (int)ev.error, (unsigned)n, (unsigned)ev.ms);
}

void fillDemo(const config::Config &cfg, const forecast::Ymd &today) {
  static const char *kConds[] = {"partlycloudy", "rainy", "sunny", "snowy", "cloudy"};
  g_store.setToday(today);
  for (const config::Tab &t : cfg.tabs)
    for (const config::Widget &w : t.widgets) {
      if (w.type != config::WidgetType::Weather || !w.forecast) continue;
      std::vector<forecast::Day> days;
      forecast::Ymd d = today;
      for (int i = 0; i < 4; ++i) {  // today is skipped by the view
        forecast::Day x;
        x.date = d;
        x.condition = kConds[i % 5];
        x.high = 14.0 - i;
        x.low = 6.0 - i;
        days.push_back(x);
        d = forecast::nextDay(d);
      }
      g_store.set(w.entity, std::move(days), 0);
    }
  g_changed = true;
}

}  // namespace forecast_glue
