#include "secure_widget.h"

#include <algorithm>

#include "alarm_popup.h"
#include "confirm_dialog.h"
#include "secure_view.h"

namespace render {

namespace {

using ui::Rect;
using uistate::Look;
using uistate::SecureIcon;

constexpr int kNarrowW = 200;  // below this: F18 text fitted to the width (portrait 3 and 4 columns)

Icon iconOf(SecureIcon i) {
  switch (i) {
    case SecureIcon::Shield: return Icon::Shield;
    case SecureIcon::ShieldOff: return Icon::ShieldOff;
    case SecureIcon::ShieldAlert: return Icon::ShieldAlert;
    case SecureIcon::Lock: return Icon::LockClosed;
    case SecureIcon::LockOpen: return Icon::LockOpen;
    case SecureIcon::Alert: return Icon::Alert;
    case SecureIcon::GarageOn: return Icon::GarageOn;
    case SecureIcon::GarageOff: return Icon::GarageOff;
    case SecureIcon::Warn: return Icon::Warn;
  }
  return Icon::Warn;
}

void frame(M5Canvas &c, const Rect &r, uint32_t fill, bool dotted) {
  if (dotted) {
    c.fillRoundRect(r.x, r.y, r.w, r.h, 9, PAPER);
    drawDashedBox(c, float(r.x + 1), float(r.y + 1), float(r.w - 2), float(r.h - 2), 8.0f, 2, 2.0f, 4.0f, OFFGREY);
  } else {
    drawBox(c, r, fill, INK);
  }
}

enum class Btn { Outline, Filled, Dotted, Pending };

void button(M5Canvas &c, const Rect &b, Btn kind) {
  if (kind == Btn::Dotted) {
    frame(c, b, PAPER, true);
  } else if (kind == Btn::Pending) {
    c.fillRoundRect(b.x, b.y, b.w, b.h, 9, PAPER);
    drawDashedBox(c, b.x + 1.5f, b.y + 1.5f, float(b.w - 3), float(b.h - 3), 8.0f, 3, 8.0f, 6.0f, INK);
  } else {
    drawBox(c, b, kind == Btn::Filled ? INK : PAPER, INK);
  }
}

uint32_t contrast(Btn kind) { return kind == Btn::Dotted ? OFFGREY : kind == Btn::Filled ? PAPER : INK; }

void focusRing(M5Canvas &c, const Rect &r) { drawRing(c, ui::inflate(r, 4), 3, 13, INK); }

void panel(M5Canvas &c, const Rect &p, const Rect &shadow) {
  c.fillRoundRect(shadow.x, shadow.y, shadow.w, shadow.h, 8, INK);
  c.fillRoundRect(p.x, p.y, p.w, p.h, 8, INK);
  c.fillRoundRect(p.x + 3, p.y + 3, p.w - 6, p.h - 6, 5, PAPER);
}

void centredIcon(M5Canvas &c, const Rect &r, Icon icon, int size, uint32_t color) {
  drawIcon(c, icon, size, r.x + (r.w - size) / 2, r.y + (r.h - size) / 2, color);
}

}  // namespace

void drawSecureWidget(M5Canvas &c, const Rect &r, const uistate::WidgetData &d, const std::string &title) {
  const EntityState *st = d.members.empty() ? nullptr : d.members[0];
  const uistate::SecureView v = uistate::secureView(d.type, st);
  const bool una = v.look == Look::Unavailable;
  const bool fill = v.fill && !una;
  frame(c, r, fill ? INK : PAPER, una);
  const int W = r.w;
  const uint32_t fg = una ? OFFGREY : (fill ? PAPER : INK);
  const bool failed = d.failed || v.alertBadge;
  const bool pending = d.pending && !una && !failed;
  const bool badge = pending || failed;
  const Icon badgeIcon = failed ? Icon::Alert : Icon::Hourglass;
  const uint32_t badgeColor = fill ? PAPER : INK;
  const bool narrow = W < kNarrowW;
  const Font tf = narrow ? Font::F18 : Font::F24;
  drawText(c, tf, fitText(c, tf, title, W - 24), r.x + 12, r.y + (narrow ? 36 : 38), Align::Left, fg);
  drawIcon(c, iconOf(v.icon), 40, r.x + 12, r.y + 49, fg);
  const int stateMax = badge ? W - 36 - 8 - 58 : W - 70;
  drawText(c, tf, fitText(c, tf, v.text, stateMax), r.x + 58, r.y + (narrow ? 75 : 77), Align::Left,
           una ? OFFGREY : (fill ? PAPER : INK));
  if (badge) drawIcon(c, badgeIcon, 24, r.x + W - 36, r.y + 57, badgeColor);
  if (pending)  // dashed 3 px border; PAPER on an INK-filled widget (spec 0)
    drawDashedBox(c, float(r.x + 5), float(r.y + 5), float(r.w - 10), float(r.h - 10), 5.0f, 3, 8.0f, 6.0f,
                  fill ? PAPER : INK);
}

void drawAlarmPopup(M5Canvas &c, const uistate::AppState &app) {
  if (!app.alarmOpen()) return;
  const uistate::AlarmPopup &pop = app.alarm();
  const uistate::AlarmLayout l = uistate::alarmLayout(app.orientation());
  const uistate::AlarmPanelInfo info = app.alarmInfo();
  panel(c, l.panel, l.shadow);
  drawTextCy(c, Font::F24, fitText(c, Font::F24, app.alarmTitle(), l.close.x - l.nameX - 16), l.nameX, l.nameCy,
             Align::Left, INK);
  button(c, l.close, Btn::Outline);
  centredIcon(c, l.close, Icon::Close, 40, INK);
  drawText(c, Font::F40, fitText(c, Font::F40, info.stateLine, l.panel.x + l.panel.w - 32 - l.stateX), l.stateX,
           l.stateBaseline, Align::Left, INK);

  // Code box: dots (drawn: the fonts have no bullet), or the "Code" placeholder.
  drawBox(c, l.codeBox, PAPER, INK);
  if (pop.codeLen() == 0) {
    drawTextCy(c, Font::F24, "Code", l.codeTextX, l.codeCy, Align::Left, DIM);
  } else {
    for (int i = 0; i < pop.codeLen(); ++i) c.fillCircle(l.codeTextX + 8 + 30 * i, l.codeCy, 9, INK);
  }
  drawTextCy(c, Font::F18, uistate::alarmMsgText(pop.message(info)), l.msgX, l.msgCy, Align::Left, INK);
  drawTextCy(c, Font::F18, "UP / DOWN walks the keys, PUSH presses", l.hintX, l.hint1Cy, Align::Left, DIM);
  drawTextCy(c, Font::F18, "Hold PUSH to close", l.hintX, l.hint2Cy, Align::Left, DIM);

  const bool keys = pop.keysActive(info);
  for (int i = 0; i < 12; ++i) {
    const Btn kind = keys ? Btn::Outline : Btn::Dotted;
    button(c, l.keys[i], kind);
    if (i == uistate::kKeyBackspace) centredIcon(c, l.keys[i], Icon::Back, 40, contrast(kind));
    else {
      const char *label = i == uistate::kKeyClear ? "C" : nullptr;
      char digit[2] = {char('0' + (i == uistate::kKeyZero ? 0 : i + 1)), 0};
      drawTextCy(c, Font::F40, label ? label : digit, l.keys[i].x + l.keys[i].w / 2, l.keys[i].y + l.keys[i].h / 2,
                 Align::Center, contrast(kind));
    }
  }
  static const char *const kModeLabels[4] = {"Disarm", "Home", "Away", "Night"};
  for (int m = 0; m < 4; ++m) {
    const bool active = pop.modeActive(m, info);
    const Btn kind = !active ? Btn::Dotted : pop.awaitingMode() == m ? Btn::Pending
                     : info.currentMode == m ? Btn::Filled : Btn::Outline;
    button(c, l.modes[m], kind);
    if (kind == Btn::Pending) centredIcon(c, l.modes[m], Icon::Hourglass, 40, INK);
    else drawTextCy(c, Font::F24, kModeLabels[m], l.modes[m].x + l.modes[m].w / 2, l.modes[m].y + l.modes[m].h / 2,
                    Align::Center, contrast(kind));
  }
  if (pop.ringVisible()) {
    const int f = pop.focus();
    focusRing(c, f == uistate::kStopClose ? l.close : f >= uistate::kStopKeys ? l.modes[f - uistate::kStopKeys] : l.keys[f]);
  }
}

void drawConfirmDialog(M5Canvas &c, const uistate::AppState &app) {
  if (!app.confirmOpen()) return;
  const uistate::ConfirmLayout l = uistate::confirmLayout(app.orientation());
  panel(c, l.panel, l.shadow);
  drawTextCy(c, Font::F24, fitText(c, Font::F24, app.confirm().title(), l.panel.w - 40), l.titleCx, l.titleCy,
             Align::Center, INK);
  drawTextCy(c, Font::F18, "Skips by itself after 15 s", l.titleCx, l.subCy, Align::Center, INK);
  struct B { const Rect &r; const char *label, *key; };
  const B bs[3] = {{l.yes, "Yes", "UP"}, {l.no, "No", "DOWN"}, {l.skip, "Skip", "PUSH"}};
  for (const B &b : bs) {
    button(c, b.r, Btn::Outline);
    drawTextCy(c, Font::F24, b.label, b.r.x + b.r.w / 2, b.r.y + 19, Align::Center, INK);
    drawTextCy(c, Font::F18, b.key, b.r.x + b.r.w / 2, b.r.y + 48, Align::Center, INK);
  }
}

}  // namespace render
