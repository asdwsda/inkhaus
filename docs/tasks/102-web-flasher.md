# 102: Web flasher on the project site

Status: done
Depends on: 061-065 (native installer, release workflow, site)
UI spec: docs/ui/installer.md (states reused; page layout by ui-designer, mockup in docs/ui/experiments/)
Needs device: yes (one install + one update from Chrome; PC part first)

## Scope
A browser installer on the GitHub Pages site (`/install/`), based on ESP Web Tools (`esp-web-install-button`,
Web Serial; Chrome/Edge/Opera desktop only). Suggested by a forum user (2026-11-03): less to maintain than three
native installers. The native installers stay as the fallback (Firefox/Safari, offline).
- Hosting: GitHub Pages serves HTTPS, which Web Serial needs. Release assets on github.com can't be fetched from
  the browser (the download redirect has no CORS headers), so the pages workflow copies the latest release's
  four segment files + manifest into `site/dist/install/firmware/` (it already runs on `release: published`).
- ESP Web Tools manifest generated from our `manifest.json`: chip ESP32, parts bootloader 0x1000,
  partitions 0x8000, boot_app0 0xe000, firmware 0x10000 (offsets read from manifest.json, never hard-coded).
- No erase on update (`new_install_prompt_erase: false`), so NVS (settings, Wi-Fi state, config cache) survives,
  same rule as the native installer (ARCHITECTURE "Flasher tool").
- Page: what it does, browser note, CH9102 driver link (Windows), "then copy config.yaml + secrets.yaml to the SD card"
  (link docs/CONFIG.md); unsupported browser -> link to the native installers.
- Pin the ESP Web Tools version (self-hosted copy or a pinned CDN URL with SRI); site/check.py keeps the weight check.
- Several releases (user 2026-11-03): version picker, newest stable preselected, "Show pre-releases" toggle
  (same idea as the native installer). The workflow copies the last 3 stable releases + the single latest pre-release (user 2026-11-04, was 5 + newer pre-releases) into `install/firmware/<tag>/` with one ESP Web Tools manifest each, plus `install/releases.json`
  (tag, date, prerelease, size). Releases without a `manifest.json` asset are skipped. Picking an older version than
  the newest shows a downgrade note ("settings from newer versions may be reset").
- Size: one release is ~2.3 MB (firmware.bin 2.2 MB), so 5-6 releases are ~12-14 MB. GitHub Pages limits: published
  site <= 1 GB, soft bandwidth 100 GB/month (~40k installs). Files live only in the deploy artifact, not in git.
- Out of scope: YAML upload over Web Serial (task 020 protocol) -> later task.

## Files to touch
- site/ (build.py, new install page, styles), .github/workflows/pages.yml
- tools/ (manifest conversion script, e.g. tools/make_webflash_manifest.py) + a native/python test for it
- docs/FLASHER.md (web option first), README links

## Acceptance criteria
- [ ] Script test: ESP Web Tools manifest from a sample manifest.json has the 4 parts with correct offsets, no erase.
- [ ] `python3 site/build.py && python3 site/check.py` pass locally with a sample release folder.
- [ ] Pages workflow copies the release files (dry run documented; same-origin URLs in the generated manifest).
- [ ] releases.json + picker: newest stable preselected, pre-release toggle, downgrade note; release count cap tested.
- [ ] Device check: fresh install from Chrome boots; update install keeps settings (Settings values unchanged).
- [ ] Page in Firefox shows the "use Chrome/Edge or the native installer" note.

## Notes
- Decision (architect, 2026-11-03): ESP Web Tools on GitHub Pages, firmware copied into the site per release.
  Reason: Web Serial needs HTTPS and same-origin/CORS files; Pages gives both for free.
- Round 2 (2026-11-03): /install/ page built per docs/ui/web-flasher.md (cap 5 then 3, workflow_run trigger, FLASHER.md/README).
  Dry run: `python3 tools/make_webflash_manifest.py build --api tools/testdata/webflash/api.json --src tools/testdata/webflash/src --out /tmp/x`
  writes firmware/<tag>/ + releases.json (4 releases, relative same-origin manifest paths). Page weight 1012 KiB (limit 1024).
- Follow-up (2026-11-04): cap 3 stable + latest pre-release; Changelog button (notes + url in releases.json, safe-text dialog).
