#pragma once
// Wake log on the SD card (task 029, battery test build only: -DWAKE_LOG_SD, env m5paper-batterytest).
// One CSV line per wake in /wakelog.csv, appended at boot (session=boot) and at sleep entry. Without the flag every function is
// an empty inline, and nothing touches the SD card on the wake path.
#include <stdint.h>

#include <string>

#include "power.h"

namespace wakelog {

#ifdef WAKE_LOG_SD
// Call right after boot::begin() and the wake detection: remembers the wake reason and the start.
void begin(const char *wakeName, power::Session session);
// Phase marks (millis() values). Only the first Wi-Fi/fetch of a wake counts.
void sessionStarted(uint32_t nowMs);
void wifiUp(uint32_t nowMs);
void fetchDone(uint32_t nowMs, bool ok);
void refreshed(uint32_t durationMs);  // one display refresh took this long
// Remember (in RTC memory) the sleep path about to be used: "deepsleep_hold", "poweroff", ...
void setSleptPath(const char *path);
// At sleep entry: appends the line (mount, append, unmount). Logs "WAKELOG: ..." (no secrets).
void append(power::Session session, int64_t epoch, int32_t batteryMv);
// Serial command `wakelog`: streams the file framed WAKELOG-BEGIN <n> / bytes / WAKELOG-END.
bool handleLine(const std::string &line);
#else
inline void begin(const char *, power::Session) {}
inline void sessionStarted(uint32_t) {}
inline void wifiUp(uint32_t) {}
inline void fetchDone(uint32_t, bool) {}
inline void refreshed(uint32_t) {}
inline void setSleptPath(const char *) {}
inline void append(power::Session, int64_t, int32_t) {}
inline bool handleLine(const std::string &) { return false; }
#endif

}  // namespace wakelog
