#include "media_widget.h"

#include <algorithm>
#include <cmath>

#include "art_store.h"
#include "media_layout.h"
#include "media_view.h"

namespace render {

namespace {

using ui::Rect;
using uistate::Look;
using uistate::PlayGlyph;

constexpr uint32_t kTrack = 0xCCCCCCU;  // STYLE.md TRACK: empty part of progress bars

// ---- glyphs without a bitmap icon (prev, next, volume, mute): drawn from primitives, 40 px box ----
// UNVERIFIED (device): the look; the generated icon set (tools/gen_icons.py) has no prev/next/volume/mute
// bitmaps yet, a follow-up can swap these for the mockup symbols (i-prev, i-next, i-vol, i-mute).

void dotLine(M5Canvas &c, float x0, float y0, float x1, float y1, float r, uint32_t color) {
  const int n = std::max(1, int(std::ceil(std::hypot(x1 - x0, y1 - y0) / 0.5f)));
  for (int i = 0; i <= n; ++i) {
    const float t = float(i) / float(n);
    c.fillCircle(int(std::lround(x0 + (x1 - x0) * t)), int(std::lround(y0 + (y1 - y0) * t)), int(std::lround(r)), color);
  }
}

void speaker(M5Canvas &c, int x, int y, uint32_t color) {
  c.fillRect(x + 6, y + 15, 7, 10, color);
  c.fillTriangle(x + 13, y + 15, x + 23, y + 7, x + 23, y + 33, color);
  c.fillTriangle(x + 13, y + 15, x + 13, y + 25, x + 23, y + 33, color);
}

enum class Glyph { Prev, Next, Volume, Mute };

void drawGlyph(M5Canvas &c, Glyph g, int x, int y, uint32_t color) {
  switch (g) {
    case Glyph::Prev:
      c.fillRect(x + 8, y + 10, 4, 20, color);
      c.fillTriangle(x + 30, y + 10, x + 30, y + 30, x + 14, y + 20, color);
      break;
    case Glyph::Next:
      c.fillRect(x + 28, y + 10, 4, 20, color);
      c.fillTriangle(x + 10, y + 10, x + 10, y + 30, x + 26, y + 20, color);
      break;
    case Glyph::Volume:
      speaker(c, x - 4, y, color);
      for (int wave = 0; wave < 2; ++wave) {
        const float rad = 8.0f + 6.0f * float(wave);
        for (int a = -45; a <= 45; a += 3) {
          const float t = float(a) * 3.14159265f / 180.0f;
          c.fillCircle(int(std::lround(float(x + 19) + rad * std::cos(t))), int(std::lround(float(y + 20) + rad * std::sin(t))), 1, color);
        }
      }
      break;
    case Glyph::Mute:
      speaker(c, x - 4, y, color);
      dotLine(c, float(x + 24), float(y + 14), float(x + 34), float(y + 26), 1.5f, color);
      dotLine(c, float(x + 34), float(y + 14), float(x + 24), float(y + 26), 1.5f, color);
      break;
  }
}

enum class Btn { Outline, Filled, Dotted };

// Outline / INK-filled / dotted button frame; `focus` adds the rocker ring (widgets.cpp drawButton rule).
void buttonFrame(M5Canvas &c, const Rect &b, Btn kind, bool focus) {
  if (focus && kind != Btn::Dotted) {
    c.fillRoundRect(b.x, b.y, b.w, b.h, 9, INK);
    c.fillRoundRect(b.x + 4, b.y + 4, b.w - 8, b.h - 8, 5, PAPER);
    if (kind == Btn::Filled) c.fillRoundRect(b.x + 6, b.y + 6, b.w - 12, b.h - 12, 3, INK);
  } else if (kind == Btn::Dotted) {
    c.fillRoundRect(b.x, b.y, b.w, b.h, 9, PAPER);
    drawDashedBox(c, float(b.x + 1), float(b.y + 1), float(b.w - 2), float(b.h - 2), 8.0f, 2, 2.0f, 4.0f, OFFGREY);
  } else {
    drawBox(c, b, kind == Btn::Filled ? INK : PAPER, INK);
  }
}

uint32_t glyphColor(Btn kind) { return kind == Btn::Dotted ? OFFGREY : kind == Btn::Filled ? PAPER : INK; }

void iconButton(M5Canvas &c, const Rect &b, Icon icon, Btn kind, bool focus = false) {
  buttonFrame(c, b, kind, focus);
  drawIcon(c, icon, 40, b.x + (b.w - 40) / 2, b.y + (b.h - 40) / 2, glyphColor(kind));
}

void glyphButton(M5Canvas &c, const Rect &b, Glyph g, Btn kind, bool focus = false) {
  buttonFrame(c, b, kind, focus);
  drawGlyph(c, g, b.x + (b.w - 40) / 2, b.y + (b.h - 40) / 2, glyphColor(kind));
}

// ---- album art (task 107, docs/ui/widgets-new-media-art.md) ----
// UNVERIFIED (device): the look of the 16-grey art on the panel and the ghosting of its GC16 partial; the tile
// glyphs (the speaker is the 40 px primitive glyph, also used where the spec says L / 48).

// The 4-bit picture at (x, y), one 8-bit grey per pixel (v * 17), edge outside. Pixels are written in one batch.
void drawArtPixels(M5Canvas &c, const art::Gray4 &g, int x, int y) {
  c.drawRect(x - 1, y - 1, g.w + 2, g.h + 2, INK);
  c.startWrite();
  for (int py = 0; py < g.h; ++py)
    for (int px = 0; px < g.w; ++px) c.drawPixel(x + px, y + py, uint32_t(art::grey4to8(g.at(px, py))) * 0x010101U);
  c.endWrite();
}

// 2 px INK square tile with PAPER fill (r = outer rect), or the dotted OFF one (unavailable).
void artTile(M5Canvas &c, const Rect &r, bool dotted) {
  if (dotted) {
    c.fillRect(r.x, r.y, r.w, r.h, PAPER);
    drawDashedBox(c, float(r.x + 1), float(r.y + 1), float(r.w - 2), float(r.h - 2), 1.0f, 2, 2.0f, 4.0f, OFFGREY);
    return;
  }
  c.fillRect(r.x, r.y, r.w, r.h, INK);
  c.fillRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, PAPER);
}

// Tile contents by state. `big` = popup (200 px art): hourglass L, failed alert M at the lower right.
void artTileContent(M5Canvas &c, const Rect &art, const Rect &tile, art::ArtState st, bool big) {
  const int cx = tile.x + tile.w / 2, cy = tile.y + tile.h / 2;
  switch (st) {
    case art::ArtState::Loading: {
      const int sz = big ? 48 : 40;
      drawIcon(c, Icon::Hourglass, sz, cx - sz / 2, cy - sz / 2, INK);
      break;
    }
    case art::ArtState::Failed:
      if (big) {
        drawGlyph(c, Glyph::Volume, cx - 20, cy - 20, INK);
        drawIcon(c, Icon::Alert, 40, tile.x + tile.w - 52, tile.y + tile.h - 52, INK);
      } else {
        drawGlyph(c, Glyph::Volume, art.x + 6, art.y + 8, INK);
        drawIcon(c, Icon::Alert, 24, art.x + 48, art.y + 48, INK);
      }
      break;
    default: drawGlyph(c, Glyph::Volume, cx - 20, cy - 20, INK); break;
  }
}

Icon playIcon(PlayGlyph g) { return g == PlayGlyph::Pause ? Icon::Pause : g == PlayGlyph::Power ? Icon::Power : Icon::Play; }

}  // namespace

void drawMediaWidget(M5Canvas &c, const ui::Placement &pl, const uistate::WidgetData &d, const std::string &title,
                     int innerSub) {
  const Rect &r = pl.rect;
  const EntityState *st = d.members.empty() ? nullptr : d.members[0];
  const uistate::MediaView v = uistate::mediaView(st, title);
  const bool una = v.look == Look::Unavailable;
  const int W = r.w;
  if (una) {
    c.fillRoundRect(r.x, r.y, r.w, r.h, 9, PAPER);
    drawDashedBox(c, float(r.x + 1), float(r.y + 1), float(r.w - 2), float(r.h - 2), 8.0f, 2, 2.0f, 4.0f, OFFGREY);
  } else {
    drawBox(c, r, PAPER, INK);
  }
  const uistate::MediaWidgetLayout wl = uistate::mediaWidgetLayout(W, d.art);
  drawTextCy(c, Font::F24, fitText(c, Font::F24, v.line1, wl.line1MaxW), r.x + wl.textX, r.y + wl.line1Cy, Align::Left, una ? OFFGREY : INK);
  drawTextCy(c, Font::F24, fitText(c, Font::F24, v.line2, wl.line2MaxW), r.x + wl.textX, r.y + wl.line2Cy, Align::Left, INK);
  if (d.art) {
    const Rect ar{r.x + wl.artRect.x, r.y + wl.artRect.y, wl.artRect.w, wl.artRect.h};
    const Rect tile{r.x + wl.tile.x, r.y + wl.tile.y, wl.tile.w, wl.tile.h};
    if (una) {
      artTile(c, tile, true);
    } else if (d.artState == art::ArtState::Ready && d.artPx) {
      drawArtPixels(c, *d.artPx, ar.x, ar.y);
    } else {
      artTile(c, tile, false);
      artTileContent(c, ar, tile, d.artState, false);
    }
  }
  if (una) drawIcon(c, Icon::Warn, 24, r.x + W - 264, r.y + 57, OFFGREY);
  else if (d.failed) drawIcon(c, Icon::Alert, 24, r.x + W - 264, r.y + 57, INK);

  const auto subs = ui::subRegions(config::WidgetType::MediaPlayer, config::WidgetSize{}, r, d.art);
  const Btn skip = v.skipDotted ? Btn::Dotted : Btn::Outline;
  const Btn play = v.allDotted ? Btn::Dotted : v.playFilled ? Btn::Filled : Btn::Outline;
  glyphButton(c, subs[0].rect, Glyph::Prev, skip, innerSub == uistate::kMediaSubPrev);
  iconButton(c, subs[1].rect, playIcon(v.glyph), play, innerSub == uistate::kMediaSubPlay);
  glyphButton(c, subs[2].rect, Glyph::Next, skip, innerSub == uistate::kMediaSubNext);
  if (innerSub == uistate::kMediaSubText) drawRing(c, ui::inflate(subs[3].rect, 2), 2, 9, INK);
}

void drawMediaPopup(M5Canvas &c, const uistate::AppState &app) {
  if (!app.mediaOpen()) return;
  const uistate::MediaLayout l = uistate::mediaLayout(app.orientation(), app.mediaArt());
  const uistate::MediaPopupView v = app.mediaPopupData();

  // Shadow (6 px solid INK, right and bottom), then the framed panel.
  c.fillRoundRect(l.panel.x + 6, l.panel.y + 6, l.panel.w, l.panel.h, 8, INK);
  c.fillRoundRect(l.panel.x, l.panel.y, l.panel.w, l.panel.h, 8, INK);
  c.fillRoundRect(l.panel.x + 3, l.panel.y + 3, l.panel.w - 6, l.panel.h - 6, 5, PAPER);

  drawTextCy(c, Font::F24, fitText(c, Font::F24, app.mediaTitle(), l.nameMaxW), l.nameX, l.nameCy, Align::Left, INK);
  iconButton(c, l.close, Icon::Close, Btn::Outline);

  if (l.art) {
    const art::ArtState st = app.mediaArtState();
    if (st == art::ArtState::Ready && app.mediaArtPixels()) {
      drawArtPixels(c, *app.mediaArtPixels(), l.artRect.x, l.artRect.y);
    } else {
      artTile(c, l.tile, false);
      artTileContent(c, l.artRect, l.tile, st, true);
    }
  }

  drawTextCy(c, Font::F40, fitText(c, Font::F40, v.title, l.titleMaxW), l.titleX, l.titleCy, Align::Left, INK);
  if (!v.artist.empty())
    drawTextCy(c, Font::F24, fitText(c, Font::F24, v.artist, l.titleMaxW), l.titleX, l.artistCy, Align::Left, INK);
  if (!v.album.empty())
    drawTextCy(c, Font::F18, fitText(c, Font::F18, v.album, l.titleMaxW), l.titleX, l.albumCy, Align::Left, INK);

  glyphButton(c, l.prev, Glyph::Prev, v.skipDotted ? Btn::Dotted : Btn::Outline);
  iconButton(c, l.play, playIcon(v.glyph), v.look == Look::Unavailable ? Btn::Dotted : v.playFilled ? Btn::Filled : Btn::Outline);
  glyphButton(c, l.next, Glyph::Next, v.skipDotted ? Btn::Dotted : Btn::Outline);
  drawTextCy(c, Font::F24, v.stateWord, l.stateX, l.stateCy, Align::Left, INK);
  const char *hint = l.hint;
  if (l.hintLeft) drawTextCy(c, Font::F18, hint, l.hintRight, l.hintCy, Align::Left, DIM);
  else drawTextCy(c, Font::F18, hint, l.hintRight, l.hintCy, Align::Right, DIM);

  // Volume row: - / bar / + , value, mute.
  const int pct = v.hasVolume ? v.volumePct : 0;
  iconButton(c, l.volMinus, Icon::Minus, !v.hasVolume || pct <= 0 ? Btn::Dotted : Btn::Outline);
  iconButton(c, l.volPlus, Icon::Plus, !v.hasVolume || pct >= 100 ? Btn::Dotted : Btn::Outline);
  c.fillRoundRect(l.barTrack.x, l.barTrack.y, l.barTrack.w, l.barTrack.h, 4, kTrack);
  const int fw = v.hasVolume ? int((long(l.barTrack.w) * pct + 50) / 100) : 0;
  if (fw > 0) c.fillRoundRect(l.barTrack.x, l.barTrack.y, fw, l.barTrack.h, std::min(4, fw / 2), INK);
  const std::string value = !v.hasVolume ? "-" : v.muted ? "Muted" : std::to_string(pct) + "%";
  drawTextCy(c, Font::F24, value, l.valueX, l.valueCy, Align::Left, v.hasVolume ? INK : OFFGREY);
  if (app.mediaFailed()) drawIcon(c, Icon::Alert, 24, l.valueX + 120, l.valueCy - 12, INK);
  glyphButton(c, l.mute, v.muted ? Glyph::Mute : Glyph::Volume, !v.hasMute ? Btn::Dotted : v.muted ? Btn::Filled : Btn::Outline);
}

}  // namespace render
