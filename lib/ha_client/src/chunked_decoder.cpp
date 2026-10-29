#include "chunked_decoder.h"

static int hexval(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

ChunkedDecoder::Status ChunkedDecoder::feed(const char* p, size_t n, const OutFn& out) {
  size_t i = 0;
  while (i < n) {
    switch (state_) {
      case State::Done: return Status::Done;
      case State::Error: return Status::Error;
      case State::Size: {
        char c = p[i++];
        int v = hexval(c);
        if (v >= 0) {
          if (++digits_ > 8) { state_ = State::Error; break; }
          remaining_ = remaining_ * 16 + (uint64_t)v;
        } else if (digits_ == 0) {
          state_ = State::Error;
        } else if (c == '\r') {
          state_ = State::SizeLF;
        } else if (c == ';' || c == ' ' || c == '\t') {
          state_ = State::Ext;
        } else {
          state_ = State::Error;
        }
        break;
      }
      case State::Ext:
        if (p[i++] == '\r') state_ = State::SizeLF;
        break;
      case State::SizeLF:
        if (p[i++] != '\n') { state_ = State::Error; break; }
        digits_ = 0;
        if (remaining_ == 0) { state_ = State::TrailerLine; line_empty_ = true; }
        else state_ = State::Data;
        break;
      case State::Data: {
        size_t take = n - i;
        if ((uint64_t)take > remaining_) take = (size_t)remaining_;
        if (!out(p + i, take)) return Status::Aborted;
        i += take;
        remaining_ -= take;
        if (remaining_ == 0) state_ = State::DataCR;
        break;
      }
      case State::DataCR:
        if (p[i++] != '\r') state_ = State::Error; else state_ = State::DataLF;
        break;
      case State::DataLF:
        if (p[i++] != '\n') state_ = State::Error; else state_ = State::Size;
        break;
      case State::TrailerLine: {
        char c = p[i++];
        if (c == '\r') state_ = State::TrailerLF;
        else line_empty_ = false;
        break;
      }
      case State::TrailerLF:
        if (p[i++] != '\n') { state_ = State::Error; break; }
        if (line_empty_) state_ = State::Done;
        else { state_ = State::TrailerLine; line_empty_ = true; }
        break;
    }
  }
  if (state_ == State::Error) return Status::Error;
  return state_ == State::Done ? Status::Done : Status::Ok;
}
