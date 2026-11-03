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

void drawButtonBox(M5Canvas &c, const Rect &b, Icon glyph, const char *label, bool pending);

void drawChip(M5Canvas &c, const Rect &r) {
  c.fillRoundRect(r.x, r.y, r.w, r.h, 8, INK);
  drawTextCy(c, Font::F18, "unsaved", r.x + r.w / 2, r.y + r.h / 2, Align::Center, PAPER);
}

void drawPip(M5Canvas &c, int cx, int cy, bool filled) {
  const int d = sui::kPipDiameter;
  c.fillCircle(cx, cy, d / 2, INK);
  if (!filled) c.fillCircle(cx, cy, d / 2 - 2, PAPER);
}

// Value of a row, right aligned: F40, then F24, then F24 with an ellipsis (spec 3.1).
void drawValue(M5Canvas &c, const std::string &val, int rightX, int cy, int freeW) {
  const int lvl = sui::valueFitLevel(textWidth(c, Font::F40, val), textWidth(c, Font::F24, val), freeW);
  if (lvl == 0) drawTextCy(c, Font::F40, val, rightX, cy, Align::Right, INK);
  else drawTextCy(c, Font::F24, lvl == 1 ? val : fitText(c, Font::F24, val, freeW), rightX, cy, Align::Right, INK);
}

// Firmware row (spec 8.6): look from sui::firmwareRow(); progress bar while downloading.
void drawFirmwareRow(M5Canvas &c, const sui::SettingsScreen &s, int i) {
  const Rect &r = s.pageLayout().rows[i];
  const sui::FirmwareRow v = s.firmwareRow();
  if (v.dashed) {
    c.fillRoundRect(r.x, r.y, r.w, r.h, 9, PAPER);
    drawDashedBox(c, r.x + 1.5f, r.y + 1.5f, float(r.w - 3), float(r.h - 3), 8.0f, 3, 8.0f, 6.0f, INK);
  } else if (v.dotted) {
    c.fillRoundRect(r.x, r.y, r.w, r.h, 9, PAPER);
    drawDashedBox(c, float(r.x + 1), float(r.y + 1), float(r.w - 2), float(r.h - 2), 8.0f, 2, 2.0f, 4.0f, OFFGREY);
  } else if (v.plain) {
    c.fillRect(r.x, r.y, r.w, 1, OFFGREY);
  } else {
    drawBox(c, r, PAPER, INK);
  }
  const int rightX = sui::rowValueRightX(r);
  if (v.bar) {  // label + value + bar, nothing else
    drawTextCy(c, Font::F24, v.label, r.x + 16, sui::hintRowLabelCy(r), Align::Left, INK);
    if (!v.value.empty()) drawTextCy(c, Font::F40, v.value, rightX, r.y + 36, Align::Right, INK);
    const int bw = r.w - 32;
    c.fillRect(r.x + 16, r.y + 56, bw, 16, INK);
    c.fillRect(r.x + 18, r.y + 58, bw - 4, 12, PAPER);
    const int pct = s.firmware().phase == sui::FwPhase::Downloading ? s.firmware().pct : 100;
    c.fillRect(r.x + 18, r.y + 58, (bw - 4) * std::max(0, std::min(100, pct)) / 100, 12, INK);
    return;
  }
  drawTextCy(c, Font::F24, v.label, r.x + 16, sui::hintRowLabelCy(r), Align::Left, v.dotted ? OFFGREY : INK);
  if (v.chip) {
    c.fillRoundRect(r.x + 144, r.y + 16, 72, 32, 8, INK);
    drawTextCy(c, Font::F18, sui::kFwNew, r.x + 144 + 36, r.y + 16 + 16, Align::Center, PAPER);
  }
  int hx = r.x + 16;
  if (v.alert) {
    drawIcon(c, Icon::Alert, 24, r.x + 16, r.y + 50, INK);
    hx = r.x + 48;
  }
  drawTextCy(c, Font::F18, fitText(c, Font::F18, v.hint, r.x + r.w - 16 - 120 - hx), hx, sui::hintRowHintCy(r), Align::Left, INK);
  drawValue(c, v.value, rightX, sui::hintRowValueCy(r), 120);
}

void drawRow(M5Canvas &c, const sui::SettingsScreen &s, int i) {
  if (s.pageLayout().rowIds[i] == sui::RowId::Firmware) {
    drawFirmwareRow(c, s, i);
    return;
  }
  const sui::PageLayout &P = s.pageLayout();
  const Rect &r = P.rows[i];
  const sui::RowId id = P.rowIds[i];
  const settings::Settings &e = s.edit();
  const bool forced = s.rowForced(id);  // spec 8.2: unavailable look (dotted OFF border, grey label, no pips)
  if (forced) {
    c.fillRoundRect(r.x, r.y, r.w, r.h, 9, PAPER);
    drawDashedBox(c, float(r.x + 1), float(r.y + 1), float(r.w - 2), float(r.h - 2), 8.0f, 2, 2.0f, 4.0f, OFFGREY);
  } else {
    drawBox(c, r, PAPER, INK);
  }
  const std::string val = sui::rowValueText(id, e, s.network());
  if (sui::isPipRow(id)) {
    const Rect lab = sui::rowLabelRect(r);
    drawTextCy(c, Font::F24, sui::rowLabel(id), lab.x, sui::rowLabelCy(r), Align::Left, forced ? OFFGREY : INK);
    const int rightX = sui::rowValueRightX(r);
    const int freeW = rightX - (lab.x + textWidth(c, Font::F24, sui::rowLabel(id)) + sui::kValueGap);
    drawValue(c, val, rightX, sui::rowValueCy(r), freeW);
    if (forced) return;
    const int cur = sui::rowValueIndex(id, e);
    for (int k = 0; k < sui::pipCount(id); ++k) {
      const ui::Point p = sui::pipCentre(r, k);
      drawPip(c, p.x, p.y, k == cur);
    }
    return;
  }
  // Hint row (88 high): label, hint (static or live), big value.
  drawTextCy(c, Font::F24, sui::hintRowLabel(id), r.x + 16, sui::hintRowLabelCy(r), Align::Left, INK);
  const std::string hint = id == sui::RowId::Wifi     ? sui::wifiHint(s.network())
                           : id == sui::RowId::HaLink ? sui::haLinkHint(e, s.network())
                           : (id == sui::RowId::SleepMode || id == sui::RowId::KeepAwake)
                               ? sui::sleepRowHint(id, e, s.capabilities())
                               : std::string(sui::hintRowStaticHint(id));
  drawTextCy(c, Font::F18, fitText(c, Font::F18, hint, r.w - 32 - 140), r.x + 16, sui::hintRowHintCy(r), Align::Left, INK);
  drawValue(c, val, sui::rowValueRightX(r), sui::hintRowValueCy(r), sui::hintRowValueWidth());
}

void drawNote(M5Canvas &c, const Rect &n, const char *line1, const char *line2) {
  drawBox(c, n, PAPER, INK);
  const Rect ic = sui::noteIconRect(n);
  drawIcon(c, Icon::Alert, 40, ic.x, ic.y, INK);
  drawTextCy(c, Font::F18, line1, sui::noteTextX(n), sui::noteLineCy(n, 0), Align::Left, INK);
  drawTextCy(c, Font::F18, line2, sui::noteTextX(n), sui::noteLineCy(n, 1), Align::Left, INK);
}

// Battery use block (spec 8.2): hairline, caption (static), level text and bar (live region).
void drawEstimate(M5Canvas &c, const Rect &b, const settings::Settings &e, const settings::Capabilities &caps) {
  c.fillRect(b.x, b.y, b.w, 1, OFFGREY);
  drawTextCy(c, Font::F18, sui::kEstimateCaption, b.x + 16, b.y + 24, Align::Left, DIM);
  const sui::EstimateView v = sui::estimateView(e, caps);
  drawTextCy(c, Font::F40, v.text, b.x + 16, b.y + 60, Align::Left, INK);
  const int pitch = b.w > 480 ? 46 : 42;
  const int segW = pitch - 4;
  for (int i = 0; i < 10; i++) {
    const int x = b.x + 16 + i * pitch;
    if (i < v.level) {
      c.fillRect(x, b.y + 92, segW, 16, INK);
    } else {
      c.fillRect(x, b.y + 92, segW, 16, INK);
      c.fillRect(x + 2, b.y + 94, segW - 4, 12, PAPER);
    }
  }
}

// Category row of the overview (spec 3.0): label, one-line summary of the edit buffer, chevron.
void drawCategory(M5Canvas &c, const sui::SettingsScreen &s, int i) {
  const Rect &r = s.layout().categories[i];
  drawBox(c, r, PAPER, INK);
  drawTextCy(c, Font::F24, sui::categoryLabel(i), sui::categoryLabelX(r), sui::categoryLabelCy(r), Align::Left, INK);
  const int maxW = sui::categorySummaryMaxWidth(r);
  const bool upd = s.updateAvailable();
  std::string sum = sui::categorySummary(i, s.edit(), s.network(), false, settings::kCapabilities, upd);
  if (textWidth(c, Font::F18, sum) > maxW) sum = sui::categorySummary(i, s.edit(), s.network(), true, settings::kCapabilities, upd);
  if (textWidth(c, Font::F18, sum) > maxW) sum = fitText(c, Font::F18, sum, maxW);
  drawTextCy(c, Font::F18, sum, sui::categoryLabelX(r), sui::categorySummaryCy(r), Align::Left, INK);
  const Rect ch = sui::categoryChevronRect(r);
  drawIcon(c, Icon::ChevR, 40, ch.x, ch.y, INK);
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

void drawFocusRing(M5Canvas &c, const sui::SettingsScreen &s) {
  // The ring lies in the gaps, 3..6 px outside the focused rect (screen.cpp's recipe); drawn first.
  if (!s.ringVisible()) return;
  const Rect f = s.focusRect();
  drawRing(c, ui::inflate(f, sui::kRingInflate), 3, 15, INK);
  c.fillRoundRect(f.x - 3, f.y - 3, f.w + 6, f.h + 6, 12, PAPER);
}

void drawHeader(M5Canvas &c, const sui::SettingsScreen &s, const char *title, const Rect &titleR, const Rect &chip,
                const Rect &rule) {
  drawTextCy(c, Font::F40, title, titleR.x, titleR.y + titleR.h / 2, Align::Left, INK);
  if (s.dirty()) drawChip(c, chip);
  c.fillRect(rule.x, rule.y, rule.w, rule.h, INK);
}

// Confirm page (spec 8.6): title, three info rows, the explanation, the alert note, Yes / No.
void drawConfirmPage(M5Canvas &c, const sui::SettingsScreen &s) {
  const sui::PageLayout &P = s.pageLayout();
  const sui::ConfirmLayout C = sui::computeConfirmLayout(P.orientation);
  const sui::FirmwareInfo &f = s.firmware();
  drawHeader(c, s, "Update", P.title, P.unsavedChip, P.headerRule);
  drawTextCy(c, Font::F40, "Install " + f.available + "?", C.title.x, C.title.y + C.title.h / 2, Align::Left, INK);
  const std::string vals[3] = {f.current, f.available, sui::formatSize(f.sizeBytes)};
  const char *labels[3] = {"Installed", "New", "Download"};
  for (int i = 0; i < 3; ++i) {
    const Rect &r = C.infoRows[i];
    c.fillRect(r.x, r.y, r.w, 1, OFFGREY);
    drawTextCy(c, Font::F18, labels[i], r.x, r.y + r.h / 2, Align::Left, DIM);
    drawTextCy(c, Font::F24, vals[i], r.x + 158, r.y + r.h / 2, Align::Left, INK);
  }
  for (int i = 0; i < 3; ++i) drawTextCy(c, Font::F18, sui::kConfirmLines[i], C.infoRows[0].x, C.lineCy[i], Align::Left, INK);
  drawNote(c, C.note, sui::kConfirmNote1, sui::kConfirmNote2);
  for (int i = 0; i < 2; ++i) {
    const Rect &b = C.buttons[i];
    drawBox(c, b, PAPER, INK);
    drawTextCy(c, Font::F24, sui::kConfirmLabels[i], b.x + 16, b.y + 32, Align::Left, INK);
    drawTextCy(c, Font::F18, sui::kConfirmKeys[i], b.x + b.w - 16, b.y + 32, Align::Right, INK);
  }
}

// About row (task 113): plain info look (hairline on top, as the Firmware row when up to date), label + build line.
void drawAboutRow(M5Canvas &c, const Rect &r, const settings_ui::BuildInfo &b) {
  c.fillRect(r.x, r.y, r.w, 1, OFFGREY);
  drawTextCy(c, Font::F24, "About", r.x + 16, sui::hintRowLabelCy(r), Align::Left, INK);
  const std::string line = settings_ui::buildInfoFit(
      b, r.w - 32, [&c](const std::string &t) { return textWidth(c, Font::F18, t); });
  drawTextCy(c, Font::F18, line, r.x + 16, sui::hintRowHintCy(r), Align::Left, INK);
}

void drawCategoryPage(M5Canvas &c, const sui::SettingsScreen &s, const settings_ui::BuildInfo &build) {
  const sui::PageLayout &P = s.pageLayout();
  drawFocusRing(c, s);
  drawHeader(c, s, sui::categoryLabel(static_cast<int>(P.page) - 1), P.title, P.unsavedChip, P.headerRule);
  for (int i = 0; i < P.rowCount; ++i) drawRow(c, s, i);
  if (P.about.w > 0) drawAboutRow(c, P.about, build);
  if (P.caption.w > 0)
    drawTextCy(c, Font::F18, sui::kDisplayCaption, P.caption.x, P.caption.y + P.caption.h / 2, Align::Left, DIM);
  if (P.page == sui::Page::Sleep) {
    const sui::SleepNotes n = sui::sleepNotes(s.edit(), s.capabilities());
    for (int i = 0; i < n.count; ++i) drawNote(c, i == 0 ? P.note : P.note2, n.note[i].line1, n.note[i].line2);
    drawEstimate(c, P.estimate, s.edit(), s.capabilities());
  } else if (P.note.w > 0 && sui::pageShowsNote(P.page, s.edit(), s.network())) {
    const sui::NoteText t = sui::pageNote(P.page, s.sdPresent());
    drawNote(c, P.note, t.line1, t.line2);
  }
  drawButtonBox(c, P.backButton, Icon::ChevL, "Back", false);
}

}  // namespace

void drawSettings(M5Canvas &c, const sui::SettingsScreen &s, const SettingsInfo &info) {
  const sui::SettingsLayout &L = s.layout();
  c.fillScreen(PAPER);
  if (s.page() == sui::Page::Confirm) {
    drawConfirmPage(c, s);
    return;
  }
  if (s.page() != sui::Page::Main) {
    drawCategoryPage(c, s, info.build);
    return;
  }
  drawFocusRing(c, s);
  drawHeader(c, s, "Settings", L.title, L.unsavedChip, L.headerRule);

  for (int i = 0; i < sui::kCategoryCount; ++i) drawCategory(c, s, i);

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
  drawButtonBox(c, L.sleepNowButton, Icon::Moon, sui::kSleepNowLabel, false);
  drawTextCy(c, Font::F18, sui::kSleepNowHint, L.sleepNowButton.x + L.sleepNowButton.w - 16,
             L.sleepNowButton.y + L.sleepNowButton.h / 2, Align::Right, INK);
  drawButtonBox(c, L.refreshButton, rg, "Refresh now", rs == sui::RefreshState::Running);
  drawButtonBox(c, L.backButton, Icon::ChevL, "Back", false);
}

}  // namespace render
