#include "energy_widget.h"

namespace render {

namespace {
constexpr uint32_t kTrack = 0xCCCCCCU;  // STYLE.md TRACK: the empty part of a progress bar (static, never redrawn alone)
}

void drawEnergyWidget(M5Canvas &c, const ui::Rect &r, const uistate::EnergyView &v) {
  using uistate::EnergyRowState;
  const bool una = v.look == uistate::Look::Unavailable;
  if (una) {
    c.fillRoundRect(r.x, r.y, r.w, r.h, 9, PAPER);
    drawDashedBox(c, float(r.x + 1), float(r.y + 1), float(r.w - 2), float(r.h - 2), 8.0f, 2, 2.0f, 4.0f, OFFGREY);
  } else {
    drawBox(c, r, PAPER, INK);
  }
  const uistate::EnergyLayout l = uistate::energyLayout(r.w, v.rows);
  if (una) {
    drawTextCy(c, Font::F24, "n/a", r.x + l.naRight, r.y + l.naCy, Align::Right, OFFGREY);
    drawIcon(c, Icon::Warn, 24, r.x + l.badgeX, r.y + l.badgeY, OFFGREY);
    return;
  }
  static const char *kLabels[2] = {"Used", "Solar"};
  for (int i = 0; i < 2; ++i) {
    const uistate::EnergyRowView &row = v.row[i];
    if (row.state == EnergyRowState::Hidden) continue;
    const uistate::EnergyRowRect &rr = l.row[i];
    drawTextCy(c, Font::F18, kLabels[i], r.x + rr.labelX, r.y + rr.cy, Align::Left, INK);
    c.fillRect(r.x + rr.track.x, r.y + rr.track.y, rr.track.w, rr.track.h, kTrack);
    if (row.fillW > 0) c.fillRect(r.x + rr.track.x, r.y + rr.track.y, row.fillW, rr.track.h, INK);
    drawTextCy(c, Font::F18, row.text, r.x + rr.valueRight, r.y + rr.cy, Align::Right, INK);
  }
}

}  // namespace render
