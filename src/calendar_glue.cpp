#include "calendar_glue.h"

#include "wakelog.h"

namespace calendar_glue {

namespace {
calendar::Store g_store;
bool g_pending = false, g_changed = false;
}  // namespace

calendar::Store &store() { return g_store; }

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
  g_store.setNow(calendar::localMinutes(nowEpoch, utcOffsetS));
  if (!drv || g_pending || graphsBusy) return;
  const std::vector<std::string> ents = calendar::entitiesOnTab(cfg, shownTab);
  if (ents.empty()) return;
  if (drv->status() != session::Status::Online || !drv->wifiUp() || !drv->sessionDone()) return;
  for (const std::string &e : ents) {
    if (!g_store.due(e, nowEpoch, scheduledWake)) continue;
    g_store.markAsked(e, nowEpoch);  // failed or not: no second try within the wake
    g_pending = true;
    app_logic::NetCommand c;
    c.type = app_logic::NetCommand::Type::Calendar;
    c.calendarEntity = e;
    drv->postCalendar(c);
    break;  // one per loop; the next entity follows after the answer
  }
}

void onDone(app_logic::NetEvent &&ev, int64_t nowEpoch) {
  g_pending = false;
  const bool ok = ev.error == HaError::None;
  size_t n = ev.calendarEvents.size();
  if (ok) {
    g_store.set(ev.calendarEntity, std::move(ev.calendarEvents), nowEpoch);
    g_changed = true;
  } else {
    n = 0;
  }
  wakelog::logf("APP: calendar err=%d events=%u ms=%u\n", (int)ev.error, (unsigned)n, (unsigned)ev.ms);
}

void fillDemo(const config::Config &cfg, int64_t nowEpoch, int32_t utcOffsetS) {
  const int64_t now = calendar::localMinutes(nowEpoch, utcOffsetS);
  g_store.setNow(now);
  const int64_t day = now / 1440 * 1440;  // local midnight today
  int k = 0;
  for (const config::Tab &t : cfg.tabs)
    for (const config::Widget &w : t.widgets) {
      if (w.type != config::WidgetType::Calendar) continue;
      std::vector<calendar::Event> ev;
      auto add = [&](const char *title, int64_t start, int64_t len, bool allDay) {
        calendar::Event e;
        e.title = title;
        e.start = start;
        e.end = start + len;
        e.allDay = allDay;
        ev.push_back(std::move(e));
      };
      switch (k++ % 4) {
        case 0:  // a timed event later today, then one on another day
          add("Dentist", day + 14 * 60 + 30, 60, false);
          add("Team stand-up", day + 2 * 1440 + 9 * 60, 30, false);
          break;
        case 1: break;  // nothing planned
        case 2:         // running now, then all day tomorrow
          add("Planning workshop", now - 20, 90, false);
          add("Anna's birthday", day + 1440, 1440, true);
          break;
        default:  // all day today, then a long title
          add("Public holiday", day, 1440, true);
          add("Quarterly review with the whole leadership team", day + 3 * 1440 + 10 * 60, 60, false);
          break;
      }
      g_store.set(w.entity, std::move(ev), 0);
    }
  g_changed = true;
}

}  // namespace calendar_glue
