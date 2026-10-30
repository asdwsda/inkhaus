#include "ws_crypto.h"

#include <cstring>

namespace ws {

namespace {
inline uint32_t rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

void block(uint32_t h[5], const uint8_t* p) {
  uint32_t w[80];
  for (int i = 0; i < 16; i++)
    w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
  for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
  for (int i = 0; i < 80; i++) {
    uint32_t f, k;
    if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
    else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
    else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
    else { f = b ^ c ^ d; k = 0xCA62C1D6; }
    uint32_t t = rol(a, 5) + f + e + k + w[i];
    e = d; d = c; c = rol(b, 30); b = a; a = t;
  }
  h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}
}  // namespace

void sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
  uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
  size_t full = len / 64;
  for (size_t i = 0; i < full; i++) block(h, data + 64 * i);
  uint8_t tail[128];
  size_t rem = len - full * 64;
  memcpy(tail, data + full * 64, rem);
  tail[rem++] = 0x80;
  size_t total = rem <= 56 ? 64 : 128;
  memset(tail + rem, 0, total - rem);
  uint64_t bits = (uint64_t)len * 8;
  for (int i = 0; i < 8; i++) tail[total - 1 - i] = (uint8_t)(bits >> (8 * i));
  block(h, tail);
  if (total == 128) block(h, tail + 64);
  for (int i = 0; i < 5; i++) {
    out[4 * i] = h[i] >> 24; out[4 * i + 1] = h[i] >> 16; out[4 * i + 2] = h[i] >> 8; out[4 * i + 3] = h[i];
  }
}

std::string base64(const uint8_t* d, size_t n) {
  static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)d[i] << 16 | (i + 1 < n ? (uint32_t)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
    o += t[v >> 18 & 63];
    o += t[v >> 12 & 63];
    o += i + 1 < n ? t[v >> 6 & 63] : '=';
    o += i + 2 < n ? t[v & 63] : '=';
  }
  return o;
}

std::string accept_for_key(const std::string& key_b64) {
  std::string s = key_b64 + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  uint8_t h[20];
  sha1(reinterpret_cast<const uint8_t*>(s.data()), s.size(), h);
  return base64(h, 20);
}

}  // namespace ws
