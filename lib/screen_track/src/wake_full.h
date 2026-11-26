#pragma once
// Task 132: at most one GC16 full per scheduled wake. Pure decision for bootFrame when the stored screen
// record is missing or invalid. A scheduled (deferred) wake draws nothing now; the sleep-entry full
// (present::finishScheduled) does the single white+content full, so the wake itself must not.
namespace screen_track {

struct InvalidRecordPlan {
    bool fullNow;       // run the white+content full in bootFrame
    bool deferChanged;  // finishScheduled must treat the frame as changed
    bool whiteFirst;    // the deferred full is white + content (unknown picture on the panel)
};

constexpr InvalidRecordPlan invalidRecordPlan(bool deferred) {
    return deferred ? InvalidRecordPlan{false, true, true} : InvalidRecordPlan{true, false, false};
}

// GC16 fulls a wake costs in total: the one in bootFrame plus the one at sleep entry (a scheduled wake always
// ends with finishScheduled(true)).
constexpr int fullsPerWake(const InvalidRecordPlan& p, bool deferred) { return (p.fullNow ? 1 : 0) + (deferred ? 1 : 0); }

}  // namespace screen_track
