#include "ws_frame.h"

#include <cstring>
#include <functional>

namespace ws {

std::vector<uint8_t> encode_frame(uint8_t opcode, const uint8_t* payload, size_t len, const uint8_t mask[4]) {
  std::vector<uint8_t> f;
  if (opcode >= 8 && len > 125) return f;
  f.push_back(0x80 | (opcode & 0x0F));
  if (len < 126) {
    f.push_back(0x80 | (uint8_t)len);
  } else if (len <= 0xFFFF) {
    f.push_back(0x80 | 126);
    f.push_back(len >> 8);
    f.push_back(len & 0xFF);
  } else {
    f.push_back(0x80 | 127);
    for (int i = 7; i >= 0; i--) f.push_back((uint8_t)((uint64_t)len >> (8 * i)));
  }
  f.insert(f.end(), mask, mask + 4);
  for (size_t i = 0; i < len; i++) f.push_back(payload[i] ^ mask[i & 3]);
  return f;
}

std::vector<uint8_t> encode_close(uint16_t code, const uint8_t mask[4]) {
  uint8_t p[2] = {(uint8_t)(code >> 8), (uint8_t)(code & 0xFF)};
  return encode_frame(kClose, p, 2, mask);
}

FeedStatus FrameDecoder::feed(const uint8_t* p, size_t n, FrameHandler& h) {
  if (err_ != FeedStatus::Ok) return err_;
  size_t i = 0;
  auto fail = [&](FeedStatus s) { err_ = s; return s; };
  std::function<FeedStatus()> end_frame;
  // Called when a frame header is complete.
  auto begin_payload = [&]() -> FeedStatus {
    if (control_) {
      if (!fin_ || len_ > 125) return fail(FeedStatus::ProtocolError);
    } else if (op_ == kCont) {
      if (!inMsg_) return fail(FeedStatus::ProtocolError);
    } else if (op_ == kText || op_ == kBinary) {
      if (inMsg_) return fail(FeedStatus::ProtocolError);
      msgBytes_ = 0;
      msgFirst_ = true;
    } else {
      return fail(FeedStatus::ProtocolError);
    }
    if (!control_ && (len_ > max_ || msgBytes_ > max_ - (size_t)len_)) return fail(FeedStatus::Oversize);
    got_ = 0;
    st_ = St::Payload;
    if (len_ == 0) {
      if (!control_) {
        bool first = msgFirst_;
        msgFirst_ = false;
        if (!h.onData(ctl_, 0, first, fin_)) { err_ = FeedStatus::Stopped; return FeedStatus::Stopped; }
      }
      FeedStatus s = end_frame();
      if (s != FeedStatus::Ok) err_ = s;
      return s;
    }
    return FeedStatus::Ok;
  };
  end_frame = [&]() -> FeedStatus {
    st_ = St::Head0;
    if (control_) {
      bool cont = true;
      if (op_ == kPing) cont = h.onPing(ctl_, (size_t)len_);
      else if (op_ == kClose) {
        uint16_t code = len_ >= 2 ? (uint16_t)(ctl_[0] << 8 | ctl_[1]) : 1005;
        cont = h.onClose(code);
      }
      return cont ? FeedStatus::Ok : FeedStatus::Stopped;
    }
    if (!fin_) { inMsg_ = true; return FeedStatus::Ok; }
    inMsg_ = false;
    return FeedStatus::Ok;
  };

  while (i < n) {
    switch (st_) {
      case St::Head0: {
        uint8_t b = p[i++];
        if (b & 0x70) return fail(FeedStatus::ProtocolError);  // RSV bits: no extensions negotiated
        fin_ = b & 0x80;
        op_ = b & 0x0F;
        control_ = op_ >= 8;
        st_ = St::Head1;
        break;
      }
      case St::Head1: {
        uint8_t b = p[i++];
        masked_ = b & 0x80;
        uint8_t l = b & 0x7F;
        maskPos_ = 0;
        if (l < 126) {
          len_ = l;
          st_ = masked_ ? St::Mask : St::Payload;
          if (!masked_) { FeedStatus s = begin_payload(); if (s != FeedStatus::Ok) return s; }
        } else {
          lenBytes_ = l == 126 ? 2 : 8;
          len_ = 0;
          st_ = St::Len;
        }
        break;
      }
      case St::Len: {
        len_ = len_ << 8 | p[i++];
        if (--lenBytes_ == 0) {
          if (len_ >> 63) return fail(FeedStatus::ProtocolError);
          if (masked_) st_ = St::Mask;
          else { FeedStatus s = begin_payload(); if (s != FeedStatus::Ok) return s; }
        }
        break;
      }
      case St::Mask: {
        mask_[maskPos_++] = p[i++];
        if (maskPos_ == 4) { FeedStatus s = begin_payload(); if (s != FeedStatus::Ok) return s; }
        break;
      }
      case St::Payload: {
        size_t take = (size_t)(len_ - got_);
        if (take > n - i) take = n - i;
        if (control_) {
          for (size_t k = 0; k < take; k++) ctl_[got_ + k] = masked_ ? p[i + k] ^ mask_[(got_ + k) & 3] : p[i + k];
        } else if (take > 0) {
          uint8_t buf[256];
          size_t done = 0;
          while (done < take) {
            size_t c = take - done < sizeof buf ? take - done : sizeof buf;
            for (size_t k = 0; k < c; k++)
              buf[k] = masked_ ? p[i + done + k] ^ mask_[(got_ + done + k) & 3] : p[i + done + k];
            bool last = fin_ && got_ + done + c == len_;
            bool first = msgFirst_;
            msgFirst_ = false;
            msgBytes_ += c;
            if (!h.onData(buf, c, first, last)) { err_ = FeedStatus::Stopped; return FeedStatus::Stopped; }
            done += c;
          }
        }
        got_ += take;
        i += take;
        if (got_ == len_) {
          FeedStatus s = end_frame();
          if (s != FeedStatus::Ok) { err_ = s; return s; }
        }
        break;
      }
    }
  }
  return FeedStatus::Ok;
}

}  // namespace ws
