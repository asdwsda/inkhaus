#pragma once
// Body pump for streaming requests (task 051 fix): reads the socket in small slices and checks the
// deadline and the cancel flag on every pass, also while no data is available. Pure logic over a
// StreamSource, so it is tested natively with a fake clock; src/ only provides the socket.
#include <cstdint>

#include "stream.h"

class StreamSource {
 public:
  virtual ~StreamSource() = default;
  // >0: bytes read; 0: nothing available now; <0: connection closed (or failed) and drained.
  virtual int read(uint8_t* buf, size_t cap) = 0;
  virtual uint32_t now() = 0;  // ms
  virtual void idle() = 0;     // short delay between empty polls
};

enum class BodyKind { Identity, Chunked };

// `length`: Content-Length for Identity, -1 = until the connection closes. `start_ms` is the
// now() of the request start (the deadline counts from it). `r.status` must be set by the caller.
// Fills r.bytes/too_large/cancelled/deadline; on a transport error or truncated body sets
// r.status = 0 and r.error.
void pump_body(StreamSource& src, BodyKind kind, int64_t length, const StreamLimits& lim,
               uint32_t start_ms, const ChunkFn& sink, StreamResult& r);
