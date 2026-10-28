#pragma once
// Drawing primitives for the UI canvas (8-bit grey). Hardware-independent apart from M5GFX; all
// positions come from the caller (lib/ui rects plus the widgets.md offsets).
#include <M5Unified.h>

#include <string>
#include <vector>

#include "icon_data.h"
#include "ui_layout.h"

namespace render {

// The five greys of STYLE.md section 1 (TRACK is unused).
constexpr uint32_t INK = 0x000000U;
constexpr uint32_t DIM = 0x444444U;
constexpr uint32_t OFFGREY = 0x888888U;
constexpr uint32_t PAPER = 0xFFFFFFU;

enum class Font { F18, F24, F40, F56 };
int fontPx(Font f);
// Baseline offset from the centre line (STYLE.md section 2): by = cy + k.
int baselineK(Font f);

enum class Align { Left, Center, Right };

// Text. UTF-8; the built-in fonts only have ASCII, so the degree sign (U+00B0) and the ellipsis
// (U+2026) are drawn here (a ring, and three dots 1 em wide).
int textWidth(M5Canvas &c, Font f, const std::string &s);
// Drops characters from the end and appends the ellipsis until the text fits `maxW`.
std::string fitText(M5Canvas &c, Font f, const std::string &s, int maxW);
// Greedy word wrap into at most maxLines lines; the last line is fitted with an ellipsis.
std::vector<std::string> wrapText(M5Canvas &c, Font f, const std::string &s, int maxW, int maxLines);

// y is the baseline (drawText) or the centre line (drawTextCy, which uses baseline cy + k).
void drawText(M5Canvas &c, Font f, const std::string &s, int x, int baseline, Align a, uint32_t color);
void drawTextCy(M5Canvas &c, Font f, const std::string &s, int x, int cy, Align a, uint32_t color);
// Value + unit on a shared baseline (widgets.md conventions): total width = value + 2 + unit.
int runWidth(M5Canvas &c, Font vf, const std::string &value, Font uf, const std::string &unit);
void drawRun(M5Canvas &c, Font vf, const std::string &value, Font uf, const std::string &unit, int x,
             int baseline, Align a, uint32_t color);

// Icons (alpha bitmaps): size 24, 40 or 48, top-left at (x, y).
void drawIcon(M5Canvas &c, Icon icon, int size, int x, int y, uint32_t color);

// Rounded box with a 2 px border (outer radius 9, inner 7: SVG stroke 2 at radius 8).
void drawBox(M5Canvas &c, const ui::Rect &r, uint32_t fill, uint32_t border, int borderPx = 2);
// Dashed or dotted outline along the centre line of a rounded rect (x, y, w, h are the centre
// line rect). Starts at the top edge after the corner, clockwise, like an SVG rect stroke.
void drawDashedBox(M5Canvas &c, float x, float y, float w, float h, float radius, int thickness, float on,
                   float off, uint32_t color);
// Solid ring: 4 px (or n) border inside r.
void drawRing(M5Canvas &c, const ui::Rect &r, int thickness, int outerRadius, uint32_t color);

}  // namespace render
