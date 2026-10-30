#include "screen.h"

#include <algorithm>

#include "brightness_screen.h"
#include "popup_history.h"
#include "widgets.h"

namespace render {

namespace {

using settings::Orientation;

void drawStatusBar(M5Canvas &c, const uistate::AppState &app, const StatusInfo &st) {
  const bool land = app.orientation() == Orientation::Landscape;
  const int cy = land ? 32 : 27;
  if (st.mockLabel) {
    drawIcon(c, Icon::Wifi, 24, 16, land ? 20 : 15, INK);
    if (land) {
      drawTextCy(c, Font::F18, "updated", 56, 21, Align::Left, INK);
      drawTextCy(c, Font::F18, "14:05", 56, 43, Align::Left, INK);
    } else {
      drawTextCy(c, Font::F18, "updated 14:05", 48, cy, Align::Left, INK);
    }
  } else if (st.link != Link::None) {
    const Icon icon = st.link == Link::Online           ? Icon::Wifi
                      : st.link == Link::ConnectingWifi ? Icon::WifiJoin
                      : st.link == Link::Fetching       ? Icon::CloudDown
                      : st.link == Link::AuthError      ? Icon::Lock
                      : st.link == Link::Demo           ? Icon::Demo
                                                        : Icon::WifiOff;
    drawIcon(c, icon, 24, 16, land ? 20 : 15, INK);
    std::string l1 = st.label1;
    if (!land && textWidth(c, Font::F18, l1) > 206 && l1.compare(0, 8, "updated ") == 0) l1 = l1.substr(8);
    if (land) {
      if (st.label2.empty()) {
        drawTextCy(c, Font::F18, l1, 56, 32, Align::Left, INK);
      } else {
        drawTextCy(c, Font::F18, l1, 56, 21, Align::Left, INK);
        drawTextCy(c, Font::F18, st.label2, 56, 43, Align::Left, INK);
      }
    } else {
      drawTextCy(c, Font::F18, l1, 48, cy, Align::Left, INK);
    }
    if (st.outdated) drawIcon(c, Icon::Clock, 24, land ? 136 : 260, land ? 20 : 15, INK);
  } else if (app.demo()) {
    drawTextCy(c, Font::F18, "demo mode", 16, cy, Align::Left, INK);
  }
  // Battery: explicit rectangles (STYLE.md 5.4).
  const std::string pctText = std::to_string(std::clamp(st.batteryPct, 0, 100)) + "%";
  const int pctRight = land ? 944 : 524;
  // STYLE.md 5.1: the percent sits right of the nub with a 4 px gap (nub ends at bx + 40, text rect
  // starts 4 px later). The built-in font is wider than the mockup's: if "100%" would come closer,
  // the icon moves left instead of the text overlapping it.
  const int pctLeft = pctRight - textWidth(c, Font::F18, pctText);
  const int by = land ? 22 : 17;
  const int bx = std::min(land ? 848 : 428, pctLeft - 4 - 40);
  c.fillRect(bx, by, 36, 20, INK);
  c.fillRect(bx + 2, by + 2, 32, 16, PAPER);
  const int fw = int((32 * std::clamp(st.batteryPct, 0, 100) + 50) / 100);
  c.fillRect(bx + 2, by + 2, fw, 16, INK);
  c.fillRect(bx + 36, by + 5, 4, 10, INK);
  drawTextCy(c, Font::F18, pctText, pctRight, cy, Align::Right, INK);
  c.fillRect(0, land ? 65 : 54, land ? 960 : 540, 2, INK);  // landscape rule y=65..66, below the 64 px bar
}

void drawTabBar(M5Canvas &c, const uistate::AppState &app) {
  const auto &lay = app.layout();
  const bool land = app.orientation() == Orientation::Landscape;
  if (!land) c.fillRect(0, 880, 540, 2, INK);
  const int activeTab = app.focus().tab;
  for (const ui::TabSlot &s : lay.tabSlots) {
    const ui::Rect r = s.rect;
    if (s.kind == ui::SlotKind::Empty) continue;
    if (s.kind == ui::SlotKind::Overflow) {
      drawBox(c, r, PAPER, INK);
      drawIcon(c, Icon::Dots, 24, r.x + 28, r.y + 16, INK);
      drawTextCy(c, Font::F18, std::to_string(lay.group + 1) + "/" + std::to_string(lay.groupCount), r.x + 60,
                 r.y + 28, Align::Left, INK);
      continue;
    }
    const bool active = s.tabIndex == activeTab;
    drawBox(c, r, active ? INK : PAPER, INK);
    const uint32_t fg = active ? PAPER : INK;
    const std::string &title = app.config().tabs[size_t(s.tabIndex)].title;
    auto lines = wrapText(c, Font::F18, title, r.w - 12, 2);
    const int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    if (lines.size() >= 2) {
      drawTextCy(c, Font::F18, lines[0], cx, cy - 11, Align::Center, fg);
      drawTextCy(c, Font::F18, lines[1], cx, cy + 11, Align::Center, fg);
    } else if (!lines.empty()) {
      drawTextCy(c, Font::F18, lines[0], cx, cy, Align::Center, fg);
    }
    // Tab focus frame (STYLE.md 6): 2 px PAPER frame inside the active tab, inset 4, radius 4.
    if (active && app.focus().ringVisible && app.focus().mode == ui::FocusMode::Tab) {
      c.fillRoundRect(r.x + 4, r.y + 4, r.w - 8, r.h - 8, 4, PAPER);
      c.fillRoundRect(r.x + 6, r.y + 6, r.w - 12, r.h - 12, 2, INK);
      // the label sits on top of the frame
      if (lines.size() >= 2) {
        drawTextCy(c, Font::F18, lines[0], cx, cy - 11, Align::Center, PAPER);
        drawTextCy(c, Font::F18, lines[1], cx, cy + 11, Align::Center, PAPER);
      } else if (!lines.empty()) {
        drawTextCy(c, Font::F18, lines[0], cx, cy, Align::Center, PAPER);
      }
    }
  }
}

void drawPager(M5Canvas &c, const uistate::AppState &app) {
  const ui::Pager &p = app.layout().pager;
  if (!p.visible) return;
  auto button = [&](const ui::Rect &r, Icon chev, bool available) {
    if (available) {
      drawBox(c, r, PAPER, INK);
      drawIcon(c, chev, 40, r.x + (r.w - 40) / 2, r.y + (r.h - 40) / 2, INK);
    } else {
      c.fillRoundRect(r.x, r.y, r.w, r.h, 9, PAPER);
      drawDashedBox(c, float(r.x + 1), float(r.y + 1), float(r.w - 2), float(r.h - 2), 8.0f, 2, 2.0f, 4.0f, OFFGREY);
      drawIcon(c, chev, 40, r.x + (r.w - 40) / 2, r.y + (r.h - 40) / 2, OFFGREY);
    }
  };
  button(p.prev, Icon::ChevL, p.subPage > 0);
  button(p.next, Icon::ChevR, p.subPage + 1 < p.subPageCount);
  drawTextCy(c, Font::F40, std::to_string(p.subPage + 1) + " / " + std::to_string(p.subPageCount),
             p.label.x + p.label.w / 2, p.label.y + p.label.h / 2, Align::Center, INK);
}

void drawErrorScreen(M5Canvas &c, const uistate::AppState &app) {
  const int W = c.width();
  const int m = 24;
  int y = 80;
  drawText(c, Font::F40, "Config error", m, y + 14, Align::Left, INK);
  c.fillRect(m, y + 40, W - 2 * m, 2, INK);
  y += 80;
  const auto &errs = app.errors();
  if (!errs.empty()) {
    const config::ConfigError &e = errs[0];
    drawText(c, Font::F24, "Where", m, y + 8, Align::Left, DIM);
    y += 32;
    std::string where = e.path.empty() ? "(whole file)" : e.path;
    for (auto &ln : wrapText(c, Font::F24, where, W - 2 * m, 3)) {
      drawText(c, Font::F24, ln, m, y + 8, Align::Left, INK);
      y += 34;
    }
    y += 12;
    drawText(c, Font::F24, "Line", m, y + 8, Align::Left, DIM);
    y += 32;
    drawText(c, Font::F24, e.line > 0 ? std::to_string(e.line) : "unknown", m, y + 8, Align::Left, INK);
    y += 46;
    drawText(c, Font::F24, "Problem", m, y + 8, Align::Left, DIM);
    y += 32;
    for (auto &ln : wrapText(c, Font::F24, e.message, W - 2 * m, 4)) {
      drawText(c, Font::F24, ln, m, y + 8, Align::Left, INK);
      y += 34;
    }
    y += 12;
    if (errs.size() > 1) {
      drawText(c, Font::F18, "+" + std::to_string(errs.size() - 1) + " more error(s)", m, y + 6, Align::Left, DIM);
      y += 30;
    }
  }
  y += 24;
  for (auto &ln : wrapText(c, Font::F18, "Fix config.yaml on the SD card and restart the device.", W - 2 * m, 2)) {
    drawText(c, Font::F18, ln, m, y + 6, Align::Left, INK);
    y += 26;
  }
}

// STYLE.md 5.5: filled INK rounded rect 96x44 r12 centred in the badge region, PAPER "Zz" centred.
// The built-in fonts have no bold, so the label (DejaVu24, the largest below the spec's 30 px bold) is
// drawn three times, 1 px apart in x, and twice in y.
void drawSleepBadge(M5Canvas &c, Orientation o) {
  const ui::Rect r = ui::sleepBadgeRect(o);
  const int cx = r.x + r.w / 2;
  const int cy = r.y + r.h / 2;
  c.fillRoundRect(cx - 48, cy - 22, 96, 44, 12, INK);
  const int w = textWidth(c, Font::F24, "Zz");
  const int x = cx - w / 2;
  for (int dx = -1; dx <= 1; ++dx)
    for (int dy = 0; dy <= 1; ++dy) drawTextCy(c, Font::F24, "Zz", x + dx, cy + 1 + dy - 1, Align::Left, PAPER);
}

}  // namespace

void drawSleepBadgeOverlay(M5Canvas &c, settings::Orientation o) { drawSleepBadge(c, o); }

void renderScreen(M5Canvas &c, const uistate::AppState &app, const StatusInfo &status) {
  c.fillScreen(PAPER);
  if (app.screen() == uistate::Screen::ConfigError) {
    drawErrorScreen(c, app);
    if (status.sleepBadge) drawSleepBadge(c, app.orientation());
    return;
  }
  const int ringIdx = app.focusedWidgetIndex();
  // The widget focus ring goes first: it lies in the gaps, 3..6 px outside the widget.
  if (ringIdx >= 0 && app.focus().ringVisible) {
    for (const ui::Placement &pl : app.layout().widgets) {
      if (pl.index != ringIdx) continue;
      drawRing(c, ui::inflate(pl.rect, 6), 3, 15, INK);
      c.fillRoundRect(pl.rect.x - 3, pl.rect.y - 3, pl.rect.w + 6, pl.rect.h + 6, 12, PAPER);
    }
  }
  drawStatusBar(c, app, status);
  for (const ui::Placement &pl : app.layout().widgets) {
    const bool inner = pl.index == ringIdx && app.focus().ringVisible && app.focus().mode == ui::FocusMode::Inner;
    drawWidget(c, pl, app.widgetData(pl.index), app.widgetTitle(pl.index), inner ? app.focus().inner : 0);
  }
  drawPager(c, app);
  drawTabBar(c, app);
  drawBrightnessPopup(c, app);
  drawHistoryPopup(c, app);
  if (status.sleepBadge) drawSleepBadge(c, app.orientation());
}

}  // namespace render
