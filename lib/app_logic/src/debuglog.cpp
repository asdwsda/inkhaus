#include "debuglog.h"

#include <cctype>
#include <cstring>

namespace app_logic {

namespace {

std::string lower(const std::string &s) {
  std::string o = s;
  for (char &c : o) c = char(tolower((unsigned char)c));
  return o;
}

bool startsWith(const std::string &s, const char *p) { return s.compare(0, strlen(p), p) == 0; }

}  // namespace

bool isDebugLogLine(const std::string &line) {
  const bool prefixOk = startsWith(line, "APP:") || startsWith(line, "NET:") || startsWith(line, "SLEEP:") ||
                        startsWith(line, "WAKE:") || startsWith(line, "BOOT:");
  if (!prefixOk) return false;
  if (startsWith(line, "BOOT:") && line.find("_ms=") != std::string::npos) return false;  // timing noise
  const std::string l = lower(line);
  static const char *const kSecret[] = {"token", "password", "passwd", "psk", "secret", "bearer",
                                        "authorization", "ssid", "pass=", "key=", "http:", "https:", "://"};
  for (const char *k : kSecret)
    if (l.find(k) != std::string::npos) return false;
  return true;
}

std::string formatDebugLogLine(uint32_t ms, const std::string &line) {
  std::string o = std::to_string((unsigned long)ms) + " ";
  size_t n = 0;
  for (char c : line) {
    if (n++ >= kDebugLogMaxLine) break;
    o += ((unsigned char)c < 0x20 || (unsigned char)c > 0x7e) ? '?' : c;
  }
  o += '\n';
  return o;
}

bool debugLogNeedsRotation(size_t curSize, size_t addBytes) { return curSize + addBytes > kDebugLogMaxBytes; }

}  // namespace app_logic
