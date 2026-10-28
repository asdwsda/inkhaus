#pragma once
// Frame comparison for the e-ink presenter: which rectangles changed between two 8-bit frames
// (exact, pixel by pixel), and the same question answered from per-tile hashes (for a frame that
// is no longer in RAM, e.g. what the panel showed before a reboot). Pure logic, no Arduino.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "screen_track.h"

namespace screen_track {

struct DiffOptions {
    int mergeGap = 8;        // changed areas closer than this (px, both axes) become one rect
    size_t maxRects = 6;     // more than this: merge the pair that grows the least
    int alignX = 4;          // outward rounding of x/width (the EPD driver works in 4 px steps); 1 = off
};

// Merges rectangles that overlap or lie closer than `gap` (both axes), repeatedly, then reduces the
// count to `maxRects` by uniting the pair with the smallest growth in area. Empty rects are dropped.
void mergeRects(std::vector<Rect>& rects, int gap, size_t maxRects);

// Rounds x/width outward to multiples of `alignX`, clamped to [0, w). No-op for alignX <= 1.
Rect alignRect(Rect r, int alignX, int w);

// Changed regions between `prev` and `cur` (both h rows of `stride` bytes, w pixels used).
// Result: disjoint-or-merged rects, aligned per opts, covering every changed pixel. Empty if equal.
std::vector<Rect> diffFrames(const uint8_t* prev, const uint8_t* cur, int w, int h, int stride,
                             const DiffOptions& opts = DiffOptions());

// ---- tile hashes -----------------------------------------------------------------------------

constexpr int kTileSize = 32;

struct TileGrid {
    int cols = 0, rows = 0;
    size_t count() const { return size_t(cols) * size_t(rows); }
};
TileGrid tileGridFor(int w, int h);

// One 16 bit hash per kTileSize x kTileSize tile (row-major, partial tiles at the right/bottom).
std::vector<uint16_t> tileHashes(const uint8_t* frame, int w, int h, int stride);

// Hash of the whole tile vector (to compare quickly, or to store in a record).
uint32_t hashOfTiles(const std::vector<uint16_t>& tiles);

// Regions covering every tile whose hash differs. `a` and `b` must have the grid's tile count; if
// not (e.g. another geometry), the whole screen is returned as one rect.
std::vector<Rect> diffTiles(const std::vector<uint16_t>& a, const std::vector<uint16_t>& b, int w,
                            int h, const DiffOptions& opts = DiffOptions());

// ---- run-length coding for the serial screenshot ---------------------------------------------
// Byte pairs (count-1, value), count 1..256. Used by "shotz".
size_t rleEncode(const uint8_t* data, size_t len, uint8_t* out, size_t outCap);

}  // namespace screen_track
