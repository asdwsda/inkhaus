#include "media_layout.h"

namespace uistate {

MediaWidgetLayout mediaWidgetLayout(int W, bool art) {
  MediaWidgetLayout l;
  l.art = art;
  if (art) {
    l.artRect = {12, 12, 80, 80};
    l.edge = {11, 11, 82, 82};
    l.tile = {13, 13, 78, 78};
    l.textX = 104;
    l.line1MaxW = W - 344;
    l.line2MaxW = W - 376;
  } else {
    l.textX = 12;
    l.line1MaxW = W - 252;
    l.line2MaxW = W - 284;
  }
  return l;
}

namespace {

MediaLayout artLayout(settings::Orientation o) {
  MediaLayout l;
  l.art = true;
  l.hint = "UP/DOWN volume";
  if (o == settings::Orientation::Landscape) {
    l.landscape = true;
    l.panel = {40, 94, 880, 408};
    l.dirty = {38, 92, 890, 418};
    l.nameX = 56; l.nameCy = 142; l.nameMaxW = 760;
    l.close = {840, 110, 64, 64};
    l.artRect = {56, 190, 200, 200};
    l.titleX = 272; l.titleCy = 206; l.titleMaxW = 552;
    l.artistCy = 254; l.albumCy = 288;
    l.prev = {272, 318, 96, 64};
    l.play = {380, 318, 96, 64};
    l.next = {488, 318, 96, 64};
    l.stateX = 604; l.stateCy = 350;
    l.hintRight = 904; l.hintCy = 350;
    l.volMinus = {56, 406, 64, 64};
    l.barTrack = {132, 426, 472, 24};
    l.volPlus = {616, 406, 64, 64};
    l.valueX = 696; l.valueCy = 438;
    l.mute = {840, 406, 64, 64};
  } else {
    // Panel (16, 120, 508, 700); the spec's panel-relative numbers + (16, 120).
    l.panel = {16, 120, 508, 700};
    l.dirty = {14, 118, 518, 710};
    l.nameX = 32; l.nameCy = 168; l.nameMaxW = 400;
    l.close = {444, 136, 64, 64};
    l.artRect = {170, 208, 200, 200};
    l.titleX = 32; l.titleCy = 444; l.titleMaxW = 476;
    l.artistCy = 492; l.albumCy = 526;
    l.prev = {32, 552, 96, 64};
    l.play = {140, 552, 96, 64};
    l.next = {248, 552, 96, 64};
    l.stateX = 360; l.stateCy = 584;
    l.hintRight = 428; l.hintCy = 752;
    l.volMinus = {32, 640, 64, 64};
    l.barTrack = {108, 660, 324, 24};
    l.volPlus = {444, 640, 64, 64};
    l.valueX = 32; l.valueCy = 752;
    l.mute = {444, 720, 64, 64};
  }
  l.edge = {l.artRect.x - 1, l.artRect.y - 1, 202, 202};
  l.tile = {l.artRect.x + 1, l.artRect.y + 1, 198, 198};
  return l;
}

}  // namespace

MediaLayout mediaLayout(settings::Orientation o, bool art) {
  if (art) return artLayout(o);
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
