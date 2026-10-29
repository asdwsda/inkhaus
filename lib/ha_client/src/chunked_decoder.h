#pragma once
// Incremental HTTP/1.1 chunked-transfer decoder (task 051 fix). Pure logic: input may be split at
// any byte. Chunk extensions and trailers are skipped.
#include <cstddef>
#include <cstdint>
#include <functional>

class ChunkedDecoder {
 public:
  enum class Status { Ok, Done, Error, Aborted };  // Aborted: the output callback returned false
  using OutFn = std::function<bool(const char*, size_t)>;

  // Decodes p[0..n). Bytes after the terminating chunk's trailers are ignored (Done).
  Status feed(const char* p, size_t n, const OutFn& out);
  bool done() const { return state_ == State::Done; }

 private:
  enum class State { Size, Ext, SizeLF, Data, DataCR, DataLF, TrailerLine, TrailerLF, Done, Error };
  State state_ = State::Size;
  uint64_t remaining_ = 0;
  int digits_ = 0;
  bool line_empty_ = true;  // trailer line has no bytes yet
};
