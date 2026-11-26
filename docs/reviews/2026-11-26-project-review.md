# Project review (committed code, HEAD f2e5303)

Scope: src/, lib/, include/, test/, tools/, platformio.ini, site/, .github/, docs vs code. Read only.
Uncommitted task 126a work is not judged. Items already open in docs/tasks or docs/device-checks.md are skipped
unless something new was found. Line numbers refer to `git show HEAD:<path>`.

---

## HIGH

### H1. Debug-log buffer is written from two cores without a lock (heap corruption)
- Evidence: `src/wakelog.cpp:143-148` `note()` appends to the global `std::string g_pending`; `flushDebug()`
  (`:104`) clears it on the UI loop. `wakelog::logf` is also called from other tasks:
  - net task (core 0): `src/ha_transport_arduino.cpp:111-112, 168-169` (`NETT:` lines, which pass
    `isDebugLogLine`), `src/net_task.cpp:164` → `wakelog::historyFetched` changes `g_rec` (`src/wakelog.cpp:230`);
  - net task / OTA job task: `src/ota.cpp:146, 152, 213` (`cancelled()`, `prepareHeap()`).
  The UI loop (core 1) writes `APP:`/`NET:`/`BOOT:` lines into the same string at the same time.
- Why it matters: two unsynchronised `std::string::operator+=` calls, or `+=` during `clear()`/`print`, can corrupt the heap.
  The likely result is a PANIC or TASK_WDT some time later. This happens only with Debug log on, but that is exactly
  the battery-test setup (`m5paper-batterytest`, `m5paper-poweroff` force it on). It may explain some of the
  "unexplained" crashes in the device checks (087 TASK_WDT, 093).
- Fix: guard `g_pending`, `g_rec` and `g_deferredBoot` with one `portMUX`/`std::mutex` (or a FreeRTOS mutex) in
  note/logf/flushDebug/historyFetched/append. Alternative: have off-UI tasks push their lines into a small
  lock-protected queue that the UI drains. Add a native test of the wrapper if it moves to lib/.

### H2. A transient SD mount failure swaps the user's dashboard for the demo, and deletes the config cache
- Evidence: `src/app_config.cpp:199-206`: any `config_sd::stat()` result other than Ok (including `NoCard` from a
  failed `SD.begin`) calls `invalidateCache()`. `:217-221` then treats `NoCard` like "no config" and loads the
  embedded demo. `src/config_sd.cpp:32-41`: `mount()` is a single `SD.begin` on the bus it shares with the IT8951,
  with no retry. `stat()` runs on every wake, scheduled ones too.
- Why it matters: one flaky mount (shared SPI bus, card contact, a cold card) turns a real device into demo mode
  for that wake. On a scheduled wake the demo dashboard (fake values) is drawn as a GC16 full and stays on the
  wall for hours, until the next wake. The cache with the secrets is also deleted, so the next wake has to parse
  the YAML again.
- Fix: retry the mount once or twice. On `NoCard`, keep using the cached config when one exists (the cache is
  the import of the card). Invalidate only on a positive "card present, file gone" (`NoFile`) or on a changed
  fingerprint. Move the decision into a lib/app_logic function and unit-test it.

---

## MEDIUM

### M1. Service calls can be sent twice (non-idempotent actions)
- Evidence: `src/ha_transport_arduino.cpp:161` marks a reused-socket failure with codes -1..-5 as `stale`,
  including -5 `HTTPC_ERROR_CONNECTION_LOST`, which HTTPClient also returns after the request was written while
  it waits for the response. `lib/ha_client/src/keep_alive.cpp:18-21` then resends the POST once.
  Independently, `lib/ha_client/src/ha_client.cpp:45-48` (`post`) falls back to the next base URL on any
  `status == 0`, including `HTTPC_ERROR_READ_TIMEOUT`, when HA most likely already got and ran the call.
- Why it matters: idempotent turn_on/turn_off is fine, but `media_player.media_next_track/previous`,
  volume steps, `cover.stop/open` on an `unknown` cover, garage open/close and alarm arm/disarm can run twice.
  The worst-case tap latency also doubles: connect 4 s + read 8 s per base.
- Fix: in `post()` for `/api/services/...`, retry or fall back only when the request was provably not sent
  (connect failure; `SEND_HEADER_FAILED` on a reused socket). Never retry after a read timeout or
  `CONNECTION_LOST`; report Network and let the confirmation fetch decide. Add a test next to
  `test_keep_alive_stale_connection_retried_once` (test/test_ha_client/test_main.cpp:765) that asserts
  "POST to /api/services is sent at most once".

### M2. Nothing bounds the awake time once setup() is done (watchdog, crash loops, OOM path)
- Evidence:
  - `src/boot.cpp:402-406` `setupDone()` removes the loop task from the TWDT and resets the crash counter. No
    watchdog covers loop() afterwards, so a hang (unbounded library wait, SD/SPI stall, mutex deadlock) keeps the
    device awake, with Wi-Fi on, until the battery is empty.
  - The crash guard only counts crashes before `setupDone()`. A deterministic panic in loop() (e.g. while
    applying a fetch) reboots forever with a Wi-Fi connect each time. There is no give-up path.
  - `src/main.cpp:165-173` frame-allocation failure: `showError("Out of memory")`, then `setupDone()` and
    `return`. But Wi-Fi and the net task were already started (`:153-156`), and `loop()` (`:254-258`) only
    polls serial: the device never sleeps. Also `present::showError` (`src/present.cpp:449`) does not invalidate
    the screen record (unlike `showNotice`). After a later restart, `bootFrame` diffs against tiles the panel no
    longer shows, so the error text can stay partly on screen.
- Fix: keep loop() on the TWDT with a generous timeout (30-60 s), fed once per pass and inside the known long
  blocking paths (OTA install loop, provisioning receive, burst/idle fulls). Count crashes until the first
  successful sleep (or N minutes of uptime), not until setupDone. In the OOM path, call
  `invalidateRecord`/`showNotice` and sleep (button wake) instead of idling.

### M3. Scheduled wake without a valid record does two GC16 full refreshes
- Evidence: in `src/present.cpp` `bootFrame`, the `if (!have)` branch (`:551`) runs `fullRefresh(true, ...)`
  (white + content) before the `g_deferred` check (`:566`). `goToSleep` then calls
  `finishScheduled(true)`, which does a third full (`src/app_sleep.cpp:85-89`).
- Why it matters: after every firmware update, crash or record loss, the first scheduled wake costs ~2-3 s of
  extra GC16 waveforms and panel flashes in the dark. This breaks the "one GC16 full per scheduled wake" decision
  (ARCHITECTURE 2026-11-02).
- Fix: when `g_deferred`, skip the immediate full. Set `g_deferChanged = true` plus a "white first" flag and let
  `finishScheduled` do one white+content full.

### M4. No CI on push; several test suites are never run automatically
- Evidence: `.github/workflows/release.yml` runs `pio test -e native` only on `v*` tags. `pages.yml` runs only
  site/python checks. `site/test/*.test.js`, `site/test/test_jsguard.py` and `tools/tests/test_provision.py`
  are not run by any workflow.
- Fix: add a small `ci.yml` (push/PR): `pio test -e native`, `pio run -e m5paper`, `node --test site/test`,
  `python3 -m pytest tools/tests site/test`. Pin third-party actions by SHA (`dtolnay/rust-toolchain@stable`
  floats).

### M5. Wi-Fi modem sleep is off for the whole session, including "Keep awake: Always" on battery
- Evidence: `src/net.cpp:341` `WiFi.setSleep(false)` at connect; nothing turns modem sleep on again
  (`:442` sets false again before power-down). The comment "powerDown() releases it" does not match the code.
- Why it matters: in long awake sessions (Always / On USB on a weak charger) the radio stays at full current
  (~100 mA versus ~20-30 mA with MIN_MODEM). The battery-use estimate assumes 80 mA awake idle.
- Fix: switch to `WIFI_PS_MIN_MODEM` after the first fetch when no user command is queued (or only while staying
  awake), and back to no-sleep on a tap. Measure tap latency before deciding.

### M6. OTA trusts GitHub + TLS only; shared strings are read across tasks
- Evidence: `src/ota.cpp:18` fixed release URL. The SHA-256 comes from `manifest.json` on the same origin
  (`installBody`), so it protects against transfer damage only, not against a compromised release or account.
  There is no image signature (no secure boot or signed app). `g_avail`/`g_err` (`:230, 212, 326`) are written by
  the job task while the UI may read them (`src/app_settings.cpp:148` `fwSnapshot()` when Settings opens during an
  automatic check).
- Fix: sign `firmware.bin` (ed25519 or ECDSA) in release.yml and verify the signature against a public key
  compiled into the firmware before `esp_ota_set_boot_partition`. Copy the strings to the UI only after `done`
  (or guard them with a mutex).

---

## LOW

- L1. Tile/record identity uses `__DATE__ __TIME__` of main.cpp only (`src/main.cpp:18-22`). Incremental dev
  builds that change rendering but not main.cpp keep the old id, so `bootFrame` trusts a record drawn by other
  code. Use `buildinfo` commit + dirty flag + a hash of the build time from tools/build_info.py.
- L2. Fixed HA entity ids `sensor.m5paper_*` (`lib/device_report/src/device_report.cpp:34-37`): two devices on
  one HA overwrite each other's battery/temperature. Add a per-device suffix (MAC tail or config key).
- L3. HTTPS works only with public CAs (`src/ha_transport_arduino.cpp:72-82`, bundle only). A LAN HA on https
  with its own CA or a self-signed certificate cannot be used. Option: an optional `ca:` PEM in secrets.yaml
  (`setCACert`).
- L4. Provisioning: the received secrets buffer is freed without being wiped (`src/provision.cpp`
  `receive()`, `free(buf)`). Use `memset`/`mbedtls_platform_zeroize` before freeing when the name is
  secrets.yaml.
- L5. DNS is resolved twice per new connection: the `WiFi.hostByName` result is unused
  (`src/ha_transport_arduino.cpp:98`), and `connect(host, ...)` resolves again. Connect to the `IPAddress`
  (keeping the SNI host for TLS) or drop the timing-only lookup.
- L6. Rocker held at sleep entry for more than 1.5 s: ext1 ALL_LOW wakes at once
  (`src/sleep.cpp:303-306`), which gives a short sleep/wake loop until it is released.
- L7. The net task polls its queue every 20 ms forever (`src/net_task.cpp:97-100`). A task notification or a
  semaphore in `post()` would let core 0 idle.
- L8. Dead or misleading code: `boot::log("version", "017")` (`src/main.cpp:170, 207`); `BOOT: dbg_*` trace lines
  (`src/boot.cpp:184, 230, 248, 256, 513`); `src/config_sd.cpp:35` comment says it deselects the display but
  drives the SD CS (pin 4); platformio.ini comments are misplaced (the "E-ink persistence" comment sits inside
  `[env:probe]`, and the "Test build: serial test commands" comment sits above `[env:epdtest]`'s build_flags
  although it describes m5paper-debug). The native env has no `-Wall -Wextra`.
- L9. A session restart (`startSession(now, true)`, `src/app_session.cpp:17-22`) replaces the driver while a
  service call of the old epoch may be in flight. The call still runs on HA, but its result is dropped and the
  mark cleared. The 047 wait covers only `actionPending()`, not a call whose mark is already Confirmed but whose
  follow-up fetch is pending. Minor.

---

## Docs drift (docs vs code)
- ARCHITECTURE.md:3 says "first draft, hardware not yet available". :31 says "Two PlatformIO envs" (there are 8).
  :61 and :66-70 say credentials live in `connection:` of config.yaml (superseded by secrets.yaml, decision
  2026-10-28). :47 mentions TLS session resumption (task 027 deferred). :125-133 describe the wake modes
  `button`/`touch` (superseded by task 085 sleep modes). The modules table (:81-92) lacks history, media_art,
  screen_track, ota_logic, provision, sleep_screen, etc.
- ARCHITECTURE.md:247 "normal wakes never touch the SD card": every wake mounts the SD for `config_sd::stat`
  (`src/app_config.cpp:199`, `src/config_sd.cpp:100-113`). Either document the cost (logged as `config_sd_ms`)
  or skip the stat on scheduled wakes.
- docs/tasks/README.md: the "Open items (2026-10-30)" and "Resume point (2026-11-03)" sections are stale (the
  branches named there were merged long ago).
- The SKILL says the port is a CP2104 (true for this board); ARCHITECTURE's flasher section says autodetect by
  CH9102 VID:PID 1a86:55d4 only. Worth a note that both bridges exist (the web install was verified on CP2104).

## Test gaps
- The config source decision (cache / SD / demo / error) lives in `src/app_config.cpp`, so it cannot be
  unit-tested (see H2). Move it to lib/app_logic.
- No test pins "a service call is sent at most once" (M1); the existing test pins the retry.
- Wakelog/debug-log thread safety cannot be tested natively today. If the buffer moves into lib/app_logic, a
  native test with two std::threads would catch H1.
- present.cpp policy that is not in lib/ (bootFrame branch order: deferred vs invalid record, M3; the
  early-clear adoption path) has no native coverage. `fullRefresh`/`bootFrame` decisions could become a pure
  "plan" function like `tab_clean.h`/`burst_clean.h`.

## Checked and fine (no finding)
- Keep-alive drops the socket on an origin change (`keep_alive.cpp:12-15`). The art token goes only to the same
  origin (`art_fetch.cpp` `planFetch`, userinfo trick rejected). WS frame decoder limits and the RSV/64-bit length
  checks. YAML depth and alias limits. Entity-id validation before Jinja templates. Debug-log secret filter.
  Wi-Fi `persistent(false)`. Bounded BUSY waits with recovery. Sleep-hold cap. Lane queue requeue/drop logic.
  NVS write volume of the screen record (510 tiles × 2 B, fine for wear).
