#pragma once
// Task 113: which firmware is running, as one line "1.3.1 debug | 7727f0d | 2026-11-03" (a dirty tree adds `+` to
// the commit). Pure formatting and fit rule; the values come from src/build_info.cpp.
#include <functional>
#include <string>

namespace settings_ui {

struct BuildInfo {
  std::string version;  // "1.3.1"
  std::string variant;  // "release", "debug", "batterytest", "epdoff", "poweroff", ...
  std::string commit;   // short hash, "" = unknown
  bool dirty = false;   // uncommitted changes at build time
  std::string date;     // "2026-11-03", "" = unknown
};

constexpr const char* kBuildSep = " | ";

inline std::string buildInfoLine(const BuildInfo& b, bool withCommit = true, bool withDate = true) {
  std::string s = b.version + " " + b.variant;
  if (withCommit && !b.commit.empty()) s += kBuildSep + b.commit + (b.dirty ? "+" : "");
  if (withDate && !b.date.empty()) s += kBuildSep + b.date;
  return s;
}

// Longest form that fits: full, without date, without commit, then the last form cut with an ellipsis.
inline std::string buildInfoFit(const BuildInfo& b, int maxWidth, const std::function<int(const std::string&)>& measure) {
  const std::string forms[3] = {buildInfoLine(b), buildInfoLine(b, true, false), buildInfoLine(b, false, false)};
  for (const std::string& f : forms)
    if (measure(f) <= maxWidth) return f;
  std::string t = forms[2];
  while (!t.empty()) {
    size_t n = t.size() - 1;
    while (n > 0 && (static_cast<unsigned char>(t[n]) & 0xC0) == 0x80) --n;
    t.resize(n);
    const std::string cand = t + "\xE2\x80\xA6";
    if (measure(cand) <= maxWidth) return cand;
  }
  return "\xE2\x80\xA6";
}

}  // namespace settings_ui
