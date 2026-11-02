#pragma once
#include <cstdint>

namespace screen_track {

// Polls `ready()` until it is true or `timeoutMs` has passed. `now()` returns milliseconds, `step()` is
// called between polls (delay and watchdog feed on the device). Wrap-safe. Returns true if ready.
template <typename Ready, typename Now, typename Step>
bool boundedWait(Ready ready, Now now, Step step, uint32_t timeoutMs) {
    const uint32_t t = now();
    while (!ready()) {
        if ((uint32_t)(now() - t) >= timeoutMs) return false;
        step();
    }
    return true;
}

}  // namespace screen_track
