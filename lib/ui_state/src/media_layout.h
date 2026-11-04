#pragma once
// Geometry and hit testing of the media popup (docs/ui/widgets-new.md 4.1) and of the media widget's
// text block. Pure; the renderer and the touch routing share it. Portrait is specified but not mocked.
#include "settings.h"
#include "ui_layout.h"

namespace uistate {

// Media widget sub-regions (ui::subRegions): 1 prev, 2 play/pause, 3 next, 4 text block (opens the popup).
// Sub-region 5 (task 107) is the cover art: touch only, same action as the text block; not a rocker stop.
constexpr int kMediaSubPrev = 1, kMediaSubPlay = 2, kMediaSubNext = 3, kMediaSubText = 4, kMediaSubArt = 5;

// Media widget content boxes, widget-relative (docs/ui/widgets-new-media-art.md 1; old layout without art).
struct MediaWidgetLayout {
  bool art = false;
  ui::Rect artRect, edge;  // (12, 12, 80, 80) and the 1 px edge (11, 11, 82, 82); empty without art
  int textX = 12;          // 104 with art
  int line1MaxW = 0, line2MaxW = 0, line1Cy = 36, line2Cy = 69;
  ui::Rect tile;           // 2 px tile square (13, 13, 78, 78) for the no-picture / loading / failed / unavailable states
};
// W = widget width (552 / 508 / 740 ...).
MediaWidgetLayout mediaWidgetLayout(int W, bool art);

struct MediaLayout {
  bool landscape = false;
  ui::Rect panel;  // frame (3 px border, radius 8)
  ui::Rect dirty;  // panel plus the 6 px shadow
  ui::Rect close, prev, play, next, volMinus, volPlus, barTrack, mute;
  bool art = false;        // task 107: cover art on the left / above the title
  ui::Rect artRect, edge;  // 200 x 200 and its 1 px edge (art only)
  ui::Rect tile;           // (x+1, y+1, 198, 198): the no-picture / loading / failed tile
  const char *hint = "Rocker: volume, PUSH play/pause";
  int nameX = 0, nameCy = 0, nameMaxW = 0;
  int titleX = 0, titleCy = 0, titleMaxW = 0;
  int artistCy = 0, albumCy = 0;
  int stateX = 0, stateCy = 0;
  int hintRight = 0, hintCy = 0;  // right edge of the hint; without art in portrait: left x (hintLeft)
  bool hintLeft = false;
  int valueX = 0, valueCy = 0;
};

// art = the widget's `art` option (default true): the new layout of docs/ui/widgets-new-media-art.md 2;
// false: the 106 layout (no picture).
MediaLayout mediaLayout(settings::Orientation o, bool art);

enum class MediaHit { Outside, Panel, Close, Prev, Play, Next, VolMinus, VolPlus, Bar, Mute };

MediaHit mediaHitTest(const MediaLayout &l, ui::Point p);
// Tap position on the track as a fraction 0..1 of its length (clamped).
double mediaBarFraction(const MediaLayout &l, int x);

}  // namespace uistate
