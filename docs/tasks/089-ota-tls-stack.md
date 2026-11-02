# 089: OTA install crashes in the TLS handshake (loop stack overflow)

Status: todo
Depends on: 087
UI spec: docs/ui/settings.md §8.6 (unchanged)
Needs device: yes

## Scope
Found 2026-11-02 testing 1.3.0 against the real GitHub release: Check now works over TLS, but Yes, install
crashes 0.4 s after `OTA: install start` with "Stack canary watchpoint triggered (loopTask)" inside the
mbedTLS handshake (ecdsa signature check). The HTTP test server never ran TLS. Evidence:
scratchpad gh/install2.log (decoded backtrace).

Decision (architect): run every OTA network job (manifest check and download+install) in a one-shot FreeRTOS
task with a 20 KB stack (same as the net task, src/net_task.cpp), created per job and deleted when done.
Reason: the TLS stack need is only temporary; raising the Arduino loop stack would cost RAM for good.
- The loop keeps the UI: it polls the job state (phase, percent, result) through a small shared struct
  (atomics or a mutex) and draws the progress row as now; input/sleep rules unchanged.
- The `otaQuiesce()` memory check (largest free block >= 40 KB) runs after the job task's stack is allocated
  (or checks 40 KB + 20 KB before creating it); if creation fails -> the existing "not enough memory" result.
- No HA/net-task calls from the job task; the loop does not touch HTTPClient/Update while a job runs.
- Keep the `#ifndef FW_VERSION` guard in include/version.h (test builds report an older version).
- Release checklist (docs/DEVELOPMENT.md): before tagging, test the install over HTTPS from the latest
  GitHub release with a debug build reporting an older version (recipe below).

## Files to touch
- src/ota.{h,cpp}, src/main.cpp (call sites only), include/version.h, docs/DEVELOPMENT.md

## Acceptance criteria
- [ ] `pio run` (m5paper, m5paper-debug) and `pio test -e native` pass.
- [ ] Device, real GitHub: debug build with FW_VERSION "1.2.9" (PLATFORMIO_BUILD_FLAGS='-DFW_VERSION=\"1.2.9\"' for
      both build and upload), Check now offers 1.3.0, Yes installs it over HTTPS: progress row advances, SHA ok,
      reboot into 1.3.0, healthy, "Updated from 1.2.9". Record download time and `APP: ota_stack_free=`.
- [ ] Check now still works; the cold-boot check works (serial log).
- [ ] Debug build with OTA_TEST_URL: `--corrupt` still rejected (quick regression).
- [ ] Landscape Firmware row look (screenshot).
- [ ] End: normal m5paper-debug built from main flashed, Keep awake still On USB.
