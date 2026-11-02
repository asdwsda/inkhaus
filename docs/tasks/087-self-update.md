# 087: Firmware self-update (check weekly, offer in Settings)

Status: testing
Depends on: 061 (release manifest), 084/085 (power, usb sense)
UI spec: docs/ui/settings.md (new section: firmware row on Tools + overview hint) — ui-designer
Needs device: yes (OTA from a local test server, then from a real GitHub release)

## Scope
User 2026-11-02: when a new version is released, the device notices and offers the upgrade. Settings only:
no popup, nothing that interrupts the user.

Architecture (architect, 2026-11-02):
- Source: the latest GitHub release of asdwsda/inkhaus. The release workflow already publishes firmware.bin
  and manifest.json (tools/make_manifest.py). Add to the manifest what OTA needs if missing: version,
  firmware.bin URL/name, size, SHA-256. Fetch over HTTPS with the embedded CA bundle (certs/x509_crt_bundle);
  follow the GitHub asset redirect.
- When to check: at a cold boot (the splash path) and then at most once per 7 days, only piggy-backing on a wake
  where Wi-Fi is already up (never a wake just for this). Last-check epoch + found version in NVS. Failures are
  silent (retry at the next eligible wake, max once per day).
- Indication: Settings only. Tools page row "Firmware 1.2.1" with hint "Update to 1.3.0 available"; the Tools
  category summary on the overview shows "Update available". Nothing on the tab screens.
- Install (user action only): row -> confirm -> download into the inactive OTA slot (partitions already
  default_16MB with app0/app1) with a progress bar, SHA-256 check, set boot partition, reboot.
  Allowed only on USB (usb_rx) or battery >= 50 %; otherwise the row says why. Wake/idle sleep is suspended
  during the download. Settings and config on SD/NVS are kept.
- Safety without bootloader rollback (Arduino bootloader has it off): NVS "ota_pending" + boot counter; the new
  firmware clears it once healthy (UI up and HA connected, or 60 s up). 3 boots without that -> switch back to the
  previous slot and log it. A failed download leaves the running firmware untouched.
- Version compare: semver of FW_VERSION (include/version.h) vs the release tag; pre-releases ignored.
- Test path: a debug-build-only override URL to a local HTTP server (tools/) serving a manifest + bin, so OTA can
  be tested before a real release.
- Out of scope: automatic install, release notes on the device (one line max), downgrade.

## Files to touch
- lib/ (new ota_logic: semver compare, check schedule, manifest parse, health/rollback decision; native tests)
- src/ota.{h,cpp} (HTTPS fetch, Update/esp_ota write, SHA-256, boot partition), src/main.cpp (hooks)
- lib/settings_ui/, src/render/settings_screen.cpp (row, confirm, progress)
- tools/make_manifest.py, .github/workflows/release.yml (manifest fields), tools/ota_test_server.py (new)
- test/

## Acceptance criteria
- [ ] Native tests: semver compare (incl. 1.10 > 1.9, pre-release ignored), check schedule (cold boot, 7 days,
      1/day after failure, never without Wi-Fi), manifest parse (missing fields -> no offer), rollback decision.
- [ ] Device, local server: newer version shows in Settings only; install on USB updates and reboots into it;
      settings/config kept; a corrupted bin (bad SHA) is rejected and the old firmware keeps running.
- [ ] Device: a firmware that never becomes healthy rolls back after 3 boots.
- [ ] Battery < 50 % and not on USB: install is refused with a reason.
- [ ] Wake log shows no extra awake time on wakes without a due check.
- [ ] `pio run` and `pio test -e native` pass; release workflow builds the new manifest (dry run).

- Architect 2026-11-02 (OPEN-S12): a version that rolled back is suppressed. Superseded 2026-11-02 by the user: no suppression, no Skip (see the last bullet).
- Architect 2026-11-02 (OPEN-S12): a version that rolled back is suppressed; offer again only for a newer version.
- User 2026-11-02 (OPEN-S13): add "Check now": tapping the up-to-date Firmware row runs a manual check (needs Wi-Fi; shows Checking..., then the result in the hint). Spec: ui-designer to add the row state.
- Review fixes (architect 2026-11-02): (1) count the boot at the very top of setup() and let the crash guard roll
  back while an update is pending; (2) healthy = setup finished, first frame drawn and 10 s in loop() without a
  crash — network/HA state doesn't matter (a router outage must not roll back a good firmware); (3) close the HA
  socket before the check/download and require >= 40 KB largest free block, else skip to the next eligible wake;
  (4) cap a chunked manifest at 8 KB; (5) doc: trust = GitHub + TLS + manifest SHA-256, no signature;
  (6) cold-boot check at most once per hour.
- Progress 2026-11-02 (firmware-dev): review fixes 1-6 and the Settings UI (8.6) are in the code, PC build + native tests pass. Next: device
  tests (local server, bad SHA, rollback after 3 boots, battery refusal). Decisions: health is also marked by a deliberate sleep after the
  first frame (a short scheduled wake must not count as unhealthy); a rolled-back or skipped version is stored in NVS "skip" and offered
  again only for a newer one; USB for installBlock is `power::usbAtSleep()` (helper `usbNow()` in main.cpp, swap to `power::usbPresent()`
  when the 085 probe is merged); trust model (GitHub + TLS + manifest SHA-256, no signature) is in src/ota.h.
- Change 2026-11-02 (user): no Skip. The confirm page has only `Yes, install` (UP) and `No, cancel` (DOWN; PUSH/LONG_PUSH = No).
  Remove the NVS "skip" store entirely: the device always offers the latest release that is newer than the running firmware,
  including one that was rolled back (harmless: install needs an explicit Yes in Settings). After a rollback, the Firmware row
  shows the existing rolled-back note once; no version is suppressed.
