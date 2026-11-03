#include "screen.h"

#include <algorithm>
#include <cstdio>

#include "brightness_screen.h"
#include "graph_widget.h"
#include "notif_widget_draw.h"
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

// Icon from the 24 px bitmap scaled to dst px (area average); the pill icons are 20 and 18 px (STYLE.md 5.6).
void drawIconScaled(M5Canvas &c, Icon icon, int dst, int x, int y, uint32_t color) {
  const uint8_t *bmp = iconBitmap(icon, 24);
  if (!bmp) return;
  uint8_t *buf = static_cast<uint8_t *>(c.getBuffer());
  const int W = c.width(), H = c.height();
  const int target = int(color & 0xFF);
  for (int row = 0; row < dst; ++row) {
    const int py = y + row;
    if (py < 0 || py >= H) continue;
    const int r0 = row * 24 / dst, r1 = std::max(r0 + 1, (row + 1) * 24 / dst);
    for (int col = 0; col < dst; ++col) {
      const int px = x + col;
      if (px < 0 || px >= W) continue;
      const int c0 = col * 24 / dst, c1 = std::max(c0 + 1, (col + 1) * 24 / dst);
      int sum = 0, n = 0;
      for (int rr = r0; rr < r1; ++rr)
        for (int cc = c0; cc < c1; ++cc) { sum += bmp[rr * 24 + cc]; ++n; }
      const int a = sum / n;
      if (!a) continue;
      uint8_t &d = buf[py * W + px];
      d = uint8_t(d + ((target - int(d)) * a + (a >> 1)) / 255);
    }
  }
}

// The built-in fonts have no bold: draw the word twice, 1 px apart in x.
void drawBold(M5Canvas &c, const std::string &s, int x, int cy, Align al, uint32_t color) {
  drawTextCy(c, Font::F18, s, x, cy, al, color);
  drawTextCy(c, Font::F18, s, x + 1, cy, al, color);
}

// STYLE.md 5.6: pill 104 x 44 (the badge region minus 2 px each side). Asleep: filled INK, icon + mode word on line
// 1, refresh icon + next update time or "off" on line 2. Awake (portrait): outline pill, AWAKE / on USB | always.
ui::Rect pillRect(Orientation o) {
  const ui::Rect r = ui::sleepBadgeRect(o);
  return {r.x + 2, r.y + 2, 104, 44};
}

void drawSleepPill(M5Canvas &c, Orientation o, int kind, int nextMin) {
  if (kind < 1 || kind > 3) return;
  const ui::Rect p = pillRect(o);
  c.fillRoundRect(p.x, p.y, p.w, p.h, 12, INK);
  const Icon ic = kind == 1 ? Icon::Power : kind == 2 ? Icon::Moon : Icon::Finger;
  const char *word = kind == 1 ? "OFF" : kind == 2 ? "DEEP" : "TAP";
  drawIconScaled(c, ic, 20, p.x + 10, p.y + 4, PAPER);
  drawBold(c, word, p.x + 36, p.y + 14, Align::Left, PAPER);
  drawIconScaled(c, Icon::Refresh, 18, p.x + 11, p.y + 23, PAPER);
  char buf[8];
  if (nextMin >= 0) std::snprintf(buf, sizeof buf, "%02d:%02d", nextMin / 60, nextMin % 60);
  else std::snprintf(buf, sizeof buf, "off");
  drawTextCy(c, Font::F18, buf, p.x + 36, p.y + 32, Align::Left, PAPER);
}

// Keep-awake indicator (device awake, will not idle-sleep). Portrait: outline pill in the badge region.
// Landscape: caption under the battery cluster (tab slot 5 stays visible); part of the Settings hit area.
void drawAwake(M5Canvas &c, Orientation o, int kind) {
  if (kind < 1 || kind > 2) return;
  if (o == Orientation::Landscape) {
    drawTextCy(c, Font::F18, kind == 1 ? "USB awake" : "always on", 944, 53, Align::Right, INK);
    return;
  }
  const ui::Rect p = pillRect(o);
  c.fillRoundRect(p.x, p.y, p.w, p.h, 12, INK);
  c.fillRoundRect(p.x + 2, p.y + 2, p.w - 4, p.h - 4, 10, PAPER);
  drawBold(c, "AWAKE", p.x + p.w / 2 - textWidth(c, Font::F18, "AWAKE") / 2, p.y + 14, Align::Left, INK);
  const char *l2 = kind == 1 ? "on USB" : "always";
  drawTextCy(c, Font::F18, l2, p.x + p.w / 2, p.y + 32, Align::Center, INK);
}

}  // namespace

void drawSleepBadgeOverlay(M5Canvas &c, settings::Orientation o, int kind, int nextMin) {
  drawSleepPill(c, o, kind, nextMin);
}

void renderScreen(M5Canvas &c, const uistate::AppState &app, const StatusInfo &status) {
  c.fillScreen(PAPER);
  if (app.screen() == uistate::Screen::ConfigError) {
    drawErrorScreen(c, app);
    if (status.sleepKind) drawSleepPill(c, app.orientation(), status.sleepKind, status.sleepNextMin);
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
    if (pl.graph) drawGraphWidget(c, pl, app);
    else drawWidget(c, pl, app.widgetData(pl.index), app.widgetTitle(pl.index), inner ? app.focus().inner : 0);
  }
  drawPager(c, app);
  drawTabBar(c, app);
  drawBrightnessPopup(c, app);
  drawHistoryPopup(c, app);
  drawNotifPopup(c, app);  // task 103
  if (status.sleepKind) drawSleepPill(c, app.orientation(), status.sleepKind, status.sleepNextMin);
  else if (status.awakeKind) drawAwake(c, app.orientation(), status.awakeKind);
}

}  // namespace render
