#pragma once
// Small SHA-1 and base64 for the WebSocket handshake (task 054). Not for security use.
#include <cstddef>
#include <cstdint>
#include <string>

namespace ws {

void sha1(const uint8_t* data, size_t len, uint8_t out[20]);
std::string base64(const uint8_t* data, size_t len);

// Sec-WebSocket-Accept for a Sec-WebSocket-Key (RFC 6455 4.2.2).
std::string accept_for_key(const std::string& key_b64);

}  // namespace ws
