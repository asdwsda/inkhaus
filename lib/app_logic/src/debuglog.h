#pragma once
// /debug.log policy (task 041). Pure; the SD access is in src/wakelog.cpp.
#include <stddef.h>
#include <stdint.h>

#include <string>

namespace app_logic {

constexpr size_t kDebugLogMaxBytes = 1024 * 1024;  // at this size /debug.log rotates to /debug.old
constexpr size_t kDebugLogMaxLine = 200;

// True for a serial log line worth keeping: APP:/NET:/SLEEP:/WAKE: and BOOT: start/config/errors (not the
// BOOT: timing lines), and never one that mentions a secret (token, password, ssid, URL, ...).
bool isDebugLogLine(const std::string &line);
// "<ms> <line>\n": control characters replaced, cut at kDebugLogMaxLine.
std::string formatDebugLogLine(uint32_t ms, const std::string &line);
// True if appending addBytes to a file of curSize would pass the cap (rotate first).
bool debugLogNeedsRotation(size_t curSize, size_t addBytes);

}  // namespace app_logic
