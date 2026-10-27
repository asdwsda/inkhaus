#pragma once
// Pure logic for "what is on the panel": a record of the last drawn screen (id + content hash +
// a signature read back from the controller's image memory) and the list of regions that were
// updated with a fast, dirty-prone refresh and still need one clean pass.
#include <cstddef>
#include <cstdint>

namespace screen_track {

// FNV-1a, 32 bit. Chain calls by passing the previous result as `seed`.
constexpr uint32_t kFnvSeed = 2166136261u;
uint32_t fnv1a(const void* data, size_t len, uint32_t seed = kFnvSeed);
uint32_t fnv1a(const char* str, uint32_t seed = kFnvSeed);
uint32_t fnv1a(uint32_t value, uint32_t seed = kFnvSeed);

// What the app stored after its last full draw.
struct Record {
    bool valid = false;
    uint32_t screen_id = 0;
    uint32_t content_hash = 0;  // hash of everything static on the screen
    uint32_t panel_sig = 0;     // hash of sample lines read back from the image memory
};

enum class Reason {
    None,           // partial update is safe
    NoRecord,       // nothing stored (first boot, erased NVS)
    ScreenChanged,  // other screen id or other content (firmware, rotation, text)
    PanelMismatch,  // record fits but the panel memory doesn't (another firmware ran, or a reset
                    // cleared the controller)
};

struct Decision {
    bool full = true;
    Reason reason = Reason::NoRecord;
};

// `panel_sig_valid` is false when the controller's image memory cannot be compared (it was
// power-cycled, so it holds nothing from the previous run); then only the record decides.
Decision decide(const Record& stored, uint32_t screen_id, uint32_t content_hash,
                uint32_t panel_sig_now, bool panel_sig_valid);

// Value for "BOOT: reason=": every reason for a full draw is "unknown_screen" (we cannot vouch
// for what is on the panel); reasonDetail() tells them apart.
const char* reasonName(Reason r);
// Finer detail for the log: no_record, screen_changed, panel_mismatch, none.
const char* reasonDetail(Reason r);

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
};

// Regions drawn with a fast refresh. Overlapping or touching rectangles are merged. If the
// capacity is exceeded the last entry grows to cover the new one (never loses a region).
class DirtyTracker {
public:
    static constexpr size_t kCapacity = 8;
    void mark(Rect r);
    size_t size() const { return n_; }
    Rect at(size_t i) const { return r_[i]; }
    void clear() { n_ = 0; }

private:
    Rect r_[kCapacity];
    size_t n_ = 0;
};

}  // namespace screen_track
