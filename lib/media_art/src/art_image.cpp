#include "art_image.h"

#include <algorithm>
#include <cmath>

namespace art {

Gray4 Gray4::make(int w, int h) {
  Gray4 g;
  if (w <= 0 || h <= 0) return g;
  g.w = w;
  g.h = h;
  g.px.assign((size_t(w) * size_t(h) + 1) / 2, 0);
  return g;
}

uint8_t Gray4::at(int x, int y) const {
  if (x < 0 || y < 0 || x >= w || y >= h) return 0;
  const size_t i = size_t(y) * size_t(w) + size_t(x);
  const uint8_t b = px[i / 2];
  return (i & 1) ? uint8_t(b & 0x0F) : uint8_t(b >> 4);
}

void Gray4::set(int x, int y, uint8_t v) {
  if (x < 0 || y < 0 || x >= w || y >= h) return;
  const size_t i = size_t(y) * size_t(w) + size_t(x);
  uint8_t &b = px[i / 2];
  v &= 0x0F;
  b = (i & 1) ? uint8_t((b & 0xF0) | v) : uint8_t((b & 0x0F) | (v << 4));
}

uint8_t luma8(uint8_t r, uint8_t g, uint8_t b) { return uint8_t((299u * r + 587u * g + 114u * b + 500u) / 1000u); }

uint8_t luma565(uint16_t c) {
  const unsigned r5 = (c >> 11) & 31u, g6 = (c >> 5) & 63u, b5 = c & 31u;
  return luma8(uint8_t((r5 * 255u + 15u) / 31u), uint8_t((g6 * 255u + 31u) / 63u), uint8_t((b5 * 255u + 15u) / 31u));
}

Gray8 squareScale(const Gray8 &src, int n) {
  Gray8 out;
  if (!src.valid() || n < 1) return out;
  const int side = std::min(src.w, src.h);
  const int x0 = (src.w - side) / 2, y0 = (src.h - side) / 2;
  out.w = out.h = n;
  out.px.resize(size_t(n) * size_t(n));
  for (int oy = 0; oy < n; ++oy) {
    // Source rows [sy0, sy1) of the crop that map onto output row oy (at least one row).
    const int sy0 = int(int64_t(oy) * side / n);
    const int sy1 = std::max(sy0 + 1, int(int64_t(oy + 1) * side / n));
    for (int ox = 0; ox < n; ++ox) {
      const int sx0 = int(int64_t(ox) * side / n);
      const int sx1 = std::max(sx0 + 1, int(int64_t(ox + 1) * side / n));
      uint32_t sum = 0, cnt = 0;
      for (int sy = sy0; sy < sy1; ++sy)
        for (int sx = sx0; sx < sx1; ++sx) {
          sum += src.px[size_t(y0 + sy) * size_t(src.w) + size_t(x0 + sx)];
          ++cnt;
        }
      out.px[size_t(oy) * size_t(n) + size_t(ox)] = uint8_t((sum + cnt / 2) / cnt);
    }
  }
  return out;
}

Gray4 dither16(const Gray8 &src) {
  Gray4 out;
  if (!src.valid()) return out;
  out = Gray4::make(src.w, src.h);
  // Error rows (current and next), in 1/16 units of a grey level.
  std::vector<int> cur(size_t(src.w) + 2, 0), nxt(size_t(src.w) + 2, 0);
  for (int y = 0; y < src.h; ++y) {
    std::fill(nxt.begin(), nxt.end(), 0);
    for (int x = 0; x < src.w; ++x) {
      int v = int(src.px[size_t(y) * size_t(src.w) + size_t(x)]) * 16 + cur[size_t(x) + 1];
      int q = (v + 136) / 272;  // round(v / 16 / 17)
      q = std::max(0, std::min(15, q));
      out.set(x, y, uint8_t(q));
      const int err = v - q * 17 * 16;
      cur[size_t(x) + 2] += err * 7 / 16;
      nxt[size_t(x)] += err * 3 / 16;
      nxt[size_t(x) + 1] += err * 5 / 16;
      nxt[size_t(x) + 2] += err / 16;
    }
    std::swap(cur, nxt);
  }
  return out;
}

Gray8 sampleImage(int n, int variant) {
  Gray8 g;
  if (n < 1) return g;
  g.w = g.h = n;
  g.px.resize(size_t(n) * size_t(n));
  const double c = (n - 1) / 2.0;
  for (int y = 0; y < n; ++y)
    for (int x = 0; x < n; ++x) {
      double v;
      if (variant == 0) {
        const double d = std::hypot(x - c, y - c) / (n / 2.0);
        const double ramp = (double(x) + double(y)) / (2.0 * (n - 1) + 1e-9);
        if (d > 1.0) v = 40.0 + 120.0 * ramp;                      // square corners: dark ramp
        else if (d < 0.08) v = 255.0;                              // spindle hole
        else if (d < 0.30) v = 90.0 + 120.0 * ramp;                // label
        else v = 30.0 + 25.0 * (0.5 + 0.5 * std::sin(d * 60.0));  // grooves
      } else {
        v = 255.0 * (double(x) / double(std::max(1, n - 1)));
      }
      g.px[size_t(y) * size_t(n) + size_t(x)] = uint8_t(std::max(0.0, std::min(255.0, std::round(v))));
    }
  return g;
}

}  // namespace art
