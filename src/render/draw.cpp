#include "draw.h"

#include <algorithm>
#include <cmath>

namespace render {

namespace {

const lgfx::IFont *fontPtr(Font f) {
  switch (f) {
    case Font::F18: return &fonts::DejaVu18;
    case Font::F24: return &fonts::DejaVu24;
    case Font::F40: return &fonts::DejaVu40;
    case Font::F56: return &fonts::DejaVu56;
  }
  return &fonts::DejaVu24;
}

constexpr const char *kEllipsis = "\xE2\x80\xA6";
constexpr const char *kDegree = "\xC2\xB0";

// Splits UTF-8 into chunks: runs of ordinary text, or one special glyph.
struct Chunk {
  enum Kind { Text, Degree, Ellipsis } kind;
  std::string text;
};

std::vector<Chunk> chunks(const std::string &s) {
  std::vector<Chunk> out;
  std::string cur;
  auto flush = [&] {
    if (!cur.empty()) out.push_back({Chunk::Text, cur});
    cur.clear();
  };
  for (size_t i = 0; i < s.size();) {
    if (s.compare(i, 2, kDegree) == 0) {
      flush();
      out.push_back({Chunk::Degree, ""});
      i += 2;
    } else if (s.compare(i, 3, kEllipsis) == 0) {
      flush();
      out.push_back({Chunk::Ellipsis, ""});
      i += 3;
    } else {
      // Anything else non-ASCII has no glyph: show '?'.
      const unsigned char ch = (unsigned char)s[i];
      if (ch < 0x80) cur += char(ch);
      else {
        cur += '?';
        while (i + 1 < s.size() && ((unsigned char)s[i + 1] & 0xC0) == 0x80) ++i;
      }
      ++i;
    }
  }
  flush();
  return out;
}

int degreeAdvance(Font f) { return int(std::lround(fontPx(f) * 0.5)); }
int ellipsisAdvance(Font f) { return fontPx(f); }

void setupFont(M5Canvas &c, Font f) {
  c.setFont(fontPtr(f));
  c.setTextSize(1);
  c.setTextDatum(textdatum_t::baseline_left);
}

void drawDegree(M5Canvas &c, Font f, int x, int baseline, uint32_t color) {
  const float px = float(fontPx(f));
  const float cx = x + degreeAdvance(f) * 0.5f;
  const float cy = baseline - px * 0.60f;
  const float ro = px * 0.17f;
  const int stroke = std::max(2, int(std::lround(px * 0.075f)));
  c.fillCircle(int(std::lround(cx)), int(std::lround(cy)), int(std::lround(ro)), color);
  c.fillCircle(int(std::lround(cx)), int(std::lround(cy)), int(std::lround(ro)) - stroke, PAPER);
}

void drawEllipsisGlyph(M5Canvas &c, Font f, int x, int baseline, uint32_t color) {
  const int px = fontPx(f);
  const int d = std::max(2, int(std::lround(px * 0.11f)));
  for (int i = 0; i < 3; ++i) {
    const int cx = x + int(std::lround(px * (1.0f / 6 + i / 3.0f)));
    c.fillRect(cx - d / 2, baseline - d, d, d, color);
  }
}

}  // namespace

int fontPx(Font f) {
  switch (f) {
    case Font::F18: return 18;
    case Font::F24: return 24;
    case Font::F40: return 40;
    case Font::F56: return 56;
  }
  return 24;
}

int baselineK(Font f) {
  switch (f) {
    case Font::F18: return 6;
    case Font::F24: return 8;
    case Font::F40: return 14;
    case Font::F56: return 19;
  }
  return 8;
}

int textWidth(M5Canvas &c, Font f, const std::string &s) {
  setupFont(c, f);
  int w = 0;
  for (const Chunk &k : chunks(s)) {
    if (k.kind == Chunk::Text) w += c.textWidth(k.text.c_str());
    else if (k.kind == Chunk::Degree) w += degreeAdvance(f);
    else w += ellipsisAdvance(f);
  }
  return w;
}

std::string fitText(M5Canvas &c, Font f, const std::string &s, int maxW) {
  if (textWidth(c, f, s) <= maxW) return s;
  // Work on code points of the ASCII/degree text: drop from the end (never inside a UTF-8 sequence).
  std::string t = s;
  while (!t.empty()) {
    size_t cut = t.size() - 1;
    while (cut > 0 && ((unsigned char)t[cut] & 0xC0) == 0x80) --cut;
    t.erase(cut);
    while (!t.empty() && t.back() == ' ') t.pop_back();
    const std::string cand = t + kEllipsis;
    if (textWidth(c, f, cand) <= maxW) return cand;
  }
  return std::string(kEllipsis);
}

std::vector<std::string> wrapText(M5Canvas &c, Font f, const std::string &s, int maxW, int maxLines) {
  std::vector<std::string> lines;
  std::string rest = s;
  while (!rest.empty() && int(lines.size()) < maxLines) {
    if (int(lines.size()) == maxLines - 1) {
      lines.push_back(fitText(c, f, rest, maxW));
      break;
    }
    if (textWidth(c, f, rest) <= maxW) {
      lines.push_back(rest);
      break;
    }
    // Longest prefix ending before a space that fits; else break inside the word.
    size_t best = 0;
    for (size_t i = 1; i < rest.size(); ++i) {
      if (rest[i] == ' ' && textWidth(c, f, rest.substr(0, i)) <= maxW) best = i;
      else if (rest[i] == ' ') break;
    }
    if (best == 0) {
      size_t i = 1;
      while (i < rest.size() && textWidth(c, f, rest.substr(0, i + 1)) <= maxW) ++i;
      best = i;
    }
    lines.push_back(rest.substr(0, best));
    rest.erase(0, best);
    while (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
  }
  return lines;
}

void drawText(M5Canvas &c, Font f, const std::string &s, int x, int baseline, Align a, uint32_t color) {
  setupFont(c, f);
  const int w = textWidth(c, f, s);
  setupFont(c, f);
  int px = x;
  if (a == Align::Center) px = x - w / 2;
  else if (a == Align::Right) px = x - w;
  c.setTextColor(color);
  for (const Chunk &k : chunks(s)) {
    if (k.kind == Chunk::Text) {
      c.drawString(k.text.c_str(), px, baseline);
      px += c.textWidth(k.text.c_str());
    } else if (k.kind == Chunk::Degree) {
      drawDegree(c, f, px, baseline, color);
      px += degreeAdvance(f);
    } else {
      drawEllipsisGlyph(c, f, px, baseline, color);
      px += ellipsisAdvance(f);
    }
  }
}

void drawTextCy(M5Canvas &c, Font f, const std::string &s, int x, int cy, Align a, uint32_t color) {
  drawText(c, f, s, x, cy + baselineK(f), a, color);
}

int runWidth(M5Canvas &c, Font vf, const std::string &value, Font uf, const std::string &unit) {
  return textWidth(c, vf, value) + (unit.empty() ? 0 : 2 + textWidth(c, uf, unit));
}

void drawRun(M5Canvas &c, Font vf, const std::string &value, Font uf, const std::string &unit, int x,
             int baseline, Align a, uint32_t color) {
  const int w = runWidth(c, vf, value, uf, unit);
  int px = x;
  if (a == Align::Center) px = x - w / 2;
  else if (a == Align::Right) px = x - w;
  drawText(c, vf, value, px, baseline, Align::Left, color);
  if (!unit.empty()) drawText(c, uf, unit, px + textWidth(c, vf, value) + 2, baseline, Align::Left, color);
}

void drawIcon(M5Canvas &c, Icon icon, int size, int x, int y, uint32_t color) {
  const uint8_t *bmp = iconBitmap(icon, size);
  if (!bmp) return;
  uint8_t *buf = static_cast<uint8_t *>(c.getBuffer());
  const int W = c.width(), H = c.height();
  const int target = int(color & 0xFF);
  for (int row = 0; row < size; ++row) {
    const int py = y + row;
    if (py < 0 || py >= H) continue;
    for (int col = 0; col < size; ++col) {
      const int a = bmp[row * size + col];
      const int px = x + col;
      if (!a || px < 0 || px >= W) continue;
      uint8_t &d = buf[py * W + px];
      d = uint8_t(d + ((target - int(d)) * a + (a >> 1)) / 255);
    }
  }
}

void drawBox(M5Canvas &c, const ui::Rect &r, uint32_t fill, uint32_t border, int borderPx) {
  c.fillRoundRect(r.x, r.y, r.w, r.h, 9, border);
  if (fill != border) c.fillRoundRect(r.x + borderPx, r.y + borderPx, r.w - 2 * borderPx, r.h - 2 * borderPx, 7, fill);
}

void drawRing(M5Canvas &c, const ui::Rect &r, int t, int outerRadius, uint32_t color) {
  c.fillRoundRect(r.x, r.y, r.w, r.h, outerRadius, color);
  c.fillRoundRect(r.x + t, r.y + t, r.w - 2 * t, r.h - 2 * t, std::max(0, outerRadius - t), PAPER);
}

void drawDashedBox(M5Canvas &c, float x, float y, float w, float h, float radius, int thickness, float on,
                   float off, uint32_t color) {
  const float sw = w - 2 * radius, sh = h - 2 * radius;
  const float arc = float(M_PI) * radius * 0.5f;
  const float total = 2 * sw + 2 * sh + 4 * arc;
  const float period = on + off;
  const float half = thickness * 0.5f;
  for (float s = 0; s < total; s += 0.5f) {
    if (std::fmod(s, period) >= on) continue;
    float px, py, t = s;
    if (t < sw) { px = x + radius + t; py = y; }
    else if ((t -= sw) < arc) {
      const float a = t / radius;  // 0..pi/2, top-right corner
      px = x + w - radius + radius * std::sin(a);
      py = y + radius - radius * std::cos(a);
    } else if ((t -= arc) < sh) { px = x + w; py = y + radius + t; }
    else if ((t -= sh) < arc) {
      const float a = t / radius;
      px = x + w - radius + radius * std::cos(a);
      py = y + h - radius + radius * std::sin(a);
    } else if ((t -= arc) < sw) { px = x + w - radius - t; py = y + h; }
    else if ((t -= sw) < arc) {
      const float a = t / radius;
      px = x + radius - radius * std::sin(a);
      py = y + h - radius + radius * std::cos(a);
    } else if ((t -= arc) < sh) { px = x; py = y + h - radius - t; }
    else {
      t -= sh;
      const float a = t / radius;
      px = x + radius - radius * std::cos(a);
      py = y + radius - radius * std::sin(a);
    }
    c.fillRect(int(std::lround(px - half)), int(std::lround(py - half)), thickness, thickness, color);
  }
}

}  // namespace render
