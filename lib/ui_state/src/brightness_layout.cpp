#include "brightness_layout.h"

namespace uistate {

BrightnessLayout brightnessLayout(settings::Orientation o) {
  BrightnessLayout l;
  if (o == settings::Orientation::Landscape) {
    l.landscape = true;
    l.panel = {152, 98, 656, 400};
    l.dirty = {152, 98, 662, 406};
    l.close = {728, 114, 64, 64};
    l.percentRegion = {160, 190, 130, 64};
    l.pending = {284, 210, 24, 24};
    l.bar = {324, 190, 468, 64};
    l.minus = {168, 266, 200, 64};
    l.plus = {380, 266, 200, 64};
    l.off = {592, 266, 200, 64};
    const int px[4] = {168, 327, 486, 645};
    for (int i = 0; i < 4; ++i) l.presets[i] = {px[i], 342, 147, 64};
    l.done = {168, 418, 624, 64};
    l.nameX = 168; l.nameCy = 146; l.nameMaxW = 540;
    l.percentX = 168; l.percentBaseline = 236; l.percentCentered = false;
  } else {
    l.panel = {16, 240, 508, 456};
    l.dirty = {16, 240, 514, 462};
    l.close = {444, 256, 64, 64};
    l.percentRegion = {190, 324, 160, 56};
    l.pending = {484, 340, 24, 24};
    l.bar = {32, 388, 476, 64};
    l.minus = {32, 464, 232, 64};
    l.plus = {276, 464, 232, 64};
    const int px[4] = {32, 154, 276, 398};
    for (int i = 0; i < 4; ++i) l.presets[i] = {px[i], 540, 110, 64};
    l.off = {32, 616, 232, 64};
    l.done = {276, 616, 232, 64};
    l.nameX = 32; l.nameCy = 288; l.nameMaxW = 380;
    l.percentX = 270; l.percentBaseline = 366; l.percentCentered = true;
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
  if (l.done.contains(p)) return BrightnessHit::Done;
  return BrightnessHit::Panel;
}

double brightnessBarFraction(const BrightnessLayout &l, int x) {
  double f = double(x - l.bar.x) / double(l.bar.w);
  return f < 0 ? 0 : f > 1 ? 1 : f;
}

}  // namespace uistate
