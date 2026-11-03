#include "media_layout.h"

namespace uistate {

MediaLayout mediaLayout(settings::Orientation o) {
  MediaLayout l;
  if (o == settings::Orientation::Landscape) {
    l.landscape = true;
    l.panel = {40, 94, 880, 408};
    l.dirty = {38, 92, 890, 418};
    l.nameX = 56; l.nameCy = 142; l.nameMaxW = 760;
    l.close = {840, 110, 64, 64};
    l.titleX = 56; l.titleCy = 206; l.titleMaxW = 784;
    l.artistCy = 254; l.albumCy = 288;
    l.prev = {56, 318, 96, 64};
    l.play = {164, 318, 96, 64};
    l.next = {272, 318, 96, 64};
    l.stateX = 392; l.stateCy = 350;
    l.hintRight = 904; l.hintCy = 350;
    l.volMinus = {56, 406, 64, 64};
    l.barTrack = {132, 426, 472, 24};
    l.volPlus = {616, 406, 64, 64};
    l.valueX = 696; l.valueCy = 438;
    l.mute = {840, 406, 64, 64};
  } else {
    // Panel (16, 188, 508, 560); coordinates below are panel-relative in the spec, absolute here.
    l.panel = {16, 188, 508, 560};
    l.dirty = {14, 186, 518, 570};
    l.nameX = 32; l.nameCy = 236; l.nameMaxW = 400;
    l.close = {444, 204, 64, 64};
    l.titleX = 32; l.titleCy = 308; l.titleMaxW = 476;
    l.artistCy = 356; l.albumCy = 390;
    l.prev = {32, 420, 96, 64};
    l.play = {140, 420, 96, 64};
    l.next = {248, 420, 96, 64};
    l.stateX = 360; l.stateCy = 452;
    l.hintLeft = true; l.hintRight = 32; l.hintCy = 712;
    l.volMinus = {32, 508, 64, 64};
    l.barTrack = {108, 528, 324, 24};
    l.volPlus = {444, 508, 64, 64};
    l.valueX = 32; l.valueCy = 620;
    l.mute = {444, 588, 64, 64};
  }
  return l;
}

MediaHit mediaHitTest(const MediaLayout &l, ui::Point p) {
  if (!l.panel.contains(p)) return MediaHit::Outside;
  if (l.close.contains(p)) return MediaHit::Close;
  if (l.prev.contains(p)) return MediaHit::Prev;
  if (l.play.contains(p)) return MediaHit::Play;
  if (l.next.contains(p)) return MediaHit::Next;
  if (l.volMinus.contains(p)) return MediaHit::VolMinus;
  if (l.volPlus.contains(p)) return MediaHit::VolPlus;
  if (l.mute.contains(p)) return MediaHit::Mute;
  // The track is only 24 px high: the touch target is as high as the buttons beside it (64 px).
  const ui::Rect bar{l.barTrack.x, l.barTrack.y - 20, l.barTrack.w, 64};
  if (bar.contains(p)) return MediaHit::Bar;
  return MediaHit::Panel;
}

double mediaBarFraction(const MediaLayout &l, int x) {
  const double f = double(x - l.barTrack.x) / double(l.barTrack.w);
  return f < 0 ? 0 : f > 1 ? 1 : f;
}

}  // namespace uistate
