#pragma once
#include <cstddef>
#include <string>

// Max accepted HTTP response body (64 KB).
constexpr size_t kMaxResponseBody = 64 * 1024;

// content_length: value of Content-Length, or <0 when unknown/chunked.
inline bool content_length_too_large(long content_length, size_t limit = kMaxResponseBody) {
  return content_length >= 0 && static_cast<unsigned long>(content_length) > limit;
}

// Whether appending `chunk` bytes to `have` bytes stays within the limit.
inline bool body_fits(size_t have, size_t chunk, size_t limit = kMaxResponseBody) {
  return have <= limit && chunk <= limit - have;
}

// Total wall-clock budget for one request (connect + headers + body).
constexpr unsigned long kRequestDeadlineMs = 15000;

// Accumulates a response body under a size cap and an overall deadline.
// accept() returns the number of bytes consumed; 0 means refused (sticky), and
// the reason is recorded in too_large / deadline_exceeded.
struct BodySink {
  std::string data;
  bool too_large = false;
  bool deadline_exceeded = false;
  size_t limit = kMaxResponseBody;

  size_t accept(const unsigned char* p, size_t n, unsigned long elapsed_ms,
                unsigned long deadline_ms = kRequestDeadlineMs) {
    if (too_large || deadline_exceeded) return 0;
    if (elapsed_ms > deadline_ms) {
      deadline_exceeded = true;
      return 0;
    }
    if (!body_fits(data.size(), n, limit)) {
      too_large = true;
      return 0;
    }
    data.append(reinterpret_cast<const char*>(p), n);
    return n;
  }
};
