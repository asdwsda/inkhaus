# 008: HTTPS certificate bundle for external_url

Status: in-progress
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
- Target server for the device check: the user's own domain (public URL, not a secret),
  `https://ha.example.org`, behind Cloudflare. Its certificate chain is Google Trust Services WE1
  (ECDSA), so GTS Root R4/R1 must be in the bundle. A request to `/api/` without a token must
  return HTTP 401 with a verified certificate. Cloudflare's bot protection may block non-browser
  clients; if the device gets 403 or an HTML challenge page, report it.
- The device check runs over the user's phone hotspot from `config.yaml` on the SD card (written
  by task 020). Until then, test the TLS handshake with a test-only build flag that sets a public
  test URL; no credentials are needed for a 401 check.
- Found during task 004: Arduino-ESP32 2.0.17 has `setCACertBundle()` but ships no bundle.
- 2026-10-28: the first attempt was blocked: the permission check refused to execute the downloaded
  `certs/gen_crt_bundle.py`. The user then approved the ESP-IDF generator and the full bundle
  (option A). Next session: run `tools/gen_ca_bundle.sh`. If the check blocks it again, the user approves
  it in the permission prompt or runs `tools/gen_ca_bundle.sh` once themselves (agents never work
  around the check). Then re-add `board_build.embed_files = certs/x509_crt_bundle` to
  platformio.ini (removed again so other builds don't fail), build, check `nm` and the flash delta,
  and run the live 401 check once the user's config is on the SD card.
- Already present (uncommitted): certs/cacert.pem (curl.se Mozilla list, 118 roots, sha256
  796f1389…3cbe), certs/gen_crt_bundle.py (sha256 1e8f0dc9…3020), tools/gen_ca_bundle.sh.
- PC part done (2026-10-28, uncommitted): bundle generated twice, identical sha256 8ca33067…5aec
  (also matches the user's own run); 54752 bytes, 118 roots incl. GTS Root R1/R4 and ISRG Root X1
  (checked with a self-written host reader); `board_build.embed_files` re-added; transport unchanged
  (`setCACertBundle` with the weak symbol). The symbols are kept by the linker only once the
  transport is referenced (wiring task 023 or the TLS_CHECK hook).
- `src/tls_check.cpp` (only with `-DTLS_CHECK -DTLS_CHECK_URL=\"…\"`): SD config → Wi-Fi → GET →
  `TLSCHK:` lines. Hook in setup() to be added by task 023.
- Device check still open: `https://ha.example.org/api/` → status=401 verified; `https://self-signed.badssl.com/`
  → rejected (status=0 + error); report a Cloudflare 403/challenge if it appears.
