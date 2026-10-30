#include "brightness_layout.h"

namespace uistate {

BrightnessLayout brightnessLayout(settings::Orientation o) {
  BrightnessLayout l;
  if (o == settings::Orientation::Landscape) {
    l.landscape = true;
    l.panel = {152, 136, 656, 324};
    l.dirty = {152, 136, 662, 330};
    l.close = {728, 152, 64, 64};
    l.percentRegion = {160, 228, 130, 64};
    l.pending = {284, 248, 24, 24};
    l.bar = {324, 228, 468, 64};
    l.minus = {168, 304, 200, 64};
    l.plus = {380, 304, 200, 64};
    l.off = {592, 304, 200, 64};
    const int px[4] = {168, 327, 486, 645};
    for (int i = 0; i < 4; ++i) l.presets[i] = {px[i], 380, 147, 64};
    l.nameX = 168; l.nameCy = 184; l.nameMaxW = 540;
    l.percentX = 168; l.percentBaseline = 274; l.percentCentered = false;
  } else {
    l.panel = {16, 278, 508, 380};
    l.dirty = {16, 278, 514, 386};
    l.close = {444, 294, 64, 64};
    l.percentRegion = {190, 362, 160, 56};
    l.pending = {484, 378, 24, 24};
    l.bar = {32, 426, 476, 64};
    l.minus = {32, 502, 150, 64};
    l.off = {194, 502, 152, 64};
    l.plus = {358, 502, 150, 64};
    const int px[4] = {32, 154, 276, 398};
    for (int i = 0; i < 4; ++i) l.presets[i] = {px[i], 578, 110, 64};
    l.nameX = 32; l.nameCy = 326; l.nameMaxW = 380;
    l.percentX = 270; l.percentBaseline = 404; l.percentCentered = true;
  }
  return l;
}

BrightnessHit brightnessHitTest(const BrightnessLayout &l, ui::Point p) {
  if (!l.panel.contains(p)) return BrightnessHit::Outside;
  if (l.close.contains(p)) return BrightnessHit::Close;
  if (l.bar.contains(p)) return BrightnessHit::Bar;
  if (l.minus.contains(p)) return BrightnessHit::Minus;
  if (l.plus.contains(p)) return BrightnessHit::Plus;
  for (int i = 0; i < 4; ++i)
    if (l.presets[i].contains(p)) return BrightnessHit(int(BrightnessHit::Preset0) + i);
  if (l.off.contains(p)) return BrightnessHit::Off;
  return BrightnessHit::Panel;
}

double brightnessBarFraction(const BrightnessLayout &l, int x) {
  double f = double(x - l.bar.x) / double(l.bar.w);
  return f < 0 ? 0 : f > 1 ? 1 : f;
}

}  // namespace uistate
