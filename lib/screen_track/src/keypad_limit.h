#pragma once
// Task 123: decides whether a keypad key press may be shown limited to the popup's code box + message line.
// Pure logic; the device code is in src/present.cpp. The limit is only safe when it drops nothing:
// every changed rect is empty or lies inside the limit. Any other change (widget, popup state) means a
// normal partial refresh of all rects, because the one-shot change flags are already consumed.
#include <vector>

#include "screen_track.h"

namespace screen_track {

inline bool rectEmpty(const Rect &r) { return r.w <= 0 || r.h <= 0; }

inline bool rectInside(const Rect &r, const Rect &outer) {
    return r.x >= outer.x && r.y >= outer.y && r.x + r.w <= outer.x + outer.w && r.y + r.h <= outer.y + outer.h;
}

inline bool keypadLimitApplies(const std::vector<Rect> &rects, const Rect &limit) {
    for (const Rect &r : rects)
        if (!rectEmpty(r) && !rectInside(r, limit)) return false;
    return true;
}

}  // namespace screen_track
