#pragma once
// Notification list popup and its confirm dialog (task 103, docs/ui/widgets-new.md sections 1.1 and 7). Pure model
// with a millisecond clock: layout rects, page / focus state, dismiss bookkeeping. No drawing, no network. The
// owner (AppState) performs the dismiss calls and reports the results.
#include <cstdint>
#include <string>
#include <vector>

#include "notif_widget.h"
#include "settings.h"
#include "ui_layout.h"
#include "ui_rocker.h"

namespace uistate {

constexpr uint32_t kNotifConfirmTimeoutMs = 15000;  // the confirm dialog skips by itself

struct NotifPopupLayout {
  bool landscape = true;
  ui::Rect panel, shadow, dirty;
  ui::Rect clearAll, prev, next, close;
  int rowsPerPage = 3;
  ui::Rect rows[6];       // row frames (first rowsPerPage valid)
  ui::Rect rowsArea;      // union of the rows (the in-popup fast refresh)
  int titleX = 0, titleCy = 0;
  int footerCountX = 0, footerCy = 0, footerHintRight = 0;  // footerHintRight 0 = no hint (portrait)
  // Inside a row (offsets from the row's x / y).
  int rowTitleDx = 16, rowTitleCy = 25, rowTitleMaxW = 0;
  int rowTimeRightDx = 0;
  int rowMsgCy = 55, rowMsgMaxW = 0;
  ui::Rect dismissDx;  // x = offset from row x, y = offset from row y, w/h = 64
  ui::Rect dismiss(int i) const { return {rows[i].x + dismissDx.x, rows[i].y + dismissDx.y, dismissDx.w, dismissDx.h}; }
};
NotifPopupLayout notifPopupLayout(settings::Orientation o);

struct NotifConfirmLayout {
  ui::Rect panel, shadow, dirty;
  ui::Rect yes, no, skip;
  int titleCx = 0, titleCy = 0, subCy = 0;
};
NotifConfirmLayout notifConfirmLayout(settings::Orientation o);

// "Notifications (4)" and "1-3 of 4" (empty list: "").
std::string notifPopupTitle(int count);
std::string notifFooterText(int page, int rowsPerPage, int count);
std::string notifConfirmTitle(int count);  // "Dismiss all 4?"

enum class NotifRowState { Normal, Pending, Failed };

// What an event asks the owner to do.
struct NotifResult {
  bool changed = false;                 // the picture changed
  std::vector<std::string> dismissIds;  // send persistent_notification/dismiss for each (already marked pending)
  bool closed = false;                  // the popup closed with this event
};

class NotifPopup {
 public:
  enum class Mode { Closed, List, Confirm };

  // viaRocker: the focus starts on the first row (ring visible); by touch no ring is shown.
  void open(const NotifStore &store, uint32_t now, bool viaRocker);
  bool close();  // true if it was open
  bool isOpen() const { return mode_ != Mode::Closed; }
  Mode mode() const { return mode_; }

  NotifResult touch(ui::Point p, settings::Orientation o, const NotifStore &store, uint32_t now);
  NotifResult rocker(ui::PhysicalKey key, settings::Orientation o, const NotifStore &store, uint32_t now);
  // Confirm dialog timeout (15 s).
  bool tick(uint32_t now);
  // The list changed (new fetch, removal): clamp page and focus.
  void sync(const NotifStore &store, settings::Orientation o);

  // Result of one dismiss call. ok: the owner removes the item from the store, then calls sync().
  void dismissDone(const std::string &id, bool ok);

  int page() const { return page_; }
  // Focus ring: index of the row (0-based over all items) or count = Clear all, -1 = no ring.
  int focus() const { return focus_; }
  NotifRowState rowState(const std::string &id) const;
  bool anyPending() const;
  int pageCount(int count, int rowsPerPage) const;

 private:
  struct Mark {
    std::string id;
    NotifRowState st;
  };
  void setMark(const std::string &id, NotifRowState st);
  void dropMark(const std::string &id);
  NotifResult activate(int index, const NotifStore &store, uint32_t now);
  NotifResult dismissOne(const NotifStore &store, int index);
  NotifResult openConfirm(const NotifStore &store, uint32_t now);
  NotifResult confirmed(const NotifStore &store);

  Mode mode_ = Mode::Closed;
  int page_ = 0;
  int focus_ = -1;
  uint32_t confirmAt_ = 0;
  std::vector<Mark> marks_;
};

}  // namespace uistate
