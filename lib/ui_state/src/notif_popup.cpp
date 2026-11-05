#include "notif_popup.h"

#include <algorithm>

namespace uistate {

using settings::Orientation;

NotifPopupLayout notifPopupLayout(Orientation o) {
  NotifPopupLayout l;
  if (o == Orientation::Landscape) {
    l.landscape = true;
    l.panel = {40, 94, 880, 408};
    l.shadow = {46, 100, 880, 408};
    l.dirty = {38, 92, 890, 418};
    l.clearAll = {520, 110, 152, 64};
    l.prev = {688, 110, 64, 64};
    l.next = {764, 110, 64, 64};
    l.close = {840, 110, 64, 64};
    l.rowsPerPage = 3;
    for (int i = 0; i < 3; ++i) l.rows[i] = {56, 190 + 88 * i, 848, 76};
    l.titleX = 56; l.titleCy = 142;
    l.footerCountX = 56; l.footerCy = 470; l.footerHintRight = 904;
    l.rowTitleMaxW = 650; l.rowTimeRightDx = 756; l.rowMsgMaxW = 730;
    l.dismissDx = {772, 6, 64, 64};
  } else {
    l.landscape = false;
    l.panel = {16, 116, 508, 704};
    l.shadow = {22, 122, 508, 704};
    l.dirty = {14, 114, 518, 714};
    l.clearAll = {32, 744, 152, 64};
    l.prev = {292, 132, 64, 64};
    l.next = {368, 132, 64, 64};
    l.close = {444, 132, 64, 64};
    l.rowsPerPage = 6;
    for (int i = 0; i < 6; ++i) l.rows[i] = {32, 212 + 88 * i, 476, 76};
    l.titleX = 32; l.titleCy = 164;
    l.footerCountX = 200; l.footerCy = 776; l.footerHintRight = 0;
    l.rowTitleMaxW = 290; l.rowTimeRightDx = 368; l.rowMsgMaxW = 352;
    l.dismissDx = {384, 6, 64, 64};
  }
  l.rowsArea = {l.rows[0].x, l.rows[0].y, l.rows[0].w,
                l.rows[l.rowsPerPage - 1].y + l.rows[0].h - l.rows[0].y};
  return l;
}

ConfirmLayout confirmLayout(Orientation o) {
  ConfirmLayout l;
  if (o == Orientation::Landscape) {
    l.panel = {200, 188, 560, 220};
    l.yes = {228, 316, 160, 64};
    l.no = {400, 316, 160, 64};
    l.skip = {572, 316, 160, 64};
  } else {
    l.panel = {30, 356, 480, 224};
    l.yes = {54, 484, 136, 64};
    l.no = {202, 484, 136, 64};
    l.skip = {350, 484, 136, 64};
  }
  l.shadow = {l.panel.x + 6, l.panel.y + 6, l.panel.w, l.panel.h};
  l.dirty = {l.panel.x - 2, l.panel.y - 2, l.panel.w + 10, l.panel.h + 10};
  l.titleCx = l.panel.x + l.panel.w / 2;
  l.titleCy = l.panel.y + 44;
  l.subCy = l.panel.y + 78;
  return l;
}

std::string notifPopupTitle(int count) { return "Notifications (" + std::to_string(count) + ")"; }

std::string notifFooterText(int page, int rowsPerPage, int count) {
  if (count <= 0) return "";
  const int first = page * rowsPerPage + 1;
  const int last = std::min(count, first + rowsPerPage - 1);
  return std::to_string(first) + "-" + std::to_string(last) + " of " + std::to_string(count);
}

std::string notifConfirmTitle(int count) { return "Dismiss all " + std::to_string(count) + "?"; }

// ---- model ----

int NotifPopup::pageCount(int count, int rowsPerPage) const {
  return std::max(1, (count + rowsPerPage - 1) / rowsPerPage);
}

void NotifPopup::open(const NotifStore &store, uint32_t, bool viaRocker) {
  mode_ = Mode::List;
  page_ = 0;
  focus_ = viaRocker && store.count() > 0 ? 0 : -1;
  marks_.clear();
}

bool NotifPopup::close() {
  const bool was = isOpen();
  mode_ = Mode::Closed;
  dialog_.cancel();
  focus_ = -1;
  page_ = 0;
  return was;
}

void NotifPopup::setMark(const std::string &id, NotifRowState st) {
  for (Mark &m : marks_)
    if (m.id == id) {
      m.st = st;
      return;
    }
  marks_.push_back({id, st});
}

void NotifPopup::dropMark(const std::string &id) {
  marks_.erase(std::remove_if(marks_.begin(), marks_.end(), [&](const Mark &m) { return m.id == id; }), marks_.end());
}

NotifRowState NotifPopup::rowState(const std::string &id) const {
  for (const Mark &m : marks_)
    if (m.id == id) return m.st;
  return NotifRowState::Normal;
}

bool NotifPopup::anyPending() const {
  for (const Mark &m : marks_)
    if (m.st == NotifRowState::Pending) return true;
  return false;
}

void NotifPopup::dismissDone(const std::string &id, bool ok) {
  if (ok) dropMark(id);
  else setMark(id, NotifRowState::Failed);
}

void NotifPopup::sync(const NotifStore &store, Orientation o) {
  if (!isOpen()) return;
  const int n = store.count();
  const int rpp = notifPopupLayout(o).rowsPerPage;
  // Marks of items that are gone (dismissed elsewhere, or by an earlier fetch) are dropped.
  marks_.erase(std::remove_if(marks_.begin(), marks_.end(),
                              [&](const Mark &m) {
                                for (const NotifItem &i : store.items())
                                  if (i.id == m.id) return false;
                                return true;
                              }),
               marks_.end());
  page_ = std::min(page_, pageCount(n, rpp) - 1);
  if (n == 0) {
    if (mode_ == Mode::Confirm) {
      mode_ = Mode::List;
      dialog_.cancel();
    }
    focus_ = -1;
  } else if (focus_ > n) {
    focus_ = n;
  }
  if (focus_ >= 0 && focus_ < n) page_ = focus_ / rpp;
  else if (focus_ == n && n > 0) page_ = pageCount(n, rpp) - 1;
}

NotifResult NotifPopup::dismissOne(const NotifStore &store, int index) {
  NotifResult r;
  if (index < 0 || index >= store.count()) return r;
  const std::string &id = store.items()[size_t(index)].id;
  if (rowState(id) == NotifRowState::Pending) return r;  // one call at a time per notification
  setMark(id, NotifRowState::Pending);
  r.dismissIds.push_back(id);
  r.changed = true;
  return r;
}

NotifResult NotifPopup::openConfirm(const NotifStore &store, uint32_t now) {
  NotifResult r;
  if (store.count() == 0 || anyPending()) return r;
  mode_ = Mode::Confirm;
  dialog_.open(notifConfirmTitle(store.count()), 0, 0, now);
  r.changed = true;
  return r;
}

NotifResult NotifPopup::confirmed(const NotifStore &store) {
  NotifResult r;
  mode_ = Mode::List;
  r.changed = true;
  for (const NotifItem &i : store.items()) {
    if (rowState(i.id) == NotifRowState::Pending) continue;
    setMark(i.id, NotifRowState::Pending);
    r.dismissIds.push_back(i.id);
  }
  return r;
}

NotifResult NotifPopup::activate(int index, const NotifStore &store, uint32_t now) {
  if (index == store.count()) return openConfirm(store, now);
  return dismissOne(store, index);
}

NotifResult NotifPopup::touch(ui::Point p, Orientation o, const NotifStore &store, uint32_t now) {
  NotifResult r;
  if (mode_ == Mode::Closed) return r;
  if (mode_ == Mode::Confirm) return answered(dialog_.touch(p, o), store);
  const NotifPopupLayout l = notifPopupLayout(o);
  if (!l.panel.contains(p) || l.close.contains(p)) {
    close();
    r.changed = r.closed = true;
    return r;
  }
  const int n = store.count();
  const int pages = pageCount(n, l.rowsPerPage);
  if (l.prev.contains(p)) {
    if (page_ > 0) { --page_; focus_ = focus_ >= 0 ? page_ * l.rowsPerPage : focus_; r.changed = true; }
    return r;
  }
  if (l.next.contains(p)) {
    if (page_ < pages - 1) { ++page_; focus_ = focus_ >= 0 ? page_ * l.rowsPerPage : focus_; r.changed = true; }
    return r;
  }
  if (l.clearAll.contains(p)) return openConfirm(store, now);
  for (int i = 0; i < l.rowsPerPage; ++i) {
    const int idx = page_ * l.rowsPerPage + i;
    if (idx < n && l.dismiss(i).contains(p)) return dismissOne(store, idx);
  }
  return r;
}

NotifResult NotifPopup::rocker(ui::PhysicalKey key, Orientation o, const NotifStore &store, uint32_t now) {
  NotifResult r;
  if (mode_ == Mode::Closed) return r;
  if (mode_ == Mode::Confirm) return answered(dialog_.rocker(key), store);  // STYLE 10b
  if (key == ui::PhysicalKey::LongPush) {
    close();
    r.changed = r.closed = true;
    return r;
  }
  const int n = store.count();
  const int rpp = notifPopupLayout(o).rowsPerPage;
  if (key == ui::PhysicalKey::Push) {
    if (focus_ < 0 || n == 0) return r;
    return activate(focus_, store, now);
  }
  if (n == 0) return r;
  const bool prev = ui::mapRocker(key, o, false) == ui::LogicalKey::Prev;
  const int old = focus_;
  if (focus_ < 0) focus_ = prev ? n : 0;
  else focus_ = std::clamp(focus_ + (prev ? -1 : 1), 0, n);  // rows 0..n-1, then Clear all (n)
  if (focus_ < n) page_ = focus_ / rpp;
  else page_ = pageCount(n, rpp) - 1;
  r.changed = focus_ != old;
  return r;
}

bool NotifPopup::tick(uint32_t now) {
  if (mode_ != Mode::Confirm || !dialog_.tick(now)) return false;
  mode_ = Mode::List;
  return true;
}

NotifResult NotifPopup::answered(ConfirmAnswer a, const NotifStore &store) {
  NotifResult r;
  if (a == ConfirmAnswer::None) return r;
  if (a == ConfirmAnswer::Yes) return confirmed(store);
  mode_ = Mode::List;  // No / Skip
  r.changed = true;
  return r;
}

}  // namespace uistate
