# 008: HTTPS certificate bundle for external_url

Status: todo
Depends on: 004
UI spec: none
Needs device: yes (final check only)

## Scope
Make `https://` external URLs work with proper certificate validation.
- Generate the binary x509 root bundle (ESP-IDF `gen_crt_bundle.py`, Mozilla roots)
  into `certs/x509_crt_bundle`, with a script in `tools/` to regenerate it and a
  note on how/when to refresh it.
- Embed it in the `m5paper` env (`board_build.embed_files`), so the weak
  `_binary_certs_x509_crt_bundle_start` symbol in `src/ha_transport_arduino.cpp` resolves.
- Measure the flash cost and report it.
- No insecure fallback, ever.

Out of scope: TLS session resumption (later, together with power measurements).

## Files to touch
- platformio.ini
- certs/x509_crt_bundle (generated)
- tools/gen_ca_bundle.sh
- src/ha_transport_arduino.cpp (only if the symbol name or setup needs adjusting)

## Acceptance criteria
- [ ] `pio run -e m5paper` passes and the bundle symbol is in the ELF (`nm` output).
- [ ] `tools/gen_ca_bundle.sh` regenerates an identical file from the same input.
- [ ] On device: an https request to a public HA-like host (e.g. a Nabu Casa URL, or any
      https host returning 401 without a token) completes the TLS handshake; a host with
      an invalid certificate (e.g. self-signed.badssl.com) is rejected.

## Notes
- Found during task 004: Arduino-ESP32 2.0.17 has `setCACertBundle()` but ships no bundle.
