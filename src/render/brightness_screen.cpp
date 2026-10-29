#include "brightness_screen.h"

#include <algorithm>

#include "brightness_layout.h"

namespace render {

namespace {

using ui::Rect;

// Outline button; filled (INK, PAPER text) means "current"; disabled uses the unavailable style.
void buttonFrame(M5Canvas &c, const Rect &r, bool filled, bool disabled) {
  if (disabled) {
    c.fillRoundRect(r.x, r.y, r.w, r.h, 9, PAPER);
    drawDashedBox(c, float(r.x + 1), float(r.y + 1), float(r.w - 2), float(r.h - 2), 8.0f, 2, 2.0f, 4.0f, OFFGREY);
  } else {
    drawBox(c, r, filled ? INK : PAPER, INK);
  }
}

void labelButton(M5Canvas &c, const Rect &r, const std::string &text, bool filled) {
  buttonFrame(c, r, filled, false);
  drawTextCy(c, Font::F24, text, r.x + r.w / 2, r.y + r.h / 2, Align::Center, filled ? PAPER : INK);
}

// minus/plus: icon (40) + 8 px + "10 %" centred as a group.
void stepButton(M5Canvas &c, const Rect &r, Icon icon, bool disabled) {
  buttonFrame(c, r, false, disabled);
  const uint32_t fg = disabled ? OFFGREY : INK;
  const int tw = textWidth(c, Font::F24, "10 %");
  const int group = 40 + 8 + tw;
  const int x = r.x + (r.w - group) / 2;
  const int cy = r.y + r.h / 2;
  drawIcon(c, icon, 40, x, cy - 20, fg);
  drawTextCy(c, Font::F24, "10 %", x + 48, cy, Align::Left, fg);
}

}  // namespace

void drawBrightnessPopup(M5Canvas &c, const uistate::AppState &app) {
  if (!app.brightnessOpen()) return;
  const uistate::BrightnessLayout l = uistate::brightnessLayout(app.orientation());
  const uistate::BrightnessPopup &pop = app.brightness();
  const int pct = pop.value();
  const bool unknown = app.brightnessUnknown();

  // Shadow (6 px solid INK, right and bottom), then the framed panel.
  c.fillRoundRect(l.panel.x + 6, l.panel.y + 6, l.panel.w, l.panel.h, 8, INK);
  c.fillRoundRect(l.panel.x, l.panel.y, l.panel.w, l.panel.h, 8, INK);
  c.fillRoundRect(l.panel.x + 3, l.panel.y + 3, l.panel.w - 6, l.panel.h - 6, 5, PAPER);

  drawTextCy(c, Font::F24, fitText(c, Font::F24, app.brightnessTitle(), l.nameMaxW), l.nameX, l.nameCy, Align::Left,
             INK);
  buttonFrame(c, l.close, false, false);
  drawIcon(c, Icon::Close, 40, l.close.x + 12, l.close.y + 12, INK);

  // Percent: "60" F40 + "%" F24 on one baseline; "Off" / "-" in F40.
  const Align pa = l.percentCentered ? Align::Center : Align::Left;
  if (unknown) drawText(c, Font::F40, "-", l.percentX, l.percentBaseline, pa, INK);
  else if (pct == 0) drawText(c, Font::F40, "Off", l.percentX, l.percentBaseline, pa, INK);
  else drawRun(c, Font::F40, std::to_string(pct), Font::F24, "%", l.percentX, l.percentBaseline, pa, INK);

  // Pending slot: hourglass while a change is unsent or unconfirmed, alert after a failed send.
  if (app.brightnessFailed()) drawIcon(c, Icon::Alert, 24, l.pending.x, l.pending.y, INK);
  else if (app.brightnessAwaiting()) drawIcon(c, Icon::Hourglass, 24, l.pending.x, l.pending.y, INK);

  // Bar: border 2, fill inside by 4.
  drawBox(c, l.bar, PAPER, INK);
  const int fwR = unknown ? 0 : int((long(l.bar.w - 8) * pct + 50) / 100);
  if (fwR > 0) c.fillRoundRect(l.bar.x + 4, l.bar.y + 4, fwR, l.bar.h - 8, std::min(4, fwR / 2), INK);

  stepButton(c, l.minus, Icon::Minus, pct <= 0 || unknown);
  stepButton(c, l.plus, Icon::Plus, pct >= 100);
  static const char *const kPresetText[4] = {"25%", "50%", "75%", "100%"};
  for (int i = 0; i < 4; ++i)
    labelButton(c, l.presets[i], kPresetText[i], !unknown && pct == uistate::kBrightnessPresets[i]);
  labelButton(c, l.off, "Off", !unknown && pct == 0);
  labelButton(c, l.done, "Done", false);
}

}  // namespace render
