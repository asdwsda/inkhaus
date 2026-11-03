#include "notif_widget_draw.h"

#include <time.h>

#include <algorithm>

#include "iso8601.h"
#include "notif_popup.h"

namespace render {

using ui::Rect;
using uistate::Look;

namespace {

// Local UTC offset now (from the TZ the config set), in seconds. 0 until the clock is set.
int32_t utcOffsetNow(int64_t now) {
  if (now < 1000000000) return 0;
  const time_t t = time_t(now);
  struct tm lt;
  localtime_r(&t, &lt);
  const int64_t localAsUtc =
      history::daysFromCivil(lt.tm_year + 1900, unsigned(lt.tm_mon + 1), unsigned(lt.tm_mday)) * 86400 +
      lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec;
  return int32_t(localAsUtc - now);
}

void frame(M5Canvas &c, const Rect &r, uint32_t fill, bool dotted) {
  if (dotted) {
    c.fillRoundRect(r.x, r.y, r.w, r.h, 9, PAPER);
    drawDashedBox(c, float(r.x + 1), float(r.y + 1), float(r.w - 2), float(r.h - 2), 8.0f, 2, 2.0f, 4.0f, OFFGREY);
  } else {
    drawBox(c, r, fill, INK);
  }
}

// Outline button; dotted OFF when inert.
void button(M5Canvas &c, const Rect &r, bool inert) { frame(c, r, PAPER, inert); }

void glyphIn(M5Canvas &c, const Rect &r, Icon icon, int size, uint32_t color) {
  drawIcon(c, icon, size, r.x + (r.w - size) / 2, r.y + (r.h - size) / 2, color);
}

void focusRing(M5Canvas &c, const Rect &r) { drawRing(c, ui::inflate(r, 4), 3, 13, INK); }

void panel(M5Canvas &c, const Rect &p, const Rect &shadow) {
  c.fillRoundRect(shadow.x, shadow.y, shadow.w, shadow.h, 8, INK);
  c.fillRoundRect(p.x, p.y, p.w, p.h, 8, INK);
  c.fillRoundRect(p.x + 3, p.y + 3, p.w - 6, p.h - 6, 5, PAPER);
}

void drawRow(M5Canvas &c, const uistate::NotifPopupLayout &l, int slot, const NotifItem &it,
             uistate::NotifRowState st, bool focused, int64_t now, int32_t off) {
  const Rect &r = l.rows[slot];
  drawBox(c, r, PAPER, INK);
  const std::string title = it.title.empty() ? "Notification" : it.title;
  drawTextCy(c, Font::F24, fitText(c, Font::F24, title, l.rowTitleMaxW), r.x + l.rowTitleDx, r.y + l.rowTitleCy,
             Align::Left, INK);
  drawTextCy(c, Font::F18, uistate::notifTimeText(it.created, now, off), r.x + l.rowTimeRightDx, r.y + l.rowTitleCy,
             Align::Right, INK);
  drawTextCy(c, Font::F18, fitText(c, Font::F18, it.message, l.rowMsgMaxW), r.x + l.rowTitleDx, r.y + l.rowMsgCy,
             Align::Left, INK);
  const Rect d = l.dismiss(slot);
  button(c, d, false);
  const Icon g = st == uistate::NotifRowState::Pending  ? Icon::Hourglass
                 : st == uistate::NotifRowState::Failed ? Icon::Alert
                                                        : Icon::Close;
  glyphIn(c, d, g, 24, INK);
  if (focused) focusRing(c, d);
}

void drawConfirm(M5Canvas &c, const uistate::AppState &app) {
  const uistate::NotifConfirmLayout l = uistate::notifConfirmLayout(app.orientation());
  panel(c, l.panel, l.shadow);
  drawTextCy(c, Font::F24, fitText(c, Font::F24, uistate::notifConfirmTitle(app.notifStore().count()), l.panel.w - 40),
             l.titleCx, l.titleCy, Align::Center, INK);
  drawTextCy(c, Font::F18, "Skips by itself after 15 s", l.titleCx, l.subCy, Align::Center, INK);
  struct B {
    const Rect &r;
    const char *label, *key;
  };
  const B bs[3] = {{l.yes, "Yes", "UP"}, {l.no, "No", "DOWN"}, {l.skip, "Skip", "PUSH"}};
  for (const B &b : bs) {
    button(c, b.r, false);
    drawTextCy(c, Font::F24, b.label, b.r.x + b.r.w / 2, b.r.y + 19, Align::Center, INK);
    drawTextCy(c, Font::F18, b.key, b.r.x + b.r.w / 2, b.r.y + 48, Align::Center, INK);
  }
}

}  // namespace

void drawNotifWidget(M5Canvas &c, const Rect &r, const uistate::NotifView &v) {
  const bool una = v.look == Look::Unavailable;
  const bool fill = v.fill && !una;
  frame(c, r, fill ? INK : PAPER, una);
  const uint32_t fg = una ? OFFGREY : (fill ? PAPER : INK);
  const int W = r.w;
  drawIcon(c, Icon::Bell, 48, r.x + 12, r.y + 28, fg);
  drawText(c, Font::F40, fitText(c, Font::F40, v.count, W - 76), r.x + 64, r.y + 53, Align::Left, fg);
  drawTextCy(c, Font::F18, fitText(c, Font::F18, v.name, W - 76), r.x + 64, r.y + 78, Align::Left,
             una ? OFFGREY : (fill ? PAPER : DIM));
  if (una) drawIcon(c, Icon::Warn, 24, r.x + W - 36, r.y + 12, OFFGREY);
}

void drawNotifPopup(M5Canvas &c, const uistate::AppState &app) {
  const uistate::NotifPopup &pop = app.notifPopup();
  if (!pop.isOpen()) return;
  const uistate::NotifStore &store = app.notifStore();
  const uistate::NotifPopupLayout l = uistate::notifPopupLayout(app.orientation());
  const int n = store.count();
  const int pages = pop.pageCount(n, l.rowsPerPage);
  const int page = pop.page();
  const int64_t now = time(nullptr);
  const int32_t off = utcOffsetNow(now);
  const int midX = l.rowsArea.x + l.rowsArea.w / 2, midY = l.rowsArea.y + l.rowsArea.h / 2;

  panel(c, l.panel, l.shadow);
  drawTextCy(c, Font::F24, uistate::notifPopupTitle(n), l.titleX, l.titleCy, Align::Left, INK);
  const bool noList = store.state() != uistate::NotifState::Ready || n == 0;
  button(c, l.clearAll, noList);
  drawTextCy(c, Font::F24, "Clear all", l.clearAll.x + l.clearAll.w / 2, l.clearAll.y + l.clearAll.h / 2,
             Align::Center, noList ? OFFGREY : INK);
  button(c, l.prev, page <= 0);
  glyphIn(c, l.prev, Icon::ChevL, 40, page <= 0 ? OFFGREY : INK);
  button(c, l.next, page >= pages - 1);
  glyphIn(c, l.next, Icon::ChevR, 40, page >= pages - 1 ? OFFGREY : INK);
  button(c, l.close, false);
  glyphIn(c, l.close, Icon::Close, 40, INK);

  if (store.state() == uistate::NotifState::Loading) {
    drawTextCy(c, Font::F24, "Loading", midX, midY, Align::Center, INK);
  } else if (store.state() == uistate::NotifState::Unavailable) {
    drawTextCy(c, Font::F24, "Not available", midX, midY, Align::Center, INK);
  } else if (n == 0) {
    drawTextCy(c, Font::F24, "No notifications", midX, midY, Align::Center, INK);
  } else {
    for (int i = 0; i < l.rowsPerPage; ++i) {
      const int idx = page * l.rowsPerPage + i;
      if (idx >= n) break;
      const NotifItem &it = store.items()[size_t(idx)];
      drawRow(c, l, i, it, pop.rowState(it.id), pop.focus() == idx, now, off);
    }
    if (pop.focus() == n) focusRing(c, l.clearAll);
  }
  drawTextCy(c, Font::F18, uistate::notifFooterText(page, l.rowsPerPage, n), l.footerCountX, l.footerCy, Align::Left,
             INK);
  if (l.footerHintRight > 0)
    drawTextCy(c, Font::F18, "UP / DOWN select, PUSH dismiss, hold to close", l.footerHintRight, l.footerCy,
               Align::Right, DIM);

  if (pop.mode() == uistate::NotifPopup::Mode::Confirm) drawConfirm(c, app);
}

}  // namespace render
