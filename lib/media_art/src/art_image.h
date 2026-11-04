#pragma once
// Album art pixels (task 107, docs/ui/widgets-new-media-art.md). Pure: centre-crop to a square, area-average
// scale, 16-grey error-diffusion dither, a built-in sample picture for the demo. The decoders (JPEG/PNG) live
// on the device side and hand a Gray8 here.
#include <cstdint>
#include <vector>

namespace art {

constexpr int kWidgetArtPx = 80;   // widget art rect (80 x 80)
constexpr int kPopupArtPx = 200;   // popup art rect (200 x 200); also the size fetched/decoded once per track

// 8-bit grey picture, row-major, 0 = black.
struct Gray8 {
  int w = 0, h = 0;
  std::vector<uint8_t> px;
  bool valid() const { return w > 0 && h > 0 && px.size() == size_t(w) * size_t(h); }
};

// 16-grey picture, two pixels per byte (even x in the high nibble). Value 0..15, 0 = black. 20 KB at 200 x 200.
struct Gray4 {
  int w = 0, h = 0;
  std::vector<uint8_t> px;
  bool valid() const { return w > 0 && h > 0 && px.size() == (size_t(w) * size_t(h) + 1) / 2; }
  uint8_t at(int x, int y) const;
  void set(int x, int y, uint8_t v);
  static Gray4 make(int w, int h);
};

// Rec.601 luma of an RGB888 pixel, 0..255.
uint8_t luma8(uint8_t r, uint8_t g, uint8_t b);
// Same from RGB565 (decoder output).
uint8_t luma565(uint16_t c);

// Centre-crops to a square and scales to n x n by area averaging (n >= 1). Invalid input -> invalid result.
Gray8 squareScale(const Gray8 &src, int n);

// Quantises to 16 greys (value v = round(grey / 17)) with Floyd-Steinberg error diffusion.
Gray4 dither16(const Gray8 &src);

// The grey (0..255) a 4-bit value is drawn with: v * 17 (so 0 = black, 15 = white).
inline uint8_t grey4to8(uint8_t v) { return uint8_t(v * 17); }

// Built-in demo picture, n x n: variant 0 = concentric rings with a diagonal ramp (a "record"), others a
// plain ramp. Deterministic. No network in demo mode.
Gray8 sampleImage(int n, int variant = 0);

}  // namespace art
