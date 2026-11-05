#include "forecast_glue.h"

#include "wakelog.h"
#include "widget_glue.h"

namespace forecast_glue {

namespace {
forecast::Store g_store;
widget_glue::FetchState g_fs;
}  // namespace

forecast::Store &store() { return g_store; }

void newSession() {
  g_store.newSession();
  g_fs.newSession();
}

bool pending() { return g_fs.pending; }

bool takeChanged() { return g_fs.takeChanged(); }

void pump(app_logic::SessionDriver *drv, const config::Config &cfg, int shownTab, bool scheduledWake, bool graphsBusy,
          int64_t nowEpoch, int32_t utcOffsetS) {
  const forecast::Ymd today = forecast::dateOf(nowEpoch, utcOffsetS);
  if (!(today == g_store.today())) g_store.setToday(today);  // the next redraw picks it up
  if (!drv || g_fs.pending || graphsBusy) return;
  const std::vector<std::string> ents = forecast::entitiesOnTab(cfg, shownTab);
  if (ents.empty()) return;
  if (drv->status() != session::Status::Online || !drv->wifiUp() || !drv->sessionDone()) return;
  widget_glue::postFirstDue(g_fs, g_store, ents, nowEpoch, scheduledWake, [&](const std::string &e) {
    app_logic::NetCommand c;
    c.type = app_logic::NetCommand::Type::Forecast;
    c.forecastEntity = e;
    drv->postForecast(c);
  });
}

void onDone(app_logic::NetEvent &&ev, int64_t nowEpoch) {
  // task 116: dropped by yielding to a user command, not a failure: keep the old data, ask again later
  if (widget_glue::finishStore(g_fs, g_store, ev.error == HaError::Cancelled)) {
    wakelog::logf("APP: forecast dropped (yield), retry later\n");
    return;
  }
  const bool ok = ev.error == HaError::None;
  size_t n = ev.forecastDays.size();
  if (ok) {
    g_store.set(ev.forecastEntity, std::move(ev.forecastDays), nowEpoch);
    g_fs.changed = true;
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
  g_fs.changed = true;
}

}  // namespace forecast_glue
