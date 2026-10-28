# 027: TLS session resumption (research)

Status: todo (research done; implementation not recommended yet)
Depends on: none
UI spec: none
Needs device: yes (to measure real handshake time before deciding)

## Scope
Research whether the HA client can resume TLS sessions across wakes to shorten
the ~6 s handshake seen in task 008 (Cloudflare-hosted domain). Nothing implemented.

## Files to touch
- none (research). If done later: a vendored ssl_client in src/ plus a small pure
  "session blob store" module in lib/ with native tests.

## Acceptance criteria
- [x] Framework capability documented
- [x] Server support checked from the PC
- [x] Storage and security options compared
- [x] Saving estimated
- [ ] (future) Decision after a device measurement of where the 6 s go

## Notes
Findings (Arduino-ESP32 2.0.17, mbedTLS 2.28.7):

1. Framework. libraries/WiFiClientSecure/src/ssl_client.cpp has no session code
   (grep for session/ticket: only a log line). start_ssl_client() does
   setup -> set_bio -> handshake in one function (lines ~257-271) and WiFiClientSecure
   keeps the context in a protected member (sslclient), so a subclass can call
   mbedtls_ssl_get_session after connect() (saving is easy), but there is no hook between
   mbedtls_ssl_setup and mbedtls_ssl_handshake. The mbedTLS side is ready:
   sdkconfig has CONFIG_MBEDTLS_CLIENT_SSL_SESSION_TICKETS=y; ssl.h has
   mbedtls_ssl_get_session / set_session / session_save / session_load
   (ssl.h:2516-2581, 3926). Minimal way: a patched copy of ssl_client.cpp/.h plus
   a WiFiClientSecure subclass or copy (about 150 lines, two extra calls:
   set_session before handshake, get_session+session_save after). Duplicate symbols
   against the framework's libWiFiClientSecure objects make this fragile (link order,
   HTTPClient uses the class from the library), so in practice it means a fully
   custom transport (own TCP + mbedTLS + HTTP/1.1 parsing, replacing HTTPClient) or
   a lib_deps-local fork of WiFiClientSecure named differently. Not "small".
2. Server (openssl 3, ha.example.org, TLS 1.2): sess_out/sess_in -> "Reused, TLSv1.2,
   ECDHE-ECDSA-CHACHA20-POLY1305". Ticket lifetime hint 64800 s (18 h). TLS 1.3 also
   negotiated by default (AES-256-GCM) but mbedTLS 2.28 is TLS 1.2 only, which is
   fine: the server picks ECDSA cert, X25519. Note the ECDSA cert means ECDSA verify
   (software on ESP32; no ECC hardware).
3. Storage. mbedtls_ssl_session_save for a TLS 1.2 ticket session is ~0.7-1.5 KB
   (master secret 48 B, ticket ~200-700 B, peer cert is dropped unless
   MBEDTLS_SSL_KEEP_PEER_CERTIFICATE; the framework build keeps it for
   get_peer_cert, so the blob can be 2-3 KB; strip by saving only after clearing
   or budget 4 KB). Touch mode: RTC slow memory (8 KB total, survives deep sleep,
   zero wear) fits. Button mode: full power-off loses RTC, so NVS blob (one write
   per wake only when the ticket changed; Cloudflare re-issues tickets often, so
   write at most once per N hours to bound wear; 4 KB per write is trivial for
   NVS wear leveling at a few writes/day). Security: the blob holds the master
   secret + ticket: whoever reads flash can resume/decrypt that one session for up
   to 18 h and impersonate this client's session. Flash encryption is off and the
   HA token already sits in the firmware image/NVS config, so the incremental
   risk is low, but it is not zero; ticket should be discarded on any TLS error.
4. Saving (estimate, ESP32 240 MHz, mbedTLS software bignum): full handshake CPU =
   X25519/ECDHE ~150-250 ms + ECDSA-P256 verify of server key exchange ~200-300 ms
   + chain verification (2 ECDSA verifies, bundle lookup) ~400-600 ms, total about
   0.8-1.2 s CPU, plus 2 RTTs and ~4 KB cert chain. Resumption (abbreviated, 1 RTT)
   skips all of it: saves roughly 1 s CPU + 1 RTT (~0.1-0.3 s). The 6 s in task 008
   was measured with debug logs, so most of it is probably log output, DNS, Wi-Fi
   association and Cloudflare RTT, not crypto. Real gain likely 1-1.5 s of a wake
   cycle, and zero if the HTTP request itself dominates.

Recommendation: do not implement now. The only workable route is replacing the
transport (high risk, touches the tested HA path) for about 1 s. First measure on the
device with debug logs off: time of DNS, TCP connect, handshake, first byte. If the
handshake alone is >2 s in release builds, revisit, using a custom ssl_client fork
and a RTC/NVS blob store behind a build flag (default off).
