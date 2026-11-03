#include "calendar_widget.h"

#include "cal_events.h"

namespace render {

void drawCalendarWidget(M5Canvas &c, const ui::Rect &r, const uistate::WidgetData &d) {
  const EntityState *st = d.members.empty() ? nullptr : d.members[0];
  const calendar::CalendarView v = calendar::calendarView(st, d.calendar, d.nowMin);
  const calendar::Layout l = calendar::layoutFor(r.w);
  auto X = [&](int dx) { return r.x + dx; };
  auto Y = [&](int dy) { return r.y + dy; };

  if (v.look == uistate::Look::Unavailable) {
    c.fillRoundRect(r.x, r.y, r.w, r.h, 9, PAPER);
    drawDashedBox(c, float(r.x + 1), float(r.y + 1), float(r.w - 2), float(r.h - 2), 8.0f, 2, 2.0f, 4.0f, OFFGREY);
    drawTextCy(c, Font::F24, "n/a", X(l.unavailX), Y(l.unavailCy), Align::Left, INK);
    drawIcon(c, Icon::Warn, 24, X(l.badgeX), Y(l.badgeY), INK);
    return;
  }
  drawBox(c, r, PAPER, INK);
  if (v.empty) {
    drawTextCy(c, Font::F24, calendar::kEmptyTitle, X(l.timeX), Y(l.cy[0]), Align::Left, INK);
    drawTextCy(c, Font::F18, calendar::kEmptySub, X(l.timeX), Y(l.cy[1]), Align::Left, INK);
    return;
  }
  for (int i = 0; i < calendar::kRows; ++i) {
    const calendar::RowView &row = v.rows[i];
    if (row.time.empty()) continue;
    drawTextCy(c, Font::F18, row.time, X(l.timeX), Y(l.cy[i]), Align::Left, INK);
    if (!row.title.empty())
      drawTextCy(c, Font::F24, fitText(c, Font::F24, row.title, l.titleMaxW), X(l.titleX), Y(l.cy[i]), Align::Left,
                 INK);
  }
}

}  // namespace render
