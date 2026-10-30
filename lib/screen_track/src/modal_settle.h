#pragma once
// Popup ghost clean-up timing (pure logic). While a modal popup is open, value changes refresh with
// epd_fastest and are recorded here (union rect + time). Once `settleMs` pass without another one,
// due() says a single quality refresh of that rect should run; done() then resets the popup's debt.
#include <stdint.h>

#include "screen_track.h"

namespace screen_track {

class ModalSettle {
public:
    explicit ModalSettle(uint32_t settleMs = 1000) : settleMs_(settleMs) {}

    void noteFastest(const Rect &r, uint32_t now) {
        if (r.w <= 0 || r.h <= 0) return;
        if (!pending_) {
            rect_ = r;
        } else {
            const int x0 = r.x < rect_.x ? r.x : rect_.x;
            const int y0 = r.y < rect_.y ? r.y : rect_.y;
            const int x1 = (r.x + r.w) > (rect_.x + rect_.w) ? (r.x + r.w) : (rect_.x + rect_.w);
            const int y1 = (r.y + r.h) > (rect_.y + rect_.h) ? (r.y + r.h) : (rect_.y + rect_.h);
            rect_ = Rect{x0, y0, x1 - x0, y1 - y0};
        }
        pending_ = true;
        last_ = now;
    }
    bool pending() const { return pending_; }
    bool due(uint32_t now) const { return pending_ && (uint32_t)(now - last_) >= settleMs_; }
    const Rect &rect() const { return rect_; }
    void done() { pending_ = false; }
    void reset() { pending_ = false; }

private:
    uint32_t settleMs_;
    bool pending_ = false;
    uint32_t last_ = 0;
    Rect rect_;
};

}  // namespace screen_track
