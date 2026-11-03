#pragma once
// Notifications widget (task 103, docs/ui/widgets-new.md section 1): the list store, the 1x1 view model and the
// row time text. Pure logic, no drawing, no network. The popup model is in notif_popup.h.
#include <cstdint>
#include <string>
#include <vector>

#include "config_model.h"
#include "notif_list.h"
#include "widget_view.h"

namespace uistate {

constexpr const char *kNotifDefaultName = "Alerts";

enum class NotifState { Loading, Ready, Unavailable };

// The last list HA sent. Loading until the first answer; a failed fetch keeps a cached list (stale), else Unavailable.
class NotifStore {
 public:
  void setList(std::vector<NotifItem> &&items);
  void setFailed();
  void remove(const std::string &id);  // after a successful dismiss
  NotifState state() const { return state_; }
  bool stale() const { return stale_; }
  const std::vector<NotifItem> &items() const { return items_; }
  int count() const { return int(items_.size()); }
  bool operator==(const NotifStore &o) const;

 private:
  NotifState state_ = NotifState::Loading;
  bool stale_ = false;
  std::vector<NotifItem> items_;
};

struct NotifView {
  Look look = Look::Loading;
  std::string count = "-";  // "0".."99", "99+", "-" loading, "n/a" unavailable
  bool fill = false;        // count > 0: INK fill, PAPER content
  std::string name = kNotifDefaultName;
};
NotifView notifView(const NotifStore &store, const std::string &configName);

// Row time (spec 1.1): "13:58" for today, "3.11." for an earlier day (local time = epoch + utcOffsetS). Empty when
// `created` does not parse. Without a plausible clock (nowEpoch < 1e9) every row shows the date.
std::string notifTimeText(const std::string &created, int64_t nowEpoch, int32_t utcOffsetS);

// Sample list for the demo mode (4 entries, newest first).
std::vector<NotifItem> demoNotifications();

// True when any widget of the config is a notifications widget (a fetch of the list is wanted).
bool configHasNotificationsWidget(const config::Config &cfg);

}  // namespace uistate
