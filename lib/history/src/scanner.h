#pragma once
// Streaming scanner for the HA REST history body
//   GET /api/history/period/<start>?filter_entity_id=X&end_time=Y&minimal_response&no_attributes
//   -> [[{"entity_id":..,"state":"21.4","last_changed":"2026-10-29T12:00:00+00:00"},
//        {"last_changed":"...","state":"21.5"}, ...]]
//
// API (for the transport sink of task 051, `void(const char*, size_t)`):
//
//   history::Collector col(params);            // see collector.h
//   history::Scanner sc(col);                  // col implements PointSink
//   sink = [&](const char* d, size_t n) { sc.feed(d, n); };   // any chunk size, even 1 byte
//   ...after the last chunk (or when the transfer stops early / is cancelled):
//   history::ScanResult r = sc.finish();
//
// feed() may be called with chunks of any size >= 0 (split anywhere, also inside strings,
// escapes or numbers). finish() must be called exactly once at the end.
//
// RAM: bounded and independent of the body size. The scanner keeps a fixed ~200 byte state
// (key/state/time string buffers; longer strings are truncated) and never stores the body.
// Together with the Collector (<= kMaxColumns columns x 13 B + <= kMaxBuckets buckets) the
// whole pipeline stays under ~20 KB.
#include <cstddef>
#include <cstdint>

namespace history {

// Receives one parsed history entry, in the order of the body.
// `state` is NOT NUL-terminated beyond `len` semantics: it is, but use `len`.
class PointSink {
 public:
  virtual ~PointSink() = default;
  virtual void onPoint(int64_t epoch, const char* state, size_t len) = 0;
};

enum class ScanResult {
  Ok,         // well-formed body (including "[]" and "[[]]": zero points)
  Truncated,  // body ended inside the JSON (limit hit / cancelled); points so far were delivered
  Malformed,  // not the expected shape (e.g. an HA error object or HTML); may have delivered nothing
};

class Scanner {
 public:
  explicit Scanner(PointSink& sink) : sink_(sink) {}

  void feed(const char* data, size_t len);
  ScanResult finish();

  size_t points() const { return points_; }
  size_t skipped() const { return skipped_; }  // entries without parseable time/state
  size_t bytes() const { return bytes_; }

 private:
  static constexpr size_t kBuf = 64;  // longest string kept; longer ones are truncated
  struct Str {
    char b[kBuf];
    size_t n = 0;
    void clear() { n = 0; b[0] = 0; }
    void add(char c) { if (n < kBuf - 1) { b[n++] = c; b[n] = 0; } }
  };
  void onChar(char c);
  void endString();
  void takeValue();
  void emit();

  PointSink& sink_;
  int depth_ = 0;
  bool inStr_ = false, esc_ = false, hasTok_ = false, bad_ = false, sawAny_ = false;
  Str cur_, tok_, key_, state_, time_, time2_;
  bool haveState_ = false;
  size_t points_ = 0, skipped_ = 0, bytes_ = 0;
};

}  // namespace history
