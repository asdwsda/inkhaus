#pragma once
// Task 105: glue of the calendar widget's calendar.get_events between the session driver and the UI.
// Asks only for calendar widgets on the shown tab, at most once per scheduled wake (same rule as the forecast).
#include <cstdint>

#include "cal_events.h"
#include "config_model.h"
#include "session_driver.h"

namespace calendar_glue {

calendar::Store &store();
// A new net session started (the old answer is dropped).
void newSession();
// Every loop: tracks the local time and posts the due request when online and the graph fetches are idle.
void pump(app_logic::SessionDriver *drv, const config::Config &cfg, int shownTab, bool scheduledWake, bool graphsBusy,
          int64_t nowEpoch, int32_t utcOffsetS);
// Handler of the CalendarDone events.
void onDone(app_logic::NetEvent &&ev, int64_t nowEpoch);
// A request is posted and not answered yet: the sleep hold waits for it.
bool pending();
// Demo mode: sample events for the calendar widgets, one different case per widget in order (no network).
void fillDemo(const config::Config &cfg, int64_t nowEpoch, int32_t utcOffsetS);
// Set by onDone / fillDemo, cleared by the call.
bool takeChanged();

}  // namespace calendar_glue
