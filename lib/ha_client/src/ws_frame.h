#pragma once
// RFC 6455 frame encoder (client side, always masked) and streaming decoder (task 054).
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ws {

enum Opcode : uint8_t { kCont = 0, kText = 1, kBinary = 2, kClose = 8, kPing = 9, kPong = 10 };

// One complete masked frame (FIN set). Control payloads must be <= 125 bytes (else empty result).
std::vector<uint8_t> encode_frame(uint8_t opcode, const uint8_t* payload, size_t len, const uint8_t mask[4]);
// Close frame with a status code (1000 = normal).
std::vector<uint8_t> encode_close(uint16_t code, const uint8_t mask[4]);

// Receives decoded events. Return false to stop decoding (feed() returns Stopped).
class FrameHandler {
 public:
  virtual ~FrameHandler() = default;
  // A piece of a text/binary message. `first`/`last` mark the message boundaries; an empty
  // message arrives as one call with n == 0, first and last true.
  virtual bool onData(const uint8_t* p, size_t n, bool first, bool last) = 0;
  virtual bool onPing(const uint8_t* p, size_t n) = 0;
  virtual bool onClose(uint16_t code) = 0;  // 1005 when the frame carried no code
};

enum class FeedStatus { Ok, Stopped, ProtocolError, Oversize };

class FrameDecoder {
 public:
  // max_message: limit for the total payload of one (possibly fragmented) message.
  explicit FrameDecoder(size_t max_message) : max_(max_message) {}
  FeedStatus feed(const uint8_t* p, size_t n, FrameHandler& h);

 private:
  enum class St { Head0, Head1, Len, Mask, Payload };
  St st_ = St::Head0;
  bool fin_ = false, masked_ = false, control_ = false;
  uint8_t op_ = 0, lenBytes_ = 0, maskPos_ = 0;
  uint8_t mask_[4] = {0, 0, 0, 0};
  uint64_t len_ = 0, got_ = 0;
  bool inMsg_ = false;      // inside a fragmented data message
  bool msgFirst_ = true;    // next data piece is the first of its message
  size_t msgBytes_ = 0;
  size_t max_;
  uint8_t ctl_[125];
  FeedStatus err_ = FeedStatus::Ok;
};

}  // namespace ws
