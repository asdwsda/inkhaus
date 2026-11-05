#include "calendar_glue.h"

#include "wakelog.h"
#include "widget_glue.h"

namespace calendar_glue {

namespace {
calendar::Store g_store;
widget_glue::FetchState g_fs;
}  // namespace

calendar::Store &store() { return g_store; }

void newSession() {
  g_store.newSession();
  g_fs.newSession();
}

bool pending() { return g_fs.pending; }

bool takeChanged() { return g_fs.takeChanged(); }

void pump(app_logic::SessionDriver *drv, const config::Config &cfg, int shownTab, bool scheduledWake, bool graphsBusy,
          int64_t nowEpoch, int32_t utcOffsetS) {
  g_store.setNow(calendar::localMinutes(nowEpoch, utcOffsetS));
  if (!drv || g_fs.pending || graphsBusy) return;
  const std::vector<std::string> ents = calendar::entitiesOnTab(cfg, shownTab);
  if (ents.empty()) return;
  if (drv->status() != session::Status::Online || !drv->wifiUp() || !drv->sessionDone()) return;
  widget_glue::postFirstDue(g_fs, g_store, ents, nowEpoch, scheduledWake, [&](const std::string &e) {
    app_logic::NetCommand c;
    c.type = app_logic::NetCommand::Type::Calendar;
    c.calendarEntity = e;
    drv->postCalendar(c);
  });
}

void onDone(app_logic::NetEvent &&ev, int64_t nowEpoch) {
  if (widget_glue::finishStore(g_fs, g_store, ev.error == HaError::Cancelled)) {  // yielded: keep the old data
    wakelog::logf("APP: calendar dropped (yield), retry later\n");
    return;
  }
  const bool ok = ev.error == HaError::None;
  size_t n = ev.calendarEvents.size();
  if (ok) {
    g_store.set(ev.calendarEntity, std::move(ev.calendarEvents), nowEpoch);
    g_fs.changed = true;
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
  g_fs.changed = true;
}

}  // namespace calendar_glue
