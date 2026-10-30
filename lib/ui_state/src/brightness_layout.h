#pragma once
// Geometry and hit testing of the brightness popup (docs/ui/brightness.md section 3). Pure; the
// renderer and the touch routing share it.
#include "settings.h"
#include "ui_layout.h"

namespace uistate {

struct BrightnessLayout {
  bool landscape = false;
  ui::Rect panel;   // frame (3 px border, radius 8)
  ui::Rect dirty;   // panel plus the 6 px shadow
  ui::Rect close, percentRegion, pending, bar, minus, plus, presets[4], off;
  int nameX = 0, nameCy = 0, nameMaxW = 0;
  int percentX = 0, percentBaseline = 0;  // portrait: centre x; landscape: left x
  bool percentCentered = true;
};

BrightnessLayout brightnessLayout(settings::Orientation o);

enum class BrightnessHit { Outside, Panel, Close, Bar, Minus, Plus, Preset0, Preset1, Preset2, Preset3, Off };

BrightnessHit brightnessHitTest(const BrightnessLayout &l, ui::Point p);
// Tap position on the bar as a fraction 0..1 of its length (clamped).
double brightnessBarFraction(const BrightnessLayout &l, int x);

}  // namespace uistate
