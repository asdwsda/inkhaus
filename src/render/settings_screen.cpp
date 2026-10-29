#include "settings_screen.h"

#include <algorithm>

namespace render {

namespace {

namespace sui = settings_ui;
using ui::Rect;

Icon mapIcon(sui::Icon i) {
  switch (i) {
    case sui::Icon::Alert: return Icon::Alert;
    case sui::Icon::Warn: return Icon::Warn;
    case sui::Icon::Bolt: return Icon::Lightning;
    case sui::Icon::Hourglass: return Icon::Hourglass;
    case sui::Icon::None: break;
  }
  return Icon::Unknown;
}

void drawChip(M5Canvas &c, const Rect &r) {
  c.fillRoundRect(r.x, r.y, r.w, r.h, 8, INK);
  drawTextCy(c, Font::F18, "unsaved", r.x + r.w / 2, r.y + r.h / 2, Align::Center, PAPER);
}

void drawPip(M5Canvas &c, int cx, int cy, bool filled) {
  const int d = sui::kPipDiameter;
  c.fillCircle(cx, cy, d / 2, INK);
  if (!filled) c.fillCircle(cx, cy, d / 2 - 2, PAPER);
}

void drawRow(M5Canvas &c, const sui::SettingsScreen &s, int i) {
  const Rect &r = s.layout().rows[i];
  const settings::Settings &e = s.edit();
  drawBox(c, r, PAPER, INK);
  const Rect lab = sui::rowLabelRect(r);
  drawTextCy(c, Font::F24, sui::rowLabel(i), lab.x, sui::rowLabelCy(r), Align::Left, INK);
  drawTextCy(c, Font::F40, sui::rowValueText(i, e), sui::rowValueRightX(r), sui::rowValueCy(r), Align::Right, INK);
  const int cur = sui::rowValueIndex(i, e);
  for (int k = 0; k < sui::pipCount(i); ++k) {
    const ui::Point p = sui::pipCentre(r, k);
    drawPip(c, p.x, p.y, k == cur);
  }
}

void drawNote(M5Canvas &c, const Rect &n) {
  drawBox(c, n, PAPER, INK);
  const Rect ic = sui::noteIconRect(n);
  drawIcon(c, Icon::Alert, 40, ic.x, ic.y, INK);
  drawTextCy(c, Font::F18, sui::kNoteLine1, sui::noteTextX(n), sui::noteLineCy(n, 0), Align::Left, INK);
  drawTextCy(c, Font::F18, sui::kNoteLine2, sui::noteTextX(n), sui::noteLineCy(n, 1), Align::Left, INK);
}

// Fit rule of the spec: F24, then F18, then an ellipsis (own measure on the canvas).
struct Fit {
  std::string text;
  Font font;
};
Fit fit(M5Canvas &c, const std::string &t, int maxW) {
  if (textWidth(c, Font::F24, t) <= maxW) return {t, Font::F24};
  if (textWidth(c, Font::F18, t) <= maxW) return {t, Font::F18};
  return {fitText(c, Font::F18, t, maxW), Font::F18};
}

void drawInfoRow(M5Canvas &c, const sui::InfoRow &r, const char *label, const sui::InfoText &v, bool twoLines) {
  c.fillRect(r.hairline.x, r.hairline.y, r.hairline.w, r.hairline.h, OFFGREY);
  const int cy = twoLines ? (r.cyLine1 + r.cyLine2) / 2 : r.cy;
  drawTextCy(c, Font::F18, label, r.labelX, cy, Align::Left, DIM);
  const bool icon = v.icon != sui::Icon::None;
  const int x = icon ? r.iconX + 32 : r.valueX;
  const int w = r.valueWidth - (icon ? 32 : 0);
  if (!twoLines) {
    if (icon) drawIcon(c, mapIcon(v.icon), 24, r.iconX, cy - 12, INK);
    const Fit f = fit(c, v.text, w);
    drawTextCy(c, f.font, f.text, x, cy, Align::Left, INK);
    return;
  }
  // Config row: up to two lines, icon on line 1; F24 wrapped, else F18 wrapped.
  if (icon) drawIcon(c, mapIcon(v.icon), 24, r.iconX, r.cyLine1 - 12, INK);
  Font f = Font::F24;
  auto lines = wrapText(c, f, v.text, w, 2);
  if (lines.size() == 2 && lines[1].find("\xE2\x80\xA6") != std::string::npos) {
    f = Font::F18;
    lines = wrapText(c, f, v.text, w, 2);
  }
  if (lines.size() < 2) {
    drawTextCy(c, f, lines.empty() ? v.text : lines[0], x, icon ? r.cyLine1 : cy, Align::Left, INK);
  } else {
    drawTextCy(c, f, lines[0], x, r.cyLine1, Align::Left, INK);
    drawTextCy(c, f, lines[1], x, r.cyLine2, Align::Left, INK);
  }
}

void drawButtonBox(M5Canvas &c, const Rect &b, Icon glyph, const char *label, bool pending) {
  if (pending) {
    c.fillRoundRect(b.x, b.y, b.w, b.h, 9, PAPER);
    drawDashedBox(c, b.x + 1.5f, b.y + 1.5f, float(b.w - 3), float(b.h - 3), 8.0f, 3, 8.0f, 6.0f, INK);
  } else {
    drawBox(c, b, PAPER, INK);
  }
  const Rect ic = sui::buttonIconRect(b);
  drawIcon(c, glyph, 40, ic.x, ic.y, INK);
  const Rect lr = sui::buttonLabelRect(b);
  drawTextCy(c, Font::F24, label, lr.x, lr.y + lr.h / 2, Align::Left, INK);
}

}  // namespace

void drawSettings(M5Canvas &c, const sui::SettingsScreen &s, const SettingsInfo &info) {
  const sui::SettingsLayout &L = s.layout();
  c.fillScreen(PAPER);

  // Focus ring first: it lies in the gaps, 3..6 px outside the focused rect (screen.cpp's recipe).
  if (s.ringVisible()) {
    const Rect f = s.focusRect();
    drawRing(c, ui::inflate(f, sui::kRingInflate), 3, 15, INK);
    c.fillRoundRect(f.x - 3, f.y - 3, f.w + 6, f.h + 6, 12, PAPER);
  }

  // Header.
  drawTextCy(c, Font::F40, "Settings", L.title.x, L.title.y + L.title.h / 2, Align::Left, INK);
  if (s.dirty()) drawChip(c, L.unsavedChip);
  c.fillRect(L.headerRule.x, L.headerRule.y, L.headerRule.w, L.headerRule.h, INK);

  for (int i = 0; i < sui::kRowCount; ++i) drawRow(c, s, i);
  if (sui::showsNote(s.edit())) drawNote(c, L.note);

  // Info block.
  const sui::HaInfo ha = [&] {
    sui::HaInfo h = info.ha;
    h.refreshing = s.refreshState() == sui::RefreshState::Running;
    return h;
  }();
  auto txt = [](const std::string &t) { return sui::InfoText{sui::Icon::None, t}; };
  drawInfoRow(c, L.info[0], "Firmware", txt(info.firmware), false);
  drawInfoRow(c, L.info[1], "Battery", sui::batteryText(info.battery), false);
  if (s.saved().demo_mode) {  // saved demo mode: the app runs the embedded demo (spec section 6)
    drawInfoRow(c, L.info[2], "Home Assistant", txt(sui::kDemoHaText), false);
    drawInfoRow(c, L.info[3], "Wi-Fi", txt(sui::kDemoWifiText), false);
    drawInfoRow(c, L.info[4], "Config", txt(sui::kDemoConfigText), true);
    drawInfoRow(c, L.info[5], "Config errors", txt("0"), false);
  } else {
    drawInfoRow(c, L.info[2], "Home Assistant", sui::haText(ha, info.now), false);
    drawInfoRow(c, L.info[3], "Wi-Fi", sui::wifiText(info.wifi), false);
    drawInfoRow(c, L.info[4], "Config", sui::configText(info.config), true);
    drawInfoRow(c, L.info[5], "Config errors", txt(sui::errorsText(info.config)), false);
  }

  // Footer.
  const sui::RefreshState rs = s.refreshState();
  const Icon rg = rs == sui::RefreshState::Running ? Icon::Hourglass
                  : rs == sui::RefreshState::Failed ? Icon::Alert
                                                    : Icon::Refresh;
  drawButtonBox(c, L.refreshButton, rg, "Refresh now", rs == sui::RefreshState::Running);
  drawButtonBox(c, L.backButton, Icon::ChevL, "Back", false);
}

}  // namespace render
