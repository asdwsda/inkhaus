#include "scanner.h"

#include <cstring>

#include "iso8601.h"

namespace history {

void Scanner::feed(const char* data, size_t len) {
  bytes_ += len;
  for (size_t i = 0; i < len; ++i) onChar(data[i]);
}

void Scanner::endString() {
  tok_ = cur_;
  hasTok_ = true;
}

// A value string was seen for the current key.
void Scanner::takeValue() {
  if (depth_ != 3 || !hasTok_) return;
  if (!strcmp(key_.b, "state")) {
    state_ = tok_;
    haveState_ = true;
  } else if (!strcmp(key_.b, "last_changed")) {
    time_ = tok_;
  } else if (!strcmp(key_.b, "last_updated")) {
    time2_ = tok_;
  }
}

void Scanner::emit() {
  int64_t t;
  const Str& ts = time_.n ? time_ : time2_;
  if (haveState_ && ts.n && parseIso8601(ts.b, ts.n, t)) {
    ++points_;
    sink_.onPoint(t, state_.b, state_.n);
  } else {
    ++skipped_;
  }
  state_.clear();
  time_.clear();
  time2_.clear();
  haveState_ = false;
}

void Scanner::onChar(char c) {
  if (inStr_) {
    if (esc_) {
      esc_ = false;
      cur_.add(c);
    } else if (c == '\\') {
      esc_ = true;
    } else if (c == '"') {
      inStr_ = false;
      endString();
    } else {
      cur_.add(c);
    }
    return;
  }
  switch (c) {
    case '"':
      inStr_ = true;
      cur_.clear();
      return;
    case ':':
      if (hasTok_) key_ = tok_;
      hasTok_ = false;
      return;
    case ',':
      takeValue();
      hasTok_ = false;
      key_.clear();
      return;
    case '[':
    case '{':
      if (!sawAny_) {
        sawAny_ = true;
        if (c != '[') bad_ = true;  // e.g. an HA error object
      }
      if (depth_ == 0 && c != '[') bad_ = true;
      if (depth_ == 1 && c != '[') bad_ = true;
      ++depth_;
      hasTok_ = false;
      key_.clear();
      return;
    case '}':
    case ']':
      takeValue();
      hasTok_ = false;
      key_.clear();
      if (depth_ == 3 && c == '}') emit();
      if (depth_ > 0) --depth_;
      return;
    default:
      return;
  }
}

ScanResult Scanner::finish() {
  if (bad_ || !sawAny_) return ScanResult::Malformed;
  if (depth_ != 0 || inStr_) return ScanResult::Truncated;
  return ScanResult::Ok;
}

}  // namespace history
