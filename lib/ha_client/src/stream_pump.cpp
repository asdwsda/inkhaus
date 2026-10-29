#include "stream_pump.h"

#include "chunked_decoder.h"

void pump_body(StreamSource& src, BodyKind kind, int64_t length, const StreamLimits& lim,
               uint32_t start_ms, const ChunkFn& sink, StreamResult& r) {
  StreamGate gate(lim, sink);
  ChunkedDecoder dec;
  int64_t left = length;  // Identity with known length
  if (kind == BodyKind::Identity && length == 0) return;
  uint8_t buf[512];
  for (;;) {
    uint32_t elapsed = src.now() - start_ms;
    if (lim.cancel && *lim.cancel) { r.cancelled = true; return; }
    if (elapsed > lim.deadline_ms) { r.deadline = true; return; }
    size_t cap = sizeof(buf);
    if (kind == BodyKind::Identity && left >= 0 && (int64_t)cap > left) cap = (size_t)left;
    int n = src.read(buf, cap);
    if (n == 0) { src.idle(); continue; }
    if (n < 0) {
      bool ok = (kind == BodyKind::Identity && left < 0);
      if (!ok) { r.status = 0; r.error = kind == BodyKind::Chunked ? "truncated chunked body" : "truncated body"; }
      return;
    }
    if (kind == BodyKind::Identity) {
      if (gate.accept((const char*)buf, (size_t)n, elapsed, r) == 0) return;
      if (left >= 0) { left -= n; if (left == 0) return; }
    } else {
      ChunkedDecoder::Status st = dec.feed((const char*)buf, (size_t)n, [&](const char* p, size_t m) {
        return gate.accept(p, m, elapsed, r) != 0;
      });
      if (st == ChunkedDecoder::Status::Aborted) return;  // flags set by the gate
      if (st == ChunkedDecoder::Status::Error) { r.status = 0; r.error = "bad chunked encoding"; return; }
      if (st == ChunkedDecoder::Status::Done) return;
    }
  }
}
