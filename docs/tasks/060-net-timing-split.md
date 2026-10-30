# 060: Split HTTPS timing (DNS / TCP / TLS / first byte)

Status: done
Depends on: 027 (research)
UI spec: none
Needs device: yes (measurement only)

## Scope
Measure where a remote HTTPS request's time goes, to decide task 027. No behaviour change.
- Time `WiFi.hostByName` in `ArduinoHttpTransport::Raw::send` (src/ha_transport_arduino.cpp) -> `dns_ms`.
- `-Wl,--wrap=mbedtls_ssl_handshake` with a wrapper that only times `__real_` -> `tls_ms`.
- `tcp_ms` = connect time - dns - tls; `ttfb_ms` = request written -> status line.
- One serial/debug line per new connection: `NETT: dns= tcp= tls= ttfb= total= reused=`.
Out of scope: session resumption itself (027).

## Files to touch
- src/ha_transport_arduino.cpp, a new src/net_timing.cpp (wrapper), platformio.ini (build flag)

## Acceptance criteria
- [x] `pio run` (all envs) and `pio test -e native` pass; the wrap links.
- [x] NETT lines only when the debug_log setting is on (Serial and /debug.log); off: no output. Device, release env, debug_log on, remote URL, ~5 cold wakes: NETT lines logged; numbers noted in 027.
- [x] Keep-alive reuse (task 036) unaffected: reused requests log `reused=1` with no tls time.

## Notes
- Details: task 027 "Research round 2".
- Device result (2026-10-30, debug_log on): local http: tcp 27-38 ms, ttfb 61-98 ms. One remote HTTPS fallback:
  dns=60 tcp=56 tls=3510 ttfb=3682 total=7333. The 5-wake remote series continues under task 027.
