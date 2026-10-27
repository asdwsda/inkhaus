#include "screen_track.h"

#include <algorithm>
#include <cstring>

namespace screen_track {

uint32_t fnv1a(const void* data, size_t len, uint32_t seed) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t h = seed;
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

uint32_t fnv1a(const char* str, uint32_t seed) { return fnv1a(str, std::strlen(str), seed); }

uint32_t fnv1a(uint32_t value, uint32_t seed) {
    const uint8_t b[4] = {uint8_t(value), uint8_t(value >> 8), uint8_t(value >> 16),
                          uint8_t(value >> 24)};
    return fnv1a(b, 4, seed);
}

Decision decide(const Record& stored, uint32_t screen_id, uint32_t content_hash,
                uint32_t panel_sig_now, bool panel_sig_valid) {
    if (!stored.valid) return {true, Reason::NoRecord};
    if (stored.screen_id != screen_id || stored.content_hash != content_hash)
        return {true, Reason::ScreenChanged};
    if (panel_sig_valid && stored.panel_sig != panel_sig_now) return {true, Reason::PanelMismatch};
    return {false, Reason::None};
}

const char* reasonName(Reason r) {
    switch (r) {
        case Reason::None: return "none";
        case Reason::NoRecord:
        case Reason::PanelMismatch:
        case Reason::ScreenChanged: return "unknown_screen";
    }
    return "none";
}

const char* reasonDetail(Reason r) {
    switch (r) {
        case Reason::None: return "none";
        case Reason::NoRecord: return "no_record";
        case Reason::PanelMismatch: return "panel_mismatch";
        case Reason::ScreenChanged: return "screen_changed";
    }
    return "none";
}

namespace {
bool touches(const Rect& a, const Rect& b) {
    return a.x <= b.x + b.w && b.x <= a.x + a.w && a.y <= b.y + b.h && b.y <= a.y + a.h;
}
Rect unite(const Rect& a, const Rect& b) {
    const int x0 = std::min(a.x, b.x), y0 = std::min(a.y, b.y);
    const int x1 = std::max(a.x + a.w, b.x + b.w), y1 = std::max(a.y + a.h, b.y + b.h);
    return {x0, y0, x1 - x0, y1 - y0};
}
}  // namespace

void DirtyTracker::mark(Rect r) {
    if (r.w <= 0 || r.h <= 0) return;
    // Merge with everything it touches; the merged rectangle may now touch more, so repeat.
    bool merged = true;
    while (merged) {
        merged = false;
        for (size_t i = 0; i < n_; ++i) {
            if (touches(r_[i], r)) {
                r = unite(r_[i], r);
                r_[i] = r_[--n_];
                merged = true;
                break;
            }
        }
    }
    if (n_ < kCapacity) {
        r_[n_++] = r;
    } else {
        r_[n_ - 1] = unite(r_[n_ - 1], r);
    }
}

}  // namespace screen_track
