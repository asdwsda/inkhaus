#pragma once
// Protocol logic for `putfile` (task 020): CRC32, command parsing, name whitelist, the receive state
// machine with 1 KB flow control, and the config summary line. Pure C++17, no Arduino, no I/O.
// Nothing here ever formats file content.

#include <cstddef>
#include <cstdint>
#include <string>

#include "config_model.h"

namespace provision {

constexpr size_t kMaxFileBytes = 64 * 1024;
constexpr size_t kAckEvery = 1024;
constexpr uint32_t kIdleTimeoutMs = 3000;  // no byte for this long aborts the transfer
constexpr size_t kMaxNameLen = 40;

// CRC-32 (IEEE 802.3, same as zlib / Python zlib.crc32). Chain with the previous result.
uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t len);
inline uint32_t crc32(const uint8_t *data, size_t len) { return crc32Update(0, data, len); }

// "config.yaml" or "test-<[A-Za-z0-9._-]+>.yaml"; no path separators.
bool nameAllowed(const std::string &name);

struct Request {
  std::string name;
  uint32_t size = 0;
  uint32_t crc = 0;
};

enum class ParseStatus { Ok, NotPutfile, BadCommand, BadName, BadSize, BadCrc };
// Parses "putfile <name> <size> <crc32 hex, 8 digits>". NotPutfile: the line is another command.
ParseStatus parsePutfile(const std::string &line, Request &out);
// Reason token for PUT-ERR ("bad-command", "bad-name", "bad-size", "bad-crc"); "" for Ok/NotPutfile.
const char *parseStatusReason(ParseStatus s);

// "%08x" lowercase.
std::string hex32(uint32_t v);

enum class Verdict { Incomplete, Ok, CrcMismatch };

class Receiver {
 public:
  struct Result {
    size_t consumed = 0;      // bytes of the input taken (they are the next bytes of the file)
    bool ack = false;         // send "PUT-ACK <ackOffset>" now
    uint32_t ackOffset = 0;
    bool complete = false;    // all `size` bytes received
  };

  void begin(const Request &req, uint32_t nowMs);
  // Takes at most up to the next 1 KB boundary / the end of the file, so the caller loops:
  //   while (n) { r = feed(p, n, now); store(p, r.consumed); p += r.consumed; n -= r.consumed; ... }
  // Bytes beyond the end of the file are not consumed.
  Result feed(const uint8_t *data, size_t n, uint32_t nowMs);
  bool timedOut(uint32_t nowMs) const;  // wrap-safe
  bool complete() const { return active_ && received_ == req_.size; }
  Verdict verdict() const;
  uint32_t received() const { return received_; }
  uint32_t crc() const { return crc_; }  // CRC of the bytes received so far
  const Request &request() const { return req_; }

 private:
  Request req_;
  bool active_ = false;
  uint32_t received_ = 0;
  uint32_t crc_ = 0;
  uint32_t lastMs_ = 0;
};

// "PUT-CONFIG ok tabs=<n> widgets=<n> connection=<yes|no>" or
// "PUT-CONFIG error count=<n> first=<path> line=<n>". Counts and the error path/line only.
std::string configSummary(const config::ParseResult &r);

}  // namespace provision
