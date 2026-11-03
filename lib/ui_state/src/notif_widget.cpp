#include "notif_widget.h"

#include <algorithm>
#include <cstdio>

#include "iso8601.h"

namespace uistate {

void NotifStore::setList(std::vector<NotifItem> &&items) {
  items_ = std::move(items);
  state_ = NotifState::Ready;
  stale_ = false;
}

void NotifStore::setFailed() {
  if (state_ == NotifState::Ready) stale_ = true;  // the cached list stays on screen
  else state_ = NotifState::Unavailable;
}

void NotifStore::remove(const std::string &id) {
  items_.erase(std::remove_if(items_.begin(), items_.end(), [&](const NotifItem &i) { return i.id == id; }),
               items_.end());
}

bool NotifStore::operator==(const NotifStore &o) const {
  if (state_ != o.state_ || stale_ != o.stale_ || items_.size() != o.items_.size()) return false;
  for (size_t i = 0; i < items_.size(); ++i)
    if (items_[i].id != o.items_[i].id || items_[i].title != o.items_[i].title ||
        items_[i].message != o.items_[i].message || items_[i].created != o.items_[i].created)
      return false;
  return true;
}

NotifView notifView(const NotifStore &store, const std::string &configName) {
  NotifView v;
  if (!configName.empty()) v.name = configName;
  switch (store.state()) {
    case NotifState::Loading: break;
    case NotifState::Unavailable:
      v.look = Look::Unavailable;
      v.count = "n/a";
      break;
    case NotifState::Ready: {
      v.look = Look::Normal;
      const int n = store.count();
      v.count = n > 99 ? "99+" : std::to_string(n);
      v.fill = n > 0;
      break;
    }
  }
  return v;
}

namespace {
int64_t floorDiv(int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
}  // namespace

std::string notifTimeText(const std::string &created, int64_t nowEpoch, int32_t utcOffsetS) {
  int64_t t = 0;
  if (!history::parseIso8601(created.c_str(), created.size(), t)) return "";
  const int64_t local = t + utcOffsetS;
  const int64_t day = floorDiv(local, 86400);
  const int64_t secs = local - day * 86400;
  const bool plausible = nowEpoch >= 1000000000;
  char b[16];
  if (plausible && day == floorDiv(nowEpoch + utcOffsetS, 86400)) {
    std::snprintf(b, sizeof b, "%02d:%02d", int(secs / 3600), int(secs / 60 % 60));
    return b;
  }
  // Civil date from days since 1970-01-01 (Hinnant's algorithm).
  const int64_t z = day + 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = unsigned(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned d = doy - (153 * mp + 2) / 5 + 1;
  const unsigned m = mp < 10 ? mp + 3 : mp - 9;
  std::snprintf(b, sizeof b, "%u.%u.", d, m);
  return b;
}

std::vector<NotifItem> demoNotifications() {
  return {
      {"demo_4", "Low battery", "Hallway sensor is at 8 %. Replace the battery soon.", "2026-11-03T12:58:00+00:00"},
      {"demo_3", "Update available", "Home Assistant Core 2026.11.1 can be installed.", "2026-11-02T19:10:00+00:00"},
      {"demo_2", "Backup failed", "The nightly backup ended with an error.", "2026-11-01T02:30:00+00:00"},
      {"demo_1", "", "Welcome home", "2026-10-30T08:00:00+00:00"},
  };
}

bool configHasNotificationsWidget(const config::Config &cfg) {
  for (const config::Tab &t : cfg.tabs)
    for (const config::Widget &w : t.widgets)
      if (w.type == config::WidgetType::Notifications) return true;
  return false;
}

}  // namespace uistate
