#pragma once
// Geometry and hit testing of the media popup (docs/ui/widgets-new.md 4.1) and of the media widget's
// text block. Pure; the renderer and the touch routing share it. Portrait is specified but not mocked.
#include "settings.h"
#include "ui_layout.h"

namespace uistate {

// Media widget sub-regions (ui::subRegions): 1 prev, 2 play/pause, 3 next, 4 text block (opens the popup).
constexpr int kMediaSubPrev = 1, kMediaSubPlay = 2, kMediaSubNext = 3, kMediaSubText = 4;

struct MediaLayout {
  bool landscape = false;
  ui::Rect panel;  // frame (3 px border, radius 8)
  ui::Rect dirty;  // panel plus the 6 px shadow
  ui::Rect close, prev, play, next, volMinus, volPlus, barTrack, mute;
  int nameX = 0, nameCy = 0, nameMaxW = 0;
  int titleX = 0, titleCy = 0, titleMaxW = 0;
  int artistCy = 0, albumCy = 0;
  int stateX = 0, stateCy = 0;
  int hintRight = 0, hintCy = 0;  // landscape: right edge; portrait: left x (hintLeft)
  bool hintLeft = false;
  int valueX = 0, valueCy = 0;
};

MediaLayout mediaLayout(settings::Orientation o);

enum class MediaHit { Outside, Panel, Close, Prev, Play, Next, VolMinus, VolPlus, Bar, Mute };

MediaHit mediaHitTest(const MediaLayout &l, ui::Point p);
// Tap position on the track as a fraction 0..1 of its length (clamped).
double mediaBarFraction(const MediaLayout &l, int x);

}  // namespace uistate
