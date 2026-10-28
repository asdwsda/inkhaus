#include "provision_proto.h"

#include <cstdio>
#include <cstdlib>

namespace provision {

uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t len) {
  crc = ~crc;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

static bool nameChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
         c == '_' || c == '-';
}

bool nameAllowed(const std::string &name) {
  if (name.empty() || name.size() > kMaxNameLen) return false;
  if (name == "config.yaml") return true;
  static const std::string kPrefix = "test-", kSuffix = ".yaml";
  if (name.size() <= kPrefix.size() + kSuffix.size()) return false;
  if (name.compare(0, kPrefix.size(), kPrefix) != 0) return false;
  if (name.compare(name.size() - kSuffix.size(), kSuffix.size(), kSuffix) != 0) return false;
  for (char c : name)
    if (!nameChar(c)) return false;
  return true;
}

ParseStatus parsePutfile(const std::string &line, Request &out) {
  static const std::string kCmd = "putfile";
  if (line.compare(0, kCmd.size(), kCmd) != 0 || (line.size() > kCmd.size() && line[kCmd.size()] != ' '))
    return ParseStatus::NotPutfile;
  // Exactly: putfile <name> <size> <crc>, single spaces.
  size_t p1 = line.find(' ');
  size_t p2 = p1 == std::string::npos ? p1 : line.find(' ', p1 + 1);
  size_t p3 = p2 == std::string::npos ? p2 : line.find(' ', p2 + 1);
  if (p3 == std::string::npos || line.find(' ', p3 + 1) != std::string::npos) return ParseStatus::BadCommand;
  const std::string name = line.substr(p1 + 1, p2 - p1 - 1);
  const std::string size = line.substr(p2 + 1, p3 - p2 - 1);
  const std::string crc = line.substr(p3 + 1);
  if (!nameAllowed(name)) return ParseStatus::BadName;
  if (size.empty() || size.size() > 6) return ParseStatus::BadSize;
  uint32_t sz = 0;
  for (char c : size) {
    if (c < '0' || c > '9') return ParseStatus::BadSize;
    sz = sz * 10 + uint32_t(c - '0');
  }
  if (sz == 0 || sz > kMaxFileBytes) return ParseStatus::BadSize;
  if (crc.size() != 8) return ParseStatus::BadCrc;
  uint32_t cv = 0;
  for (char c : crc) {
    uint32_t d;
    if (c >= '0' && c <= '9') d = uint32_t(c - '0');
    else if (c >= 'a' && c <= 'f') d = uint32_t(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') d = uint32_t(c - 'A' + 10);
    else return ParseStatus::BadCrc;
    cv = (cv << 4) | d;
  }
  out.name = name;
  out.size = sz;
  out.crc = cv;
  return ParseStatus::Ok;
}

const char *parseStatusReason(ParseStatus s) {
  switch (s) {
    case ParseStatus::BadCommand: return "bad-command";
    case ParseStatus::BadName: return "bad-name";
    case ParseStatus::BadSize: return "bad-size";
    case ParseStatus::BadCrc: return "bad-crc";
    default: return "";
  }
}

std::string hex32(uint32_t v) {
  char buf[9];
  snprintf(buf, sizeof buf, "%08x", (unsigned)v);
  return buf;
}

void Receiver::begin(const Request &req, uint32_t nowMs) {
  req_ = req;
  active_ = true;
  received_ = 0;
  crc_ = 0;
  lastMs_ = nowMs;
}

Receiver::Result Receiver::feed(const uint8_t *data, size_t n, uint32_t nowMs) {
  Result r;
  if (!active_ || n == 0) return r;
  size_t take = req_.size - received_;
  const size_t toBoundary = kAckEvery - (received_ % kAckEvery);
  if (take > toBoundary) take = toBoundary;
  if (take > n) take = n;
  crc_ = crc32Update(crc_, data, take);
  received_ += uint32_t(take);
  lastMs_ = nowMs;
  r.consumed = take;
  r.complete = received_ == req_.size;
  if (!r.complete && received_ % kAckEvery == 0) {
    r.ack = true;
    r.ackOffset = received_;
  }
  return r;
}

bool Receiver::timedOut(uint32_t nowMs) const {
  return active_ && !complete() && uint32_t(nowMs - lastMs_) > kIdleTimeoutMs;
}

Verdict Receiver::verdict() const {
  if (!complete()) return Verdict::Incomplete;
  return crc_ == req_.crc ? Verdict::Ok : Verdict::CrcMismatch;
}

std::string configSummary(const config::ParseResult &r) {
  char buf[160];
  if (r.ok()) {
    size_t widgets = 0;
    for (const auto &t : r.config.tabs) widgets += t.widgets.size();
    snprintf(buf, sizeof buf, "PUT-CONFIG ok tabs=%u widgets=%u connection=%s",
             (unsigned)r.config.tabs.size(), (unsigned)widgets, r.config.hasConnection ? "yes" : "no");
    return buf;
  }
  const config::ConfigError &e = r.errors.front();
  std::string path;
  for (char c : e.path) {  // path only; keep it one token and bounded
    if (path.size() >= 60) break;
    path += (c > ' ' && c < 0x7f) ? c : '_';
  }
  if (path.empty()) path = "-";
  snprintf(buf, sizeof buf, "PUT-CONFIG error count=%u first=%s line=%d", (unsigned)r.errors.size(),
           path.c_str(), e.line);
  return buf;
}

}  // namespace provision
