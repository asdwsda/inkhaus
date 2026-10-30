#pragma once
// Minimal WebSocket client for one HA command per connection (task 054, 046 part B).
// The byte socket is abstract: the TLS connect lives in the device code (task 055).
//
// Flow: upgrade -> auth_required -> auth -> auth_ok -> command -> result (streamed to a sink)
// -> close frame. Bounded by StreamLimits (bytes of the result message, deadline, cancel flag).
// Error text never contains the token.
#include <cstddef>
#include <cstdint>
#include <string>

#include "ha_client.h"
#include "stream.h"

namespace ws {

class Socket {
 public:
  virtual ~Socket() = default;
  // Returns bytes written (n), or <0 on error.
  virtual int write(const uint8_t* p, size_t n) = 0;
  // Waits up to timeout_ms. Returns bytes read (>0), 0 on timeout, <0 when the peer closed / error.
  virtual int read(uint8_t* p, size_t cap, uint32_t timeout_ms) = 0;
  virtual void close() = 0;
  virtual uint32_t nowMs() = 0;  // monotonic
  // Randomness for the handshake key and the frame masks (device: esp_random). The default is a
  // deterministic xorshift for tests and host tools only.
  virtual uint32_t random32() {
    rnd_ ^= rnd_ << 13; rnd_ ^= rnd_ >> 17; rnd_ ^= rnd_ << 5;
    return rnd_;
  }

 private:
  uint32_t rnd_ = 0x9E3779B9u;
};

// Upgrade request for GET /api/websocket. key16: 16 random bytes. `host` is the Host header value.
std::string build_upgrade_request(const std::string& host, const uint8_t key16[16]);
std::string upgrade_key(const uint8_t key16[16]);  // the Sec-WebSocket-Key (base64)

// Checks the HTTP response head (up to and including the blank line): status 101, Upgrade and
// Sec-WebSocket-Accept matching the key.
bool check_upgrade_response(const std::string& head, const std::string& key_b64);

struct Request {
  std::string host;           // Host header (host[:port])
  std::string token;          // long-lived access token
  std::string command_json;   // the full command (id must be 1), e.g. from build_statistics_command
  uint8_t key[16] = {0};      // random (Socket::random32): the handshake key
  StreamLimits limits;
};

struct FlowResult {
  HaError error = HaError::None;
  std::string message;      // short, never contains the token
  std::string code;         // HA error code of success:false (e.g. "unknown_command")
  bool server_error = false;  // success:false
  size_t bytes = 0;         // result message bytes delivered to the sink
  uint32_t ms = 0;
  bool ok() const { return error == HaError::None; }
};

// Streams the `result` message (the whole JSON object, any chunking) to `sink`. Returning false
// from the sink cancels. Closes the socket at the end.
FlowResult run_command(Socket& sock, const Request& req, const ChunkFn& sink);

}  // namespace ws
