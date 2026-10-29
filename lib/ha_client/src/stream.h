#pragma once
// Streaming GET/POST for large responses (task 051): the body is handed to a chunk callback and
// never accumulated, so the 64 KB cap of normal calls does not apply; a per-request byte limit,
// a deadline and a cancel flag take its place.
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

constexpr size_t kStreamMaxBytes = 512 * 1024;
constexpr uint32_t kStreamDeadlineMs = 15000;

struct StreamLimits {
  size_t max_bytes = kStreamMaxBytes;
  uint32_t deadline_ms = kStreamDeadlineMs;
  const volatile bool* cancel = nullptr;  // set by another task to abort the transfer
};

// Returns false from on_chunk to abort (counts as cancelled).
using ChunkFn = std::function<bool(const char*, size_t)>;

struct StreamResult {
  int status = 0;  // 0 = no response
  std::string error;
  size_t bytes = 0;  // delivered to the sink
  bool too_large = false;
  bool cancelled = false;
  bool deadline = false;
  bool reused = false;
  uint32_t ms = 0;
  // A complete body arrived: 2xx and no abort of any kind.
  bool completed() const { return status >= 200 && status < 300 && !too_large && !cancelled && !deadline; }
};

// Per-byte accounting shared by the real transport and the tests. accept() returns the number of
// bytes consumed; 0 means refused (sticky) and the reason is recorded in `r`.
class StreamGate {
 public:
  StreamGate(const StreamLimits& limits, ChunkFn sink) : lim_(limits), sink_(std::move(sink)) {}
  size_t accept(const char* p, size_t n, uint32_t elapsed_ms, StreamResult& r) {
    if (r.too_large || r.cancelled || r.deadline) return 0;
    if (lim_.cancel && *lim_.cancel) { r.cancelled = true; return 0; }
    if (elapsed_ms > lim_.deadline_ms) { r.deadline = true; return 0; }
    if (r.bytes > lim_.max_bytes || n > lim_.max_bytes - r.bytes) { r.too_large = true; return 0; }
    if (sink_ && !sink_(p, n)) { r.cancelled = true; return 0; }
    r.bytes += n;
    return n;
  }

 private:
  StreamLimits lim_;
  ChunkFn sink_;
};
