#include "ota_logic.h"

#include <ArduinoJson.h>

#include <cctype>

namespace ota_logic {

static bool num(const std::string &s, size_t &i, int &out) {
  size_t st = i;
  long v = 0;
  while (i < s.size() && isdigit((unsigned char)s[i])) {
    v = v * 10 + (s[i] - '0');
    if (v > 100000) return false;
    i++;
  }
  if (i == st) return false;
  out = (int)v;
  return true;
}

bool parseVersion(const std::string &str, Version &out) {
  size_t i = 0;
  if (i < str.size() && (str[i] == 'v' || str[i] == 'V')) i++;
  Version v;
  if (!num(str, i, v.major)) return false;
  if (i < str.size() && str[i] == '.') {
    i++;
    if (!num(str, i, v.minor)) return false;
    if (i < str.size() && str[i] == '.') {
      i++;
      if (!num(str, i, v.patch)) return false;
    }
  }
  if (i < str.size()) {
    if (str[i] != '-' && str[i] != '+') return false;
    v.pre = str[i] == '-';
  }
  out = v;
  return true;
}

int compare(const Version &a, const Version &b) {
  if (a.major != b.major) return a.major < b.major ? -1 : 1;
  if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
  if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
  return 0;
}

bool isNewer(const std::string &current, const std::string &candidate) {
  Version c, n;
  if (!parseVersion(current, c) || !parseVersion(candidate, n)) return false;
  if (n.pre) return false;
  return compare(n, c) > 0;
}

static bool isHex64(const std::string &s) {
  if (s.size() != 64) return false;
  for (char ch : s)
    if (!isxdigit((unsigned char)ch)) return false;
  return true;
}

bool parseManifest(const std::string &json, Manifest &out) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  const char *ver = doc["version"] | static_cast<const char *>(nullptr);
  JsonObjectConst o = doc["ota"].as<JsonObjectConst>();
  if (!ver || o.isNull()) return false;
  const char *file = o["file"] | static_cast<const char *>(nullptr);
  const char *sha = o["sha256"] | static_cast<const char *>(nullptr);
  uint32_t size = o["size"] | 0u;
  if (!file || !*file || !sha || !size) return false;
  Manifest m;
  m.version = ver;
  m.file = file;
  m.size = size;
  for (const char *p = sha; *p; p++) m.sha256 += (char)tolower((unsigned char)*p);
  if (!isHex64(m.sha256)) return false;
  Version v;
  if (!parseVersion(m.version, v)) return false;
  // The asset name is appended to a URL directory: no path tricks.
  if (m.file.find('/') != std::string::npos || m.file.find("..") != std::string::npos) return false;
  out = m;
  return true;
}

bool offer(const std::string &current, const Manifest &m, uint32_t maxSize) {
  return m.size > 0 && m.size <= maxSize && isNewer(current, m.version);
}

bool manifestLenOk(int64_t contentLen) { return contentLen <= (int64_t)kMaxManifestBytes; }
bool memoryOk(uint32_t largestFreeBlock) { return largestFreeBlock >= kMinFreeBlock; }

bool jobExpired(bool install, uint32_t elapsedMs) {
  return elapsedMs >= (install ? kInstallDeadlineMs : kCheckDeadlineMs);
}

bool checkDue(const CheckState &s, int64_t now, bool coldBoot, bool wifiUp) {
  if (!wifiUp) return false;
  if (coldBoot) return now <= 0 || s.lastAttempt <= 0 || now - s.lastAttempt >= kColdCheckMinS;
  if (now <= 0) return false;
  if (s.lastSuccess > 0 && now - s.lastSuccess < kCheckIntervalS) return false;
  if (s.lastAttempt > 0 && now - s.lastAttempt < kRetryIntervalS) return false;
  return true;
}

InstallBlock installAllowed(bool usb, int batteryPct) {
  return (usb || batteryPct >= 50) ? InstallBlock::None : InstallBlock::Battery;
}

bool rollbackNow(int earlierBoots) { return earlierBoots >= kMaxUnhealthyBoots; }

bool healthy(bool firstFrameDrawn, uint32_t loopMs) { return firstFrameDrawn && loopMs >= kHealthyAfterMs; }

}  // namespace ota_logic
