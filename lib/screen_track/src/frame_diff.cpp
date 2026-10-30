#include "frame_diff.h"

#include <algorithm>
#include <cstring>

namespace screen_track {

namespace {

long area(const Rect& r) { return long(r.w) * long(r.h); }

Rect unite(const Rect& a, const Rect& b) {
    const int x0 = std::min(a.x, b.x), y0 = std::min(a.y, b.y);
    const int x1 = std::max(a.x + a.w, b.x + b.w), y1 = std::max(a.y + a.h, b.y + b.h);
    return {x0, y0, x1 - x0, y1 - y0};
}

int axisDist(int a0, int a1, int b0, int b1) {
    return std::max(std::max(a0 - b1, b0 - a1), 0);
}

bool near(const Rect& a, const Rect& b, int gap) {
    return axisDist(a.x, a.x + a.w, b.x, b.x + b.w) <= gap &&
           axisDist(a.y, a.y + a.h, b.y, b.y + b.h) <= gap;
}

struct RowExt {
    int y, x0, x1;  // x1 inclusive
};

}  // namespace

void mergeRects(std::vector<Rect>& rects, int gap, size_t maxRects) {
    rects.erase(std::remove_if(rects.begin(), rects.end(),
                               [](const Rect& r) { return r.w <= 0 || r.h <= 0; }),
                rects.end());
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < rects.size() && !changed; ++i) {
            for (size_t j = i + 1; j < rects.size(); ++j) {
                if (near(rects[i], rects[j], gap)) {
                    rects[i] = unite(rects[i], rects[j]);
                    rects.erase(rects.begin() + long(j));
                    changed = true;
                    break;
                }
            }
        }
    }
    if (maxRects == 0) maxRects = 1;
    while (rects.size() > maxRects) {
        size_t bi = 0, bj = 1;
        long best = -1;
        for (size_t i = 0; i < rects.size(); ++i) {
            for (size_t j = i + 1; j < rects.size(); ++j) {
                const long growth = area(unite(rects[i], rects[j])) - area(rects[i]) - area(rects[j]);
                if (best < 0 || growth < best) {
                    best = growth;
                    bi = i;
                    bj = j;
                }
            }
        }
        rects[bi] = unite(rects[bi], rects[bj]);
        rects.erase(rects.begin() + long(bj));
    }
}

Rect alignRect(Rect r, int alignX, int w) {
    if (alignX <= 1) return r;
    int x0 = (r.x / alignX) * alignX;
    int x1 = ((r.x + r.w + alignX - 1) / alignX) * alignX;
    x0 = std::max(x0, 0);
    x1 = std::min(x1, w);
    r.x = x0;
    r.w = std::max(x1 - x0, 0);
    return r;
}

std::vector<Rect> diffFrames(const uint8_t* prev, const uint8_t* cur, int w, int h, int stride,
                             const DiffOptions& opts) {
    std::vector<Rect> out;
    if (!prev || !cur || w <= 0 || h <= 0) return out;

    // Changed runs per row; runs closer than the merge gap are one run.
    std::vector<RowExt> rows;
    for (int y = 0; y < h; ++y) {
        const uint8_t* a = prev + size_t(y) * size_t(stride);
        const uint8_t* b = cur + size_t(y) * size_t(stride);
        if (std::memcmp(a, b, size_t(w)) == 0) continue;
        int start = -1, last = -1;
        for (int x = 0; x < w; ++x) {
            if (a[x] == b[x]) continue;
            if (start >= 0 && x - last > opts.mergeGap + 1) {
                rows.push_back({y, start, last});
                start = -1;
            }
            if (start < 0) start = x;
            last = x;
        }
        if (start >= 0) rows.push_back({y, start, last});
    }

    // Bands of nearby changed rows; inside a band, split horizontally where nothing changed.
    std::vector<int> cover(size_t(w) + 1);
    size_t i = 0;
    while (i < rows.size()) {
        size_t j = i + 1;
        while (j < rows.size() && rows[j].y - rows[j - 1].y <= opts.mergeGap + 1) ++j;

        std::fill(cover.begin(), cover.end(), 0);
        for (size_t k = i; k < j; ++k) {
            ++cover[size_t(rows[k].x0)];
            --cover[size_t(rows[k].x1) + 1];
        }
        int run = 0, segStart = -1, lastCovered = -1;
        std::vector<std::pair<int, int>> segs;
        for (int x = 0; x < w; ++x) {
            run += cover[size_t(x)];
            if (run > 0) {
                if (segStart < 0) segStart = x;
                else if (x - lastCovered > opts.mergeGap + 1) {
                    segs.push_back({segStart, lastCovered});
                    segStart = x;
                }
                lastCovered = x;
            }
        }
        if (segStart >= 0) segs.push_back({segStart, lastCovered});

        for (const auto& s : segs) {
            int ymin = h, ymax = -1;
            for (size_t k = i; k < j; ++k) {
                if (rows[k].x1 >= s.first && rows[k].x0 <= s.second) {
                    ymin = std::min(ymin, rows[k].y);
                    ymax = std::max(ymax, rows[k].y);
                }
            }
            if (ymax >= 0) out.push_back({s.first, ymin, s.second - s.first + 1, ymax - ymin + 1});
        }
        i = j;
    }

    mergeRects(out, opts.mergeGap, opts.maxRects);
    for (Rect& r : out) r = alignRect(r, opts.alignX, w);
    return out;
}

TileGrid tileGridFor(int w, int h) {
    TileGrid g;
    g.cols = (w + kTileSize - 1) / kTileSize;
    g.rows = (h + kTileSize - 1) / kTileSize;
    return g;
}

std::vector<uint16_t> tileHashes(const uint8_t* frame, int w, int h, int stride) {
    const TileGrid g = tileGridFor(w, h);
    std::vector<uint16_t> out(g.count(), 0);
    for (int ty = 0; ty < g.rows; ++ty) {
        for (int tx = 0; tx < g.cols; ++tx) {
            const int x0 = tx * kTileSize, y0 = ty * kTileSize;
            const int x1 = std::min(x0 + kTileSize, w), y1 = std::min(y0 + kTileSize, h);
            uint32_t hv = kFnvSeed;
            for (int y = y0; y < y1; ++y) {
                hv = fnv1a(frame + size_t(y) * size_t(stride) + size_t(x0), size_t(x1 - x0), hv);
            }
            out[size_t(ty) * size_t(g.cols) + size_t(tx)] = uint16_t((hv >> 16) ^ (hv & 0xFFFFu));
        }
    }
    return out;
}

uint32_t hashOfTiles(const std::vector<uint16_t>& tiles) {
    return fnv1a(tiles.data(), tiles.size() * sizeof(uint16_t));
}

std::vector<Rect> diffTiles(const std::vector<uint16_t>& a, const std::vector<uint16_t>& b, int w,
                            int h, const DiffOptions& opts) {
    std::vector<Rect> out;
    const TileGrid g = tileGridFor(w, h);
    if (a.size() != g.count() || b.size() != g.count()) {
        out.push_back({0, 0, w, h});
        return out;
    }
    for (int ty = 0; ty < g.rows; ++ty) {
        int runStart = -1;
        for (int tx = 0; tx <= g.cols; ++tx) {
            const bool dirty =
                tx < g.cols && a[size_t(ty) * size_t(g.cols) + size_t(tx)] !=
                                   b[size_t(ty) * size_t(g.cols) + size_t(tx)];
            if (dirty && runStart < 0) runStart = tx;
            if (!dirty && runStart >= 0) {
                const int x0 = runStart * kTileSize, y0 = ty * kTileSize;
                const int x1 = std::min(tx * kTileSize, w), y1 = std::min(y0 + kTileSize, h);
                out.push_back({x0, y0, x1 - x0, y1 - y0});
                runStart = -1;
            }
        }
    }
    mergeRects(out, opts.mergeGap, opts.maxRects);
    for (Rect& r : out) r = alignRect(r, opts.alignX, w);
    return out;
}

size_t adoptClearedTiles(std::vector<uint16_t>& stored, const std::vector<uint16_t>& now,
                         const std::vector<uint16_t>& nowWithBadge, int w, int h, const Rect& cleared) {
    const TileGrid g = tileGridFor(w, h);
    if (stored.size() != g.count() || now.size() != g.count() || nowWithBadge.size() != g.count() ||
        cleared.w <= 0 || cleared.h <= 0)
        return 0;
    const int tx0 = std::max(cleared.x, 0) / kTileSize;
    const int ty0 = std::max(cleared.y, 0) / kTileSize;
    const int tx1 = std::min((cleared.x + cleared.w - 1) / kTileSize, g.cols - 1);
    const int ty1 = std::min((cleared.y + cleared.h - 1) / kTileSize, g.rows - 1);
    size_t n = 0;
    for (int ty = ty0; ty <= ty1; ++ty)
        for (int tx = tx0; tx <= tx1; ++tx) {
            const size_t i = size_t(ty) * size_t(g.cols) + size_t(tx);
            if (stored[i] != now[i] && stored[i] == nowWithBadge[i]) {
                stored[i] = now[i];
                ++n;
            }
        }
    return n;
}

size_t rleEncode(const uint8_t* data, size_t len, uint8_t* out, size_t outCap) {
    size_t n = 0, i = 0;
    while (i < len) {
        size_t run = 1;
        while (i + run < len && run < 256 && data[i + run] == data[i]) ++run;
        if (n + 2 > outCap) return 0;
        out[n++] = uint8_t(run - 1);
        out[n++] = data[i];
        i += run;
    }
    return n;
}

}  // namespace screen_track

namespace screen_track {
void withGhost(std::vector<Rect>& rects, Rect ghost) {
    if (rects.empty() || ghost.w <= 0 || ghost.h <= 0) return;
    rects.push_back(ghost);
    mergeRects(rects, 0, rects.size());
}

void mergeIntoRegions(std::vector<Rect>& rects, const std::vector<Rect>& regions) {
    if (rects.empty() || regions.empty()) return;
    auto hit = [](const Rect& a, const Rect& b) {
        return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
    };
    std::vector<bool> used(regions.size(), false);
    std::vector<Rect> out;
    for (const Rect& r : rects) {
        bool inRegion = false, outside = false;
        for (size_t i = 0; i < regions.size(); ++i) {
            if (regions[i].w <= 0 || regions[i].h <= 0 || !hit(r, regions[i])) continue;
            used[i] = true;
            inRegion = true;
            const Rect& g = regions[i];
            if (r.x < g.x || r.y < g.y || r.x + r.w > g.x + g.w || r.y + r.h > g.y + g.h) outside = true;
        }
        // A rect reaching beyond the region it touches (a popup over a graph) stays: the region alone would drop
        // the part outside (task 077); mergeRects() below unites the two.
        if (!inRegion || outside) out.push_back(r);
    }
    for (size_t i = 0; i < regions.size(); ++i)
        if (used[i]) out.push_back(regions[i]);
    mergeRects(out, 0, out.size());
    rects.swap(out);
}
}  // namespace screen_track
