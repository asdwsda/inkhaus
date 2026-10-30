#pragma once
// Wake log + debug log on the SD card (task 029, 041). Compiled into every build, runs only when the debug_log setting is on
// (setEnabled); -DWAKE_LOG_SD (env m5paper-batterytest) forces it on. One CSV line per wake in /wakelog.csv (boot and sleep
// entry) and the filtered key log lines in /debug.log (1 MB, then rotated to /debug.old). Disabled: no SD access at all.
#include <stdint.h>

#include <string>

#include "power.h"

namespace wakelog {

// Runtime switch (debug_log setting). Ignored (stays on) in a -DWAKE_LOG_SD build. Turning it off drops pending lines.
void setEnabled(bool on);
bool enabled();  // debug_log on (or forced)
// A serial log line: kept (RAM, flushed at the next boot/sleep append) if it passes the key-line filter.
void note(const char *line);
// printf to Serial (as before) and note() of the same line (without its newline).
void logf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
// Call right after boot::begin() and the wake detection: remembers the wake reason and the start.
// deferWrite: keep the boot line in RAM instead of writing it to the SD card now (task 058: no SD access while
// the wake-feedback waveform runs); writeDeferred() appends it later.
void begin(const char *wakeName, power::Session session, bool deferWrite = false);
// Writes the boot line kept by begin(..., true), if any.
void writeDeferred();
// Phase marks (millis() values). Only the first Wi-Fi/fetch of a wake counts.
void sessionStarted(uint32_t nowMs);
void wifiUp(uint32_t nowMs);
void fetchDone(uint32_t nowMs, bool ok);
void refreshed(uint32_t durationMs);  // one display refresh took this long
// Remember (in RTC memory) the sleep path about to be used: "deepsleep_hold", "poweroff", ...
void setSleptPath(const char *path);
// At sleep entry: appends the line (mount, append, unmount). Logs "WAKELOG: ..." (no secrets).
void append(power::Session session, int64_t epoch, int32_t batteryMv);
// Serial commands `wakelog` / `debuglog` (read-only, every build): stream the file framed
// WAKELOG-BEGIN <n> / bytes / WAKELOG-END (WAKELOG-ERR <why>).
bool handleLine(const std::string &line);

}  // namespace wakelog
