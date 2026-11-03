#pragma once
// Task 104: glue of the weather forecast (weather.get_forecasts) between the session driver and the UI.
// Asks only for weather widgets with `forecast: true` on the shown tab, at most once per scheduled wake.
#include <cstdint>

#include "config_model.h"
#include "forecast.h"
#include "session_driver.h"

namespace forecast_glue {

forecast::Store &store();
// A new net session started (the old answer is dropped).
void newSession();
// Every loop: tracks the local date and posts the due request when online and the graph fetches are idle.
void pump(app_logic::SessionDriver *drv, const config::Config &cfg, int shownTab, bool scheduledWake, bool graphsBusy,
          int64_t nowEpoch, int32_t utcOffsetS);
// Handler of the ForecastDone events.
void onDone(app_logic::NetEvent &&ev, int64_t nowEpoch);
// A request is posted and not answered yet: the sleep hold waits for it.
bool pending();
// Demo mode: synthetic days for the forecast widgets (no network).
void fillDemo(const config::Config &cfg, const forecast::Ymd &today);
// Set by onDone / fillDemo, cleared by the call.
bool takeChanged();

}  // namespace forecast_glue
