// Task 060: times mbedtls_ssl_handshake via the linker wrap (-Wl,--wrap=mbedtls_ssl_handshake).
// Measurement only; the real function is always called and its result returned unchanged.
#include <Arduino.h>
#include <mbedtls/ssl.h>

extern "C" int __real_mbedtls_ssl_handshake(mbedtls_ssl_context* ssl);

static volatile uint32_t g_tls_ms = 0;

// Accumulates: the handshake may be driven in several calls (WANT_READ/WANT_WRITE).
extern "C" int __wrap_mbedtls_ssl_handshake(mbedtls_ssl_context* ssl) {
  const uint32_t t0 = millis();
  const int r = __real_mbedtls_ssl_handshake(ssl);
  g_tls_ms = g_tls_ms + (millis() - t0);
  return r;
}

void net_timing_tls_reset() { g_tls_ms = 0; }
uint32_t net_timing_tls_ms() { return g_tls_ms; }
